#include "nnue.h"
#include "position.h"
#include "bitboard.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h>
#endif

namespace Bully {
namespace NNUE {

bool use_nnue = false;
std::string eval_file = "nn-7821938.nnue";
static bool net_loaded = false;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

// ============================================================================
// Network Weights Structure (HalfKP 256x2-32-32-1)
// ============================================================================
struct NetworkWeights {
    alignas(64) std::array<int16_t, TransformerHalfDim> feature_biases{};
    alignas(64) std::vector<int16_t> feature_weights; // 256 * 40960 = 10,485,760 int16_t (20.97 MB)
    
    alignas(64) std::array<int32_t, L1Dim> l1_biases{};
    alignas(64) std::array<int8_t, L1Dim * 2 * TransformerHalfDim> l1_weights{};
    
    alignas(64) std::array<int32_t, 1> output_bias{};
    alignas(64) std::array<int8_t, L1Dim> output_weights{};

    NetworkWeights() {
        feature_weights.resize(TransformerHalfDim * HalfKPFeatures, 0);
    }
} static net_weights;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// ============================================================================
// SIMD Hardware Optimization Layer
// ============================================================================
namespace Detail {

// ----------------------------------------------------------------------------
// AVX-512 Implementations
// ----------------------------------------------------------------------------
#if defined(__AVX512F__) && defined(__AVX512BW__)
inline void vec_add_avx512(int16_t* acc, const int16_t* w) {
    for (int i = 0; i < 8; ++i) {
        __m512i a = _mm512_loadu_si512(acc + i * 32);
        __m512i b = _mm512_loadu_si512(w + i * 32);
        _mm512_storeu_si512(acc + i * 32, _mm512_add_epi16(a, b));
    }
}

inline void vec_sub_avx512(int16_t* acc, const int16_t* w) {
    for (int i = 0; i < 8; ++i) {
        __m512i a = _mm512_loadu_si512(acc + i * 32);
        __m512i b = _mm512_loadu_si512(w + i * 32);
        _mm512_storeu_si512(acc + i * 32, _mm512_sub_epi16(a, b));
    }
}

inline void pack_avx512(uint8_t* out, const int16_t* us, const int16_t* them) {
    const __m512i zero = _mm512_setzero_si512();
    const __m512i max_val = _mm512_set1_epi16(127);
    for (int i = 0; i < 8; ++i) {
        __m512i u = _mm512_loadu_si512(us + i * 32);
        u = _mm512_min_epi16(_mm512_max_epi16(u, zero), max_val);
        __m512i t = _mm512_loadu_si512(them + i * 32);
        t = _mm512_min_epi16(_mm512_max_epi16(t, zero), max_val);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i * 32), _mm512_cvtepi16_epi8(u));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + 256 + i * 32), _mm512_cvtepi16_epi8(t));
    }
}

#if defined(USE_VNNI) || defined(__AVX512VNNI__)
inline int32_t dot_avx512_vnni(const uint8_t* act, const int8_t* w) {
    __m512i sum = _mm512_setzero_si512();
    for (int i = 0; i < 8; ++i) {
        __m512i a = _mm512_loadu_si512(act + i * 64);
        __m512i b = _mm512_loadu_si512(w + i * 64);
        sum = _mm512_dpbusd_epi32(sum, a, b);
    }
    return _mm512_reduce_add_epi32(sum);
}
#endif
#endif

// ----------------------------------------------------------------------------
// AVX2 Implementations
// ----------------------------------------------------------------------------
#if defined(__AVX2__)
inline void vec_add_avx2(int16_t* acc, const int16_t* w) {
    for (int i = 0; i < 16; ++i) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(acc + i * 16));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w + i * 16));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(acc + i * 16), _mm256_add_epi16(a, b));
    }
}

inline void vec_sub_avx2(int16_t* acc, const int16_t* w) {
    for (int i = 0; i < 16; ++i) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(acc + i * 16));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w + i * 16));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(acc + i * 16), _mm256_sub_epi16(a, b));
    }
}

inline void pack_avx2(uint8_t* out, const int16_t* us, const int16_t* them) {
    const __m256i zero = _mm256_setzero_si256();
    const __m256i max_val = _mm256_set1_epi16(127);
    for (int i = 0; i < 16; ++i) {
        __m256i u = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(us + i * 16));
        u = _mm256_min_epi16(_mm256_max_epi16(u, zero), max_val);
        __m256i t = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(them + i * 16));
        t = _mm256_min_epi16(_mm256_max_epi16(t, zero), max_val);
        
        __m256i p = _mm256_packus_epi16(u, t);
        p = _mm256_permute4x64_epi64(p, _MM_SHUFFLE(3, 1, 2, 0)); // Reorder lane-crossing
        
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i * 16), _mm256_castsi256_si128(p));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 256 + i * 16), _mm256_extracti128_si256(p, 1));
    }
}

inline int32_t dot_avx2(const uint8_t* act, const int8_t* w) {
    __m256i sum = _mm256_setzero_si256();
    const __m256i ones = _mm256_set1_epi16(1);
    for (int i = 0; i < 16; ++i) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(act + i * 32));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w + i * 32));
        __m256i mult = _mm256_maddubs_epi16(a, b);
        __m256i dot = _mm256_madd_epi16(mult, ones);
        sum = _mm256_add_epi32(sum, dot);
    }
    __m128i hi = _mm256_extracti128_si256(sum, 1);
    __m128i lo = _mm256_castsi256_si128(sum);
    hi = _mm_add_epi32(hi, lo);
    hi = _mm_hadd_epi32(hi, hi);
    hi = _mm_hadd_epi32(hi, hi);
    return _mm_cvtsi128_si32(hi);
}

#if defined(USE_VNNI) || defined(__AVX_VNNI__)
inline int32_t dot_avx2_vnni(const uint8_t* act, const int8_t* w) {
    __m256i sum = _mm256_setzero_si256();
    for (int i = 0; i < 16; ++i) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(act + i * 32));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w + i * 32));
        sum = _mm256_dpbusd_epi32(sum, a, b);
    }
    __m128i hi = _mm256_extracti128_si256(sum, 1);
    __m128i lo = _mm256_castsi256_si128(sum);
    hi = _mm_add_epi32(hi, lo);
    hi = _mm_hadd_epi32(hi, hi);
    hi = _mm_hadd_epi32(hi, hi);
    return _mm_cvtsi128_si32(hi);
}
#endif
#endif

// ----------------------------------------------------------------------------
// ARM NEON Implementations
// ----------------------------------------------------------------------------
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
inline void vec_add_neon(int16_t* acc, const int16_t* w) {
    for (int i = 0; i < 32; ++i) {
        int16x8_t a = vld1q_s16(acc + i * 8);
        int16x8_t b = vld1q_s16(w + i * 8);
        vst1q_s16(acc + i * 8, vaddq_s16(a, b));
    }
}

inline void vec_sub_neon(int16_t* acc, const int16_t* w) {
    for (int i = 0; i < 32; ++i) {
        int16x8_t a = vld1q_s16(acc + i * 8);
        int16x8_t b = vld1q_s16(w + i * 8);
        vst1q_s16(acc + i * 8, vsubq_s16(a, b));
    }
}

inline void pack_neon(uint8_t* out, const int16_t* us, const int16_t* them) {
    const int16x8_t zero = vdupq_n_s16(0);
    const int16x8_t max_val = vdupq_n_s16(127);
    for (int i = 0; i < 32; ++i) {
        int16x8_t u = vld1q_s16(us + i * 8);
        u = vminq_s16(vmaxq_s16(u, zero), max_val);
        int16x8_t t = vld1q_s16(them + i * 8);
        t = vminq_s16(vmaxq_s16(t, zero), max_val);

        uint8x8_t u8 = vqmovun_s16(u);
        uint8x8_t t8 = vqmovun_s16(t);

        vst1_u8(out + i * 8, u8);
        vst1_u8(out + 256 + i * 8, t8);
    }
}

inline int32_t dot_neon(const uint8_t* act, const int8_t* w) {
    int32x4_t sum = vdupq_n_s32(0);
    for (int i = 0; i < 32; ++i) {
        int8x16_t a = vld1q_s8(reinterpret_cast<const int8_t*>(act + i * 16));
        int8x16_t b = vld1q_s8(w + i * 16);
        
        int16x8_t m_low = vmull_s8(vget_low_s8(a), vget_low_s8(b));
        int16x8_t m_high = vmull_s8(vget_high_s8(a), vget_high_s8(b));
        
        sum = vpadalq_s16(sum, m_low);
        sum = vpadalq_s16(sum, m_high);
    }
    return vaddvq_s32(sum);
}

#if defined(USE_DOTPROD) || defined(__ARM_FEATURE_DOTPROD)
inline int32_t dot_neon_dotprod(const uint8_t* act, const int8_t* w) {
    int32x4_t sum = vdupq_n_s32(0);
    for (int i = 0; i < 32; ++i) {
        int8x16_t a = vld1q_s8(reinterpret_cast<const int8_t*>(act + i * 16));
        int8x16_t b = vld1q_s8(w + i * 16);
        sum = vdotq_s32(sum, a, b);
    }
    return vaddvq_s32(sum);
}
#endif
#endif

// ----------------------------------------------------------------------------
// Universal Scalar Fallbacks
// ----------------------------------------------------------------------------
inline void vec_add_scalar(int16_t* acc, const int16_t* w) {
    for (size_t i = 0; i < TransformerHalfDim; ++i) {
        acc[i] = static_cast<int16_t>(acc[i] + w[i]);
    }
}

inline void vec_sub_scalar(int16_t* acc, const int16_t* w) {
    for (size_t i = 0; i < TransformerHalfDim; ++i) {
        acc[i] = static_cast<int16_t>(acc[i] - w[i]);
    }
}

inline void pack_scalar(uint8_t* out, const int16_t* us, const int16_t* them) {
    for (size_t i = 0; i < TransformerHalfDim; ++i) {
        out[i] = static_cast<uint8_t>(std::clamp<int16_t>(us[i], 0, 127));
        out[256 + i] = static_cast<uint8_t>(std::clamp<int16_t>(them[i], 0, 127));
    }
}

inline int32_t dot_scalar(const uint8_t* act, const int8_t* w) {
    int32_t sum = 0;
    for (size_t i = 0; i < 2 * TransformerHalfDim; ++i) {
        sum += act[i] * static_cast<int32_t>(w[i]);
    }
    return sum;
}

} // namespace Detail

// ============================================================================
// Core Execution Dispatchers
// ============================================================================

static inline void vec_add(int16_t* acc, const int16_t* weights) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
    Detail::vec_add_avx512(acc, weights);
#elif defined(__AVX2__)
    Detail::vec_add_avx2(acc, weights);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    Detail::vec_add_neon(acc, weights);
#else
    Detail::vec_add_scalar(acc, weights);
#endif
}

static inline void vec_sub(int16_t* acc, const int16_t* weights) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
    Detail::vec_sub_avx512(acc, weights);
#elif defined(__AVX2__)
    Detail::vec_sub_avx2(acc, weights);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    Detail::vec_sub_neon(acc, weights);
#else
    Detail::vec_sub_scalar(acc, weights);
#endif
}

static inline void pack_activations(uint8_t* out, const int16_t* us, const int16_t* them) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
    Detail::pack_avx512(out, us, them);
#elif defined(__AVX2__)
    Detail::pack_avx2(out, us, them);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    Detail::pack_neon(out, us, them);
#else
    Detail::pack_scalar(out, us, them);
#endif
}

static inline int32_t compute_l1_dot_product(const uint8_t* act, const int8_t* w) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
    #if defined(USE_VNNI) || defined(__AVX512VNNI__)
        return Detail::dot_avx512_vnni(act, w);
    #else
        return Detail::dot_avx2(act, w); // Fallback to AVX2 logic if AVX-512 VNNI is absent
    #endif
#elif defined(__AVX2__)
    #if defined(USE_VNNI) || defined(__AVX_VNNI__)
        return Detail::dot_avx2_vnni(act, w);
    #else
        return Detail::dot_avx2(act, w);
    #endif
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #if defined(USE_DOTPROD) || defined(__ARM_FEATURE_DOTPROD)
        return Detail::dot_neon_dotprod(act, w);
    #else
        return Detail::dot_neon(act, w);
    #endif
#else
    return Detail::dot_scalar(act, w);
#endif
}


// ============================================================================
// Public NNUE Interface
// ============================================================================

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
    if (magic != NnueVersionMagic && magic != 0x7AF32F20) {
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

    acc.white = net_weights.feature_biases;
    acc.black = net_weights.feature_biases;

    Bitboard occ = pos.occupied();
    while (occ) {
        Square sq = pop_lsb(occ);
        Piece pc = pos.piece_on(sq);
        if (type_of(pc) == KING) continue;

        size_t w_idx = feature_index(WHITE, w_ksq, pc, sq);
        const int16_t* w_w = &net_weights.feature_weights[w_idx * TransformerHalfDim];
        vec_add(acc.white.data(), w_w);

        size_t b_idx = feature_index(BLACK, b_ksq, pc, sq);
        const int16_t* b_w = &net_weights.feature_weights[b_idx * TransformerHalfDim];
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
                    const int16_t* w = &net_weights.feature_weights[idx * TransformerHalfDim];
                    vec_sub(target_acc, w);
                }
                if (dp.to[i] != SQ_NONE) {
                    size_t idx = feature_index(c, ksq, pc, dp.to[i]);
                    const int16_t* w = &net_weights.feature_weights[idx * TransformerHalfDim];
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

    alignas(64) std::array<uint8_t, TransformerHalfDim * 2> packed_activations;
    pack_activations(packed_activations.data(), us_acc, them_acc);

    std::array<int32_t, L1Dim> l1_out = net_weights.l1_biases;

    for (size_t i = 0; i < L1Dim; ++i) {
        const int8_t* w = &net_weights.l1_weights[i * 2 * TransformerHalfDim];
        l1_out[i] += compute_l1_dot_product(packed_activations.data(), w);
    }

    int32_t out = net_weights.output_bias[0];
    for (size_t i = 0; i < L1Dim; ++i) {
        int32_t val = l1_out[i] / 64;
        int32_t activated = std::clamp<int32_t>(val, 0, 127);
        out += activated * static_cast<int32_t>(net_weights.output_weights[i]);
    }

    int score = out / 16; 
    return score;
}

} // namespace NNUE
} // namespace Bully