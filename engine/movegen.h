#pragma once

#include "board.h"
#include "move.h"
#include <cstdint>

namespace panda {

enum class GameTermination : uint8_t {
    None,
    Checkmate,
    Stalemate,
    FiftyMoveRule
};

MoveList generate_legal(const Board& board);

uint64_t perft(const Board& board, int depth);

bool in_check(const Board& board);
bool is_checkmate(const Board& board);
bool is_stalemate(const Board& board);
bool is_draw_by_fifty_move_rule(const Board& board);
GameTermination game_termination(const Board& board);

} // namespace panda
