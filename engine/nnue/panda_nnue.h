#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "move.h"
#include "types.h"

namespace panda {
class Board;

namespace nnue {

struct DirtyPiece {
    Piece pc = NoPiece;
    Square from = NoSquare;
    Square to = NoSquare;
    Square remove_sq = NoSquare;
    Square add_sq = NoSquare;
    Piece remove_pc = NoPiece;
    Piece add_pc = NoPiece;
};

struct DirtyThreat {
    static constexpr int PcSqOffset = 0;
    static constexpr int ThreatenedSqOffset = 8;
    static constexpr int ThreatenedPcOffset = 16;
    static constexpr int PcOffset = 20;

    DirtyThreat() = default;
    explicit DirtyThreat(uint32_t rawValue) : data(rawValue) {}
    DirtyThreat(Piece pc, Piece threatenedPc, Square pcSq, Square threatenedSq, bool add) {
        data = (uint32_t(add) << 31) | (uint32_t(pc) << PcOffset) |
               (uint32_t(threatenedPc) << ThreatenedPcOffset) |
               (uint32_t(threatenedSq) << ThreatenedSqOffset) | (uint32_t(pcSq) << PcSqOffset);
    }

    Piece pc() const {
        return Piece((data >> PcOffset) & 0xFFu);
    }
    Piece threatened_pc() const {
        return Piece((data >> ThreatenedPcOffset) & 0xFFu);
    }
    Square threatened_sq() const {
        return Square((data >> ThreatenedSqOffset) & 0xFFu);
    }
    Square pc_sq() const {
        return Square((data >> PcSqOffset) & 0xFFu);
    }
    bool add() const {
        return (data >> 31) != 0;
    }
    uint32_t raw() const {
        return data;
    }

   private:
    uint32_t data = 0;
};

struct DirtyThreatList {
    static constexpr std::size_t MaxSize = 96;

    std::size_t size() const {
        return count;
    }
    int ssize() const {
        return int(count);
    }
    void push_back(const DirtyThreat& value) {
        if (count < MaxSize)
            values[count++] = value;
    }
    const DirtyThreat* begin() const {
        return values;
    }
    const DirtyThreat* end() const {
        return values + count;
    }
    const DirtyThreat& operator[](int index) const {
        return values[index];
    }
    void clear() {
        count = 0;
    }

   private:
    DirtyThreat values[MaxSize]{};
    std::size_t count = 0;
};

struct DirtyThreats {
    DirtyThreatList list;
    Color us = White;
    Square prevKsq = NoSquare;
    Square ksq = NoSquare;
    uint64_t threatenedSqs = 0;
    uint64_t threateningSqs = 0;

    void clear() {
        list.clear();
        us = White;
        prevKsq = NoSquare;
        ksq = NoSquare;
        threatenedSqs = 0;
        threateningSqs = 0;
    }
};

class SearchNnueContext {
   public:
    SearchNnueContext();
    ~SearchNnueContext();

    SearchNnueContext(const SearchNnueContext&) = delete;
    SearchNnueContext& operator=(const SearchNnueContext&) = delete;

    SearchNnueContext(SearchNnueContext&&) noexcept;
    SearchNnueContext& operator=(SearchNnueContext&&) noexcept;

    void reset(const Board& board);
    void on_make_move(const Board& board, Move m, const DirtyPiece& dirtyPiece,
                      const DirtyThreats& dirtyThreats);
    void on_unmake_move(const Board& board);
    void on_null_move(const Board& board);
    void on_unmake_null_move(const Board& board);

    int evaluate(const Board& board);
    bool is_available() const;
    bool is_loaded() const;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

bool backend_loaded();

}  // namespace nnue
}  // namespace panda
