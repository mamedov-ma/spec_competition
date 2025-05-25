#pragma once
#include <array>
#include <cstdint>
#include <immintrin.h>

#include "data_type.h"

class Orderbook {
public:
    struct Order {
        int32_t price_offset;
        int32_t volume;
    } __attribute__((packed));

    struct Wvap {
        uint32_t numerator;
        uint32_t denominator;
    } __attribute__((packed));

    Orderbook() noexcept : bids_(orders_), asks_(orders_ + 10), old_vwap(), cur_vwap(), bid_size_(0), ask_size_(0) {}

    inline void new_order_from_snap(const SnapshotBookEntry &upd) noexcept __attribute__((always_inline))
    {
        bool is_ask = (upd.direction == '1');
        orders_[is_ask * (10 + ask_size_) + !is_ask * bid_size_] = {
            static_cast<int32_t>(upd.price / tick_size_ + 0.5) * upd.volume, upd.volume};
        bid_size_ += !is_ask;
        ask_size_ += is_ask;
    }

    inline void new_order(const BookUpdate &upd) noexcept __attribute__((always_inline))
    {
        bool is_ask = (upd.md_entry_type == '1');
        std::move_backward(&orders_[is_ask * 10 + upd.price_level - 1],
                           &orders_[is_ask * (10 + ask_size_) + !is_ask * bid_size_],
                           &orders_[is_ask * (10 + ask_size_) + !is_ask * bid_size_ + 1]);
        orders_[is_ask * 10 + upd.price_level - 1] = {static_cast<int32_t>(upd.price_offset + ref_price_) *
                                                          static_cast<int32_t>(upd.volume),
                                                      static_cast<int32_t>(upd.volume)};
        bid_size_ += !is_ask;
        ask_size_ += is_ask;
    }

    inline void modify_order(const BookUpdate &upd) noexcept __attribute__((always_inline))
    {
        orders_[(upd.md_entry_type == '1') * 10 + upd.price_level - 1] = {
            static_cast<int32_t>(upd.price_offset + ref_price_) * static_cast<int32_t>(upd.volume),
            static_cast<int32_t>(upd.volume)};
    }

    inline void delete_order(const BookUpdate &upd) noexcept __attribute__((always_inline))
    {
        bool is_ask = (upd.md_entry_type == '1');
        std::move(&orders_[is_ask * 10 + upd.price_level], &orders_[is_ask * (10 + ask_size_) + !is_ask * bid_size_],
                  &orders_[is_ask * 10 + upd.price_level - 1]);
        bid_size_ -= !is_ask;
        ask_size_ -= is_ask;
    }

    inline bool ob_is_correct() const noexcept __attribute__((always_inline))
    {
        return ask_size_ && bid_size_;
    }

    inline void clear() noexcept __attribute__((always_inline))
    {
        ask_size_ = 0;
        bid_size_ = 0;
    }

    inline void update_vwap() noexcept __attribute__((always_inline))
    {
        old_vwap = cur_vwap;
        uint64_t num = 0;
        uint64_t den = 0;

        if ((bid_size_ & ask_size_) == 5) [[likely]] {
#pragma GCC unroll 5
            for (int i = 0; i < 5; ++i) {
                den += static_cast<uint32_t>(asks_[i].volume) + static_cast<uint32_t>(bids_[i].volume);
                num += static_cast<uint32_t>(asks_[i].price_offset) + static_cast<uint32_t>(bids_[i].price_offset);
            }
        } else {
            for (uint8_t i = 0; i < ask_size_; ++i) {
                den += static_cast<uint32_t>(asks_[i].volume);
                num += static_cast<uint32_t>(asks_[i].price_offset);
            }
            for (uint8_t i = 0; i < bid_size_; ++i) {
                den += static_cast<uint32_t>(bids_[i].volume);
                num += static_cast<uint32_t>(bids_[i].price_offset);
            }
        }
        cur_vwap.denominator = static_cast<uint32_t>(den);
        cur_vwap.numerator = static_cast<uint32_t>(num);
    }

    inline void reset_vwap() noexcept __attribute__((always_inline))
    {
        old_vwap = cur_vwap;
    }

    inline bool vwap_changed() const noexcept __attribute__((always_inline))
    {
        return cur_vwap.numerator != old_vwap.numerator || cur_vwap.denominator != old_vwap.denominator;
    }

    inline const Wvap *get_vwap() const noexcept __attribute__((always_inline))
    {
        return &cur_vwap;
    }

    inline void trim() noexcept __attribute__((always_inline))
    {
        ask_size_ = (ask_size_ > 5) ? 5 : ask_size_;
        bid_size_ = (bid_size_ > 5) ? 5 : bid_size_;
    }

    inline void set_ref_and_tick_size(const double &_tick_size, const double &_reference_price) noexcept
        __attribute__((always_inline))
    {
        tick_size_ = _tick_size;
        ref_price_ = static_cast<int32_t>(_reference_price / tick_size_ + 0.5);
    }

private:
    alignas(64) Order orders_[20];  // arbitrary size
    Order *const bids_;
    Order *const asks_;
    Wvap old_vwap;
    Wvap cur_vwap;
    double tick_size_;
    int32_t ref_price_;
    uint8_t bid_size_;
    uint8_t ask_size_;
};