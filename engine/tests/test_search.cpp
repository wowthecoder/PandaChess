#include "../attacks.h"
#include "../board.h"
#include "../eval.h"
#include "../move.h"
#include "../movegen.h"
#include "../search.h"
#include "../tt.h"
#include "../zobrist.h"
#include <gtest/gtest.h>

using namespace panda;

class SearchTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        zobrist::init();
        attacks::init();
    }
};

// ============================================================
// Transposition Table tests
// ============================================================

TEST(TTTest, StoreAndProbe) {
    TranspositionTable tt(1); // 1 MB
    uint64_t key = 0x123456789ABCDEF0ULL;
    Move m = make_move(E2, E4);

    tt.store(key, 42, 5, TT_EXACT, m);

    TTEntry entry;
    ASSERT_TRUE(tt.probe(key, entry));
    EXPECT_EQ(entry.key, key);
    EXPECT_EQ(entry.score, 42);
    EXPECT_EQ(entry.depth, 5);
    EXPECT_EQ(entry.flag, TT_EXACT);
    EXPECT_EQ(entry.bestMove, m);
}

TEST(TTTest, ProbeMiss) {
    TranspositionTable tt(1);
    uint64_t key = 0x123456789ABCDEF0ULL;

    TTEntry entry;
    EXPECT_FALSE(tt.probe(key, entry));
}

TEST(TTTest, Clear) {
    TranspositionTable tt(1);
    uint64_t key = 0xDEADBEEFDEADBEEFULL;
    tt.store(key, 100, 3, TT_BETA, make_move(D2, D4));

    tt.clear();

    TTEntry entry;
    EXPECT_FALSE(tt.probe(key, entry));
}

// ============================================================
// Evaluation tests
// ============================================================

TEST(EvalTest, StartPositionSymmetric) {
    Board board;
    board.set_fen(StartFEN);
    // Starting position should be roughly 0 (symmetric)
    int score = evaluate(board);
    EXPECT_GE(score, -50);
    EXPECT_LE(score, 50);
}

TEST(EvalTest, MaterialAdvantage) {
    // White has an extra queen
    Board board;
    board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int baseScore = evaluate(board);

    Board board2;
    // Remove black queen
    board2.set_fen("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int advantageScore = evaluate(board2);

    // White should score higher when black is missing a queen
    EXPECT_GT(advantageScore, baseScore);
}

// ============================================================
// Search tests: Mate in 1
// ============================================================

TEST(SearchTest, MateIn1_BackRankMate) {
    // White to move, Qh7# is mate in 1
    // Position: Black king on g8, white queen on h1, white king on a1
    // Simplified: 6k1/5ppp/8/8/8/8/8/K6Q w - - 0 1
    Board board;
    board.set_fen("6k1/5ppp/8/8/8/8/8/K6Q w - - 0 1");

    TranspositionTable tt(1);
    SearchResult result = search(board, 3, tt);

    // The best move should deliver checkmate
    Board after = board;
    after.make_move(result.bestMove);
    EXPECT_TRUE(is_checkmate(after));
    EXPECT_GT(result.score, MATE_SCORE - 100);
}

TEST(SearchTest, MateIn1_ScholarsMate) {
    // White to move, Qxf7# is mate
    Board board;
    board.set_fen("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 4 4");

    TranspositionTable tt(1);
    SearchResult result = search(board, 3, tt);

    Board after = board;
    after.make_move(result.bestMove);
    EXPECT_TRUE(is_checkmate(after));
}

// ============================================================
// Search tests: Mate in 2
// ============================================================

TEST(SearchTest, MateIn2) {
    // White: Kc8, Ra1, pawn b6; Black: Ka8, Bb8, pawns a7, b7
    // 1. Ra6! bxa6 (or any) 2. b7# is checkmate
    Board board;
    board.set_fen("kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1");

    TranspositionTable tt(1);
    SearchResult result = search(board, 5, tt);

    // Should find a forced mate
    EXPECT_GT(result.score, MATE_SCORE - 100);
}

// ============================================================
// Search tests: Avoid stalemate
// ============================================================

TEST(SearchTest, AvoidStalemate) {
    // White is up huge material but must not stalemate
    // K7/8/1Q6/8/8/8/8/7k w - - 0 1
    // White queen on b6, white king a8, black king h1
    // Qb1+ would be stalemate if black had no moves after... but let's use a cleaner position
    // Better: Kf6, Qg5, black king h8 — Qg7 would be stalemate
    Board board;
    board.set_fen("7k/8/5K2/6Q1/8/8/8/8 w - - 0 1");

    TranspositionTable tt(1);
    SearchResult result = search(board, 4, tt);

    // Should not play Qg7 (stalemate). Should find checkmate instead.
    Board after = board;
    after.make_move(result.bestMove);
    EXPECT_FALSE(is_stalemate(after));
    // In this position, white should find mate
    EXPECT_GT(result.score, MATE_SCORE - 100);
}

// ============================================================
// Search tests: Basic sanity
// ============================================================

TEST(SearchTest, StartPositionReturnsMove) {
    Board board;
    board.set_fen(StartFEN);

    TranspositionTable tt(1);
    SearchResult result = search(board, 4, tt);

    EXPECT_NE(result.bestMove, NullMove);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new SearchTestEnvironment());
    return RUN_ALL_TESTS();
}
