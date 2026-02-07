#include "board.h"
#include "zobrist.h"
#include <sstream>
#include <cassert>

namespace panda {

// Map piece chars to Piece enum
static Piece char_to_piece(char c) {
    switch (c) {
        case 'P': return WhitePawn;   case 'N': return WhiteKnight;
        case 'B': return WhiteBishop; case 'R': return WhiteRook;
        case 'Q': return WhiteQueen;  case 'K': return WhiteKing;
        case 'p': return BlackPawn;   case 'n': return BlackKnight;
        case 'b': return BlackBishop; case 'r': return BlackRook;
        case 'q': return BlackQueen;  case 'k': return BlackKing;
        default:  return NoPiece;
    }
}

static char piece_to_char(Piece p) {
    constexpr const char* chars = "PNBRQKpnbrqk";
    if (p < PieceCount) return chars[p];
    return '.';
}

Board::Board() {
    clear();
}

void Board::clear() {
    for (int i = 0; i < 12; ++i) pieceBB[i] = 0;
    for (int i = 0; i < 3; ++i) occupancy[i] = 0;
    for (int i = 0; i < 64; ++i) mailbox[i] = NoPiece;
    sideToMove = White;
    castling = NoCastling;
    epSquare = NoSquare;
    halfmoveClock = 0;
    fullmoveNumber = 1;
    hash = 0;
}

Piece Board::piece_on(Square s) const {
    return mailbox[s];
}

void Board::put_piece(Piece p, Square s) {
    assert(p < PieceCount);
    assert(mailbox[s] == NoPiece);

    Bitboard bb = square_bb(s);
    pieceBB[p] |= bb;
    occupancy[piece_color(p)] |= bb;
    occupancy[2] |= bb;
    mailbox[s] = p;
    hash ^= zobrist::pieceKeys[p][s];
}

void Board::remove_piece(Square s) {
    Piece p = mailbox[s];
    assert(p != NoPiece);

    Bitboard bb = square_bb(s);
    pieceBB[p] ^= bb;
    occupancy[piece_color(p)] ^= bb;
    occupancy[2] ^= bb;
    mailbox[s] = NoPiece;
    hash ^= zobrist::pieceKeys[p][s];
}

void Board::set_fen(const std::string& fen) {
    clear();

    std::istringstream ss(fen);
    std::string token;

    // 1. Piece placement
    ss >> token;
    int rank = 7, file = 0;
    for (char c : token) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
        } else {
            Piece p = char_to_piece(c);
            if (p != NoPiece) {
                put_piece(p, make_square(file, rank));
                file++;
            }
        }
    }

    // 2. Side to move
    ss >> token;
    sideToMove = (token == "b") ? Black : White;

    // 3. Castling rights
    ss >> token;
    castling = NoCastling;
    for (char c : token) {
        switch (c) {
            case 'K': castling |= WhiteKingSide;  break;
            case 'Q': castling |= WhiteQueenSide; break;
            case 'k': castling |= BlackKingSide;   break;
            case 'q': castling |= BlackQueenSide;  break;
            default: break;
        }
    }

    // 4. En passant square
    ss >> token;
    if (token != "-" && token.size() == 2) {
        int f = token[0] - 'a';
        int r = token[1] - '1';
        epSquare = make_square(f, r);
    } else {
        epSquare = NoSquare;
    }

    // 5. Halfmove clock
    if (ss >> token) halfmoveClock = std::stoi(token);
    // 6. Fullmove number
    if (ss >> token) fullmoveNumber = std::stoi(token);

    // Rebuild hash to include castling, EP, and side
    hash ^= zobrist::castlingKeys[castling];
    if (epSquare != NoSquare)
        hash ^= zobrist::enPassantKeys[square_file(epSquare)];
    if (sideToMove == Black)
        hash ^= zobrist::sideKey;
}

std::string Board::to_fen() const {
    std::ostringstream fen;

    // 1. Piece placement
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Piece p = mailbox[make_square(file, rank)];
            if (p == NoPiece) {
                empty++;
            } else {
                if (empty > 0) { fen << empty; empty = 0; }
                fen << piece_to_char(p);
            }
        }
        if (empty > 0) fen << empty;
        if (rank > 0) fen << '/';
    }

    // 2. Side to move
    fen << ' ' << (sideToMove == White ? 'w' : 'b');

    // 3. Castling
    fen << ' ';
    if (castling == NoCastling) {
        fen << '-';
    } else {
        if (castling & WhiteKingSide)  fen << 'K';
        if (castling & WhiteQueenSide) fen << 'Q';
        if (castling & BlackKingSide)  fen << 'k';
        if (castling & BlackQueenSide) fen << 'q';
    }

    // 4. En passant
    fen << ' ';
    if (epSquare == NoSquare) {
        fen << '-';
    } else {
        fen << char('a' + square_file(epSquare)) << char('1' + square_rank(epSquare));
    }

    // 5. Halfmove clock and fullmove number
    fen << ' ' << halfmoveClock << ' ' << fullmoveNumber;

    return fen.str();
}

uint64_t Board::compute_hash() const {
    uint64_t h = 0;
    for (int p = 0; p < 12; ++p) {
        Bitboard bb = pieceBB[p];
        while (bb) {
            Square s = pop_lsb(bb);
            h ^= zobrist::pieceKeys[p][s];
        }
    }
    h ^= zobrist::castlingKeys[castling];
    if (epSquare != NoSquare)
        h ^= zobrist::enPassantKeys[square_file(epSquare)];
    if (sideToMove == Black)
        h ^= zobrist::sideKey;
    return h;
}

std::string Board::print() const {
    std::ostringstream os;
    os << "+---+---+---+---+---+---+---+---+\n";
    for (int rank = 7; rank >= 0; --rank) {
        for (int file = 0; file < 8; ++file) {
            Piece p = mailbox[make_square(file, rank)];
            os << "| " << piece_to_char(p) << ' ';
        }
        os << "| " << (rank + 1) << '\n';
        os << "+---+---+---+---+---+---+---+---+\n";
    }
    os << "  a   b   c   d   e   f   g   h\n\n";
    os << "FEN: " << to_fen() << '\n';
    os << "Hash: 0x" << std::hex << hash << std::dec << '\n';
    return os.str();
}

} // namespace panda
