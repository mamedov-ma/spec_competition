#pragma once
#include <cstdint>
#include <immintrin.h>

inline int64_t readVint1Byte(const uint8_t *&ptr) noexcept
{
    uint64_t b0 = ptr[0];
    ptr += 1;
    return (b0 >> 1) ^ -(b0 & 1);
}

inline int64_t readVint2Bytes(const uint8_t *&ptr) noexcept
{
    uint64_t b0 = ptr[0];
    if ((b0 & 0x80) == 0) {
        ptr += 1;
        return (b0 >> 1) ^ -(b0 & 1);
    }

    uint64_t b1 = ptr[1];
    ptr += 2;
    uint64_t val = ((b0 & 0x7F)) | ((b1 & 0x7F) << 7);
    return (val >> 1) ^ -(val & 1);
}

inline int64_t readVint3Bytes(const uint8_t *&ptr) noexcept
{
    uint64_t b0 = ptr[0];
    uint64_t b1 = ptr[1];

    if ((b1 & 0x80) == 0) {
        ptr += 2;
        uint64_t val = ((b0 & 0x7F)) | ((b1 & 0x7F) << 7);
        return (val >> 1) ^ -(val & 1);
    }

    uint64_t b2 = ptr[2];
    ptr += 3;
    uint64_t val = ((b0 & 0x7F)) | ((b1 & 0x7F) << 7) | ((b2 & 0x7F) << 14);
    return (val >> 1) ^ -(val & 1);
}

inline uint16_t encode_vint(uint32_t id) noexcept
{
    uint16_t out;
    uint32_t zz = id << 1;

    uint8_t *p = reinterpret_cast<uint8_t *>(&out);
    do {
        uint8_t byte = zz & 0x7F;
        zz >>= 7;
        if (zz)
            byte |= 0x80;
        *p++ = byte;
    } while (zz);

    return out;
}