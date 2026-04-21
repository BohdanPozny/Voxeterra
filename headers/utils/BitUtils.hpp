#pragma once

#include <cstdint>

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

// Count trailing zeros of a 64-bit word. Assumes value != 0.
inline int ctz64(uint64_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(value);
#elif defined(_MSC_VER)
    unsigned long index;
    #if defined(_M_X64) || defined(_M_ARM64)
        _BitScanForward64(&index, value);
    #else
        if (_BitScanForward(&index, static_cast<unsigned long>(value))) {
            return static_cast<int>(index);
        }
        _BitScanForward(&index, static_cast<unsigned long>(value >> 32));
        return static_cast<int>(index) + 32;
    #endif
    return static_cast<int>(index);
#else
    int count = 0;
    while ((value & 1ULL) == 0) { value >>= 1; ++count; }
    return count;
#endif
}
