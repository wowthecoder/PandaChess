#pragma once

#include "types.h"
#include <cstdint>

namespace panda {

namespace zobrist {

extern uint64_t pieceKeys[12][64];
extern uint64_t castlingKeys[16];
extern uint64_t enPassantKeys[8];
extern uint64_t sideKey;

void init();

} // namespace zobrist
} // namespace panda
