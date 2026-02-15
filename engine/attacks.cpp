#include "attacks.h"

namespace panda {
namespace attacks {

Bitboard PawnAttacks[2][64];
Bitboard KnightAttacks[64];
Bitboard KingAttacks[64];

Magic BishopMagics[64];
Magic RookMagics[64];

Bitboard BishopTable[0x1480];
Bitboard RookTable[0x19000];

// Known magic numbers (from public domain / Stockfish)
static constexpr Bitboard RookMagicNumbers[64] = {
    0x8a80104000800020ULL, 0x140002000100040ULL,  0x2801880a0017001ULL,  0x100081001000420ULL,
    0x200020010080420ULL,  0x3001c0002010008ULL,  0x8480008002000100ULL, 0x2080088004402900ULL,
    0x800098204000ULL,     0x2024401000200040ULL, 0x100802000801000ULL,  0x120800800801000ULL,
    0x208808088000400ULL,  0x2802200800400ULL,    0x2200800100020080ULL, 0x801000060821100ULL,
    0x80044006422000ULL,   0x100808020004000ULL,  0x12108a0010204200ULL, 0x140848010000802ULL,
    0x481828014002800ULL,  0x8094004002004100ULL, 0x4010040010010802ULL, 0x20008806104ULL,
    0x100400080208000ULL,  0x2040002120081000ULL, 0x21200680100081ULL,   0x20100080080080ULL,
    0x2000a00200410ULL,    0x20080800400ULL,      0x80088400100102ULL,   0x80004600042881ULL,
    0x4040008040800020ULL, 0x440003000200801ULL,  0x4200011004500ULL,    0x188020010100100ULL,
    0x14800401802800ULL,   0x2080040080800200ULL, 0x124080204001001ULL,  0x200046000122ULL,
    0x480040080024001ULL,  0x2040004010002000ULL, 0x104044020020010ULL,  0x208008010100ULL,
    0x2008020400040800ULL, 0x2008020004008080ULL, 0x10010004040080ULL,   0x4020410820004ULL,
    0x200040100a0ULL,      0x3000600002200100ULL, 0x420200201001100ULL,  0x100080600080100ULL,
    0x480040080800800ULL,  0x2060080040200ULL,    0x8020401004040200ULL, 0x201040200ULL,
    0x101004102004081ULL,  0x4001008400a20001ULL, 0x2000100084120ULL,    0x8000400082002101ULL,
    0x1000200040100042ULL, 0x2000040800100244ULL, 0x800000444810a01ULL,  0x100880082000401ULL,
};

static constexpr Bitboard BishopMagicNumbers[64] = {
    0x40040844404084ULL,   0x2004208a004208ULL,   0x10190041080202ULL,   0x108060845042010ULL,
    0x581104180800210ULL,  0x2112080446200010ULL, 0x1080820820060210ULL, 0x3c0808410220200ULL,
    0x4050404440404ULL,    0x21001420088ULL,      0x24d0080801082102ULL, 0x1020a0a020400ULL,
    0x40308200402ULL,      0x4011002100800ULL,    0x401484104104005ULL,  0x801010402020200ULL,
    0x400210c3880100ULL,   0x404022024108200ULL,  0x810018200204102ULL,  0x4002801a02003ULL,
    0x85040820080400ULL,   0x810102c808880400ULL, 0xe900410884800ULL,    0x2002c5080020ULL,
    0x8040303010a100ULL,   0x2003084000804001ULL, 0x100140080800800ULL,  0x2a80081012800ULL,
    0x400200100200ULL,     0x1010000104016200ULL, 0x800040800100225ULL,  0x400101002004108ULL,
    0x2282020020ULL,       0x8280140408010800ULL, 0x2082a0010400ULL,     0x14000440108800ULL,
    0x8b04000a000c0200ULL, 0x6002002004100840ULL, 0x4000202040402ULL,    0x604004401002110ULL,
    0x2010a04018010ULL,    0x4100120400800ULL,    0x12020402020300ULL,   0x1080040000200ULL,
    0x40441100201020ULL,   0x8020008000810100ULL, 0x2001a0080401800ULL,  0x200024040200ULL,
    0x4040800800100ULL,    0x400a000400ULL,       0x8042020200ULL,       0x2110100200100ULL,
    0x1240804000ULL,       0x8000202040104ULL,    0x44000810400100ULL,   0x20010414200040ULL,
    0x20048008000ULL,      0x2000404040201ULL,    0x1101010040200ULL,    0x2010200080100ULL,
    0x4080808040000ULL,    0x2000440041002200ULL, 0x200c005000ULL,       0x41000a2000084ULL,
};

static constexpr int BishopDeltas[4] = {9, 7, -9, -7};
static constexpr int RookDeltas[4] = {8, -8, 1, -1};

// Compute sliding attack rays for initialization
static Bitboard sliding_attack(Square sq, Bitboard occ, const int deltas[4]) {
    Bitboard attacks = 0;
    for (int i = 0; i < 4; ++i) {
        int s = sq;
        while (true) {
            int prev_rank = s / 8;
            int prev_file = s % 8;
            s += deltas[i];
            if (s < 0 || s > 63)
                break;
            int new_rank = s / 8;
            int new_file = s % 8;
            // Check for wrapping
            int dr = new_rank - prev_rank;
            int df = new_file - prev_file;
            if (dr < -1 || dr > 1 || df < -1 || df > 1)
                break;
            attacks |= Bitboard(1) << s;
            if (occ & (Bitboard(1) << s))
                break;
        }
    }
    return attacks;
}

static void init_pawn_attacks() {
    for (int sq = 0; sq < 64; ++sq) {
        Bitboard bb = Bitboard(1) << sq;
        // White pawn attacks: up-left, up-right
        Bitboard wl = (bb & ~FileMask[0]) << 7;
        Bitboard wr = (bb & ~FileMask[7]) << 9;
        PawnAttacks[White][sq] = wl | wr;
        // Black pawn attacks: down-left, down-right
        Bitboard bl = (bb & ~FileMask[7]) >> 7;
        Bitboard br = (bb & ~FileMask[0]) >> 9;
        PawnAttacks[Black][sq] = bl | br;
    }
}

static void init_knight_attacks() {
    for (int sq = 0; sq < 64; ++sq) {
        Bitboard bb = Bitboard(1) << sq;
        Bitboard attacks = 0;
        // 8 L-shaped moves with file-wrap guards
        attacks |= (bb & ~FileMask[0] & ~FileMask[1]) << 6;   // up-1, left-2
        attacks |= (bb & ~FileMask[0]) << 15;                 // up-2, left-1
        attacks |= (bb & ~FileMask[7]) << 17;                 // up-2, right-1
        attacks |= (bb & ~FileMask[6] & ~FileMask[7]) << 10;  // up-1, right-2
        attacks |= (bb & ~FileMask[6] & ~FileMask[7]) >> 6;   // down-1, right-2
        attacks |= (bb & ~FileMask[7]) >> 15;                 // down-2, right-1
        attacks |= (bb & ~FileMask[0]) >> 17;                 // down-2, left-1
        attacks |= (bb & ~FileMask[0] & ~FileMask[1]) >> 10;  // down-1, left-2
        KnightAttacks[sq] = attacks;
    }
}

static void init_king_attacks() {
    for (int sq = 0; sq < 64; ++sq) {
        Bitboard bb = Bitboard(1) << sq;
        Bitboard attacks = 0;
        attacks |= (bb & ~FileMask[0]) >> 1;  // left
        attacks |= (bb & ~FileMask[7]) << 1;  // right
        attacks |= bb << 8;                   // up
        attacks |= bb >> 8;                   // down
        attacks |= (bb & ~FileMask[0]) << 7;  // up-left
        attacks |= (bb & ~FileMask[7]) << 9;  // up-right
        attacks |= (bb & ~FileMask[7]) >> 7;  // down-right
        attacks |= (bb & ~FileMask[0]) >> 9;  // down-left
        KingAttacks[sq] = attacks;
    }
}

static Bitboard bishop_mask(int sq) {
    Bitboard mask = sliding_attack(Square(sq), 0, BishopDeltas);
    // Remove edges (they don't matter for occupancy)
    mask &= ~(RankMask[0] | RankMask[7] | FileMask[0] | FileMask[7]);
    return mask;
}

static Bitboard rook_mask(int sq) {
    Bitboard mask = 0;
    int r = sq / 8, f = sq % 8;
    for (int i = r + 1; i < 7; ++i) mask |= Bitboard(1) << (i * 8 + f);
    for (int i = r - 1; i > 0; --i) mask |= Bitboard(1) << (i * 8 + f);
    for (int i = f + 1; i < 7; ++i) mask |= Bitboard(1) << (r * 8 + i);
    for (int i = f - 1; i > 0; --i) mask |= Bitboard(1) << (r * 8 + i);
    return mask;
}

static void init_magics(Magic* magics, Bitboard* table, const Bitboard* magic_numbers,
                        bool is_bishop) {
    const int* deltas = is_bishop ? BishopDeltas : RookDeltas;

    int offset = 0;
    for (int sq = 0; sq < 64; ++sq) {
        Magic& m = magics[sq];
        m.mask = is_bishop ? bishop_mask(sq) : rook_mask(sq);
        m.magic = magic_numbers[sq];
        m.shift = 64 - popcount(m.mask);
        m.attacks = table + offset;

        // Enumerate all occupancy subsets using Carry-Rippler
        Bitboard occ = 0;
        int size = 0;
        do {
            int idx = (int)((occ * m.magic) >> m.shift);
            m.attacks[idx] = sliding_attack(Square(sq), occ, deltas);
            ++size;
            occ = (occ - m.mask) & m.mask;
        } while (occ);

        offset += size;
    }
}

Bitboard bishop_attacks(Square s, Bitboard occ) {
    return sliding_attack(s, occ, BishopDeltas);
}

Bitboard rook_attacks(Square s, Bitboard occ) {
    return sliding_attack(s, occ, RookDeltas);
}

void init() {
    init_pawn_attacks();
    init_knight_attacks();
    init_king_attacks();
    init_magics(BishopMagics, BishopTable, BishopMagicNumbers, true);
    init_magics(RookMagics, RookTable, RookMagicNumbers, false);
}

}  // namespace attacks
}  // namespace panda
