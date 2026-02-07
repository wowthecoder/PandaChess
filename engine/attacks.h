#pragma once

#include "types.h"
#include "bitboard.h"
#include <cstdint>

namespace panda {
namespace attacks {

// Non-sliding piece attack tables
extern Bitboard PawnAttacks[2][64];
extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];

// Magic bitboard structures for sliding pieces
struct Magic {
    Bitboard  mask;
    Bitboard  magic;
    Bitboard* attacks;
    int       shift;

    Bitboard operator()(Bitboard occ) const {
        return attacks[((occ & mask) * magic) >> shift];
    }
};

extern Magic BishopMagics[64];
extern Magic RookMagics[64];

extern Bitboard BishopTable[0x1480];
extern Bitboard RookTable[0x19000];

// Lookup functions
inline Bitboard pawn_attacks(Color c, Square s) { return PawnAttacks[c][s]; }
inline Bitboard knight_attacks(Square s) { return KnightAttacks[s]; }
inline Bitboard king_attacks(Square s) { return KingAttacks[s]; }
Bitboard bishop_attacks(Square s, Bitboard occ);
Bitboard rook_attacks(Square s, Bitboard occ);
inline Bitboard queen_attacks(Square s, Bitboard occ) {
    return bishop_attacks(s, occ) | rook_attacks(s, occ);
}

void init();

} // namespace attacks
} // namespace panda
