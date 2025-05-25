#pragma once
#include "Orderbook.h"

struct alignas(64) TickerTools {
    TickerTools() : change_no_when_lost_data(0), change_no_from_snapshot(0) {}

    Orderbook obook;
    int32_t change_no_when_lost_data;
    int32_t change_no_from_snapshot;
    int instrument_id;
};