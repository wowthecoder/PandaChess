#include "movegen.h"

#include "attacks.h"

namespace panda {

static void generate_pawn_moves(const Board& board, MoveList& moves) {
    Color us = board.side_to_move();
    Color them = ~us;
    Bitboard own = board.pieces(us);
    Bitboard enemy = board.pieces(them);
    Bitboard occ = board.all_pieces();
    Bitboard pawns = board.pieces(us, Pawn);

    int pushDir = (us == White) ? 8 : -8;
    Bitboard promoRank = (us == White) ? RankMask[7] : RankMask[0];
    Bitboard startRank = (us == White) ? RankMask[1] : RankMask[6];

    // Single push
    Bitboard singlePush = (us == White) ? (pawns << 8) : (pawns >> 8);
    singlePush &= ~occ;

    // Double push
    Bitboard doublePush =
        (us == White) ? ((singlePush & RankMask[2]) << 8) : ((singlePush & RankMask[5]) >> 8);
    doublePush &= ~occ;

    // Non-promotion single pushes
    Bitboard pushNoPromo = singlePush & ~promoRank;
    while (pushNoPromo) {
        Square to = pop_lsb(pushNoPromo);
        Square from = Square(to - pushDir);
        moves.add(make_move(from, to));
    }

    // Promotion pushes
    Bitboard pushPromo = singlePush & promoRank;
    while (pushPromo) {
        Square to = pop_lsb(pushPromo);
        Square from = Square(to - pushDir);
        moves.add(make_promotion(from, to, Queen));
        moves.add(make_promotion(from, to, Rook));
        moves.add(make_promotion(from, to, Bishop));
        moves.add(make_promotion(from, to, Knight));
    }

    // Double pushes
    while (doublePush) {
        Square to = pop_lsb(doublePush);
        Square from = Square(to - 2 * pushDir);
        moves.add(make_move(from, to));
    }

    // Captures
    Bitboard targets = enemy;
    // Left captures
    Bitboard leftCap, rightCap;
    if (us == White) {
        leftCap = (pawns & ~FileMask[0]) << 7;
        rightCap = (pawns & ~FileMask[7]) << 9;
    } else {
        leftCap = (pawns & ~FileMask[7]) >> 7;
        rightCap = (pawns & ~FileMask[0]) >> 9;
    }

    Bitboard leftCapNoPromo = leftCap & targets & ~promoRank;
    while (leftCapNoPromo) {
        Square to = pop_lsb(leftCapNoPromo);
        Square from = (us == White) ? Square(to - 7) : Square(to + 7);
        moves.add(make_move(from, to));
    }

    Bitboard leftCapPromo = leftCap & targets & promoRank;
    while (leftCapPromo) {
        Square to = pop_lsb(leftCapPromo);
        Square from = (us == White) ? Square(to - 7) : Square(to + 7);
        moves.add(make_promotion(from, to, Queen));
        moves.add(make_promotion(from, to, Rook));
        moves.add(make_promotion(from, to, Bishop));
        moves.add(make_promotion(from, to, Knight));
    }

    Bitboard rightCapNoPromo = rightCap & targets & ~promoRank;
    while (rightCapNoPromo) {
        Square to = pop_lsb(rightCapNoPromo);
        Square from = (us == White) ? Square(to - 9) : Square(to + 9);
        moves.add(make_move(from, to));
    }

    Bitboard rightCapPromo = rightCap & targets & promoRank;
    while (rightCapPromo) {
        Square to = pop_lsb(rightCapPromo);
        Square from = (us == White) ? Square(to - 9) : Square(to + 9);
        moves.add(make_promotion(from, to, Queen));
        moves.add(make_promotion(from, to, Rook));
        moves.add(make_promotion(from, to, Bishop));
        moves.add(make_promotion(from, to, Knight));
    }

    // En passant
    Square ep = board.en_passant_square();
    if (ep != NoSquare) {
        Bitboard epAttackers = attacks::pawn_attacks(them, ep) & pawns;
        while (epAttackers) {
            Square from = pop_lsb(epAttackers);
            moves.add(make_move(from, ep, EnPassant));
        }
    }
}

static void generate_piece_moves(const Board& board, MoveList& moves) {
    Color us = board.side_to_move();
    Bitboard own = board.pieces(us);
    Bitboard occ = board.all_pieces();

    // Knights
    Bitboard knights = board.pieces(us, Knight);
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard targets = attacks::knight_attacks(from) & ~own;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.add(make_move(from, to));
        }
    }

    // Bishops
    Bitboard bishops = board.pieces(us, Bishop);
    while (bishops) {
        Square from = pop_lsb(bishops);
        Bitboard targets = attacks::bishop_attacks(from, occ) & ~own;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.add(make_move(from, to));
        }
    }

    // Rooks
    Bitboard rooks = board.pieces(us, Rook);
    while (rooks) {
        Square from = pop_lsb(rooks);
        Bitboard targets = attacks::rook_attacks(from, occ) & ~own;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.add(make_move(from, to));
        }
    }

    // Queens
    Bitboard queens = board.pieces(us, Queen);
    while (queens) {
        Square from = pop_lsb(queens);
        Bitboard targets = attacks::queen_attacks(from, occ) & ~own;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.add(make_move(from, to));
        }
    }

    // King (non-castling)
    Square kingSq = lsb(board.pieces(us, King));
    Bitboard kingTargets = attacks::king_attacks(kingSq) & ~own;
    while (kingTargets) {
        Square to = pop_lsb(kingTargets);
        moves.add(make_move(kingSq, to));
    }

    // Castling
    Color them = ~us;
    CastlingRights cr = board.castling_rights();
    if (us == White) {
        if ((cr & WhiteKingSide) && !(occ & (square_bb(F1) | square_bb(G1))) &&
            !board.is_square_attacked(E1, them) && !board.is_square_attacked(F1, them) &&
            !board.is_square_attacked(G1, them)) {
            moves.add(make_move(E1, G1, Castling));
        }
        if ((cr & WhiteQueenSide) && !(occ & (square_bb(B1) | square_bb(C1) | square_bb(D1))) &&
            !board.is_square_attacked(E1, them) && !board.is_square_attacked(D1, them) &&
            !board.is_square_attacked(C1, them)) {
            moves.add(make_move(E1, C1, Castling));
        }
    } else {
        if ((cr & BlackKingSide) && !(occ & (square_bb(F8) | square_bb(G8))) &&
            !board.is_square_attacked(E8, them) && !board.is_square_attacked(F8, them) &&
            !board.is_square_attacked(G8, them)) {
            moves.add(make_move(E8, G8, Castling));
        }
        if ((cr & BlackQueenSide) && !(occ & (square_bb(B8) | square_bb(C8) | square_bb(D8))) &&
            !board.is_square_attacked(E8, them) && !board.is_square_attacked(D8, them) &&
            !board.is_square_attacked(C8, them)) {
            moves.add(make_move(E8, C8, Castling));
        }
    }
}

MoveList generate_legal(const Board& board) {
    MoveList pseudo;
    generate_pawn_moves(board, pseudo);
    generate_piece_moves(board, pseudo);

    Color us = board.side_to_move();
    MoveList legal;

    for (int i = 0; i < pseudo.size(); ++i) {
        Board copy = board;
        copy.make_move(pseudo[i]);
        // After making the move, side has flipped, so check if our king is attacked by opponent
        Square kingSq = lsb(copy.pieces(us, King));
        if (!copy.is_square_attacked(kingSq, ~us)) {
            legal.add(pseudo[i]);
        }
    }

    return legal;
}

bool in_check(const Board& board) {
    Color us = board.side_to_move();
    Square kingSq = lsb(board.pieces(us, King));
    return board.is_square_attacked(kingSq, ~us);
}

bool is_checkmate(const Board& board) {
    if (!in_check(board))
        return false;
    return generate_legal(board).size() == 0;
}

bool is_stalemate(const Board& board) {
    if (in_check(board))
        return false;
    return generate_legal(board).size() == 0;
}

bool is_draw_by_fifty_move_rule(const Board& board) {
    return board.halfmove_clock() >= 100;
}

GameTermination game_termination(const Board& board) {
    MoveList legal = generate_legal(board);
    if (legal.size() == 0) {
        return in_check(board) ? GameTermination::Checkmate : GameTermination::Stalemate;
    }
    if (is_draw_by_fifty_move_rule(board)) {
        return GameTermination::FiftyMoveRule;
    }
    return GameTermination::None;
}

uint64_t perft(const Board& board, int depth) {
    if (depth == 0)
        return 1;

    MoveList moves = generate_legal(board);

    if (depth == 1)
        return moves.size();

    uint64_t nodes = 0;
    for (int i = 0; i < moves.size(); ++i) {
        Board copy = board;
        copy.make_move(moves[i]);
        nodes += perft(copy, depth - 1);
    }
    return nodes;
}

}  // namespace panda
