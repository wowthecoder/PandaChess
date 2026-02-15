#include <gtest/gtest.h>

#include <string>

#include "../bitboard.h"
#include "../board.h"
#include "../zobrist.h"

using namespace panda;

class BoardTestEnvironment : public ::testing::Environment {
   public:
    void SetUp() override {
        zobrist::init();
    }
};

TEST(BoardTest, StartPosition) {
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

TEST(BoardTest, CustomFen) {
    Board board;
    // Position after 1. e4
    board.set_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

    ASSERT_EQ(board.side_to_move(), Black);
    ASSERT_EQ(board.en_passant_square(), E3);
    ASSERT_EQ(board.piece_on(E4), WhitePawn);
    ASSERT_EQ(board.piece_on(E2), NoPiece);
    ASSERT_EQ(board.castling_rights(), AllCastling);
}

TEST(BoardTest, PartialCastling) {
    Board board;
    board.set_fen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w Kq - 0 1");

    CastlingRights cr = board.castling_rights();
    ASSERT_TRUE(cr & WhiteKingSide);
    ASSERT_TRUE(!(cr & WhiteQueenSide));
    ASSERT_TRUE(!(cr & BlackKingSide));
    ASSERT_TRUE(cr & BlackQueenSide);
}

TEST(BoardTest, FenRoundtrip) {
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

TEST(BoardTest, ZobristSamePosition) {
    Board b1, b2;
    b1.set_fen(StartFEN);
    b2.set_fen(StartFEN);

    ASSERT_EQ(b1.hash_key(), b2.hash_key());
}

TEST(BoardTest, ZobristDifferentPositions) {
    Board b1, b2;
    b1.set_fen(StartFEN);
    b2.set_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

    ASSERT_TRUE(b1.hash_key() != b2.hash_key());
}

TEST(BoardTest, ZobristIncrementalVsRecompute) {
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

TEST(BoardTest, PutRemovePiece) {
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

TEST(BoardTest, PopcountLsb) {
    Bitboard b = square_bb(A1) | square_bb(C3) | square_bb(H8);
    ASSERT_EQ(popcount(b), 3);
    ASSERT_EQ(lsb(b), A1);

    Square s = pop_lsb(b);
    ASSERT_EQ(s, A1);
    ASSERT_EQ(popcount(b), 2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new BoardTestEnvironment());
    return RUN_ALL_TESTS();
}
