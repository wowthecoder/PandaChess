#pragma once

#include <chrono>
#include <cstring>

#include "board.h"
#include "move.h"
#include "tt.h"

namespace panda {

constexpr int MATE_SCORE = 100000;
constexpr int MAX_PLY = 64;

struct SearchResult {
    Move bestMove;
    int score;
};

struct SearchState {
    TranspositionTable& tt;
    Move killers[MAX_PLY][2];  // 2 killer moves per ply
    int history[2][64][64];    // [color][from][to] history scores
    std::chrono::steady_clock::time_point startTime;
    int timeLimitMs;
    bool stopped;

    explicit SearchState(TranspositionTable& tt_) : tt(tt_), timeLimitMs(0), stopped(false) {
        clear();
    }

    void clear() {
        std::memset(killers, 0, sizeof(killers));
        std::memset(history, 0, sizeof(history));
        stopped = false;
    }

    bool checkTime() {
        if (timeLimitMs <= 0)
            return false;
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= timeLimitMs) {
            stopped = true;
            return true;
        }
        return false;
    }
};

// Time-limited search (iterative deepening)
SearchResult search(const Board& board, int timeLimitMs, TranspositionTable& tt);

// Fixed-depth search (for tests, backward compatibility)
SearchResult searchDepth(const Board& board, int depth, TranspositionTable& tt);

}  // namespace panda
