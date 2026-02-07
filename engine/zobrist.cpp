#include "zobrist.h"
#include <cstdint>

namespace panda {
namespace zobrist {

uint64_t pieceKeys[12][64];
uint64_t castlingKeys[16];
uint64_t enPassantKeys[8];
uint64_t sideKey;

// Simple xorshift64 PRNG with fixed seed for reproducibility
static uint64_t rand64(uint64_t& state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

void init() {
    uint64_t seed = 0x3A4F6C8E1B2D5A7CULL; // fixed seed

    for (int p = 0; p < 12; ++p)
        for (int s = 0; s < 64; ++s)
            pieceKeys[p][s] = rand64(seed);

    for (int i = 0; i < 16; ++i)
        castlingKeys[i] = rand64(seed);

    for (int i = 0; i < 8; ++i)
        enPassantKeys[i] = rand64(seed);

    sideKey = rand64(seed);
}

} // namespace zobrist
} // namespace panda
