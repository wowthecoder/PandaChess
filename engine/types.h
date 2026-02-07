#pragma once

#include <cstdint>

namespace panda {

enum Color : uint8_t {
    White,
    Black,
    ColorCount = 2
};

constexpr Color operator~(Color c) { return Color(c ^ 1); }

enum PieceType : uint8_t {
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
    PieceTypeCount = 6
};

// 12 piece values: [0..5] = white pieces, [6..11] = black pieces
enum Piece : uint8_t {
    WhitePawn,   WhiteKnight, WhiteBishop, WhiteRook, WhiteQueen, WhiteKing,
    BlackPawn,   BlackKnight, BlackBishop, BlackRook, BlackQueen, BlackKing,
    PieceCount = 12,
    NoPiece = 255
};

enum Square : uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SquareCount = 64,
    NoSquare = 255
};

enum CastlingRights : uint8_t {
    NoCastling       = 0,
    WhiteKingSide    = 1,
    WhiteQueenSide   = 2,
    BlackKingSide    = 4,
    BlackQueenSide   = 8,
    AllCastling      = 15
};

constexpr CastlingRights operator|(CastlingRights a, CastlingRights b) {
    return CastlingRights(uint8_t(a) | uint8_t(b));
}
constexpr CastlingRights operator&(CastlingRights a, CastlingRights b) {
    return CastlingRights(uint8_t(a) & uint8_t(b));
}
constexpr CastlingRights operator~(CastlingRights a) {
    return CastlingRights(~uint8_t(a) & 0xF);
}
constexpr CastlingRights& operator|=(CastlingRights& a, CastlingRights b) {
    return a = a | b;
}
constexpr CastlingRights& operator&=(CastlingRights& a, CastlingRights b) {
    return a = a & b;
}

// Helper functions

constexpr Piece make_piece(Color c, PieceType pt) {
    return Piece(c * 6 + pt);
}

constexpr Color piece_color(Piece p) {
    return Color(p / 6);
}

constexpr PieceType piece_type(Piece p) {
    return PieceType(p % 6);
}

constexpr int square_rank(Square s) { return s / 8; }
constexpr int square_file(Square s) { return s % 8; }

constexpr Square make_square(int file, int rank) {
    return Square(rank * 8 + file);
}

} // namespace panda
