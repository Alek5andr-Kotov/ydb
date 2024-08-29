#include "application.h"
#include "logging.h"
#include "parameters.h"
#include <util/generic/guid.h>
#include <random>

TApplication::TApplication(const TParameters& params) :
    Parameters(params),
    Driver(GetDriverConfig(Parameters)),
    ProducerIdPrefix(TGUID::CreateTimebased().AsUuidString() + "_")
{
}

NYdb::TDriverConfig TApplication::GetDriverConfig(const TParameters& params)
{
    return NYdb::TDriverConfig()
        .SetEndpoint(params.YdbEndpoint)
        .SetDatabase(params.YdbDatabase)
        .SetAuthToken(params.YdbToken)
        .SetLog(GetLogBackend())
        ;
}

//bool TApplication::Ready()
//{
//    return TopicWriteSessions.size() == ReadyWriteSessionCount.load();
//}

void TApplication::BeginTransaction()
{
    LOG_D("BeginTransaction");
    auto session = BeginTableSession();
    auto result = session.BeginTransaction().ExtractValueSync();
    Y_ABORT_UNLESS(result.IsSuccess(), "%s", result.GetIssues().ToString().data());
    Tx = result.GetTransaction();
    LOG_D("TxId=" << Tx->GetId());
}

void TApplication::CommitTransaction()
{
    LOG_D("CommitTransaction TxId=" << Tx->GetId());
    auto result = Tx->Commit().ExtractValueSync();
    Y_ABORT_UNLESS(result.IsSuccess(), "%s", result.GetIssues().ToString().data());
    Tx = Nothing();
}

void TApplication::WaitForAcks()
{
    LOG_D("WaitForAcks");
    WriteCompleted.GetFuture().Wait();
    WriteCompleted = NThreading::NewPromise<void>();
//    while (true) {
//        bool hasInfly = false;
//        for (auto& [partitionId, context] : TopicWriteSessions) {
////            LOG_D("partitionId=" << partitionId <<
////                  ", InflyCount=" << context.InflyCount.load() <<
////                  ", WriteCount=" << context.WriteCount.load() <<
////                  ", AckCount=" << context.AckCount.load());
//            if ((context.InflyCount > 0) || (context.WriteCount != context.AckCount)) {
//                hasInfly = true;
//                break;
//            }
//        }
//
//        if (!hasInfly) {
//            break;
//        }
//    }
    LOG_D("all acks are received");
}

NTable::TSession TApplication::BeginTableSession()
{
    LOG_D("BeginTableSession");
    NTable::TTableClient client(Driver);
    auto result = client.CreateSession().ExtractValueSync();
    Y_ABORT_UNLESS(result.IsSuccess(), "%s", result.GetIssues().ToString().data());
    NTable::TSession session = result.GetSession();
    LOG_D("SessionId=" << session.GetId());
    return session;
}

void TApplication::CreateWriteSessions(const TVector<ui32>& partitionIds)
{
    LOG_D("CreateWriteSessions");

    WriteSessionsAreReady = NThreading::NewPromise();
    WriteCompleted = NThreading::NewPromise<void>();

    for (auto partitionId : partitionIds) {
        CreateWriteSession(partitionId);
    }

    LOG_D("wait for WriteSessionsAreReady");
    WriteSessionsAreReady.GetFuture().Wait();

    LOG_D("topic write sessions are created");
}

void TApplication::ExecuteTransaction(size_t width)
{
    LOG_D("ExecuteTransaction width=" << width);

    BeginTransaction();

    TVector<ui32> partitionIds;
    for (ui32 i = 0, e = TopicWriteSessions.size(); i < e; ++i) {
        partitionIds.push_back(i);
    }

    TVector<ui32> workPartititionIds;
    std::sample(partitionIds.begin(), partitionIds.end(),
                std::back_inserter(workPartititionIds),
                width,
                std::mt19937(std::random_device()()));

    for (ui32 partitionId : workPartititionIds) {
        WriteMessages(partitionId);
    }

    WaitForAcks();

    CommitTransaction();
}

NTopic::TWriteSessionSettings::TEventHandlers TApplication::MakeWriteEventHandlers(ui32 partitionId)
{
    NTopic::TWriteSessionSettings::TEventHandlers handlers;
    handlers
        .ReadyToAcceptHandler([this, partitionId](NTopic::TWriteSessionEvent::TReadyToAcceptEvent& ev) { OnReadyToAccept(partitionId, ev); })
        .AcksHandler([this, partitionId](const NTopic::TWriteSessionEvent::TAcksEvent& ev) { OnAcks(partitionId, ev); })
        .SessionClosedHandler([this, partitionId](const NTopic::TSessionClosedEvent& ev) { OnSessionClosed(partitionId, ev); })
        ;
    return handlers;
}

void TApplication::OnReadyToAccept(ui32 partitionId,
                                   NTopic::TWriteSessionEvent::TReadyToAcceptEvent& ev)
{
    LOG_D("OnReadyToAccept partitionId=" << partitionId);

    TWriteSessionContext& context = TopicWriteSessions.at(partitionId);
    std::lock_guard guard(context.Mutex);
    context.ContinuationToken.SetValue(std::move(ev.ContinuationToken));

    if (!context.Ready) {
        context.Ready = true;
        LOG_D("ReadyWriteSessionCount=" << ReadyWriteSessionCount.load() << ", TopicWriteSessions.size=" << TopicWriteSessions.size());
        if (++ReadyWriteSessionCount == TopicWriteSessions.size()) {
            LOG_D("signal WriteSessionsAreReady");
            WriteSessionsAreReady.SetValue();
        }
    }

    LOG_D("NotReadySessionCount=" << (TopicWriteSessions.size() - ReadyWriteSessionCount.load()));
}

void TApplication::OnAcks(ui32 partitionId,
                          const NTopic::TWriteSessionEvent::TAcksEvent& ev)
{
    LOG_D("OnAcks partitionId=" << partitionId);
//    TWriteSessionContext& context = TopicWriteSessions.at(partitionId);
//    LOG_D("> InflyCount=" << context.InflyCount.load() <<
//          ", WriteCount=" << context.WriteCount.load() <<
//          ", AckCount=" << context.AckCount.load());
    for (auto& ack : ev.Acks) {
        LOG_D("state=" << static_cast<int>(ack.State));
        if (ack.State == NTopic::TWriteSessionEvent::TWriteAck::EES_WRITTEN_IN_TX) {
            if (--InflyCount == 0) {
                LOG_D("all messages are written");
                WriteCompleted.SetValue();
            }
//            ++context.AckCount;
        }
    }
//    LOG_D("< InflyCount=" << context.InflyCount.load() <<
//          ", WriteCount=" << context.WriteCount.load() <<
//          ", AckCount=" << context.AckCount.load());
}

void TApplication::OnSessionClosed(ui32 partitionId,
                                   const NTopic::TSessionClosedEvent&)
{
    LOG_D("OnSessionClosed partitionId=" << partitionId);
    TWriteSessionContext& context = TopicWriteSessions.at(partitionId);
    context.Closed = true;
}

void TApplication::CreateWriteSession(ui32 partitionId)
{
    TString producerId = MakeProducerId(partitionId);
    auto settings = NTopic::TWriteSessionSettings()
        .Path(Parameters.TopicPath)
        .ProducerId(producerId)
        .MessageGroupId(producerId)
        .PartitionId(partitionId)
        .Codec(NTopic::ECodec::RAW)
        .EventHandlers(MakeWriteEventHandlers(partitionId));
    NTopic::TTopicClient client(Driver);

    TWriteSessionContext& context = TopicWriteSessions[partitionId];
    context.PartitionId = partitionId;
    context.Session = client.CreateWriteSession(settings);
    context.ContinuationToken = NThreading::NewPromise<NTopic::TContinuationToken>();
//    context.WriteCompleted = NThreading::NewPromise<void>();
    context.Ready = false;
//    context.InflyCount = 0;
//    context.WriteCount = 0;
//    context.AckCount = 0;
    context.Closed = false;
}

//void TApplication::WriteMessages(const TVector<ui32>& partitionIds)
//{
//    for (auto partitionId : partitionIds) {
//        WriteMessages(partitionId);
//    }
//}

//size_t RandomMessageSize()
//{
//    //return 10_KB + RandomNumber<size_t>(10_KB);
//    return 128_KB;
//}
//
//size_t RandomMessageCount()
//{
//    //return RandomNumber<size_t>(10);
//    return 256;
//}

void TApplication::WriteMessages(ui32 partitionId)
{
    LOG_D("write messages to partition " << partitionId);

    auto& context = TopicWriteSessions.at(partitionId);

    for (size_t remainSize = RandomNumber<size_t>(50_MB) + 1; remainSize > 0; ) {
        size_t messageSize = Min(RandomNumber<size_t>(100_KB) + 1, remainSize);
        remainSize -= messageSize;

        LOG_D("messageSize=" << messageSize << ", remainSize=" << remainSize);

        WriteMessage(context, messageSize);
    }

//    for (size_t i = 0, n = RandomMessageCount(); i < n; ++i) {
//        auto& context = TopicWriteSessions.at(partitionId);
//        WriteMessage(context);
//    }
}

void TApplication::WriteMessage(TWriteSessionContext& context, size_t size)
{
    context.ContinuationToken.GetFuture().Wait();

    auto msg = NTopic::TWriteMessage(TString(size, 'x'))
        .Tx(*Tx);

    ++InflyCount;
//    ++context.WriteCount;

    auto token = context.ContinuationToken.ExtractValue();
    context.ContinuationToken = NThreading::NewPromise<NTopic::TContinuationToken>();

    context.Session->Write(std::move(token), std::move(msg));
}

TString TApplication::MakeProducerId(ui32 partitionId)
{
    TString producerId = ProducerIdPrefix;
    producerId += std::to_string(partitionId);
    return producerId;
}
