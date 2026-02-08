#pragma once

#include "board.h"

namespace panda {

// Returns score in centipawns from the side-to-move's perspective.
// Positive = good for side to move.
int evaluate(const Board& board);

} // namespace panda
