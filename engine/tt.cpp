#include "tt.h"

#include <algorithm>

namespace panda {

TranspositionTable::TranspositionTable(size_t sizeMB) {
    size_t entryCount = (sizeMB * 1024 * 1024) / sizeof(TTEntry);
    // Round down to nearest power of 2
    size_t size = 1;
    while (size * 2 <= entryCount) size *= 2;
    table.resize(size);
    mask = size - 1;
    currentGeneration = 1;
    clear();
}

void TranspositionTable::new_search() {
    ++currentGeneration;
    if (currentGeneration == 0)
        currentGeneration = 1;
}

void TranspositionTable::store(uint64_t key, int score, int depth, TTFlag flag, Move bestMove) {
    TTEntry& entry = table[key & mask];

    bool replace = false;

    // Rule A: empty slot
    if (entry.key == 0) {
        replace = true;
    } else if (entry.key == key) {
        // Rule B: same key
        replace = (depth >= entry.depth) || (flag == TT_EXACT);
    } else {
        // Rule C: different key (collision)
        uint8_t age = static_cast<uint8_t>(currentGeneration - entry.generation);
        bool stale = age >= 2;
        replace = stale || (depth > entry.depth) ||
                  (depth == entry.depth && flag == TT_EXACT && entry.flag != TT_EXACT);
    }

    if (!replace)
        return;

    // Rule D: write replacement
    entry.key = key;
    entry.score = score;
    entry.depth = depth;
    entry.flag = flag;
    entry.bestMove = bestMove;
    entry.generation = currentGeneration;
}

bool TranspositionTable::probe(uint64_t key, TTEntry& entry) const {
    const TTEntry& stored = table[key & mask];
    if (stored.key == key) {
        entry = stored;
        return true;
    }
    return false;
}

void TranspositionTable::clear() {
    currentGeneration = 1;
    for (auto& entry : table) {
        entry.key = 0;
        entry.score = 0;
        entry.depth = 0;
        entry.flag = TT_EXACT;
        entry.bestMove = NullMove;
        entry.generation = 0;
    }
}

int TranspositionTable::hashfull_permille(size_t sampleSize) const {
    if (table.empty())
        return 0;

    size_t sample = std::min(sampleSize, table.size());
    if (sample == 0)
        return 0;

    size_t used = 0;
    for (size_t i = 0; i < sample; ++i) {
        if (table[i].key != 0)
            ++used;
    }

    return static_cast<int>((used * 1000) / sample);
}

}  // namespace panda
