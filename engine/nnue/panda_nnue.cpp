#include "nnue/panda_nnue.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "board.h"
#include "stockfish_src/bitboard.h"
#include "stockfish_src/nnue/network.h"
#include "stockfish_src/nnue/nnue_accumulator.h"
#include "stockfish_src/position.h"
#include "stockfish_src/types.h"

namespace panda::nnue {

namespace {

namespace sf = Stockfish;

constexpr const char* kBigNetName = "sfnn_v10_big.nnue";
constexpr const char* kSmallNetName = "sfnn_v10_small.nnue";

std::string resolve_net_path(const char* netName) {
    std::vector<std::string> candidates;
#ifdef PANDA_ENGINE_SOURCE_DIR
    candidates.push_back(std::string(PANDA_ENGINE_SOURCE_DIR) + "/nnue/" + netName);
#endif
    candidates.push_back(std::string("nnue/") + netName);
    candidates.push_back(std::string("engine/nnue/") + netName);
    candidates.push_back(std::string("../nnue/") + netName);
    candidates.push_back(std::string("../../nnue/") + netName);
    candidates.push_back(std::string("../../engine/nnue/") + netName);
    candidates.push_back(std::string("../engine/nnue/") + netName);
    candidates.push_back(std::string(netName));

    for (const std::string& path : candidates) {
        if (std::filesystem::exists(path))
            return path;
    }
    return {};
}

sf::Eval::NNUE::EvalFile make_eval_file(const std::string& defaultName) {
    sf::Eval::NNUE::EvalFile file{};
    file.defaultName = sf::FixedString<256>(defaultName);
    return file;
}

sf::PieceType to_sf_piece_type(PieceType pt) {
    switch (pt) {
        case Pawn:
            return sf::PAWN;
        case Knight:
            return sf::KNIGHT;
        case Bishop:
            return sf::BISHOP;
        case Rook:
            return sf::ROOK;
        case Queen:
            return sf::QUEEN;
        case King:
            return sf::KING;
        default:
            return sf::NO_PIECE_TYPE;
    }
}

sf::Color to_sf_color(Color c) {
    return c == White ? sf::WHITE : sf::BLACK;
}

sf::Piece to_sf_piece(Piece p) {
    if (p == NoPiece)
        return sf::NO_PIECE;
    return sf::make_piece(to_sf_color(piece_color(p)), to_sf_piece_type(piece_type(p)));
}

sf::Square to_sf_square(Square s) {
    if (s == NoSquare)
        return sf::SQ_NONE;
    return sf::Square(static_cast<uint8_t>(s));
}

sf::DirtyPiece to_sf_dirty_piece(const DirtyPiece& dirty) {
    sf::DirtyPiece out{};
    out.pc = to_sf_piece(dirty.pc);
    out.from = to_sf_square(dirty.from);
    out.to = to_sf_square(dirty.to);
    out.remove_sq = to_sf_square(dirty.remove_sq);
    out.add_sq = to_sf_square(dirty.add_sq);
    out.remove_pc = to_sf_piece(dirty.remove_pc);
    out.add_pc = to_sf_piece(dirty.add_pc);
    return out;
}

bool has_nonempty_threat_delta(const DirtyThreats& dirtyThreats) {
    return dirtyThreats.list.size() != 0 || dirtyThreats.threatenedSqs != 0 ||
           dirtyThreats.threateningSqs != 0;
}

sf::DirtyThreats to_sf_dirty_threats(const DirtyThreats& dirtyThreats) {
    sf::DirtyThreats out{};
    out.us = to_sf_color(dirtyThreats.us);
    out.prevKsq = to_sf_square(dirtyThreats.prevKsq);
    out.ksq = to_sf_square(dirtyThreats.ksq);
    out.threatenedSqs = sf::Bitboard(dirtyThreats.threatenedSqs);
    out.threateningSqs = sf::Bitboard(dirtyThreats.threateningSqs);

    for (const DirtyThreat& dirty : dirtyThreats.list) {
        out.list.push_back(sf::DirtyThreat(to_sf_piece(dirty.pc()), to_sf_piece(dirty.threatened_pc()),
                                           to_sf_square(dirty.pc_sq()),
                                           to_sf_square(dirty.threatened_sq()), dirty.add()));
    }
    return out;
}

sf::Move to_sf_move(const Board& board, Move m) {
    const Square from = move_from(m);
    const Square to = move_to(m);
    const MoveType mt = move_type(m);

    const sf::Square sfFrom = to_sf_square(from);
    const sf::Square sfTo = to_sf_square(to);

    if (mt == Castling) {
        const bool kingSide = to > from;
        const Square rookFrom = make_square(kingSide ? 7 : 0, square_rank(from));
        return sf::Move::make<sf::CASTLING>(sfFrom, to_sf_square(rookFrom));
    }

    if (mt == EnPassant)
        return sf::Move::make<sf::EN_PASSANT>(sfFrom, sfTo);

    if (mt == Promotion) {
        const sf::PieceType promo = to_sf_piece_type(promotion_type(m));
        return sf::Move::make<sf::PROMOTION>(sfFrom, sfTo, promo);
    }

    return sf::Move(sfFrom, sfTo);
}

struct Backend {
    std::once_flag initFlag;
    bool loaded = false;
    bool initTried = false;
    std::string bigPath;
    std::string smallPath;
    std::unique_ptr<sf::Eval::NNUE::Networks> networks;
};

Backend& backend() {
    static Backend b;
    return b;
}

void init_backend_once() {
    Backend& b = backend();
    b.initTried = true;

    b.bigPath = resolve_net_path(kBigNetName);
    b.smallPath = resolve_net_path(kSmallNetName);

    if (b.bigPath.empty() || b.smallPath.empty()) {
        std::cerr << "NNUE: missing SF18 nets (" << kBigNetName << ", " << kSmallNetName
                  << "), falling back to handcrafted eval" << std::endl;
        b.loaded = false;
        return;
    }

    sf::Bitboards::init();
    sf::Position::init();

    auto bigFile = make_eval_file(b.bigPath);
    auto smallFile = make_eval_file(b.smallPath);
    b.networks = std::make_unique<sf::Eval::NNUE::Networks>(bigFile, smallFile);

    const bool bigLoaded = b.networks->big.load("", b.bigPath);
    const bool smallLoaded = b.networks->small.load("", b.smallPath);
    b.loaded = bigLoaded && smallLoaded;

    if (!b.loaded) {
        std::cerr << "NNUE: failed to load SF18 nets from " << b.bigPath << " / " << b.smallPath
                  << ", falling back to handcrafted eval" << std::endl;
        b.networks.reset();
    }
}

bool ensure_backend() {
    Backend& b = backend();
    std::call_once(b.initFlag, init_backend_once);
    return b.loaded;
}

class SfContext {
   public:
    SfContext() = default;

    bool available() const {
        return ensure_backend();
    }

    void reset(const Board& board) {
        if (!ensure_backend())
            return;

        if (!caches)
            caches = std::make_unique<sf::Eval::NNUE::AccumulatorCaches>(*backend().networks);

        const std::string fen = board.to_fen();
        states.resize(MaxPly + 8);
        pos.set(fen, false, &states[0]);
        accumulators.reset();
        moves.clear();
        synced = true;
        syncedHash = board.hash_key();
    }

    void on_make_move(const Board& board, Move m, const DirtyPiece& dirtyPiece,
                      const DirtyThreats& dirtyThreats) {
        if (!ensure_backend())
            return;

        if (!synced) {
            reset(board);
            return;
        }

        if (moves.size() + 1 >= states.size()) {
            synced = false;
            reset(board);
            return;
        }

        const sf::Move sfMove = to_sf_move(board, m);
        auto [sfDirtyPiece, sfDirtyThreats] = accumulators.push();
        const bool givesCheck = pos.gives_check(sfMove);
        pos.do_move(sfMove, states[moves.size() + 1], givesCheck, sfDirtyPiece, sfDirtyThreats,
                    nullptr, nullptr);

        sfDirtyPiece = to_sf_dirty_piece(dirtyPiece);
        if (has_nonempty_threat_delta(dirtyThreats))
            sfDirtyThreats = to_sf_dirty_threats(dirtyThreats);

        moves.push_back(sfMove);
        syncedHash = board.hash_key();
    }

    void on_unmake_move(const Board& board) {
        if (!ensure_backend())
            return;

        if (!synced || moves.empty()) {
            reset(board);
            return;
        }

        const sf::Move sfMove = moves.back();
        moves.pop_back();
        pos.undo_move(sfMove);
        accumulators.pop();
        syncedHash = board.hash_key();
    }

    void on_null_move(const Board&) {
        synced = false;
    }

    void on_unmake_null_move(const Board&) {
        synced = false;
    }

    int evaluate(const Board& board) {
        if (!ensure_backend())
            return 0;

        if (!synced || syncedHash != board.hash_key())
            reset(board);

        if (!synced)
            return 0;

        const sf::Color stm = pos.side_to_move();
        const int simpleEval = sf::PawnValue * (pos.count<sf::PAWN>(stm) - pos.count<sf::PAWN>(~stm))
                             + pos.non_pawn_material(stm) - pos.non_pawn_material(~stm);

        bool useSmall = std::abs(simpleEval) > 962;

        sf::Value psqt = 0;
        sf::Value positional = 0;
        if (useSmall) {
            std::tie(psqt, positional) = backend().networks->small.evaluate(pos, accumulators,
                                                                             caches->small);
        } else {
            std::tie(psqt, positional) =
                backend().networks->big.evaluate(pos, accumulators, caches->big);
        }

        int nnueScore = (125 * int(psqt) + 131 * int(positional)) / 128;
        if (useSmall && std::abs(nnueScore) < 277) {
            std::tie(psqt, positional) =
                backend().networks->big.evaluate(pos, accumulators, caches->big);
            nnueScore = (125 * int(psqt) + 131 * int(positional)) / 128;
        }

        return nnueScore;
    }

   private:
    static constexpr std::size_t MaxPly = 256;

    sf::Position pos;
    std::vector<sf::StateInfo> states;
    std::vector<sf::Move> moves;
    sf::Eval::NNUE::AccumulatorStack accumulators;
    std::unique_ptr<sf::Eval::NNUE::AccumulatorCaches> caches;
    bool synced = false;
    uint64_t syncedHash = 0;
};

}  // namespace

struct SearchNnueContext::Impl {
    SfContext ctx;
};

SearchNnueContext::SearchNnueContext() : impl(std::make_unique<Impl>()) {}

SearchNnueContext::~SearchNnueContext() = default;

SearchNnueContext::SearchNnueContext(SearchNnueContext&&) noexcept = default;

SearchNnueContext& SearchNnueContext::operator=(SearchNnueContext&&) noexcept = default;

void SearchNnueContext::reset(const Board& board) {
    impl->ctx.reset(board);
}

void SearchNnueContext::on_make_move(const Board& board, Move m, const DirtyPiece& dirtyPiece,
                                     const DirtyThreats& dirtyThreats) {
    impl->ctx.on_make_move(board, m, dirtyPiece, dirtyThreats);
}

void SearchNnueContext::on_unmake_move(const Board& board) {
    impl->ctx.on_unmake_move(board);
}

void SearchNnueContext::on_null_move(const Board& board) {
    impl->ctx.on_null_move(board);
}

void SearchNnueContext::on_unmake_null_move(const Board& board) {
    impl->ctx.on_unmake_null_move(board);
}

int SearchNnueContext::evaluate(const Board& board) {
    return impl->ctx.evaluate(board);
}

bool SearchNnueContext::is_available() const {
    return impl->ctx.available();
}

bool SearchNnueContext::is_loaded() const {
    return backend_loaded();
}

bool backend_loaded() {
    return ensure_backend();
}

}  // namespace panda::nnue
