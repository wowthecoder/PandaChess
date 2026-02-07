#pragma once

#include "types.h"
#include <cstdint>
#include <string>

namespace panda {

using Bitboard = uint64_t;

// Square-to-bitboard lookup
constexpr Bitboard square_bb(Square s) {
    return Bitboard(1) << s;
}

// Rank masks (rank 1 = index 0)
constexpr Bitboard RankMask[8] = {
    0xFFULL,
    0xFFULL << 8,
    0xFFULL << 16,
    0xFFULL << 24,
    0xFFULL << 32,
    0xFFULL << 40,
    0xFFULL << 48,
    0xFFULL << 56
};

// File masks (file A = index 0)
constexpr Bitboard FileMask[8] = {
    0x0101010101010101ULL,
    0x0202020202020202ULL,
    0x0404040404040404ULL,
    0x0808080808080808ULL,
    0x1010101010101010ULL,
    0x2020202020202020ULL,
    0x4040404040404040ULL,
    0x8080808080808080ULL
};

// Bit manipulation utilities
inline int popcount(Bitboard b) {
    return __builtin_popcountll(b);
}

inline Square lsb(Bitboard b) {
    return Square(__builtin_ctzll(b));
}

inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// Debug: print bitboard as 8x8 grid
std::string print_bitboard(Bitboard b);

} // namespace panda
