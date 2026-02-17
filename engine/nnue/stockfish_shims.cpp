#include "stockfish_src/syzygy/tbprobe.h"
#include "stockfish_src/tt.h"
#include "stockfish_src/types.h"
#include "stockfish_src/uci.h"

namespace Stockfish {

TTEntry* TranspositionTable::first_entry(const Key) const {
    return nullptr;
}

std::string UCIEngine::square(Square s) {
    std::string out;
    out.push_back(static_cast<char>('a' + file_of(s)));
    out.push_back(static_cast<char>('1' + rank_of(s)));
    return out;
}

}  // namespace Stockfish

namespace Stockfish::Tablebases {

int MaxCardinality = 0;

WDLScore probe_wdl(Position&, ProbeState* result) {
    if (result)
        *result = FAIL;
    return WDLDraw;
}

int probe_dtz(Position&, ProbeState* result) {
    if (result)
        *result = FAIL;
    return 0;
}

}  // namespace Stockfish::Tablebases
