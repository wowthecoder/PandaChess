#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

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

// Info sent after each iterative deepening iteration
struct SearchInfo {
    int depth;
    int score;
    bool isMate;    // true if score is a mate score
    int mateInPly;  // positive = we mate, negative = we get mated
    uint64_t nodes;
    int64_t timeMs;
    std::vector<Move> pv;
};

// Callback type for search info updates
using InfoCallback = std::function<void(const SearchInfo&)>;

// Time-limited search (iterative deepening)
SearchResult search(const Board& board, int timeLimitMs, TranspositionTable& tt);

// Fixed-depth search (for tests, backward compatibility)
SearchResult searchDepth(const Board& board, int depth, TranspositionTable& tt);

// UCI-friendly search with external stop and info callback
SearchResult search(const Board& board, int timeLimitMs, int maxDepth, TranspositionTable& tt,
                    std::atomic<bool>& stopFlag, InfoCallback infoCallback = nullptr);

// UCI-friendly search with repetition history (hashes from game start to current position).
SearchResult search(const Board& board, int timeLimitMs, int maxDepth, TranspositionTable& tt,
                    std::atomic<bool>& stopFlag, const std::vector<uint64_t>& repetitionHistory,
                    InfoCallback infoCallback = nullptr);

// Extract principal variation from transposition table
std::vector<Move> extractPV(const Board& board, TranspositionTable& tt, int maxLen);

}  // namespace panda
