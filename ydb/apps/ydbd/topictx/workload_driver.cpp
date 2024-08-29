#include "workload_driver.h"
#include "logging.h"
#include "counter_toucher.h"
#include "application.h"
#include <util/datetime/base.h>

#include <semaphore>
#include <latch>
#include <barrier>

std::atomic<bool> TWorkloadDriver::Quit = false;

TWorkloadDriver::TWorkloadDriver()
{
    signal(SIGTERM, &TWorkloadDriver::OnTerminate);
    signal(SIGINT, &TWorkloadDriver::OnTerminate);
}

void TWorkloadDriver::OnTerminate(int)
{
    Quit = true;
}

void TWorkloadDriver::StartWorkThreads(size_t begin, size_t end,
                                       size_t txCount)
{
    for (size_t i = begin; i < end; ++i) {
        Threads.emplace_back([this, i, txCount](){ RunWorkload(i, txCount); });
    }
}

void TWorkloadDriver::StartWorkloadThreads(const TParameters& params)
{
//    WorkerCount = 0;
    Parameters = params;

    LOG_I("random seed " << Parameters.RandomSeed);
    SetRandomSeed(params.RandomSeed);

    auto onWorkThreadsSynced = []() {
    };

    LOG_I("application starting");
    Barrier = std::make_shared<TBarrier>(1 + Parameters.ThreadCount, onWorkThreadsSynced);

    size_t rem = Parameters.TxCount % Parameters.ThreadCount;
    size_t quo = Parameters.TxCount / Parameters.ThreadCount;

    StartWorkThreads(0, rem, quo + 1);
    StartWorkThreads(rem, Parameters.ThreadCount, quo);

    Barrier->arrive_and_wait();
    LOG_I("application started");


//    while (WorkerCount < Parameters.ThreadCount) {
//        for (size_t i = 0; (i < 10) && !Quit && (WorkerCount < Parameters.ThreadCount); ++i) {
//            Sleep(TDuration::MilliSeconds(1'000));
//        }
//        LOG_I(WorkerCount.load());
//    }
//    LOG_I(WorkerCount.load());
//    LOG_I("=== started ===");
}

TString MakeThreadLogPrefix(size_t n)
{
    TStringStream ss("thread-");
    ss << n;
    return ss.Str();
}

void TWorkloadDriver::RunWorkload(size_t threadId, size_t txCount)
{
//    TCounterToucher Y_GENERATE_UNIQUE_ID(x)(WorkerCount);

    SetLogPrefix(MakeThreadLogPrefix(threadId));

    LOG_I("thread started threadId=" << threadId << ", txCount=" << txCount);

    TVector<ui32> partitionIds;
    for (size_t i = 0; i < Parameters.PartitionCount; ++i) {
        partitionIds.emplace_back(i);
    }

    TApplication app(Parameters);

    app.CreateWriteSessions(partitionIds);
    Barrier->arrive_and_wait();

    auto stopEngine = [txCount](size_t k) {
        return (k >= txCount) || Quit;
    };

    for (size_t i = 0; !stopEngine(i); ) {
        for (size_t width = 1; (width <= Parameters.PartitionCount) && !stopEngine(i); ++width, ++i) {
            app.ExecuteTransaction(width);
        }
        for (size_t width = Parameters.PartitionCount - 1; (width > 1) && !stopEngine(i); --width, ++i) {
            app.ExecuteTransaction(width);
        }
//#if 0
//        app.BeginTransaction();
//        app.WriteMessages(partitionIds);
//        app.CommitTransaction();
//#else
//        Sleep(TDuration::Seconds(1));
//#endif
    }

//    app.DestroyWriteSessions();
    Barrier->arrive_and_wait();

    LOG_I("thread stopped");
}

//bool TWorkloadDriver::StopEngine() const
//{
////    return Quit || (WorkerCount == 0);
//    return Quit;
//}

void TWorkloadDriver::Run()
{
//    for (size_t i = 0; !StopEngine(); ++i) {
//        if (i % 10 == Max<size_t>() % 10) {
//            LOG_I(WorkerCount.load())
//        }
//
//        Sleep(TDuration::MilliSeconds(1'000));
//    }

    Barrier->arrive_and_wait();
//    while (!StopEngine()) {
//        Sleep(TDuration::MilliSeconds(1'000));
//    }
}

void TWorkloadDriver::StopWorkloadThreads()
{
    LOG_I("application stopping");
//    for (size_t i = 0; WorkerCount; ++i) {
//        if (i % 10 == Max<size_t>() % 10) {
//            LOG_I(WorkerCount.load());
//        }
//
//        Sleep(TDuration::MilliSeconds(1'000));
//    }
    for (auto& t : Threads) {
        t.join();
    }
//    LOG_I(WorkerCount.load());
    LOG_I("application stoped");
}
