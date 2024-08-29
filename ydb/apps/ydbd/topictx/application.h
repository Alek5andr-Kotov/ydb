#pragma once

#include <ydb/public/sdk/cpp/client/ydb_table/table.h>
#include <ydb/public/sdk/cpp/client/ydb_topic/topic.h>

#include <util/generic/maybe.h>
#include <util/generic/vector.h>
#include <util/system/types.h>

#include <atomic>
#include <memory>
#include <thread>
#include <cstddef>

namespace NTopic = NYdb::NTopic;
namespace NTable = NYdb::NTable;

struct TParameters;

class TApplication {
public:
    explicit TApplication(const TParameters& params);

    void CreateWriteSessions(const TVector<ui32>& partitionIds);
    void ExecuteTransaction(size_t width);

//    void BeginTransaction();
//    void CommitTransaction();
//
//    void CreateWriteSessions(const TVector<ui32>& partitionIds);
//    void WriteMessages(const TVector<ui32>& partitionIds);
//
//    bool Ready();

private:
    using TTopicWriteSessionPtr = std::shared_ptr<NTopic::IWriteSession>;

    struct TTransactionContext {
        TMaybe<NTable::TTransaction> Transaction;
        NThreading::TPromise<void> WriteCompleted;
        std::atomic<size_t> InflyCount = 0;
    };

    struct TWriteSessionContext {
        ui32 PartitionId = Max<ui32>();
        TTopicWriteSessionPtr Session;

        std::mutex Mutex;
        NThreading::TPromise<NTopic::TContinuationToken> ContinuationToken;
//        NThreading::TFuture<void> WriteCompleted;
        bool Ready = false;

//        std::atomic<size_t> InflyCount = 0;
//        std::atomic<size_t> WriteCount = 0;
//        std::atomic<size_t> AckCount = 0;
        std::atomic<bool> Closed = false;
    };

    NTable::TSession BeginTableSession();
    void CreateWriteSession(ui32 partitionId);
    void WriteMessages(ui32 partitionId);
    void WriteMessage(TWriteSessionContext& context, size_t size);

    void BeginTransaction();
    void CommitTransaction();

    void WaitForAcks();

    TString MakeProducerId(ui32 partitionId);
    NTopic::TWriteSessionSettings::TEventHandlers MakeWriteEventHandlers(ui32 partitionId);

    void OnReadyToAccept(ui32 partitionId,
                         NTopic::TWriteSessionEvent::TReadyToAcceptEvent& ev);
    void OnAcks(ui32 partitionId,
                const NTopic::TWriteSessionEvent::TAcksEvent& ev);
    void OnSessionClosed(ui32 partitionId,
                         const NTopic::TSessionClosedEvent& ev);

    static NYdb::TDriverConfig GetDriverConfig(const TParameters& params);

    const TParameters& Parameters;
    NYdb::TDriver Driver;
    TString ProducerIdPrefix;
    THashMap<ui32, TWriteSessionContext> TopicWriteSessions;
    //TMaybe<TTransactionContext> Tx;

    NThreading::TPromise<void> WriteSessionsAreReady;
    std::atomic<size_t> ReadyWriteSessionCount = 0;

    TMaybe<NTable::TTransaction> Tx;
    NThreading::TPromise<void> WriteCompleted;
    std::atomic<size_t> InflyCount = 0;
};
