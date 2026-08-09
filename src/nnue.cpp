#include "nnue.h"
#include "position.h"
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
        __m512i a = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(acc + i * 32));
        __m512i w = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(weights + i * 32));
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(acc + i * 32), _mm512_add_epi16(a, w));
    }
#elif defined(USE_AVX2)
    for (int i = 0; i < 16; ++i) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(acc + i * 16));
        __m256i w = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights + i * 16));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(acc + i * 16), _mm256_add_epi16(a, w));
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

// Helper function to vector-subtract feature weights from accumulator
static inline void vec_sub(int16_t* acc, const int16_t* weights) {
#if defined(USE_AVX512)
    for (int i = 0; i < 8; ++i) {
        __m512i a = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(acc + i * 32));
        __m512i w = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(weights + i * 32));
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(acc + i * 32), _mm512_sub_epi16(a, w));
    }
#elif defined(USE_AVX2)
    for (int i = 0; i < 16; ++i) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(acc + i * 16));
        __m256i w = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights + i * 16));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(acc + i * 16), _mm256_sub_epi16(a, w));
    }
#elif defined(USE_NEON)
    for (int i = 0; i < 32; ++i) {
        int16x8_t a = vld1q_s16(acc + i * 8);
        int16x8_t w = vld1q_s16(weights + i * 8);
        vst1q_s16(acc + i * 8, vsubq_s16(a, w));
    }
#else
    for (size_t i = 0; i < TRANSFORMER_HALF_DIM; ++i) {
        acc[i] = static_cast<int16_t>(acc[i] - weights[i]);
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

static void update_accumulator(const Position& pos, Accumulator& acc, Color c) {
    if (acc.computed[to_index(c)]) {
        return;
    }

    const StateInfo* st = pos.state();
    std::array<const StateInfo*, MAX_PLY> chain;
    size_t chain_len = 0;

    while (st && st->accumulator && !st->accumulator->computed[to_index(c)] && chain_len < MAX_PLY) {
        chain[chain_len++] = st;
        st = st->previous;
    }

    if (st && st->accumulator && st->accumulator->computed[to_index(c)]) {
        if (c == WHITE) {
            acc.white = st->accumulator->white;
        } else {
            acc.black = st->accumulator->black;
        }

        Square ksq = pos.king_square(c);
        int16_t* target_acc = (c == WHITE) ? acc.white.data() : acc.black.data();

        for (size_t idx_chain = chain_len; idx_chain > 0; --idx_chain) {
            const DirtyPiece& dp = chain[idx_chain - 1]->dirty_piece;
            for (int i = 0; i < dp.count; ++i) {
                Piece pc = dp.piece[i];
                if (dp.from[i] != SQ_NONE) {
                    size_t idx = feature_index(c, ksq, pc, dp.from[i]);
                    const int16_t* w = &net_weights.feature_weights[idx * TRANSFORMER_HALF_DIM];
                    vec_sub(target_acc, w);
                }
                if (dp.to[i] != SQ_NONE) {
                    size_t idx = feature_index(c, ksq, pc, dp.to[i]);
                    const int16_t* w = &net_weights.feature_weights[idx * TRANSFORMER_HALF_DIM];
                    vec_add(target_acc, w);
                }
            }
        }
        acc.computed[to_index(c)] = true;
    } else {
        refresh_accumulator(pos, acc);
    }
}

Value evaluate(const Position& pos) {
    if (!use_nnue || !net_loaded) {
        return VALUE_ZERO;
    }

    Accumulator stack_acc;
    Accumulator& acc = pos.state()->accumulator ? *pos.state()->accumulator : stack_acc;

    update_accumulator(pos, acc, WHITE);
    update_accumulator(pos, acc, BLACK);

    Color us = pos.side_to_move();
    const int16_t* us_acc   = (us == WHITE) ? acc.white.data() : acc.black.data();
    const int16_t* them_acc = (us == WHITE) ? acc.black.data() : acc.white.data();

    alignas(64) std::array<uint8_t, TRANSFORMER_HALF_DIM * 2> packed_activations;

#if defined(USE_AVX2) || defined(USE_AVX512)
    // 1. Vectorized ClippedReLU & Packing (int16_t -> uint8_t)
    const __m256i zero = _mm256_setzero_si256();
    const __m256i max_val = _mm256_set1_epi16(127);
    
    for (size_t i = 0; i < TRANSFORMER_HALF_DIM / 16; ++i) {
        // Load Us and Them accumulators
        __m256i u = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(us_acc + i * 16));
        __m256i t = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(them_acc + i * 16));
        
        // Clamp to [0, 127]
        u = _mm256_min_epi16(_mm256_max_epi16(u, zero), max_val);
        t = _mm256_min_epi16(_mm256_max_epi16(t, zero), max_val);
        
        // Pack 16-bit ints into 8-bit ints (Warning: AVX2 packs interleave lanes, so we store directly)
        __m256i packed = _mm256_packs_epi16(u, t); 
        
        // Permute to fix AVX lane crossing
        packed = _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(packed_activations.data() + i * 32), packed);
    }
#else
    // Generic Fallback
    for (size_t i = 0; i < TRANSFORMER_HALF_DIM; ++i) {
        packed_activations[i] = static_cast<uint8_t>(std::clamp<int16_t>(us_acc[i], 0, 127));
        packed_activations[TRANSFORMER_HALF_DIM + i] = static_cast<uint8_t>(std::clamp<int16_t>(them_acc[i], 0, 127));
    }
#endif

    std::array<int32_t, L1_DIM> l1_out = net_weights.l1_biases;

    // 2. Vectorized Dot Product for L1 Layer
    for (size_t i = 0; i < L1_DIM; ++i) {
        const int8_t* w = &net_weights.l1_weights[i * 2 * TRANSFORMER_HALF_DIM];
        int32_t sum = 0;

#if defined(USE_AVX2) || defined(USE_AVX512)
        __m256i v_sum = _mm256_setzero_si256();
        const __m256i ones = _mm256_set1_epi16(1);

        for (size_t j = 0; j < (2 * TRANSFORMER_HALF_DIM) / 32; ++j) {
            __m256i act = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(packed_activations.data() + j * 32));
            __m256i weight = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w + j * 32));

            // Multiply uint8_t activations by int8_t weights -> int16_t pairs
            __m256i mult = _mm256_maddubs_epi16(act, weight);
            
            // Multiply int16_t by 1 and horizontally add to int32_t
            __m256i dot = _mm256_madd_epi16(mult, ones);
            
            v_sum = _mm256_add_epi32(v_sum, dot);
        }

        // Horizontal sum of the 8 int32_t values in v_sum
        __m128i hi = _mm256_extracti128_si256(v_sum, 1);
        __m128i lo = _mm256_castsi256_si128(v_sum);
        hi = _mm_add_epi32(hi, lo);
        hi = _mm_add_epi32(hi, _mm_shuffle_epi32(hi, _MM_SHUFFLE(1, 0, 3, 2)));
        hi = _mm_add_epi32(hi, _mm_shuffle_epi32(hi, _MM_SHUFFLE(2, 3, 0, 1)));
        sum += _mm_cvtsi128_si32(hi);
#else
        for (size_t j = 0; j < 2 * TRANSFORMER_HALF_DIM; ++j) {
            sum += packed_activations[j] * static_cast<int32_t>(w[j]);
        }
#endif
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
