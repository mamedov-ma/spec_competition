#pragma once
#include "data_type.h"
#include "TickerTools.h"
#include "config.h"

class MdReceiver;
class DataFeed {
    friend MdReceiver;

public:
    DataFeed(size_t *_tickers_in_recovery);
    ~DataFeed() = default;

    void proc_data(const IncrementalGroup &group, uint8_t update_cnt);
    void proc_data(const SnapshotInstrumentGroup *snap, uint8_t entry_cnt);

private:
    void apply_tick_ordinary(const BookUpdate *book_updates, uint8_t update_cnt);
    void store_inc_updates(const BookUpdate *book_updates, uint8_t update_cnt, int32_t change_no);
    void apply_stored_inc_update(const StoredUpdate &upd);
    void cleanup_after_recovery_from_snapshotes();
    void start_recovery_from_snaphot_feed();
    void clear_obook();

    TickerTools ticker_tools_;
    std::vector<StoredUpdate> stored_inc_updates_;
    size_t *tickers_in_recovery_count_;
    int32_t inc_changeno_;
    int32_t change_no_when_snaps_generated_;
    bool in_recovery_cicle_;
};