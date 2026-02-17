#include "eval.h"

#include <atomic>
#include <cctype>

#include "attacks.h"
#include "bitboard.h"
#include "nnue.h"
#include "types.h"

namespace panda {

namespace {

std::atomic<EvalMode> g_evalMode{EvalMode::NNUE};

bool equalsCaseInsensitive(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size())
        return false;

    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i])))
            return false;
    }
    return true;
}

}  // namespace

// ============================================================
// PeSTO Tapered Evaluation
// ============================================================
//
// PST tables are in CPW visual format (index 0 = a8).
// Access: White piece at square s → table[s ^ 56], Black → table[s].

// Phase weights per piece type
constexpr int PHASE_WEIGHT[6] = {0, 1, 1, 2, 4, 0};
constexpr int TOTAL_PHASE = 24;

// MG / EG base piece values
constexpr int MG_PIECE_VALUE[6] = {82, 337, 365, 477, 1025, 0};
constexpr int EG_PIECE_VALUE[6] = {94, 281, 297, 512, 936, 0};

// clang-format off

// ---- Middlegame piece-square tables ----

constexpr int MG_PAWN_TABLE[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0
};

constexpr int MG_KNIGHT_TABLE[64] = {
   -167, -89, -34, -49,  61, -97, -15,-107,
    -73, -41,  72,  36,  23,  62,   7, -17,
    -47,  60,  37,  65,  84, 129,  73,  44,
     -9,  17,  19,  53,  37,  69,  18,  22,
    -13,   4,  16,  13,  28,  19,  21,  -8,
    -23,  -9,  12,  10,  19,  17,  25, -16,
    -29, -53, -12,  -3,  -1,  18, -14, -19,
   -105, -21, -58, -33, -17, -28, -19, -23
};

constexpr int MG_BISHOP_TABLE[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21
};

constexpr int MG_ROOK_TABLE[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26
};

constexpr int MG_QUEEN_TABLE[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50
};

constexpr int MG_KING_TABLE[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14
};

// ---- Endgame piece-square tables ----

constexpr int EG_PAWN_TABLE[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8, -10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0
};

constexpr int EG_KNIGHT_TABLE[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64
};

constexpr int EG_BISHOP_TABLE[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17
};

constexpr int EG_ROOK_TABLE[64] = {
     13,  10,  18,  15,  12,  12,   8,   5,
     11,  13,  13,  11,  -3,   3,   8,   3,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
      4,   3,  13,   1,   2,   1,  -1,   2,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -9,   2,   3,  -1,  -5, -13,   4, -20
};

constexpr int EG_QUEEN_TABLE[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41
};

constexpr int EG_KING_TABLE[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

// clang-format on

constexpr const int* MG_PST[6] = {MG_PAWN_TABLE,   MG_KNIGHT_TABLE, MG_BISHOP_TABLE,
                                   MG_ROOK_TABLE,   MG_QUEEN_TABLE,  MG_KING_TABLE};

constexpr const int* EG_PST[6] = {EG_PAWN_TABLE,   EG_KNIGHT_TABLE, EG_BISHOP_TABLE,
                                   EG_ROOK_TABLE,   EG_QUEEN_TABLE,  EG_KING_TABLE};

// ============================================================
// Helper masks for pawn structure
// ============================================================

// Adjacent files: for isolated pawn and passed pawn detection
constexpr Bitboard AdjacentFileMask[8] = {
    FileMask[1],
    FileMask[0] | FileMask[2],
    FileMask[1] | FileMask[3],
    FileMask[2] | FileMask[4],
    FileMask[3] | FileMask[5],
    FileMask[4] | FileMask[6],
    FileMask[5] | FileMask[7],
    FileMask[6],
};

// Forward ranks: all ranks ahead of a given rank for each color
// ForwardRanks[White][r] = all squares on ranks r+1..7
// ForwardRanks[Black][r] = all squares on ranks 0..r-1
constexpr Bitboard ForwardRanks[2][8] = {
    {// White
     0xFFFFFFFFFFFFFF00ULL, 0xFFFFFFFFFFFF0000ULL, 0xFFFFFFFFFF000000ULL,
     0xFFFFFFFF00000000ULL, 0xFFFFFF0000000000ULL, 0xFFFF000000000000ULL,
     0xFF00000000000000ULL, 0x0000000000000000ULL},
    {// Black
     0x0000000000000000ULL, 0x00000000000000FFULL, 0x000000000000FFFFULL,
     0x0000000000FFFFFFULL, 0x00000000FFFFFFFFULL, 0x000000FFFFFFFFFFULL,
     0x0000FFFFFFFFFFFFULL, 0x00FFFFFFFFFFFFFFULL},
};

// ============================================================
// Evaluation bonuses/penalties
// ============================================================

// Passed pawn bonus by rank (from the pawn's perspective, 0=rank1, 7=rank8)
// Index is the pawn's rank for white; for black, use (7-rank).
// clang-format off
constexpr int PassedPawnMG[8] = {0,  5, 10, 15, 25, 40, 65, 0};
constexpr int PassedPawnEG[8] = {0, 10, 15, 25, 45, 75,120, 0};
// clang-format on

// Isolated pawn penalty
constexpr int IsolatedPawnMG = -10;
constexpr int IsolatedPawnEG = -15;

// Doubled pawn penalty (per extra pawn on a file)
constexpr int DoubledPawnMG = -10;
constexpr int DoubledPawnEG = -15;

// Bishop pair bonus
constexpr int BishopPairMG = 30;
constexpr int BishopPairEG = 50;

// Rook on open / semi-open file
constexpr int RookOpenFileMG = 20;
constexpr int RookOpenFileEG = 10;
constexpr int RookSemiOpenFileMG = 10;
constexpr int RookSemiOpenFileEG = 5;

// King pawn shield (MG penalty per missing pawn in shield zone)
constexpr int PawnShieldPenaltyMG = -10;

// King attacker weights by piece type (indexed by PieceType)
// Higher weight = more dangerous attacker near the king
constexpr int KingAttackerWeight[5] = {0, 2, 2, 3, 5};  // Pawn, Knight, Bishop, Rook, Queen

// Non-linear king danger penalty table (indexed by total attack weight, clamped to 0..99)
// The penalty accelerates as more/stronger pieces attack the king zone.
// clang-format off
constexpr int KingDangerTable[100] = {
      0,   0,   1,   2,   3,   5,   7,  9,   12,  15,
     18,  22,  26,  30,  35,  39,  44,  50,  56,  62,
     68,  75,  82,  85,  89,  97, 105, 113, 122, 131,
    140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
    260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
    377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
    494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
};
// clang-format on

// Mobility tables: bonus per number of safe squares
// clang-format off
constexpr int KnightMobilityMG[9] = {-15, -5,  0,  5, 10, 15, 18, 20, 22};
constexpr int KnightMobilityEG[9] = {-20, -8, -2,  3,  8, 13, 17, 20, 22};

constexpr int BishopMobilityMG[14] = {-15, -8, -2,  2,  6, 10, 14, 17, 19, 21, 23, 25, 27, 28};
constexpr int BishopMobilityEG[14] = {-20,-10, -4,  0,  5, 10, 14, 17, 20, 22, 24, 26, 27, 28};

constexpr int RookMobilityMG[15] = {-15,-10, -5, -2,  0,  2,  5,  7,  9, 11, 13, 14, 15, 16, 17};
constexpr int RookMobilityEG[15] = {-25,-15, -8, -3,  0,  5,  9, 13, 16, 19, 21, 23, 25, 26, 27};

constexpr int QueenMobilityMG[28] = {
    -10, -7, -4, -2,  0,  1,  2,  3,  4,  5,  5,  6,  6,  7,
      7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9, 10, 10
};
constexpr int QueenMobilityEG[28] = {
    -20,-14, -8, -4, -1,  1,  3,  5,  7,  9, 11, 12, 14, 15,
     16, 17, 18, 19, 20, 20, 21, 21, 22, 22, 22, 23, 23, 23
};
// clang-format on

// Mirror a square vertically (flip rank).
constexpr Square mirror(Square s) {
    return Square(s ^ 56);
}

// ============================================================
// Evaluation helpers
// ============================================================

// Evaluate pawn structure for one color, accumulate into mg/eg scores (from White's perspective).
// sign: +1 for White, -1 for Black.
static void evalPawns(const Board& board, Color us, int sign, int& mgScore, int& egScore) {
    Color them = ~us;
    Bitboard ourPawns = board.pieces(us, Pawn);
    Bitboard theirPawns = board.pieces(them, Pawn);

    // --- Doubled pawns: count per file ---
    for (int f = 0; f < 8; ++f) {
        int cnt = popcount(ourPawns & FileMask[f]);
        if (cnt > 1) {
            mgScore += sign * DoubledPawnMG * (cnt - 1);
            egScore += sign * DoubledPawnEG * (cnt - 1);
        }
    }

    // --- Per-pawn terms: passed, isolated ---
    Bitboard pawns = ourPawns;
    while (pawns) {
        Square s = pop_lsb(pawns);
        int file = square_file(s);
        int rank = square_rank(s);

        // Rank from the pawn's perspective (how far advanced)
        int relRank = (us == White) ? rank : (7 - rank);

        // Passed pawn: no enemy pawns on same/adjacent files ahead
        Bitboard frontSpan =
            (FileMask[file] | AdjacentFileMask[file]) & ForwardRanks[us][rank];
        if (!(theirPawns & frontSpan)) {
            mgScore += sign * PassedPawnMG[relRank];
            egScore += sign * PassedPawnEG[relRank];
        }

        // Isolated pawn: no friendly pawns on adjacent files
        if (!(ourPawns & AdjacentFileMask[file])) {
            mgScore += sign * IsolatedPawnMG;
            egScore += sign * IsolatedPawnEG;
        }
    }
}

// Evaluate mobility for one color, accumulate into mg/eg scores.
static void evalMobility(const Board& board, Color us, int sign, int& mgScore, int& egScore) {
    Bitboard occ = board.all_pieces();
    Bitboard ownPieces = board.pieces(us);

    // Knights
    Bitboard pieces = board.pieces(us, Knight);
    while (pieces) {
        Square s = pop_lsb(pieces);
        int mob = popcount(attacks::knight_attacks(s) & ~ownPieces);
        if (mob > 8) mob = 8;
        mgScore += sign * KnightMobilityMG[mob];
        egScore += sign * KnightMobilityEG[mob];
    }

    // Bishops
    pieces = board.pieces(us, Bishop);
    while (pieces) {
        Square s = pop_lsb(pieces);
        int mob = popcount(attacks::bishop_attacks(s, occ) & ~ownPieces);
        if (mob > 13) mob = 13;
        mgScore += sign * BishopMobilityMG[mob];
        egScore += sign * BishopMobilityEG[mob];
    }

    // Rooks
    pieces = board.pieces(us, Rook);
    while (pieces) {
        Square s = pop_lsb(pieces);
        int mob = popcount(attacks::rook_attacks(s, occ) & ~ownPieces);
        if (mob > 14) mob = 14;
        mgScore += sign * RookMobilityMG[mob];
        egScore += sign * RookMobilityEG[mob];
    }

    // Queens
    pieces = board.pieces(us, Queen);
    while (pieces) {
        Square s = pop_lsb(pieces);
        int mob = popcount(attacks::queen_attacks(s, occ) & ~ownPieces);
        if (mob > 27) mob = 27;
        mgScore += sign * QueenMobilityMG[mob];
        egScore += sign * QueenMobilityEG[mob];
    }
}

// Evaluate pieces (bishop pair, rook on open/semi-open file) for one color.
static void evalPieces(const Board& board, Color us, int sign, int& mgScore, int& egScore) {
    Bitboard ourPawns = board.pieces(us, Pawn);
    Bitboard theirPawns = board.pieces(~us, Pawn);
    Bitboard allPawns = ourPawns | theirPawns;

    // Bishop pair
    if (popcount(board.pieces(us, Bishop)) >= 2) {
        mgScore += sign * BishopPairMG;
        egScore += sign * BishopPairEG;
    }

    // Rook on open / semi-open file
    Bitboard rooks = board.pieces(us, Rook);
    while (rooks) {
        Square s = pop_lsb(rooks);
        int file = square_file(s);
        Bitboard fileBB = FileMask[file];

        if (!(allPawns & fileBB)) {
            // Open file: no pawns at all
            mgScore += sign * RookOpenFileMG;
            egScore += sign * RookOpenFileEG;
        } else if (!(ourPawns & fileBB)) {
            // Semi-open file: no friendly pawns
            mgScore += sign * RookSemiOpenFileMG;
            egScore += sign * RookSemiOpenFileEG;
        }
    }
}

// Evaluate king safety for one color.  MG-only.
// Combines pawn shield and attacker-based king danger.
static void evalKingSafety(const Board& board, Color us, int sign, int& mgScore) {
    Color them = ~us;
    Square kingSq = lsb(board.pieces(us, King));
    int kingFile = square_file(kingSq);
    int kingRank = square_rank(kingSq);
    Bitboard occ = board.all_pieces();
    Bitboard ourPawns = board.pieces(us, Pawn);

    // --- Pawn shield (only when king is on back ranks) ---
    bool onBackRanks = (us == White) ? (kingRank <= 1) : (kingRank >= 6);
    if (onBackRanks) {
        int shieldRank = (us == White) ? kingRank + 1 : kingRank - 1;
        if (shieldRank >= 0 && shieldRank <= 7) {
            int missingShield = 0;
            int minFile = (kingFile > 0) ? kingFile - 1 : 0;
            int maxFile = (kingFile < 7) ? kingFile + 1 : 7;

            for (int f = minFile; f <= maxFile; ++f) {
                Square shieldSq = make_square(f, shieldRank);
                if (!(ourPawns & square_bb(shieldSq)))
                    ++missingShield;
            }
            mgScore += sign * PawnShieldPenaltyMG * missingShield;
        }
    }

    // --- Attacker-based king danger ---
    // King zone: squares the king attacks + the king square itself
    Bitboard kingZone = attacks::king_attacks(kingSq) | square_bb(kingSq);

    int attackWeight = 0;
    int attackerCount = 0;

    // Enemy knights attacking king zone
    Bitboard enemyKnights = board.pieces(them, Knight);
    while (enemyKnights) {
        Square s = pop_lsb(enemyKnights);
        if (attacks::knight_attacks(s) & kingZone) {
            attackWeight += KingAttackerWeight[Knight];
            ++attackerCount;
        }
    }

    // Enemy bishops attacking king zone
    Bitboard enemyBishops = board.pieces(them, Bishop);
    while (enemyBishops) {
        Square s = pop_lsb(enemyBishops);
        if (attacks::bishop_attacks(s, occ) & kingZone) {
            attackWeight += KingAttackerWeight[Bishop];
            ++attackerCount;
        }
    }

    // Enemy rooks attacking king zone
    Bitboard enemyRooks = board.pieces(them, Rook);
    while (enemyRooks) {
        Square s = pop_lsb(enemyRooks);
        if (attacks::rook_attacks(s, occ) & kingZone) {
            attackWeight += KingAttackerWeight[Rook];
            ++attackerCount;
        }
    }

    // Enemy queens attacking king zone
    Bitboard enemyQueens = board.pieces(them, Queen);
    while (enemyQueens) {
        Square s = pop_lsb(enemyQueens);
        if (attacks::queen_attacks(s, occ) & kingZone) {
            attackWeight += KingAttackerWeight[Queen];
            ++attackerCount;
        }
    }

    // Only apply danger penalty if 2+ pieces are attacking
    if (attackerCount >= 2) {
        int dangerIdx = attackWeight;
        if (dangerIdx > 99) dangerIdx = 99;
        mgScore -= sign * KingDangerTable[dangerIdx];
    }
}

// ============================================================
// Main evaluation function
// ============================================================

int evaluate_handcrafted(const Board& board) {
    int mgScore = 0;
    int egScore = 0;
    int phase = 0;

    // 1. Material + PST (existing PeSTO tapered eval)
    for (int pt = Pawn; pt <= King; ++pt) {
        Bitboard bb = board.pieces(White, PieceType(pt));
        while (bb) {
            Square s = pop_lsb(bb);
            int idx = mirror(s);
            mgScore += MG_PIECE_VALUE[pt] + MG_PST[pt][idx];
            egScore += EG_PIECE_VALUE[pt] + EG_PST[pt][idx];
            phase += PHASE_WEIGHT[pt];
        }

        bb = board.pieces(Black, PieceType(pt));
        while (bb) {
            Square s = pop_lsb(bb);
            int idx = s;
            mgScore -= MG_PIECE_VALUE[pt] + MG_PST[pt][idx];
            egScore -= EG_PIECE_VALUE[pt] + EG_PST[pt][idx];
            phase += PHASE_WEIGHT[pt];
        }
    }

    // 2. Pawn structure (passed, isolated, doubled)
    evalPawns(board, White, +1, mgScore, egScore);
    evalPawns(board, Black, -1, mgScore, egScore);

    // 3. Piece terms (bishop pair, rook on open file)
    evalPieces(board, White, +1, mgScore, egScore);
    evalPieces(board, Black, -1, mgScore, egScore);

    // 4. King safety (pawn shield, MG only)
    evalKingSafety(board, White, +1, mgScore);
    evalKingSafety(board, Black, -1, mgScore);

    // 5. Mobility
    evalMobility(board, White, +1, mgScore, egScore);
    evalMobility(board, Black, -1, mgScore, egScore);

    // Clamp phase
    if (phase > TOTAL_PHASE)
        phase = TOTAL_PHASE;

    // Tapered interpolation
    int score = (mgScore * phase + egScore * (TOTAL_PHASE - phase)) / TOTAL_PHASE;

    return (board.side_to_move() == White) ? score : -score;
}

void set_eval_mode(EvalMode mode) {
    g_evalMode.store(mode, std::memory_order_relaxed);
}

EvalMode get_eval_mode() {
    return g_evalMode.load(std::memory_order_relaxed);
}

const char* eval_mode_name(EvalMode mode) {
    return mode == EvalMode::NNUE ? "NNUE" : "Handcrafted";
}

bool parse_eval_mode(std::string_view value, EvalMode& modeOut) {
    if (equalsCaseInsensitive(value, "NNUE")) {
        modeOut = EvalMode::NNUE;
        return true;
    }

    if (equalsCaseInsensitive(value, "Handcrafted")) {
        modeOut = EvalMode::Handcrafted;
        return true;
    }

    return false;
}

int evaluate(const Board& board, nnue::SearchNnueContext* ctx) {
    if (get_eval_mode() == EvalMode::NNUE)
        return evaluate_nnue(board, ctx);
    return evaluate_handcrafted(board);
}

int evaluate(const Board& board) {
    return evaluate(board, nullptr);
}

}  // namespace panda
