#pragma once

#include "board.h"
#include "move.h"
#include <cstdint>

namespace panda {

MoveList generate_legal(const Board& board);

uint64_t perft(const Board& board, int depth);

} // namespace panda
