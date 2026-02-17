#include <gtest/gtest.h>

#include <chrono>

#include "../attacks.h"
#include "../board.h"
#include "../eval.h"
#include "../move.h"
#include "../movegen.h"
#include "../search.h"
#include "../tt.h"
#include "../zobrist.h"

using namespace panda;

class SearchTestEnvironment : public ::testing::Environment {
   public:
    void SetUp() override {
        zobrist::init();
        attacks::init();
        set_eval_mode(EvalMode::Handcrafted);
    }
};

static Move findMoveByUci(const Board& board, const std::string& uci) {
    MoveList legal = generate_legal(board);
    for (int i = 0; i < legal.size(); ++i) {
        Move m = legal[i];
        if (move_to_uci(m) == uci)
            return m;
    }
    return NullMove;
}

static void applyUciSequence(Board& board, std::vector<uint64_t>& history,
                             const std::vector<std::string>& sequence) {
    for (const std::string& uci : sequence) {
        Move m = findMoveByUci(board, uci);
        ASSERT_NE(m, NullMove) << "Illegal move in test sequence: " << uci;
        board.make_move(m);
        history.push_back(board.hash_key());
    }
}

// ============================================================
// Transposition Table tests
// ============================================================

TEST(TTTest, StoreAndProbe) {
    TranspositionTable tt(1);  // 1 MB
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

TEST(TTTest, CollisionKeepsDeeperEntry) {
    TranspositionTable tt(0);  // Force one-slot table (mask = 0)
    uint64_t keyA = 0x123456789ABCDEF0ULL;
    uint64_t keyB = 0x0FEDCBA987654321ULL;

    tt.store(keyA, 100, 10, TT_ALPHA, make_move(E2, E4));
    tt.store(keyB, 50, 4, TT_BETA, make_move(D2, D4));

    TTEntry entry;
    EXPECT_TRUE(tt.probe(keyA, entry));
    EXPECT_FALSE(tt.probe(keyB, entry));
}

TEST(TTTest, CollisionPrefersExactAtEqualDepth) {
    TranspositionTable tt(0);  // Force one-slot table (mask = 0)
    uint64_t keyA = 0x1111111111111111ULL;
    uint64_t keyB = 0x2222222222222222ULL;

    tt.store(keyA, 10, 6, TT_ALPHA, make_move(E2, E4));
    tt.store(keyB, 20, 6, TT_EXACT, make_move(D2, D4));

    TTEntry entry;
    EXPECT_FALSE(tt.probe(keyA, entry));
    ASSERT_TRUE(tt.probe(keyB, entry));
    EXPECT_EQ(entry.depth, 6);
    EXPECT_EQ(entry.flag, TT_EXACT);
}

TEST(TTTest, CollisionReplacedWhenStale) {
    TranspositionTable tt(0);  // Force one-slot table (mask = 0)
    uint64_t keyA = 0x3333333333333333ULL;
    uint64_t keyB = 0x4444444444444444ULL;

    tt.store(keyA, 100, 10, TT_EXACT, make_move(E2, E4));
    tt.new_search();
    tt.new_search();
    tt.store(keyB, 25, 2, TT_ALPHA, make_move(D2, D4));

    TTEntry entry;
    EXPECT_FALSE(tt.probe(keyA, entry));
    EXPECT_TRUE(tt.probe(keyB, entry));
}

TEST(TTTest, SameKeyShallowerBoundDoesNotDegrade) {
    TranspositionTable tt(1);
    uint64_t key = 0x5555555555555555ULL;

    tt.store(key, 120, 8, TT_EXACT, make_move(E2, E4));
    tt.store(key, 80, 5, TT_ALPHA, make_move(D2, D4));

    TTEntry entry;
    ASSERT_TRUE(tt.probe(key, entry));
    EXPECT_EQ(entry.depth, 8);
    EXPECT_EQ(entry.flag, TT_EXACT);
    EXPECT_EQ(entry.score, 120);
    EXPECT_EQ(entry.bestMove, make_move(E2, E4));
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
    Board board;
    board.set_fen("6k1/5ppp/8/8/8/8/8/K6Q w - - 0 1");

    TranspositionTable tt(1);
    SearchResult result = searchDepth(board, 3, tt);

    // The best move should deliver checkmate
    Board after = board;
    after.make_move(result.bestMove);
    EXPECT_TRUE(is_checkmate(after));
    EXPECT_GT(result.score, MATE_SCORE - 100);
}

TEST(SearchTest, MateIn1_ScholarsMate) {
    Board board;
    board.set_fen("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 4 4");

    TranspositionTable tt(1);
    SearchResult result = searchDepth(board, 3, tt);

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
    SearchResult result = searchDepth(board, 5, tt);

    // Should find a forced mate
    EXPECT_GT(result.score, MATE_SCORE - 100);
}

// ============================================================
// Search tests: Avoid stalemate
// ============================================================

TEST(SearchTest, AvoidStalemate) {
    Board board;
    board.set_fen("7k/8/5K2/6Q1/8/8/8/8 w - - 0 1");

    TranspositionTable tt(1);
    SearchResult result = searchDepth(board, 4, tt);

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
    SearchResult result = searchDepth(board, 4, tt);

    EXPECT_NE(result.bestMove, NullMove);
}

// ============================================================
// Iterative deepening with time limit
// ============================================================

TEST(SearchTest, IterativeDeepeningReturnsMove) {
    Board board;
    board.set_fen(StartFEN);

    TranspositionTable tt(1);
    auto start = std::chrono::steady_clock::now();
    SearchResult result = search(board, 500, tt);  // 500ms time limit
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();

    EXPECT_NE(result.bestMove, NullMove);
    // Should complete within reasonable time (allow some overhead)
    EXPECT_LT(elapsed, 2000);
}

TEST(SearchTest, IterativeDeepeningFindsMate) {
    // Mate in 1 should be found almost instantly
    Board board;
    board.set_fen("6k1/5ppp/8/8/8/8/8/K6Q w - - 0 1");

    TranspositionTable tt(1);
    SearchResult result = search(board, 5000, tt);  // generous time

    Board after = board;
    after.make_move(result.bestMove);
    EXPECT_TRUE(is_checkmate(after));
    EXPECT_GT(result.score, MATE_SCORE - 100);
}

// ============================================================
// Quiescence regression tests
// ============================================================

TEST(SearchTest, QuiescenceQuietLeafKeepsMaterialAdvantage) {
    // White is up a rook in a quiet endgame. A depth-2 search should not collapse to draw score.
    Board board;
    board.set_fen("4k3/8/8/8/8/8/8/4KR2 w - - 0 1");

    TranspositionTable tt(1);
    SearchResult result = searchDepth(board, 2, tt);

    EXPECT_GT(result.score, 200);
}

TEST(SearchTest, QuiescenceInCheckDoesNotReturnFalseMate) {
    // This is not a mate-in-1 position, but broken in-check qsearch can report mate.
    Board board;
    board.set_fen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1");

    TranspositionTable tt(1);
    SearchResult result = searchDepth(board, 1, tt);

    Board after = board;
    after.make_move(result.bestMove);

    EXPECT_FALSE(is_checkmate(after));
    EXPECT_LT(result.score, MATE_SCORE - MAX_PLY);
}

TEST(SearchTest, QuiescenceMateDistanceConsistentAcrossDepths) {
    // In this tactical position, d2 and d3 should agree on mate distance.
    // If qsearch does not increment ply on recursion, d2 is off by one.
    Board board;
    board.set_fen("r2qkbnr/pb1n3p/1p1pp3/2pP1QP1/8/2N1P3/PPP1BPP1/R1B1K1NR w KQkq - 1 10");

    TranspositionTable tt2(1);
    SearchResult d2 = searchDepth(board, 2, tt2);

    TranspositionTable tt3(1);
    SearchResult d3 = searchDepth(board, 3, tt3);

    EXPECT_GT(d3.score, MATE_SCORE - MAX_PLY);
    EXPECT_EQ(d2.score, d3.score);
}

TEST(SearchTest, QuiescenceStalemateReturnsDraw) {
    // Black to move is stalemated.
    Board board;
    board.set_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");

    int score = quiescenceForTests(board, -MATE_SCORE, MATE_SCORE);
    EXPECT_EQ(score, 0);
}

TEST(SearchTest, QuiescenceHonorsFiftyMoveRule) {
    // Rule-50 draw should score as draw at qsearch entry.
    Board board;
    board.set_fen("4k3/8/8/8/8/8/8/4KR2 w - - 100 1");

    int score = quiescenceForTests(board, -MATE_SCORE, MATE_SCORE);
    EXPECT_EQ(score, 0);
}

// ============================================================
// Threefold repetition tests
// ============================================================

TEST(SearchTest, ThreefoldRepetitionEvaluatesAsDraw) {
    Board board;
    board.set_fen("4k3/8/8/8/8/8/8/4KR2 w - - 0 1");

    std::vector<uint64_t> history{board.hash_key()};
    applyUciSequence(board, history, {"f1f2", "e8e7", "f2f1", "e7e8", "f1f2", "e8e7", "f2f1", "e7e8"});

    TranspositionTable tt(1);
    std::atomic<bool> stopFlag{false};
    SearchResult result = search(board, 0, 4, tt, stopFlag, history);

    EXPECT_EQ(result.score, 0);
    EXPECT_NE(result.bestMove, NullMove);
}

TEST(SearchTest, TwofoldRepetitionIsNotForcedDraw) {
    Board board;
    board.set_fen("4k3/8/8/8/8/8/8/4KR2 w - - 0 1");

    std::vector<uint64_t> history{board.hash_key()};
    applyUciSequence(board, history, {"f1f2", "e8e7", "f2f1", "e7e8"});

    TranspositionTable tt(1);
    std::atomic<bool> stopFlag{false};
    SearchResult result = search(board, 0, 4, tt, stopFlag, history);

    EXPECT_GT(result.score, 200);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new SearchTestEnvironment());
    return RUN_ALL_TESTS();
}
