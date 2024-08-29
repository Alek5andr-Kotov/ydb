#pragma once

#include <atomic>
#include <cstddef>

class TCounterToucher {
public:
    explicit TCounterToucher(std::atomic<size_t>& v);
    TCounterToucher(const TCounterToucher& rhs) = delete;
    TCounterToucher(TCounterToucher&& rhs) = delete;
    ~TCounterToucher();

    TCounterToucher& operator=(const TCounterToucher& rhs) = delete;
    TCounterToucher& operator=(TCounterToucher&& rhs) = delete;

private:
    std::atomic<size_t>& V;
};
