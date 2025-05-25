#pragma once
#include <cstdlib>
#include <cstdint>
#include <limits>

using IsinIDType = int32_t;
inline constexpr int32_t NAN_CHANGENO = std::numeric_limits<int32_t>::max();

struct PacketHeader {
    uint8_t dummy;
    uint8_t type_id;
    uint16_t message_length;
} __attribute__((__packed__));

struct SnapshotHeader {
    PacketHeader header;
    uint8_t extra_data[4];
} __attribute__((__packed__));

struct IncrementalHeader {
    PacketHeader header;
    uint8_t extra_data[20];
} __attribute__((__packed__));

struct FieldHeader {
    uint16_t field_id;
    uint16_t field_len;
} __attribute__((__packed__));

struct SnapshotInstrumentInfo {
    FieldHeader header;
    char instrument_name[31];
    char reserved[61];
    double tick_size;
    double reference_price;
    int32_t instrument_id;
} __attribute__((__packed__));

struct SnapshotInstrumentTradeInfo {
    FieldHeader header;
    char reserved[150];
    int32_t change_no;
} __attribute__((__packed__));

struct SnapshotBookEntry {
    FieldHeader header;
    int32_t instrument_id;
    char direction;
    double price;
    int32_t volume;
} __attribute__((__packed__));

struct SnapshotInstrumentGroup {
    SnapshotInstrumentInfo info;             // field_id = 0x0101
    SnapshotInstrumentTradeInfo trade_info;  // field_id = 0x0102
    SnapshotBookEntry entries[10];           // field_id = 0x0103
} __attribute__((__packed__));

struct BookUpdate {
    char event_type;
    char md_entry_type;
    int32_t price_level;   // vint
    int32_t price_offset;  // vint
    int32_t volume;        // vint
} __attribute__((__packed__));

struct IncrementalGroup {
    int32_t change_no;
    BookUpdate book_updates[20];  // field_id = 0x1001
};

struct StoredUpdate {
    StoredUpdate() {}
    BookUpdate update;
    int32_t change_no;
    char md_update_action;
} __attribute__((__packed__));