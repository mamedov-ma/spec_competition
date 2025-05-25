#include <cstdint>

#include "DataFeed.h"

DataFeed::DataFeed(size_t *_tickers_in_recovery_count)
    : tickers_in_recovery_count_(_tickers_in_recovery_count),
      inc_changeno_(0),
      change_no_when_snaps_generated_(0),
      in_recovery_cicle_(true)
{
    stored_inc_updates_.reserve(1000);
}

void DataFeed::proc_data(const IncrementalGroup &group, uint8_t update_cnt)
{
    const int32_t &tick_change_no = group.change_no;

    if (!in_recovery_cicle_) [[likely]] {
        if (tick_change_no == inc_changeno_ + 1) [[likely]] {
            inc_changeno_ = tick_change_no;
            apply_tick_ordinary(group.book_updates, update_cnt);
        } else if (tick_change_no > inc_changeno_) [[unlikely]] {
            in_recovery_cicle_ = true;
            start_recovery_from_snaphot_feed();
            store_inc_updates(group.book_updates, update_cnt, group.change_no);
        }
    } else [[unlikely]] {
        store_inc_updates(group.book_updates, update_cnt, group.change_no);
    }
}

void DataFeed::proc_data(const SnapshotInstrumentGroup *snap, const uint8_t entry_cnt)
{
    if (!in_recovery_cicle_) [[likely]] {
        return;
    }
    change_no_when_snaps_generated_ = snap->trade_info.change_no;
    if (change_no_when_snaps_generated_ <= ticker_tools_.change_no_when_lost_data) {
        return;
    }
    inc_changeno_ = snap->trade_info.change_no;
    ticker_tools_.instrument_id = snap->info.instrument_id;
    ticker_tools_.change_no_from_snapshot = change_no_when_snaps_generated_;
    ticker_tools_.obook.set_ref_and_tick_size(snap->info.tick_size, snap->info.reference_price);

    for (size_t i = 0; i < entry_cnt; ++i) {
        ticker_tools_.obook.new_order_from_snap(snap->entries[i]);
    }

    cleanup_after_recovery_from_snapshotes();
    change_no_when_snaps_generated_ = 0;
}

void DataFeed::apply_tick_ordinary(const BookUpdate *book_updates, uint8_t update_cnt)
{
    for (uint8_t i = 0; i < update_cnt; ++i) {
        switch (book_updates[i].event_type) {
            case '1':
                ticker_tools_.obook.new_order(book_updates[i]);
                break;
            case '2':
                ticker_tools_.obook.modify_order(book_updates[i]);
                break;
            case '3':
                ticker_tools_.obook.delete_order(book_updates[i]);
                break;
            default:
                break;
        }
    }
    ticker_tools_.obook.trim();
    ticker_tools_.obook.update_vwap();
}

void DataFeed::store_inc_updates(const BookUpdate *book_updates, uint8_t update_cnt, int32_t change_no)
{
    for (uint8_t i = 0; i < update_cnt; ++i) {
        StoredUpdate update_to_store;
        update_to_store.md_update_action = book_updates[i].event_type;
        update_to_store.change_no = change_no;
        update_to_store.update = book_updates[i];
        stored_inc_updates_.emplace_back(update_to_store);
    }
    StoredUpdate update_to_store;
    update_to_store.md_update_action = '4';
    update_to_store.change_no = change_no;
    stored_inc_updates_.emplace_back(update_to_store);

    if (ticker_tools_.change_no_when_lost_data == NAN_CHANGENO) {
        ticker_tools_.change_no_when_lost_data = change_no;
    }
}

void DataFeed::clear_obook()
{
    ticker_tools_.obook.clear();
}

void DataFeed::start_recovery_from_snaphot_feed()
{
    ++(*tickers_in_recovery_count_);
    clear_obook();
    stored_inc_updates_.clear();
    change_no_when_snaps_generated_ = 0;
    ticker_tools_.change_no_when_lost_data = NAN_CHANGENO;
}

void DataFeed::cleanup_after_recovery_from_snapshotes()
{
    --(*tickers_in_recovery_count_);
    in_recovery_cicle_ = false;
    for (const auto &stored_upd : stored_inc_updates_) {
        if (stored_upd.change_no <= ticker_tools_.change_no_from_snapshot) {
            continue;
        }
        apply_stored_inc_update(stored_upd);
    }
    stored_inc_updates_.clear();
}

void DataFeed::apply_stored_inc_update(const StoredUpdate &upd)
{
    switch (upd.md_update_action) {
        case '1':
            ticker_tools_.obook.new_order(upd.update);
            break;
        case '2':
            ticker_tools_.obook.modify_order(upd.update);
            break;
        case '3':
            ticker_tools_.obook.delete_order(upd.update);
            break;
        case '4':
            ticker_tools_.obook.trim();
            break;
        default:
            break;
    }
}
