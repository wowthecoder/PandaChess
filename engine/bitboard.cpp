#include "bitboard.h"
#include <sstream>

namespace panda {

std::string print_bitboard(Bitboard b) {
    std::ostringstream os;
    os << "+---+---+---+---+---+---+---+---+\n";
    for (int rank = 7; rank >= 0; --rank) {
        for (int file = 0; file < 8; ++file) {
            Square s = make_square(file, rank);
            os << "| " << ((b & square_bb(s)) ? '1' : '.') << ' ';
        }
        os << "| " << (rank + 1) << '\n';
        os << "+---+---+---+---+---+---+---+---+\n";
    }
    os << "  a   b   c   d   e   f   g   h\n";
    return os.str();
}

} // namespace panda
