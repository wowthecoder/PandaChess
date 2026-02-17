#pragma once

#include <string_view>

#include "board.h"

namespace panda {
namespace nnue {
class SearchNnueContext;
}

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
enum class EvalMode {
    Handcrafted,
    NNUE,
};

void set_eval_mode(EvalMode mode);
EvalMode get_eval_mode();
const char* eval_mode_name(EvalMode mode);
bool parse_eval_mode(std::string_view value, EvalMode& modeOut);

// Handcrafted static evaluation.
int evaluate_handcrafted(const Board& board);

// NNUE static evaluation. Falls back to handcrafted eval if NNUE backend is unavailable.
int evaluate_nnue(const Board& board);
int evaluate_nnue(const Board& board, nnue::SearchNnueContext* ctx);

// Main evaluation entry point selected by current eval mode.
int evaluate(const Board& board);
int evaluate(const Board& board, nnue::SearchNnueContext* ctx);

}  // namespace panda
