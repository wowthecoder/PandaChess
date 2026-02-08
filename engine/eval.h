#pragma once

#include "board.h"

namespace panda {

// Material values in centipawns (indexed by PieceType)
inline constexpr int PieceValue[6] = {
    100,  // Pawn
    320,  // Knight
    330,  // Bishop
    500,  // Rook
    900,  // Queen
    0     // King (not counted in material)
};

// Returns score in centipawns from the side-to-move's perspective.
// Positive = good for side to move.
int evaluate(const Board& board);

} // namespace panda
