#include "tt.h"

namespace panda {

TranspositionTable::TranspositionTable(size_t sizeMB) {
    size_t entryCount = (sizeMB * 1024 * 1024) / sizeof(TTEntry);
    // Round down to nearest power of 2
    size_t size = 1;
    while (size * 2 <= entryCount) size *= 2;
    table.resize(size);
    mask = size - 1;
    clear();
}

void TranspositionTable::store(uint64_t key, int score, int depth, TTFlag flag, Move bestMove) {
    TTEntry& entry = table[key & mask];
    // Always-replace scheme
    entry.key = key;
    entry.score = score;
    entry.depth = depth;
    entry.flag = flag;
    entry.bestMove = bestMove;
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
    for (auto& entry : table) {
        entry.key = 0;
        entry.score = 0;
        entry.depth = 0;
        entry.flag = TT_EXACT;
        entry.bestMove = NullMove;
    }
}

}  // namespace panda
