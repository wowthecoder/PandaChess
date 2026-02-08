#pragma once

#include "board.h"
#include "move.h"
#include "tt.h"

namespace panda {

constexpr int MATE_SCORE = 100000;
constexpr int DEFAULT_DEPTH = 5;

struct SearchResult {
    Move bestMove;
    int score;
};

SearchResult search(const Board& board, int depth, TranspositionTable& tt);

} // namespace panda
