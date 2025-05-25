
#include <iostream>
#include <cstring>
#include <immintrin.h>

#include "MdReceiver.h"
#include "vint_decoder.h"

MdReceiver::MdReceiver(const MdConfig &cfg, SPSCQueue &in_queue, SPSCQueue &out_queue)
    : in_queue_(in_queue),
      out_queue_(out_queue),
      tickers_in_recovery_count_(cfg.tickers_list.size()),
      all_names_resolved_(false)
{
    std::memset(inst_ids_, 255, sizeof(inst_ids_));
    data_feeds_collection_.reserve(cfg.tickers_list.size());
    for (size_t i = 0; i < cfg.tickers_list.size(); ++i) {
        data_feeds_collection_.emplace_back(&tickers_in_recovery_count_);
        unknown_tickers_.emplace(cfg.tickers_list[i], &data_feeds_collection_[i]);
    }
}

void MdReceiver::start()
{
    receive_md();
}

void MdReceiver::receive_md()
{
    constexpr size_t offset = 42;
    char *message_buffer_;
    result_buffer_ = out_queue_.get_ptr_to_write();

    for (;;) {
        in_queue_.read(message_buffer_);
        _mm_prefetch(result_buffer_, _MM_HINT_T0);
        _mm_prefetch(message_buffer_ + offset, _MM_HINT_T0);
        res_count_ = 0;

        switch (*(message_buffer_ + offset + 1)) {
            case 0x01U:
                [[likely]] proc_increment(message_buffer_ + offset);
                break;
            case 0x32U:
                [[unlikely]]
                if (tickers_in_recovery_count_ > 0) {
                    proc_snapshot(message_buffer_ + offset);
                }
                break;
            default:
                [[unlikely]] break;
        }

        *result_buffer_ = res_count_;
        result_buffer_ = out_queue_.write((res_count_ * 3 + 1) * sizeof(uint32_t));
        in_queue_.notify_read();
    }
}

void MdReceiver::proc_increment(const char *data)
{
    // std::cout << "[ENTER proc_increment]\n";
    static const __m512i va = _mm512_set1_epi8(0x03U);
    static const __m512i vb = _mm512_set1_epi8(0x00U);
    IncrementalGroup inc_group;
    size_t parsed = sizeof(IncrementalHeader);
    const size_t msg_len = sizeof(IncrementalHeader) + *reinterpret_cast<const uint16_t *>(data + 2);
    const FieldHeader *field_hdr;
    DataFeed *data_feed_ptr;
    uint32_t *write_pos;
    const uint8_t *cursor;
    const Orderbook::Wvap *wvap;
    uint8_t update_cnt;

    const __m512i instid_chunk = _mm512_load_si512(inst_ids_);

    for (; parsed + 64 <= msg_len; parsed += 63) {
        _mm_prefetch(data + parsed + 63, _MM_HINT_T0);
        const __m512i chunk = _mm512_loadu_si512(data + parsed);
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, va) & (_mm512_cmpeq_epi8_mask(chunk, vb) >> 1);

        for (; mask; mask &= mask - 1) {
            size_t offset = parsed + _tzcnt_u64(mask);
            const uint16_t instid = *reinterpret_cast<const uint16_t *>(data + offset + 4);
            if (!instid_bitmask_[instid]) [[likely]] {
                continue;
            }

            const __m512i pattern = _mm512_set1_epi16(instid);
            const __mmask32 instid_mask = _mm512_cmpeq_epi16_mask(instid_chunk, pattern);
            const int idx = _tzcnt_u32(instid_mask);
            data_feed_ptr = data_feed_by_id_[idx];
            field_hdr = reinterpret_cast<const FieldHeader *>(data + offset);
            offset += sizeof(FieldHeader);
            cursor = reinterpret_cast<const uint8_t *>(data) + offset + 2;
            inc_group.change_no = readVint3Bytes(cursor);
            offset += field_hdr->field_len;
            update_cnt = 0;

            while (offset < msg_len) {
                field_hdr = reinterpret_cast<const FieldHeader *>(data + offset);
                if (field_hdr->field_id != 0x1001U) {
                    offset += (field_hdr->field_id == 0x0003U) ? 0 : sizeof(FieldHeader) + field_hdr->field_len;
                    break;
                }
                offset += sizeof(FieldHeader);
                cursor = reinterpret_cast<const uint8_t *>(data) + offset + 2;
                BookUpdate &single_update = inc_group.book_updates[update_cnt];
                uint32_t packed = *reinterpret_cast<const uint32_t *>(data + offset);
                single_update.event_type = packed & 0xFF;
                single_update.md_entry_type = (packed >> 8) & 0xFF;
                single_update.price_level = readVint1Byte(cursor);
                single_update.price_offset = readVint2Bytes(cursor);
                single_update.volume = readVint2Bytes(cursor);
                offset += field_hdr->field_len;
                ++update_cnt;
            }
            data_feed_ptr->proc_data(inc_group, update_cnt);
            bool is_correct = data_feed_ptr->ticker_tools_.obook.ob_is_correct();
            bool vwap_changed = data_feed_ptr->ticker_tools_.obook.vwap_changed();
            if (!data_feed_ptr->in_recovery_cicle_ && is_correct && vwap_changed) [[likely]] {
                wvap = data_feed_ptr->ticker_tools_.obook.get_vwap();
                write_pos = result_buffer_ + res_count_++ * 3 + 1;
                *write_pos = data_feed_ptr->ticker_tools_.instrument_id;
                *(write_pos + 1) = wvap->numerator;
                *(write_pos + 2) = wvap->denominator;
                data_feed_ptr->ticker_tools_.obook.reset_vwap();
            }
        }
    }

    const __m512i chunk = _mm512_loadu_si512(data + parsed);
    __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, va) & (_mm512_cmpeq_epi8_mask(chunk, vb) >> 1);
    mask &= (1ULL << (msg_len - parsed - 7)) - 1;
    for (; mask; mask &= mask - 1) {
        size_t offset = parsed + _tzcnt_u64(mask);
        const __m512i pattern = _mm512_set1_epi16(*reinterpret_cast<const uint16_t *>(data + offset + 4));
        const __mmask32 instid_mask = _mm512_cmpeq_epi16_mask(instid_chunk, pattern);
        if (!instid_mask) [[likely]] {
            continue;
        }

        const int idx = _tzcnt_u32(instid_mask);
        data_feed_ptr = data_feed_by_id_[idx];
        field_hdr = reinterpret_cast<const FieldHeader *>(data + offset);
        offset += sizeof(FieldHeader);
        cursor = reinterpret_cast<const uint8_t *>(data) + offset + 2;
        inc_group.change_no = readVint3Bytes(cursor);
        offset += field_hdr->field_len;
        update_cnt = 0;

        while (offset < msg_len) {
            field_hdr = reinterpret_cast<const FieldHeader *>(data + offset);
            if (field_hdr->field_id != 0x1001U) {
                offset += (field_hdr->field_id == 0x0003U) ? 0 : sizeof(FieldHeader) + field_hdr->field_len;
                break;
            }
            offset += sizeof(FieldHeader);
            cursor = reinterpret_cast<const uint8_t *>(data) + offset + 2;
            BookUpdate &single_update = inc_group.book_updates[update_cnt];
            uint32_t packed = *reinterpret_cast<const uint32_t *>(data + offset);
            single_update.event_type = packed & 0xFF;
            single_update.md_entry_type = (packed >> 8) & 0xFF;
            single_update.price_level = readVint1Byte(cursor);
            single_update.price_offset = readVint2Bytes(cursor);
            single_update.volume = readVint2Bytes(cursor);
            offset += field_hdr->field_len;
            ++update_cnt;
        }
        data_feed_ptr->proc_data(inc_group, update_cnt);
        bool is_correct = data_feed_ptr->ticker_tools_.obook.ob_is_correct();
        bool vwap_changed = data_feed_ptr->ticker_tools_.obook.vwap_changed();
        if (!data_feed_ptr->in_recovery_cicle_ && is_correct && vwap_changed) [[likely]] {
            wvap = data_feed_ptr->ticker_tools_.obook.get_vwap();
            write_pos = result_buffer_ + res_count_++ * 3 + 1;
            *write_pos = data_feed_ptr->ticker_tools_.instrument_id;
            *(write_pos + 1) = wvap->numerator;
            *(write_pos + 2) = wvap->denominator;
            data_feed_ptr->ticker_tools_.obook.reset_vwap();
        }
    }

    // std::cout << "[EXIT proc_increment]\n";
}

void MdReceiver::proc_snapshot(const char *data)
{
    const size_t snap_len = sizeof(SnapshotHeader) + *reinterpret_cast<const uint16_t *>(data + 2);
    size_t parsed = sizeof(SnapshotHeader);
    SnapshotInstrumentGroup *group;
    uint8_t entry_cnt = 0;

    if (*reinterpret_cast<const uint16_t *>(data + parsed) != 0x0101U) {
        parsed += 111;
        for (; parsed < snap_len;) {
            if (*reinterpret_cast<const uint16_t *>(data + parsed) == 0x0101U) {
                break;
            }
            parsed += 9;
        }
    }

    for (; parsed < snap_len;) {
        group = reinterpret_cast<SnapshotInstrumentGroup *>(const_cast<char *>(data) + parsed);
        parsed += sizeof(SnapshotInstrumentInfo) + sizeof(SnapshotInstrumentTradeInfo);
        entry_cnt = 0;

        for (; parsed < snap_len;) {
            if (*reinterpret_cast<const uint16_t *>(data + parsed) == 0x0101U) {
                break;
            }
            ++entry_cnt;
            parsed += sizeof(SnapshotBookEntry);
        }

        if (!unknown_tickers_.empty()) {
            auto it = unknown_tickers_.find(group->info.instrument_name);
            if (it != unknown_tickers_.end()) {
                inst_ids_[tickers_in_recovery_count_ - 1] = encode_vint(group->info.instrument_id);
                data_feed_by_id_[tickers_in_recovery_count_ - 1] = it->second;
                instid_bitmask_[encode_vint(group->info.instrument_id)] = true;
                unknown_tickers_.erase(it);
                if (unknown_tickers_.empty()) {
                    all_names_resolved_ = true;
                }
                it->second->proc_data(group, entry_cnt);
            }
            continue;
        }

        __m512i pattern = _mm512_set1_epi16(encode_vint(group->info.instrument_id));
        __m512i chunk = _mm512_loadu_si512(inst_ids_);
        __mmask32 mask = _mm512_cmpeq_epi16_mask(chunk, pattern);
        if (mask) {
            int idx = _tzcnt_u32(mask);
            data_feed_by_id_[idx]->proc_data(group, entry_cnt);
        }
    }
}