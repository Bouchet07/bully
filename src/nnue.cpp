#include "nnue.h"
#include "bitboard.h"
#include <iostream>
#include <fstream>
#include <format>
#include <algorithm>
#include <vector>
#include <cstring>

#if defined(__AVX512F__) && defined(__AVX512BW__)
    #include <immintrin.h>
    #define USE_AVX512
#elif defined(__AVX2__)
    #include <immintrin.h>
    #define USE_AVX2
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define USE_NEON
#endif

namespace Bully {
namespace NNUE {

bool use_nnue = false;
std::string eval_file = "nn-7821938.nnue";
static bool net_loaded = false;

// ============================================================================
// Network Weights Structure (HalfKP 256x2-32-32-1)
// ============================================================================
struct NetworkWeights {
    alignas(64) std::array<int16_t, TRANSFORMER_HALF_DIM> feature_biases{};
    alignas(64) std::vector<int16_t> feature_weights; // 256 * 40960 = 10,485,760 int16_t (20.97 MB)
    
    alignas(64) std::array<int32_t, L1_DIM> l1_biases{};
    alignas(64) std::array<int8_t, L1_DIM * 2 * TRANSFORMER_HALF_DIM> l1_weights{};
    
    alignas(64) std::array<int32_t, 1> output_bias{};
    alignas(64) std::array<int8_t, L1_DIM> output_weights{};

    NetworkWeights() {
        feature_weights.resize(TRANSFORMER_HALF_DIM * HALFKP_FEATURES, 0);
    }
} static net_weights;

// Helper function to vector-add feature weights to accumulator
static inline void vec_add(int16_t* acc, const int16_t* weights) {
#if defined(USE_AVX512)
    for (int i = 0; i < 8; ++i) {
        __m512i a = _mm512_load_si512(reinterpret_cast<const __m512i*>(acc + i * 32));
        __m512i w = _mm512_load_si512(reinterpret_cast<const __m512i*>(weights + i * 32));
        _mm512_store_si512(reinterpret_cast<__m512i*>(acc + i * 32), _mm512_add_epi16(a, w));
    }
#elif defined(USE_AVX2)
    for (int i = 0; i < 16; ++i) {
        __m256i a = _mm256_load_si256(reinterpret_cast<const __m256i*>(acc + i * 16));
        __m256i w = _mm256_load_si256(reinterpret_cast<const __m256i*>(weights + i * 16));
        _mm256_store_si256(reinterpret_cast<__m256i*>(acc + i * 16), _mm256_add_epi16(a, w));
    }
#elif defined(USE_NEON)
    for (int i = 0; i < 32; ++i) {
        int16x8_t a = vld1q_s16(acc + i * 8);
        int16x8_t w = vld1q_s16(weights + i * 8);
        vst1q_s16(acc + i * 8, vaddq_s16(a, w));
    }
#else
    for (size_t i = 0; i < TRANSFORMER_HALF_DIM; ++i) {
        acc[i] = static_cast<int16_t>(acc[i] + weights[i]);
    }
#endif
}

void init() {
    net_loaded = false;
}

static inline uint32_t read_u32(std::ifstream& f) {
    uint32_t val = 0;
    f.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

bool load_net(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        net_loaded = false;
        return false;
    }

    uint32_t magic = read_u32(file);
    if (magic != NNUE_VERSION && magic != 0x7AF32F20) {
        // Unrecognized NNUE magic header
        net_loaded = false;
        return false;
    }

    // Read Feature Transformer Biases & Weights
    file.read(reinterpret_cast<char*>(net_weights.feature_biases.data()), static_cast<std::streamsize>(sizeof(net_weights.feature_biases)));
    file.read(reinterpret_cast<char*>(net_weights.feature_weights.data()), static_cast<std::streamsize>(net_weights.feature_weights.size() * sizeof(int16_t)));

    // Read Layer 1 Biases & Weights
    file.read(reinterpret_cast<char*>(net_weights.l1_biases.data()), static_cast<std::streamsize>(sizeof(net_weights.l1_biases)));
    file.read(reinterpret_cast<char*>(net_weights.l1_weights.data()), static_cast<std::streamsize>(sizeof(net_weights.l1_weights)));

    // Read Output Layer Bias & Weights
    file.read(reinterpret_cast<char*>(net_weights.output_bias.data()), static_cast<std::streamsize>(sizeof(net_weights.output_bias)));
    file.read(reinterpret_cast<char*>(net_weights.output_weights.data()), static_cast<std::streamsize>(sizeof(net_weights.output_weights)));

    if (!file.good()) {
        net_loaded = false;
        return false;
    }

    eval_file = path;
    net_loaded = true;
    return true;
}

bool is_ready() {
    return net_loaded;
}

void refresh_accumulator(const Position& pos, Accumulator& acc) {
    Square w_ksq = pos.king_square(WHITE);
    Square b_ksq = pos.king_square(BLACK);

    // Initialize accumulators with feature biases
    acc.white = net_weights.feature_biases;
    acc.black = net_weights.feature_biases;

    Bitboard occ = pos.occupied();
    while (occ) {
        Square sq = pop_lsb(occ);
        Piece pc = pos.piece_on(sq);
        if (type_of(pc) == KING) continue;

        // White perspective feature
        size_t w_idx = feature_index(WHITE, w_ksq, pc, sq);
        const int16_t* w_w = &net_weights.feature_weights[w_idx * TRANSFORMER_HALF_DIM];
        vec_add(acc.white.data(), w_w);

        // Black perspective feature
        size_t b_idx = feature_index(BLACK, b_ksq, pc, sq);
        const int16_t* b_w = &net_weights.feature_weights[b_idx * TRANSFORMER_HALF_DIM];
        vec_add(acc.black.data(), b_w);
    }

    acc.computed[WHITE] = true;
    acc.computed[BLACK] = true;
}

Value evaluate(const Position& pos) {
    if (!use_nnue || !net_loaded) {
        return VALUE_ZERO;
    }

    // Refresh accumulator if needed
    Accumulator acc;
    refresh_accumulator(pos, acc);

    Color us = pos.side_to_move();
    const int16_t* us_acc   = (us == WHITE) ? acc.white.data() : acc.black.data();
    const int16_t* them_acc = (us == WHITE) ? acc.black.data() : acc.white.data();

    // Layer 1 Forward Pass (Clipped ReLU + Linear transform to 32 neurons)
    std::array<int32_t, L1_DIM> l1_out = net_weights.l1_biases;

    for (size_t i = 0; i < L1_DIM; ++i) {
        const int8_t* w = &net_weights.l1_weights[i * 2 * TRANSFORMER_HALF_DIM];
        int32_t sum = 0;

        // First 256 inputs (us perspective)
        for (size_t j = 0; j < TRANSFORMER_HALF_DIM; ++j) {
            int16_t val = us_acc[j];
            int16_t activated = std::clamp<int16_t>(val, 0, 127);
            sum += activated * static_cast<int32_t>(w[j]);
        }

        // Next 256 inputs (them perspective)
        for (size_t j = 0; j < TRANSFORMER_HALF_DIM; ++j) {
            int16_t val = them_acc[j];
            int16_t activated = std::clamp<int16_t>(val, 0, 127);
            sum += activated * static_cast<int32_t>(w[TRANSFORMER_HALF_DIM + j]);
        }

        l1_out[i] += sum;
    }

    // Output Layer Forward Pass (Clipped ReLU + Linear transform to 1 score value)
    int32_t out = net_weights.output_bias[0];
    for (size_t i = 0; i < L1_DIM; ++i) {
        int32_t val = l1_out[i] / 64;
        int32_t activated = std::clamp<int32_t>(val, 0, 127);
        out += activated * static_cast<int32_t>(net_weights.output_weights[i]);
    }

    int score = out / 16; // Scale down to centipawns
    return static_cast<Value>(score);
}

} // namespace NNUE
} // namespace Bully
