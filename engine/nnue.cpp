#include "nnue.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "board.h"

namespace panda {
namespace nnue {

// ─── LEB128 decompression ───────────────────────────────────────────────────
// Stockfish stores FT weights/biases with LEB128 variable-length encoding.

template <typename IntType>
static bool read_leb128(std::istream& stream, IntType* out, std::size_t count) {
    constexpr std::size_t BitsPerByte = 8;
    constexpr std::size_t IntBits     = sizeof(IntType) * BitsPerByte;

    for (std::size_t i = 0; i < count; ++i) {
        typename std::make_unsigned<IntType>::type result = 0;
        std::size_t shift = 0;
        int byte;

        do {
            byte = stream.get();
            if (stream.fail())
                return false;

            result |= static_cast<typename std::make_unsigned<IntType>::type>(byte & 0x7F) << shift;
            shift += 7;
        } while (byte & 0x80);

        // Sign-extend for signed types
        if constexpr (std::is_signed_v<IntType>) {
            if (shift < IntBits && (byte & 0x40))
                result |= ~(static_cast<typename std::make_unsigned<IntType>::type>(0)) << shift;
        }

        out[i] = static_cast<IntType>(result);
    }
    return true;
}

// ─── Little-endian reading ──────────────────────────────────────────────────

static bool read_le_u32(std::istream& stream, uint32_t& val) {
    uint8_t buf[4];
    stream.read(reinterpret_cast<char*>(buf), 4);
    if (!stream)
        return false;
    val = uint32_t(buf[0]) | (uint32_t(buf[1]) << 8) | (uint32_t(buf[2]) << 16) |
          (uint32_t(buf[3]) << 24);
    return true;
}

template <typename T>
static bool read_le_array(std::istream& stream, T* out, std::size_t count) {
    // For 8-bit types, just read directly
    if constexpr (sizeof(T) == 1) {
        stream.read(reinterpret_cast<char*>(out), count);
        return stream.good();
    } else {
        // Read as bytes and reconstruct in little-endian order
        for (std::size_t i = 0; i < count; ++i) {
            uint8_t buf[sizeof(T)];
            stream.read(reinterpret_cast<char*>(buf), sizeof(T));
            if (!stream)
                return false;

            typename std::make_unsigned<T>::type val = 0;
            for (std::size_t b = 0; b < sizeof(T); ++b)
                val |= static_cast<typename std::make_unsigned<T>::type>(buf[b]) << (b * 8);
            out[i] = static_cast<T>(val);
        }
        return true;
    }
}

// ─── NNUENetwork constructor ────────────────────────────────────────────────

NNUENetwork::NNUENetwork() = default;

// ─── File loading ───────────────────────────────────────────────────────────

bool NNUENetwork::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "NNUE: cannot open " << path << std::endl;
        return false;
    }

    // --- Header ---
    uint32_t version, archHash, descLen;
    if (!read_le_u32(file, version) || !read_le_u32(file, archHash) || !read_le_u32(file, descLen)) {
        std::cerr << "NNUE: failed to read header" << std::endl;
        return false;
    }

    if (version != 0x7AF32F20) {
        std::cerr << "NNUE: unexpected version 0x" << std::hex << version << std::endl;
        return false;
    }

    // Skip description string
    file.seekg(descLen, std::ios::cur);
    if (!file) {
        std::cerr << "NNUE: failed to skip description" << std::endl;
        return false;
    }

    // --- Feature Transformer ---
    uint32_t ftHash;
    if (!read_le_u32(file, ftHash)) {
        std::cerr << "NNUE: failed to read FT hash" << std::endl;
        return false;
    }

    // Allocate FT arrays
    ft.biases  = std::make_unique<int16_t[]>(HalfDimensions);
    ft.weights = std::make_unique<int16_t[]>(static_cast<std::size_t>(HalfDimensions) * InputDimensions);
    ft.psqt    = std::make_unique<int32_t[]>(static_cast<std::size_t>(InputDimensions) * PSQTBuckets);

    // Read LEB128-compressed data
    if (!read_leb128<int16_t>(file, ft.biases.get(), HalfDimensions)) {
        std::cerr << "NNUE: failed to read FT biases" << std::endl;
        return false;
    }
    if (!read_leb128<int16_t>(file, ft.weights.get(),
                               static_cast<std::size_t>(HalfDimensions) * InputDimensions)) {
        std::cerr << "NNUE: failed to read FT weights" << std::endl;
        return false;
    }
    if (!read_leb128<int32_t>(file, ft.psqt.get(),
                               static_cast<std::size_t>(InputDimensions) * PSQTBuckets)) {
        std::cerr << "NNUE: failed to read FT PSQT" << std::endl;
        return false;
    }

    // Post-read: scale biases and weights by 2
    for (int i = 0; i < HalfDimensions; ++i)
        ft.biases[i] *= 2;
    for (std::size_t i = 0; i < static_cast<std::size_t>(HalfDimensions) * InputDimensions; ++i)
        ft.weights[i] *= 2;

    // --- Layer Stacks (x8) ---
    for (int s = 0; s < LayerStacks; ++s) {
        uint32_t bodyHash;
        if (!read_le_u32(file, bodyHash)) {
            std::cerr << "NNUE: failed to read body hash for stack " << s << std::endl;
            return false;
        }

        LayerStack& ls = stacks[s];

        // fc_0: biases int32[16], weights int8[16 * 3072]
        if (!read_le_array<int32_t>(file, ls.fc0_bias, FC0_OUT))
            return false;
        ls.fc0_weight = std::make_unique<int8_t[]>(static_cast<std::size_t>(FC0_OUT) * HalfDimensions);
        if (!read_le_array<int8_t>(file, ls.fc0_weight.get(),
                                    static_cast<std::size_t>(FC0_OUT) * HalfDimensions))
            return false;

        // fc_1: biases int32[32], weights int8[32 * 32]
        if (!read_le_array<int32_t>(file, ls.fc1_bias, FC1_OUT))
            return false;
        if (!read_le_array<int8_t>(file, ls.fc1_weight, FC1_OUT * FC1_PAD))
            return false;

        // fc_2: biases int32[1], weights int8[1 * 32]
        if (!read_le_array<int32_t>(file, ls.fc2_bias, FC2_OUT))
            return false;
        if (!read_le_array<int8_t>(file, ls.fc2_weight, FC2_OUT * FC1_OUT))
            return false;
    }

    loaded = true;
    std::cout << "NNUE: loaded " << path << " successfully" << std::endl;
    return true;
}

// ─── Feature index computation ──────────────────────────────────────────────

static int feature_index(Color perspective, Square ksq, Piece piece, Square sq) {
    // Orient square for horizontal mirroring
    int oriented_sq = int(sq) ^ OrientTBL[ksq] ^ (56 * perspective);
    int king_bucket = KingBuckets[int(ksq) ^ (56 * perspective)];
    int piece_offset = PieceSquareIndex[perspective][piece];

    return oriented_sq + piece_offset + king_bucket;
}

// ─── Accumulator computation ────────────────────────────────────────────────

void NNUENetwork::compute_accumulator(const Board& board, Color perspective,
                                       int16_t* accumulator, int32_t* psqt_out) const {
    // Start with biases
    std::memcpy(accumulator, ft.biases.get(), sizeof(int16_t) * HalfDimensions);
    std::memset(psqt_out, 0, sizeof(int32_t) * PSQTBuckets);

    // Find king square for this perspective
    Bitboard king_bb = board.pieces(perspective, King);
    Square ksq = lsb(king_bb);

    // Iterate over all pieces on the board
    for (int c = 0; c < 2; ++c) {
        Color color = Color(c);
        for (int pt = 0; pt < 6; ++pt) {
            PieceType ptype = PieceType(pt);
            Piece piece = make_piece(color, ptype);
            Bitboard bb = board.pieces(color, ptype);

            while (bb) {
                Square sq = pop_lsb(bb);
                int idx = feature_index(perspective, ksq, piece, sq);

                // Add weights for this feature to accumulator
                const int16_t* w = &ft.weights[static_cast<std::size_t>(idx) * HalfDimensions];
                for (int j = 0; j < HalfDimensions; ++j)
                    accumulator[j] += w[j];

                // Add PSQT weights
                const int32_t* p = &ft.psqt[static_cast<std::size_t>(idx) * PSQTBuckets];
                for (int b = 0; b < PSQTBuckets; ++b)
                    psqt_out[b] += p[b];
            }
        }
    }
}

// ─── Activation functions ───────────────────────────────────────────────────

static inline uint8_t clipped_relu(int32_t x, int shift) {
    // ClippedReLU: clamp(x >> shift, 0, 127)
    x >>= shift;
    if (x < 0) return 0;
    if (x > 127) return 127;
    return static_cast<uint8_t>(x);
}

static inline uint8_t sqr_clipped_relu(int32_t x, int shift) {
    // SqrClippedReLU: min(127, clamp(x,0,127)^2 >> (shift - 6))
    // Actually: clamp(x >> 6, 0, 127), then min(127, val*val >> (shift-6))
    // Stockfish: min(127, x*x >> 19) where x is already shifted
    x >>= 6;
    if (x < 0) x = 0;
    if (x > 127) x = 127;
    int32_t sq = x * x;
    sq >>= (shift - 6);  // shift = 13, so >> 7
    if (sq > 127) sq = 127;
    return static_cast<uint8_t>(sq);
}

// ─── Dense layer propagation ────────────────────────────────────────────────

int NNUENetwork::propagate(const LayerStack& stack, const uint8_t* input,
                            int32_t fc0_out15) const {
    // fc_0: 3072 → 16 (sparse input, but we do dense for now)
    int32_t fc0[FC0_OUT];
    for (int i = 0; i < FC0_OUT; ++i) {
        int32_t sum = stack.fc0_bias[i];
        const int8_t* w = &stack.fc0_weight[static_cast<std::size_t>(i) * HalfDimensions];
        for (int j = 0; j < HalfDimensions; ++j) {
            // Input is uint8, weight is int8 → multiply and accumulate
            sum += static_cast<int32_t>(input[j]) * static_cast<int32_t>(w[j]);
        }
        fc0[i] = sum;
    }

    // Dual activation on fc0[0..14] → 30 values
    // SqrClippedReLU(fc0[0..14]) ∥ ClippedReLU(fc0[0..14])
    uint8_t ac0[FC1_PAD];  // 32 bytes (padded from 30)
    std::memset(ac0, 0, sizeof(ac0));
    for (int i = 0; i < 15; ++i) {
        ac0[i]      = sqr_clipped_relu(fc0[i], 13);
        ac0[15 + i] = clipped_relu(fc0[i], 6);
    }

    // fc_1: 30 → 32 (padded input = 32)
    int32_t fc1[FC1_OUT];
    for (int i = 0; i < FC1_OUT; ++i) {
        int32_t sum = stack.fc1_bias[i];
        const int8_t* w = &stack.fc1_weight[i * FC1_PAD];
        for (int j = 0; j < FC1_PAD; ++j) {
            sum += static_cast<int32_t>(ac0[j]) * static_cast<int32_t>(w[j]);
        }
        fc1[i] = sum;
    }

    // ClippedReLU → 32 uint8
    uint8_t ac1[FC1_OUT];
    for (int i = 0; i < FC1_OUT; ++i) {
        ac1[i] = clipped_relu(fc1[i], 6);
    }

    // fc_2: 32 → 1
    int32_t fc2 = stack.fc2_bias[0];
    for (int i = 0; i < FC1_OUT; ++i) {
        fc2 += static_cast<int32_t>(ac1[i]) * static_cast<int32_t>(stack.fc2_weight[i]);
    }

    // Skip connection: fc0[15] * (600 * 16) / (127 * 64)
    // = fc0[15] * 9600 / 8128
    // Stockfish actually does: fc0[15] * (600 * OutputScale) / (127 * 64)
    int32_t skip = fc0_out15 * (600 * OutputScale) / (127 * 64);
    fc2 += skip;

    return fc2;
}

// ─── Full evaluation ────────────────────────────────────────────────────────

int NNUENetwork::evaluate(const Board& board) const {
    if (!loaded)
        return 0;

    Color stm  = board.side_to_move();
    Color nstm = ~stm;

    // Compute accumulators for both perspectives
    int16_t acc_stm[HalfDimensions];
    int16_t acc_nstm[HalfDimensions];
    int32_t psqt_stm[PSQTBuckets];
    int32_t psqt_nstm[PSQTBuckets];

    compute_accumulator(board, stm, acc_stm, psqt_stm);
    compute_accumulator(board, nstm, acc_nstm, psqt_nstm);

    // Pairwise activation on each accumulator:
    // output[j] = clamp(acc[j], 0, 254) * clamp(acc[j + HalfDimensions/2], 0, 254) / 512
    // → 1536 uint8 per perspective, 3072 total
    constexpr int Half = HalfDimensions / 2;  // 1536
    uint8_t transformed[HalfDimensions];  // 3072 = 1536 stm + 1536 nstm

    // STM perspective → first 1536 bytes
    for (int j = 0; j < Half; ++j) {
        int16_t a = std::clamp<int16_t>(acc_stm[j], 0, 254);
        int16_t b = std::clamp<int16_t>(acc_stm[j + Half], 0, 254);
        transformed[j] = static_cast<uint8_t>((int32_t(a) * int32_t(b)) / 512);
    }
    // NSTM perspective → next 1536 bytes
    for (int j = 0; j < Half; ++j) {
        int16_t a = std::clamp<int16_t>(acc_nstm[j], 0, 254);
        int16_t b = std::clamp<int16_t>(acc_nstm[j + Half], 0, 254);
        transformed[Half + j] = static_cast<uint8_t>((int32_t(a) * int32_t(b)) / 512);
    }

    // Determine layer stack bucket: (piece_count - 1) / 4
    int piece_count = popcount(board.all_pieces());
    int bucket = (piece_count - 1) / 4;
    if (bucket < 0) bucket = 0;
    if (bucket >= LayerStacks) bucket = LayerStacks - 1;

    // Propagate through network body
    // fc0[15] is needed for skip connection — we compute it here
    const LayerStack& stack = stacks[bucket];
    int32_t fc0_15 = stack.fc0_bias[15];
    const int8_t* w15 = &stack.fc0_weight[static_cast<std::size_t>(15) * HalfDimensions];
    for (int j = 0; j < HalfDimensions; ++j) {
        fc0_15 += static_cast<int32_t>(transformed[j]) * static_cast<int32_t>(w15[j]);
    }

    int positional = propagate(stack, transformed, fc0_15);

    // PSQT score
    int32_t psqt = (psqt_stm[bucket] - psqt_nstm[bucket]) / 2;

    // Final score
    int score = (positional + psqt) / OutputScale;

    return score;
}

}  // namespace nnue
}  // namespace panda
