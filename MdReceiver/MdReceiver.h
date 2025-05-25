#pragma once
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>
#include <bitset>

#include "TickerTools.h"
#include "config.h"
#include "DataFeed.h"
#include "shm_queue.h"
#include "data_type.h"

class alignas(64) MdReceiver {
public:
    MdReceiver(const MdConfig &cfg, SPSCQueue &in_queue, SPSCQueue &out_queue);
    ~MdReceiver() = default;
    void start();

private:
    void receive_md();
    void proc_increment(const char *data) noexcept;
    void proc_snapshot(const char *data) noexcept;

    std::bitset<65536> instid_bitmask_;
    alignas(64) DataFeed *data_feed_by_id_[32];  // arbitrary size
    alignas(64) uint16_t inst_ids_[32];          // arbitrary size
    std::unordered_map<std::string, DataFeed *> unknown_tickers_;
    std::vector<DataFeed> data_feeds_collection_;
    SPSCQueue &in_queue_;
    SPSCQueue &out_queue_;
    uint32_t *result_buffer_;
    size_t tickers_in_recovery_count_;
    uint32_t res_count_;
    bool all_names_resolved_;
};