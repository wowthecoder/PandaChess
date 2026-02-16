#pragma once

#include <cstdint>
#include <vector>

#include "move.h"

namespace panda {

enum TTFlag : uint8_t {
    TT_EXACT,
    TT_ALPHA,  // upper bound
    TT_BETA    // lower bound
};

struct TTEntry {
    uint64_t key;  // full Zobrist hash for collision detection
    int score;
    int depth;
    TTFlag flag;
    Move bestMove;
};

class TranspositionTable {
   public:
    explicit TranspositionTable(size_t sizeMB = 64);

    void store(uint64_t key, int score, int depth, TTFlag flag, Move bestMove);
    bool probe(uint64_t key, TTEntry& entry) const;
    void clear();
    int hashfull_permille(size_t sampleSize = 1000) const;

   private:
    std::vector<TTEntry> table;
    size_t mask;  // size - 1, for fast modulo (size is power of 2)
};

}  // namespace panda
