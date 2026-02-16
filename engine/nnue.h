#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "types.h"

namespace panda {

class Board;

namespace nnue {

// ─── Architecture constants ─────────────────────────────────────────────────
constexpr int HalfDimensions   = 3072;
constexpr int PSQTBuckets      = 8;
constexpr int LayerStacks      = 8;
constexpr int SquareNB         = 64;
constexpr int PS_NB            = 704;   // 11 * 64
constexpr int InputDimensions  = 32 * PS_NB;  // 22528
constexpr int MaxActiveDims    = 32;
constexpr int OutputScale      = 16;

// Dense layer sizes
constexpr int FC0_OUT = 16;
constexpr int FC1_IN  = 30;   // SqrClippedReLU(15) + ClippedReLU(15)
constexpr int FC1_PAD = 32;   // ceil(30, 32)
constexpr int FC1_OUT = 32;
constexpr int FC2_OUT = 1;

// ─── Feature index tables (HalfKAv2_hm) ────────────────────────────────────

// Horizontal mirroring: XOR square with this value based on king file
// file < 4 → XOR with 7 (mirror), file >= 4 → XOR with 0 (no mirror)
constexpr int OrientTBL[64] = {
    7,7,7,7, 0,0,0,0,  7,7,7,7, 0,0,0,0,
    7,7,7,7, 0,0,0,0,  7,7,7,7, 0,0,0,0,
    7,7,7,7, 0,0,0,0,  7,7,7,7, 0,0,0,0,
    7,7,7,7, 0,0,0,0,  7,7,7,7, 0,0,0,0,
};

// King bucket offsets (already multiplied by PS_NB = 704)
constexpr int KingBuckets[64] = {
    19712, 20416, 21120, 21824, 21824, 21120, 20416, 19712,
    16896, 17600, 18304, 19008, 19008, 18304, 17600, 16896,
    14080, 14784, 15488, 16192, 16192, 15488, 14784, 14080,
    11264, 11968, 12672, 13376, 13376, 12672, 11968, 11264,
     8448,  9152,  9856, 10560, 10560,  9856,  9152,  8448,
     5632,  6336,  7040,  7744,  7744,  7040,  6336,  5632,
     2816,  3520,  4224,  4928,  4928,  4224,  3520,  2816,
        0,   704,  1408,  2112,  2112,  1408,   704,     0,
};

// PieceSquareIndex[perspective][PandaChess Piece]
// Maps (perspective, piece) to an offset within a king bucket.
// White perspective: "our" pieces use PS_W_*, "their" pieces use PS_B_*
// Black perspective: reversed — "our" (black) pieces use PS_W_*, "their" (white) use PS_B_*
constexpr int PieceSquareIndex[2][12] = {
    // WHITE perspective
    //  WP   WN   WB   WR   WQ   WK   BP   BN   BB   BR   BQ   BK
    {    0, 128, 256, 384, 512, 640,  64, 192, 320, 448, 576, 640 },
    // BLACK perspective
    //  WP   WN   WB   WR   WQ   WK   BP   BN   BB   BR   BQ   BK
    {   64, 192, 320, 448, 576, 640,   0, 128, 256, 384, 512, 640 },
};

// ─── Weight structures ──────────────────────────────────────────────────────

struct FeatureTransformer {
    std::unique_ptr<int16_t[]> biases;   // [HalfDimensions]
    std::unique_ptr<int16_t[]> weights;  // [HalfDimensions * InputDimensions]
    std::unique_ptr<int32_t[]> psqt;     // [InputDimensions * PSQTBuckets]
};

struct LayerStack {
    int32_t fc0_bias[FC0_OUT];
    std::unique_ptr<int8_t[]> fc0_weight;  // [FC0_OUT * HalfDimensions]  (3072 inputs)

    int32_t fc1_bias[FC1_OUT];
    int8_t  fc1_weight[FC1_OUT * FC1_PAD]; // [32 * 32]

    int32_t fc2_bias[FC2_OUT];
    int8_t  fc2_weight[FC2_OUT * FC1_OUT]; // [1 * 32]
};

// ─── NNUENetwork ────────────────────────────────────────────────────────────

class NNUENetwork {
public:
    NNUENetwork();

    // Load weights from a .nnue file. Returns true on success.
    bool load(const std::string& path);

    // Evaluate a position. Returns score in centipawns from STM perspective.
    int evaluate(const Board& board) const;

    bool is_loaded() const { return loaded; }

private:
    FeatureTransformer ft;
    LayerStack stacks[LayerStacks];
    bool loaded = false;

    // Internal helpers
    void compute_accumulator(const Board& board, Color perspective,
                             int16_t* accumulator, int32_t* psqt_out) const;
    int propagate(const LayerStack& stack, const uint8_t* input,
                  int32_t fc0_out15) const;
};

}  // namespace nnue
}  // namespace panda
