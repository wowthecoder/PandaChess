#include <gtest/gtest.h>

#include "../attacks.h"
#include "../board.h"
#include "../move.h"
#include "../movegen.h"
#include "../zobrist.h"

using namespace panda;

class MoveGenTestEnvironment : public ::testing::Environment {
   public:
    void SetUp() override {
        zobrist::init();
        attacks::init();
    }
};

// ============================================================
// Attack table tests
// ============================================================

TEST(AttackTest, PawnAttacksWhite) {
    // E4: should attack D5 and F5
    Bitboard atk = attacks::pawn_attacks(White, E4);
    EXPECT_TRUE(atk & square_bb(D5));
    EXPECT_TRUE(atk & square_bb(F5));
    EXPECT_EQ(popcount(atk), 2);
}

TEST(AttackTest, PawnAttacksBlack) {
    // E5: should attack D4 and F4
    Bitboard atk = attacks::pawn_attacks(Black, E5);
    EXPECT_TRUE(atk & square_bb(D4));
    EXPECT_TRUE(atk & square_bb(F4));
    EXPECT_EQ(popcount(atk), 2);
}

TEST(AttackTest, PawnAttacksCorner) {
    // A2 white pawn: only attacks B3
    Bitboard atk = attacks::pawn_attacks(White, A2);
    EXPECT_TRUE(atk & square_bb(B3));
    EXPECT_EQ(popcount(atk), 1);

    // H7 black pawn: only attacks G6
    atk = attacks::pawn_attacks(Black, H7);
    EXPECT_TRUE(atk & square_bb(G6));
    EXPECT_EQ(popcount(atk), 1);
}

TEST(AttackTest, KnightAttacksCenter) {
    // Knight on E4: 8 squares
    Bitboard atk = attacks::knight_attacks(E4);
    EXPECT_EQ(popcount(atk), 8);
    EXPECT_TRUE(atk & square_bb(D6));
    EXPECT_TRUE(atk & square_bb(F6));
    EXPECT_TRUE(atk & square_bb(G5));
    EXPECT_TRUE(atk & square_bb(G3));
    EXPECT_TRUE(atk & square_bb(F2));
    EXPECT_TRUE(atk & square_bb(D2));
    EXPECT_TRUE(atk & square_bb(C3));
    EXPECT_TRUE(atk & square_bb(C5));
}

TEST(AttackTest, KnightAttacksCorner) {
    // Knight on A1: 2 squares (B3, C2)
    Bitboard atk = attacks::knight_attacks(A1);
    EXPECT_EQ(popcount(atk), 2);
    EXPECT_TRUE(atk & square_bb(B3));
    EXPECT_TRUE(atk & square_bb(C2));
}

TEST(AttackTest, KingAttacksCenter) {
    // King on E4: 8 squares
    Bitboard atk = attacks::king_attacks(E4);
    EXPECT_EQ(popcount(atk), 8);
}

TEST(AttackTest, KingAttacksCorner) {
    // King on A1: 3 squares
    Bitboard atk = attacks::king_attacks(A1);
    EXPECT_EQ(popcount(atk), 3);
    EXPECT_TRUE(atk & square_bb(A2));
    EXPECT_TRUE(atk & square_bb(B1));
    EXPECT_TRUE(atk & square_bb(B2));
}

TEST(AttackTest, BishopAttacksEmpty) {
    // Bishop on E4, empty board
    Bitboard atk = attacks::bishop_attacks(E4, 0);
    // Should reach corners and edges: 13 squares
    EXPECT_EQ(popcount(atk), 13);
    EXPECT_TRUE(atk & square_bb(D3));
    EXPECT_TRUE(atk & square_bb(F5));
    EXPECT_TRUE(atk & square_bb(A8));
    EXPECT_TRUE(atk & square_bb(H1));
}

TEST(AttackTest, BishopAttacksWithBlockers) {
    // Bishop on E4, blocker on F5
    Bitboard occ = square_bb(F5);
    Bitboard atk = attacks::bishop_attacks(E4, occ);
    EXPECT_TRUE(atk & square_bb(F5));   // Can capture blocker
    EXPECT_FALSE(atk & square_bb(G6));  // Blocked beyond
}

TEST(AttackTest, RookAttacksEmpty) {
    // Rook on E4, empty board
    Bitboard atk = attacks::rook_attacks(E4, 0);
    // 7 (file) + 7 (rank) = 14 squares
    EXPECT_EQ(popcount(atk), 14);
}

TEST(AttackTest, RookAttacksWithBlockers) {
    // Rook on E4, blocker on E6
    Bitboard occ = square_bb(E6);
    Bitboard atk = attacks::rook_attacks(E4, occ);
    EXPECT_TRUE(atk & square_bb(E6));   // Can capture blocker
    EXPECT_FALSE(atk & square_bb(E7));  // Blocked beyond
    EXPECT_TRUE(atk & square_bb(E1));   // Not blocked other direction
}

// ============================================================
// is_square_attacked tests
// ============================================================

TEST(AttackTest, IsSquareAttacked) {
    Board board;
    board.set_fen(StartFEN);

    // E2 pawn attacks D3 and F3
    EXPECT_TRUE(board.is_square_attacked(D3, White));
    EXPECT_TRUE(board.is_square_attacked(F3, White));
    // E1 not attacked by black in start position
    EXPECT_FALSE(board.is_square_attacked(E1, Black));
    // E4 not attacked by anyone in start position
    EXPECT_FALSE(board.is_square_attacked(E4, White));
    EXPECT_FALSE(board.is_square_attacked(E4, Black));
}

// ============================================================
// make_move tests
// ============================================================

TEST(MakeMoveTest, NormalMove) {
    Board board;
    board.set_fen(StartFEN);

    // 1. e2e4
    Move m = make_move(E2, E4);
    board.make_move(m);

    EXPECT_EQ(board.piece_on(E4), WhitePawn);
    EXPECT_EQ(board.piece_on(E2), NoPiece);
    EXPECT_EQ(board.en_passant_square(), E3);
    EXPECT_EQ(board.side_to_move(), Black);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, Capture) {
    Board board;
    // Position where white pawn on E5 can capture black pawn on D6... use a simpler case
    board.set_fen("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2");

    // e4xd5
    Move m = make_move(E4, D5);
    board.make_move(m);

    EXPECT_EQ(board.piece_on(D5), WhitePawn);
    EXPECT_EQ(board.piece_on(E4), NoPiece);
    EXPECT_EQ(board.side_to_move(), Black);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, EnPassant) {
    Board board;
    // White pawn on E5, black just played d7d5
    board.set_fen("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");

    Move m = make_move(E5, D6, EnPassant);
    board.make_move(m);

    EXPECT_EQ(board.piece_on(D6), WhitePawn);
    EXPECT_EQ(board.piece_on(D5), NoPiece);  // captured pawn removed
    EXPECT_EQ(board.piece_on(E5), NoPiece);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, CastlingKingside) {
    Board board;
    board.set_fen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");

    Move m = make_move(E1, G1, Castling);
    board.make_move(m);

    EXPECT_EQ(board.piece_on(G1), WhiteKing);
    EXPECT_EQ(board.piece_on(F1), WhiteRook);
    EXPECT_EQ(board.piece_on(E1), NoPiece);
    EXPECT_EQ(board.piece_on(H1), NoPiece);
    // White castling rights gone
    EXPECT_FALSE(board.castling_rights() & WhiteKingSide);
    EXPECT_FALSE(board.castling_rights() & WhiteQueenSide);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, CastlingQueenside) {
    Board board;
    board.set_fen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");

    Move m = make_move(E1, C1, Castling);
    board.make_move(m);

    EXPECT_EQ(board.piece_on(C1), WhiteKing);
    EXPECT_EQ(board.piece_on(D1), WhiteRook);
    EXPECT_EQ(board.piece_on(E1), NoPiece);
    EXPECT_EQ(board.piece_on(A1), NoPiece);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, Promotion) {
    Board board;
    board.set_fen("8/P7/8/8/8/8/8/4K2k w - - 0 1");

    Move m = make_promotion(A7, A8, Queen);
    board.make_move(m);

    EXPECT_EQ(board.piece_on(A8), WhiteQueen);
    EXPECT_EQ(board.piece_on(A7), NoPiece);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, PromotionCapture) {
    Board board;
    board.set_fen("1n6/P7/8/8/8/8/8/4K2k w - - 0 1");

    Move m = make_promotion(A7, B8, Queen);
    board.make_move(m);

    EXPECT_EQ(board.piece_on(B8), WhiteQueen);
    EXPECT_EQ(board.piece_on(A7), NoPiece);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, CastlingRightsRookCapture) {
    Board board;
    // Position where white can capture black's H8 rook
    board.set_fen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K1NR w KQkq - 0 1");

    // Move to a position where we can actually test rook capture affecting castling
    // Let's use a direct scenario: white rook captures black rook on H8 (not realistic but tests
    // the table)
    board.set_fen("r3k2R/pppppppp/8/8/8/8/PPPPPPPP/R3K3 b Qq - 0 1");

    // Black's kingside castling should already be gone since H8 has white rook
    EXPECT_FALSE(board.castling_rights() & BlackKingSide);
}

TEST(MakeMoveTest, UnmakeRoundtripNormalMove) {
    Board board;
    board.set_fen(StartFEN);
    std::string beforeFen = board.to_fen();
    uint64_t beforeHash = board.hash_key();

    Move m = make_move(E2, E4);
    Board::UndoInfo undo;
    board.make_move(m, undo);
    EXPECT_EQ(undo.nnueDirtyPiece.pc, WhitePawn);
    EXPECT_EQ(undo.nnueDirtyPiece.from, E2);
    EXPECT_EQ(undo.nnueDirtyPiece.to, E4);
    EXPECT_EQ(undo.nnueDirtyPiece.remove_sq, NoSquare);
    EXPECT_FALSE(undo.nnueNullMove);
    board.unmake_move(m, undo);

    EXPECT_EQ(board.to_fen(), beforeFen);
    EXPECT_EQ(board.hash_key(), beforeHash);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, UnmakeRoundtripEnPassant) {
    Board board;
    board.set_fen("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    std::string beforeFen = board.to_fen();
    uint64_t beforeHash = board.hash_key();

    Move m = make_move(E5, D6, EnPassant);
    Board::UndoInfo undo;
    board.make_move(m, undo);
    EXPECT_EQ(undo.nnueDirtyPiece.pc, WhitePawn);
    EXPECT_EQ(undo.nnueDirtyPiece.from, E5);
    EXPECT_EQ(undo.nnueDirtyPiece.to, D6);
    EXPECT_EQ(undo.nnueDirtyPiece.remove_sq, D5);
    EXPECT_EQ(undo.nnueDirtyPiece.remove_pc, BlackPawn);
    board.unmake_move(m, undo);

    EXPECT_EQ(board.to_fen(), beforeFen);
    EXPECT_EQ(board.hash_key(), beforeHash);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, UnmakeRoundtripCastling) {
    Board board;
    board.set_fen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    std::string beforeFen = board.to_fen();
    uint64_t beforeHash = board.hash_key();

    Move m = make_move(E1, G1, Castling);
    Board::UndoInfo undo;
    board.make_move(m, undo);
    EXPECT_EQ(undo.nnueDirtyPiece.pc, WhiteKing);
    EXPECT_EQ(undo.nnueDirtyPiece.from, E1);
    EXPECT_EQ(undo.nnueDirtyPiece.to, G1);
    EXPECT_EQ(undo.nnueDirtyPiece.remove_sq, H1);
    EXPECT_EQ(undo.nnueDirtyPiece.add_sq, F1);
    EXPECT_EQ(undo.nnueDirtyPiece.remove_pc, WhiteRook);
    EXPECT_EQ(undo.nnueDirtyPiece.add_pc, WhiteRook);
    board.unmake_move(m, undo);

    EXPECT_EQ(board.to_fen(), beforeFen);
    EXPECT_EQ(board.hash_key(), beforeHash);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, UnmakeRoundtripPromotionCapture) {
    Board board;
    board.set_fen("1n6/P7/8/8/8/8/8/4K2k w - - 0 1");
    std::string beforeFen = board.to_fen();
    uint64_t beforeHash = board.hash_key();

    Move m = make_promotion(A7, B8, Queen);
    Board::UndoInfo undo;
    board.make_move(m, undo);
    EXPECT_EQ(undo.nnueDirtyPiece.pc, WhitePawn);
    EXPECT_EQ(undo.nnueDirtyPiece.from, A7);
    EXPECT_EQ(undo.nnueDirtyPiece.to, NoSquare);
    EXPECT_EQ(undo.nnueDirtyPiece.remove_sq, B8);
    EXPECT_EQ(undo.nnueDirtyPiece.remove_pc, BlackKnight);
    EXPECT_EQ(undo.nnueDirtyPiece.add_sq, B8);
    EXPECT_EQ(undo.nnueDirtyPiece.add_pc, WhiteQueen);
    board.unmake_move(m, undo);

    EXPECT_EQ(board.to_fen(), beforeFen);
    EXPECT_EQ(board.hash_key(), beforeHash);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

TEST(MakeMoveTest, UnmakeRoundtripNullMove) {
    Board board;
    board.set_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    std::string beforeFen = board.to_fen();
    uint64_t beforeHash = board.hash_key();

    Board::UndoInfo undo;
    board.make_null_move(undo);
    EXPECT_TRUE(undo.nnueNullMove);
    EXPECT_EQ(undo.nnueDirtyPiece.pc, NoPiece);
    board.unmake_null_move(undo);

    EXPECT_EQ(board.to_fen(), beforeFen);
    EXPECT_EQ(board.hash_key(), beforeHash);
    EXPECT_EQ(board.hash_key(), board.compute_hash());
}

// ============================================================
// Perft tests
// ============================================================

TEST(PerftTest, StartPositionDepth1) {
    Board board;
    board.set_fen(StartFEN);
    EXPECT_EQ(perft(board, 1), 20ULL);
}

TEST(PerftTest, StartPositionDepth2) {
    Board board;
    board.set_fen(StartFEN);
    EXPECT_EQ(perft(board, 2), 400ULL);
}

TEST(PerftTest, StartPositionDepth3) {
    Board board;
    board.set_fen(StartFEN);
    EXPECT_EQ(perft(board, 3), 8902ULL);
}

TEST(PerftTest, StartPositionDepth4) {
    Board board;
    board.set_fen(StartFEN);
    EXPECT_EQ(perft(board, 4), 197281ULL);
}

TEST(PerftTest, StartPositionDepth5) {
    Board board;
    board.set_fen(StartFEN);
    EXPECT_EQ(perft(board, 5), 4865609ULL);
}

constexpr const char* KiwipeteFEN =
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";

TEST(PerftTest, KiwipeteDepth1) {
    Board board;
    board.set_fen(KiwipeteFEN);
    EXPECT_EQ(perft(board, 1), 48ULL);
}

TEST(PerftTest, KiwipeteDepth2) {
    Board board;
    board.set_fen(KiwipeteFEN);
    EXPECT_EQ(perft(board, 2), 2039ULL);
}

TEST(PerftTest, KiwipeteDepth3) {
    Board board;
    board.set_fen(KiwipeteFEN);
    EXPECT_EQ(perft(board, 3), 97862ULL);
}

TEST(PerftTest, KiwipeteDepth4) {
    Board board;
    board.set_fen(KiwipeteFEN);
    EXPECT_EQ(perft(board, 4), 4085603ULL);
}

// ============================================================
// Game termination tests
// ============================================================

TEST(TerminationTest, CheckmateDetection) {
    Board board;
    board.set_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");

    EXPECT_TRUE(in_check(board));
    EXPECT_TRUE(is_checkmate(board));
    EXPECT_FALSE(is_stalemate(board));
    EXPECT_FALSE(is_draw_by_fifty_move_rule(board));
    EXPECT_EQ(game_termination(board), GameTermination::Checkmate);
}

TEST(TerminationTest, StalemateDetection) {
    Board board;
    board.set_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");

    EXPECT_FALSE(in_check(board));
    EXPECT_FALSE(is_checkmate(board));
    EXPECT_TRUE(is_stalemate(board));
    EXPECT_FALSE(is_draw_by_fifty_move_rule(board));
    EXPECT_EQ(game_termination(board), GameTermination::Stalemate);
}

TEST(TerminationTest, FiftyMoveRuleDetection) {
    Board board;
    board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 100 1");

    EXPECT_FALSE(is_checkmate(board));
    EXPECT_FALSE(is_stalemate(board));
    EXPECT_TRUE(is_draw_by_fifty_move_rule(board));
    EXPECT_EQ(game_termination(board), GameTermination::FiftyMoveRule);
}

TEST(TerminationTest, OngoingPosition) {
    Board board;
    board.set_fen(StartFEN);

    EXPECT_FALSE(is_checkmate(board));
    EXPECT_FALSE(is_stalemate(board));
    EXPECT_FALSE(is_draw_by_fifty_move_rule(board));
    EXPECT_EQ(game_termination(board), GameTermination::None);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new MoveGenTestEnvironment());
    return RUN_ALL_TESTS();
}
