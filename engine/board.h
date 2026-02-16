#pragma once

#include <cstdint>
#include <string>

#include "bitboard.h"
#include "move.h"
#include "types.h"

namespace panda {

constexpr const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

class Board {
   public:
    struct UndoInfo {
        Piece moved = NoPiece;
        Piece captured = NoPiece;
        Square capturedSquare = NoSquare;
        Color sideToMove = White;
        CastlingRights castling = NoCastling;
        Square epSquare = NoSquare;
        int halfmoveClock = 0;
        int fullmoveNumber = 1;
        uint64_t hash = 0;
    };

    Board();

    // FEN interface
    void set_fen(const std::string& fen);
    std::string to_fen() const;

    // Piece manipulation
    Piece piece_on(Square s) const;
    void put_piece(Piece p, Square s);
    void remove_piece(Square s);

    // Recompute Zobrist hash from scratch
    uint64_t compute_hash() const;

    // ASCII display
    std::string print() const;

    // Accessors
    Color side_to_move() const {
        return sideToMove;
    }
    CastlingRights castling_rights() const {
        return castling;
    }
    Square en_passant_square() const {
        return epSquare;
    }
    int halfmove_clock() const {
        return halfmoveClock;
    }
    int fullmove_number() const {
        return fullmoveNumber;
    }
    uint64_t hash_key() const {
        return hash;
    }

    Bitboard pieces(Piece p) const {
        return pieceBB[p];
    }
    Bitboard pieces(Color c) const {
        return occupancy[c];
    }
    Bitboard pieces(Color c, PieceType pt) const {
        return pieceBB[make_piece(c, pt)];
    }
    Bitboard all_pieces() const {
        return occupancy[2];
    }

    // Attack detection
    bool is_square_attacked(Square s, Color attacker) const;

    // Make a move on the board (modifies in place)
    void make_move(Move m);
    void make_move(Move m, UndoInfo& undo);
    void unmake_move(Move m, const UndoInfo& undo);

    // Null move: flip side to move, clear en passant (for null move pruning)
    void make_null_move();
    void make_null_move(UndoInfo& undo);
    void unmake_null_move(const UndoInfo& undo);

   private:
    void clear();

    Bitboard pieceBB[12];   // one per Piece enum value
    Bitboard occupancy[3];  // [White], [Black], [All]
    Piece mailbox[64];

    Color sideToMove;
    CastlingRights castling;
    Square epSquare;
    int halfmoveClock;
    int fullmoveNumber;
    uint64_t hash;
};

}  // namespace panda
