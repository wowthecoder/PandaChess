#pragma once

#include "types.h"
#include <cstdint>
#include <string>

namespace panda {

// 16-bit move encoding:
// Bits 0-5:   from square
// Bits 6-11:  to square
// Bits 12-13: move type (0=Normal, 1=Promotion, 2=EnPassant, 3=Castling)
// Bits 14-15: promotion piece (0=Knight, 1=Bishop, 2=Rook, 3=Queen)

using Move = uint16_t;
constexpr Move NullMove = 0;

enum MoveType : uint8_t {
    Normal    = 0,
    Promotion = 1,
    EnPassant = 2,
    Castling  = 3
};

constexpr Move make_move(Square from, Square to) {
    return Move(from | (to << 6));
}

constexpr Move make_move(Square from, Square to, MoveType mt) {
    return Move(from | (to << 6) | (mt << 12));
}

constexpr Move make_promotion(Square from, Square to, PieceType pt) {
    // pt: Knight=1, Bishop=2, Rook=3, Queen=4 in PieceType enum
    // promotion bits: 0=Knight, 1=Bishop, 2=Rook, 3=Queen
    return Move(from | (to << 6) | (Promotion << 12) | ((pt - Knight) << 14));
}

constexpr Square move_from(Move m) {
    return Square(m & 0x3F);
}

constexpr Square move_to(Move m) {
    return Square((m >> 6) & 0x3F);
}

constexpr MoveType move_type(Move m) {
    return MoveType((m >> 12) & 0x3);
}

constexpr PieceType promotion_type(Move m) {
    return PieceType(((m >> 14) & 0x3) + Knight);
}

inline std::string square_to_str(Square s) {
    std::string str;
    str += char('a' + square_file(s));
    str += char('1' + square_rank(s));
    return str;
}

inline std::string move_to_uci(Move m) {
    std::string str = square_to_str(move_from(m)) + square_to_str(move_to(m));
    if (move_type(m) == Promotion) {
        constexpr char promo[] = "nbrq";
        str += promo[promotion_type(m) - Knight];
    }
    return str;
}

struct MoveList {
    Move moves[256];
    int count = 0;

    void add(Move m) { moves[count++] = m; }
    int size() const { return count; }
    Move operator[](int i) const { return moves[i]; }
    Move* begin() { return moves; }
    Move* end() { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end() const { return moves + count; }
};

} // namespace panda
