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

// Magic numbers generated for this engine's square mapping and occupancy masks.
static constexpr Bitboard BishopMagicNumbers[64] = {
    0x40045002080b1010ULL, 0x805430ac0100a010ULL, 0x12440042000000ULL,   0x8060148108000ULL,
    0x4042050028000ULL,    0x8011040242813004ULL, 0x4190110500020ULL,    0x2091420201114000ULL,
    0x1880040444040404ULL, 0x1242021002108100ULL, 0x1100400a02c00ULL,    0x3320c240080a200ULL,
    0x8680011041080040ULL, 0x8001010840024aULL,   0x420060851080800ULL,  0x4014288201100228ULL,
    0x204000809300400ULL,  0x21428404040048ULL,   0xb408000400401604ULL, 0x2828004402480802ULL,
    0x4072002401210000ULL, 0x3000280414000ULL,    0x10808400849042ULL,   0x102002100422200ULL,
    0x20098010104100ULL,   0x401210008682101ULL,  0x4000440028002400ULL, 0x8004840108012020ULL,
    0x800104002200210cULL, 0xe088004100080ULL,    0x205040001041140ULL,  0x408402008401ULL,
    0x80202c014200800ULL,  0x8028900800040880ULL, 0x1020203400080800ULL, 0x80a40108040100ULL,
    0x8020400141100ULL,    0x400808100020100ULL,  0x82008200040220ULL,   0x4020243040200a0ULL,
    0x8008080808006680ULL, 0x1101112010b042ULL,   0x9a002024004810ULL,   0x8206001148004400ULL,
    0xc0210a010101ULL,     0x62a00410205100ULL,   0x868808010c000070ULL, 0x28009100508200ULL,
    0xa80842420841400ULL,  0x10c00c4040000ULL,    0x100002402082040ULL,  0x22000884042210ULL,
    0x26031002022900ULL,   0x210c20408008000ULL,  0x4043a802048100ULL,   0x10840800405400ULL,
    0x808410114408ULL,     0x60002420084a000ULL,  0x4820b24060800ULL,    0x400018460802ULL,
    0x81648a05150400ULL,   0x1200804084080080ULL, 0x5009080801242400ULL, 0x600800d1004202ULL,
};

static constexpr Bitboard RookMagicNumbers[64] = {
    0xd080008040001020ULL, 0x940100020004000ULL,  0x80200010008008ULL,   0x4500100084600900ULL,
    0x2080040008008002ULL, 0x8180020004000180ULL, 0x2100008200040100ULL, 0x4200040880204201ULL,
    0x8800802040008000ULL, 0x1002802000400086ULL, 0x86808020001000ULL,   0x4100800800100080ULL,
    0x32800400824800ULL,   0x1a10800401020080ULL, 0x48800100020080ULL,   0x43d00008a224100ULL,
    0x200848000400020ULL,  0x52c0010041008020ULL, 0x82410011022000ULL,   0x10808010040802ULL,
    0x44008004080080ULL,   0x20c808002000400ULL,  0x262040001104802ULL,  0x1400020004004b95ULL,
    0x80005040002001ULL,   0x440400140201000ULL,  0x1004100200010ULL,    0x4e8000880801000ULL,
    0x2108000404004020ULL, 0x4461000900040002ULL, 0x8120010400100288ULL, 0x2000005200048c05ULL,
    0x40004889800220ULL,   0x5080804212002100ULL, 0x400410017002000ULL,  0x41801001801802ULL,
    0x4280040080800800ULL, 0x2000800400800200ULL, 0x983904204000108ULL,  0x204102001c84ULL,
    0x6080004000828020ULL, 0x80402010004000ULL,   0x6080200100410011ULL, 0x91001008b0020ULL,
    0x20e80100110004ULL,   0x1012008408820010ULL, 0x8209020004010100ULL, 0x20000a041020004ULL,
    0x821008000244300ULL,  0x4000208104400300ULL, 0x1800200080100c80ULL, 0x1000100020090100ULL,
    0x208041008010100ULL,  0x84010002004040ULL,   0x800081001020400ULL,  0x144008804c090a00ULL,
    0x2d00104108208001ULL, 0x2c10210010400083ULL, 0x481000842200411ULL,  0x401002010000409ULL,
    0x8002014820500402ULL, 0xa000190040802ULL,    0x200088008201300cULL, 0x82040940102ULL,
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
    return BishopMagics[s](occ);
}

Bitboard rook_attacks(Square s, Bitboard occ) {
    return RookMagics[s](occ);
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
