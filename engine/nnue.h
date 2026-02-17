#pragma once

namespace panda {

class Board;
namespace nnue {
class SearchNnueContext;
}

// Evaluates a position with NNUE when available.
// Returns side-to-move score in centipawns.
int evaluate_nnue(const Board& board);
int evaluate_nnue(const Board& board, nnue::SearchNnueContext* ctx);

// Returns true when SF18 NNUE backend and both nets are loaded.
bool nnue_backend_ready();

}  // namespace panda
