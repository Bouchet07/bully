#pragma once

#include <string>
#include <string_view>
#include <array>
#include <memory>
#include <cstdint>

#include "types.h"
#include "position.h"

namespace Bully {
namespace NNUE {

// ============================================================================
// NNUE Architecture Constants (HalfKP 256x2-32-32-1)
// ============================================================================
constexpr size_t TRANSFORMER_HALF_DIM = 256;
constexpr size_t PIECE_SQUARE_FEATURES = 64 * 10; // 10 non-king piece types * 64 squares = 640
constexpr size_t HALFKP_FEATURES = 64 * PIECE_SQUARE_FEATURES; // 64 king squares * 640 = 40,960

// SIMD 64-byte Cache-Aligned Accumulator for 256 16-bit values per perspective
struct alignas(64) Accumulator {
    std::array<int16_t, TRANSFORMER_HALF_DIM> white;
    std::array<int16_t, TRANSFORMER_HALF_DIM> black;
    std::array<bool, 2> computed = {false, false};
};

// Global NNUE Configuration Options
extern bool use_nnue;
extern std::string eval_file;

// Initialize NNUE evaluation subsystem
void init();

// Load binary NNUE weights file (.nnue)
[[nodiscard]] bool load_net(const std::string& path);

// Check if NNUE network is currently loaded and ready
[[nodiscard]] bool is_ready();

// High-performance NNUE static evaluation function
[[nodiscard]] Value evaluate(const Position& pos);

} // namespace NNUE
} // namespace Bully
