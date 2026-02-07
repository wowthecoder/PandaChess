#include "../board.h"
#include "../zobrist.h"
#include "../bitboard.h"
#include <iostream>
#include <cstdlib>
#include <string>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void name()
#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cerr << "  FAIL: " << #a << " != " << #b \
                  << " (got " << (a) << " vs " << (b) << ")" \
                  << " at " << __FILE__ << ":" << __LINE__ << '\n'; \
        tests_failed++; return; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << #expr \
                  << " at " << __FILE__ << ":" << __LINE__ << '\n'; \
        tests_failed++; return; \
    } \
} while(0)

#define RUN_TEST(name) do { \
    std::cout << "Running " << #name << "... "; \
    name(); \
    if (tests_failed == prev_failed) { \
        tests_passed++; \
        std::cout << "OK\n"; \
    } else { prev_failed = tests_failed; } \
} while(0)

using namespace panda;

TEST(test_start_position) {
    Board board;
    board.set_fen(StartFEN);

    // Side to move
    ASSERT_EQ(board.side_to_move(), White);

    // Castling
    ASSERT_EQ(board.castling_rights(), AllCastling);

    // No en passant
    ASSERT_EQ(board.en_passant_square(), NoSquare);

    // Halfmove and fullmove
    ASSERT_EQ(board.halfmove_clock(), 0);
    ASSERT_EQ(board.fullmove_number(), 1);

    // White pawns on rank 2
    ASSERT_EQ(board.pieces(WhitePawn), RankMask[1]);

    // Black pawns on rank 7
    ASSERT_EQ(board.pieces(BlackPawn), RankMask[6]);

    // Specific pieces
    ASSERT_EQ(board.piece_on(E1), WhiteKing);
    ASSERT_EQ(board.piece_on(E8), BlackKing);
    ASSERT_EQ(board.piece_on(A1), WhiteRook);
    ASSERT_EQ(board.piece_on(H8), BlackRook);
    ASSERT_EQ(board.piece_on(D1), WhiteQueen);
    ASSERT_EQ(board.piece_on(D8), BlackQueen);
    ASSERT_EQ(board.piece_on(E4), NoPiece);

    // Occupancy
    ASSERT_EQ(board.pieces(White), RankMask[0] | RankMask[1]);
    ASSERT_EQ(board.pieces(Black), RankMask[6] | RankMask[7]);
    ASSERT_EQ(board.all_pieces(), RankMask[0] | RankMask[1] | RankMask[6] | RankMask[7]);
}

TEST(test_custom_fen) {
    Board board;
    // Position after 1. e4
    board.set_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

    ASSERT_EQ(board.side_to_move(), Black);
    ASSERT_EQ(board.en_passant_square(), E3);
    ASSERT_EQ(board.piece_on(E4), WhitePawn);
    ASSERT_EQ(board.piece_on(E2), NoPiece);
    ASSERT_EQ(board.castling_rights(), AllCastling);
}

TEST(test_partial_castling) {
    Board board;
    board.set_fen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w Kq - 0 1");

    CastlingRights cr = board.castling_rights();
    ASSERT_TRUE(cr & WhiteKingSide);
    ASSERT_TRUE(!(cr & WhiteQueenSide));
    ASSERT_TRUE(!(cr & BlackKingSide));
    ASSERT_TRUE(cr & BlackQueenSide);
}

TEST(test_fen_roundtrip) {
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w Kq - 5 20",
        "8/8/8/8/8/8/8/8 w - - 0 1",
        "r1bqkb1r/pppppppp/2n2n2/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 3 3",
    };

    Board board;
    for (const char* fen : fens) {
        board.set_fen(fen);
        std::string result = board.to_fen();
        ASSERT_EQ(result, std::string(fen));
    }
}

TEST(test_zobrist_same_position) {
    Board b1, b2;
    b1.set_fen(StartFEN);
    b2.set_fen(StartFEN);

    ASSERT_EQ(b1.hash_key(), b2.hash_key());
}

TEST(test_zobrist_different_positions) {
    Board b1, b2;
    b1.set_fen(StartFEN);
    b2.set_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

    ASSERT_TRUE(b1.hash_key() != b2.hash_key());
}

TEST(test_zobrist_incremental_vs_recompute) {
    Board board;
    board.set_fen(StartFEN);

    // Incremental hash should match full recompute
    ASSERT_EQ(board.hash_key(), board.compute_hash());

    // Modify position: remove a piece and put it elsewhere
    board.remove_piece(E2);
    board.put_piece(WhitePawn, E4);

    // Incremental hash should still match recompute
    ASSERT_EQ(board.hash_key(), board.compute_hash());
}

TEST(test_put_remove_piece) {
    Board board;
    board.set_fen("8/8/8/8/8/8/8/8 w - - 0 1");

    // Put a white knight on D4
    board.put_piece(WhiteKnight, D4);
    ASSERT_EQ(board.piece_on(D4), WhiteKnight);
    ASSERT_TRUE(board.pieces(WhiteKnight) & square_bb(D4));
    ASSERT_TRUE(board.pieces(White) & square_bb(D4));
    ASSERT_TRUE(board.all_pieces() & square_bb(D4));

    // Remove it
    board.remove_piece(D4);
    ASSERT_EQ(board.piece_on(D4), NoPiece);
    ASSERT_TRUE(!(board.pieces(WhiteKnight) & square_bb(D4)));
    ASSERT_TRUE(!(board.pieces(White) & square_bb(D4)));
    ASSERT_TRUE(!(board.all_pieces() & square_bb(D4)));
}

TEST(test_popcount_lsb) {
    Bitboard b = square_bb(A1) | square_bb(C3) | square_bb(H8);
    ASSERT_EQ(popcount(b), 3);
    ASSERT_EQ(lsb(b), A1);

    Square s = pop_lsb(b);
    ASSERT_EQ(s, A1);
    ASSERT_EQ(popcount(b), 2);
}

int main() {
    zobrist::init();

    int prev_failed = 0;

    RUN_TEST(test_start_position);
    RUN_TEST(test_custom_fen);
    RUN_TEST(test_partial_castling);
    RUN_TEST(test_fen_roundtrip);
    RUN_TEST(test_zobrist_same_position);
    RUN_TEST(test_zobrist_different_positions);
    RUN_TEST(test_zobrist_incremental_vs_recompute);
    RUN_TEST(test_put_remove_piece);
    RUN_TEST(test_popcount_lsb);

    std::cout << "\n" << tests_passed << " passed, " << tests_failed << " failed.\n";

    return tests_failed > 0 ? 1 : 0;
}
