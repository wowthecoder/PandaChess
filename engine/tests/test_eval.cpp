#include <gtest/gtest.h>

#include "../attacks.h"
#include "../board.h"
#include "../eval.h"
#include "../zobrist.h"

using namespace panda;

class EvalTestEnvironment : public ::testing::Environment {
   public:
    void SetUp() override {
        zobrist::init();
        attacks::init();
    }
};

static auto* env = ::testing::AddGlobalTestEnvironment(new EvalTestEnvironment);

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// Helper: create a board from FEN and return its evaluation.
static int evalFen(const std::string& fen) {
    Board board;
    board.set_fen(fen);
    return evaluate(board);
}

// ============================================================
// Basic sanity
// ============================================================

TEST(EvalTest, StartPositionIsRoughlyEqual) {
    int score = evalFen(StartFEN);
    // Starting position should be close to 0 (small white advantage is fine).
    EXPECT_GT(score, -50);
    EXPECT_LT(score, 50);
}

TEST(EvalTest, SymmetricPositionIsZero) {
    // A perfectly symmetric position. Side-to-move is white, score should be ~0.
    int score = evalFen(StartFEN);
    // With tapered eval + PST the start pos is very close to 0.
    EXPECT_NEAR(score, 0, 30);
}

// ============================================================
// Material advantage
// ============================================================

TEST(EvalTest, ExtraQueenIsLargeAdvantage) {
    // White has an extra queen vs bare king.
    int score = evalFen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1");
    EXPECT_GT(score, 500);
}

TEST(EvalTest, MaterialDownIsNegative) {
    // White is missing a rook.
    int score = evalFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/1NBQKBNR w Kkq - 0 1");
    EXPECT_LT(score, -200);
}

// ============================================================
// Pawn structure
// ============================================================

TEST(EvalTest, PassedPawnBonus) {
    // White has a passed pawn on e5 (no black pawns on d,e,f files ahead).
    // Compared to a position where the pawn is blocked.
    int withPassed = evalFen("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1");
    int withBlocked = evalFen("4k3/5p2/8/4P3/8/8/8/4K3 w - - 0 1");
    EXPECT_GT(withPassed, withBlocked);
}

TEST(EvalTest, IsolatedPawnPenalty) {
    // White has an isolated pawn on e4 (no pawns on d or f files).
    int isolated = evalFen("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
    // White has a non-isolated pawn on e4 with support on d2.
    int supported = evalFen("4k3/8/8/8/4P3/8/3P4/4K3 w - - 0 1");
    // Supported position should score better (has extra pawn AND no isolation).
    EXPECT_GT(supported, isolated);
}

TEST(EvalTest, DoubledPawnPenalty) {
    // White has doubled pawns on e-file.
    int doubled = evalFen("4k3/8/8/4P3/4P3/8/8/4K3 w - - 0 1");
    // White has two non-doubled pawns.
    int notDoubled = evalFen("4k3/8/8/4P3/3P4/8/8/4K3 w - - 0 1");
    EXPECT_GT(notDoubled, doubled);
}

// ============================================================
// Bishop pair
// ============================================================

TEST(EvalTest, BishopPairBonus) {
    // White has two bishops, black has two knights. Otherwise equal.
    int bishopPair = evalFen("4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1");
    // White has one bishop and one knight.
    int noPair = evalFen("4k3/8/8/8/8/8/8/2N1KB2 w - - 0 1");
    EXPECT_GT(bishopPair, noPair);
}

// ============================================================
// Rook on open / semi-open file
// ============================================================

TEST(EvalTest, RookOnOpenFileBonus) {
    // White rook on open e-file (no pawns on e-file).
    int openFile = evalFen("4k3/pppp1ppp/8/8/8/8/PPPP1PPP/4RK2 w - - 0 1");
    // White rook on closed e-file (pawns present).
    int closedFile = evalFen("4k3/pppppppp/8/8/8/8/PPPPPPPP/4RK2 w - - 0 1");
    EXPECT_GT(openFile, closedFile);
}

TEST(EvalTest, RookOnSemiOpenFileBonus) {
    // Semi-open: white rook on e-file, no friendly pawn on e, black pawn on e7.
    // Compare rook on e1 (semi-open e-file) vs rook on d1 (closed d-file, d-pawn present).
    int semiOpen = evalFen("4k3/4p3/8/8/8/8/3P4/3RK3 w - - 0 1");
    int closed = evalFen("4k3/4p3/8/8/8/8/4P3/3RK3 w - - 0 1");
    // In semiOpen, the d-file has our pawn so rook gets nothing.
    // In closed, the e-file has our pawn so rook on d1 also blocked.
    // Actually let me just test: rook on file with no friendly pawn vs rook on file with friendly pawn.
    semiOpen = evalFen("4k3/3p4/8/8/8/8/4P3/3RK3 w - - 0 1");  // Rook on d-file, no friendly d-pawn, enemy d-pawn = semi-open
    closed = evalFen("4k3/3p4/8/8/8/8/3PP3/3RK3 w - - 0 1");    // Rook on d-file, friendly d-pawn present = closed (but extra pawn material)
    // The extra pawn gives material advantage that can mask the semi-open bonus.
    // Instead, keep material equal and just change pawn placement.
    semiOpen = evalFen("4k3/3p4/8/8/8/8/1P6/3RK3 w - - 0 1");  // Rook on d, pawn on b, enemy pawn on d = semi-open d-file
    closed = evalFen("4k3/3p4/8/8/8/8/3P4/3RK3 w - - 0 1");    // Rook on d, pawn on d, enemy pawn on d = closed d-file
    EXPECT_GT(semiOpen, closed);
}

// ============================================================
// King safety
// ============================================================

TEST(EvalTest, PawnShieldBenefitsKing) {
    // White king on g1 with intact pawn shield.
    int shielded = evalFen("r3k3/pppppppp/8/8/8/8/PPPPPPPP/R5K1 w q - 0 1");
    // White king on g1 with no pawn shield (f,g,h pawns removed).
    int exposed = evalFen("r3k3/pppppppp/8/8/8/8/PPPPP3/R5K1 w q - 0 1");
    EXPECT_GT(shielded, exposed);
}

TEST(EvalTest, KingDangerFromMultipleAttackers) {
    // White king under pressure from black queen and rook aiming at king zone.
    int danger = evalFen("6k1/8/8/8/8/5q2/5r2/6K1 w - - 0 1");
    // White king with less pressure (only one attacker).
    int safe = evalFen("6k1/8/8/8/8/8/5r2/6K1 w - - 0 1");
    // More attackers = worse for white = lower score.
    EXPECT_LT(danger, safe);
}

// ============================================================
// Mobility
// ============================================================

TEST(EvalTest, TrappedKnightHasLowMobility) {
    // White knight on a1 (corner, very few moves) vs knight on e4 (central, many moves).
    int corner = evalFen("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");
    int center = evalFen("4k3/8/8/8/4N3/8/8/4K3 w - - 0 1");
    EXPECT_GT(center, corner);
}

TEST(EvalTest, BishopMobilityMatters) {
    // Bishop on open diagonal (e4, many squares) vs bishop on edge (a1, fewer squares).
    // Same material, just different bishop placement.
    int open = evalFen("4k3/8/8/8/4B3/8/8/4K3 w - - 0 1");
    int corner = evalFen("4k3/8/8/8/8/8/8/B3K3 w - - 0 1");
    // Central bishop should have more mobility and a better PST score.
    EXPECT_GT(open, corner);
}

TEST(EvalTest, RookMobilityOnOpenBoard) {
    // Rook centralized (e4, many squares) vs rook in corner (a1, fewer squares).
    // Same material, just different rook placement.
    int central = evalFen("4k3/8/8/8/4R3/8/8/4K3 w - - 0 1");
    int corner = evalFen("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    EXPECT_GT(central, corner);
}

// ============================================================
// Perspective correctness
// ============================================================

TEST(EvalTest, FlippedSideFlipsSign) {
    // Same material imbalance, but from black's perspective.
    // White up a knight.
    int whiteToMove = evalFen("4k3/8/8/8/8/8/8/4KN2 w - - 0 1");
    // Now black to move in the same position.
    int blackToMove = evalFen("4k3/8/8/8/8/8/8/4KN2 b - - 0 1");
    // Both should see white as better, but sign flips relative to side-to-move.
    EXPECT_GT(whiteToMove, 0);
    EXPECT_LT(blackToMove, 0);
}

TEST(EvalTest, MirroredPositionOppositeScores) {
    // White has extra knight.
    int whiteUp = evalFen("4k3/8/8/8/8/8/8/2N1K3 w - - 0 1");
    // Black has extra knight (mirrored).
    int blackUp = evalFen("2n1k3/8/8/8/8/8/8/4K3 w - - 0 1");
    // Scores should be roughly opposite.
    EXPECT_GT(whiteUp, 0);
    EXPECT_LT(blackUp, 0);
}
