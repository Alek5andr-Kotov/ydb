#include "counter_toucher.h"

TCounterToucher::TCounterToucher(std::atomic<size_t>& v) :
    V(v)
{
    ++V;
}

TCounterToucher::~TCounterToucher()
{
    --V;
}
