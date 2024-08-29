#pragma once

#include "parameters.h"

#include <util/generic/vector.h>

#include <atomic>
#include <thread>
#include <cstddef>
#include <barrier>
#include <functional>

class TWorkloadDriver {
public:
    TWorkloadDriver();

    void StartWorkloadThreads(const TParameters& params);
    void Run();
    void StopWorkloadThreads();

private:
    using TBarrier = std::barrier<std::function<void ()>>;

    static void OnTerminate(int);
    void RunWorkload(size_t threadId, size_t txCount);
    bool StopEngine() const;

    void StartWorkThreads(size_t begin, size_t end,
                          size_t txCount);

    static std::atomic<bool> Quit;
    TVector<std::thread> Threads;
//    std::atomic<size_t> WorkerCount;
    TParameters Parameters;
    std::shared_ptr<TBarrier> Barrier;
};
