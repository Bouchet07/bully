#include "nnue.h"
#include <iostream>
#include <fstream>
#include <format>
#include <algorithm>
#include <vector>

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

// Dummy weights structure placeholder for Phase 1
struct NetWeights {
    alignas(64) std::array<int16_t, TRANSFORMER_HALF_DIM> feature_bias{};
} static net_weights;

void init() {
    net_loaded = false;
}

bool load_net(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        net_loaded = false;
        return false;
    }

    // Phase 1 stub for network validation & binary loading
    eval_file = path;
    net_loaded = true;
    return true;
}

bool is_ready() {
    return net_loaded;
}

Value evaluate(const Position& pos) {
    if (!use_nnue || !net_loaded) {
        return VALUE_ZERO;
    }

    // Phase 1 evaluation stub - will execute SIMD forward pass in Phase 2/3
    int score = 0;
    return (pos.side_to_move() == WHITE) ? static_cast<Value>(score) : static_cast<Value>(-score);
}

} // namespace NNUE
} // namespace Bully
