/**
 * @file nnue.h
 * @brief Efficiently Updatable Neural Network (NNUE) Evaluation Architecture.
 *
 * Defines HalfKP 256x2-32-32-1 NNUE evaluation data structures, 64-byte aligned accumulators,
 * feature indexing relative to King square perspectives, and binary net loading functions (.nnue).
 */

#pragma once

#include <string>
#include <string_view>
#include <array>
#include <memory>
#include <cstdint>

#include "types.h"

namespace Bully {

class Position; // Forward declaration

namespace NNUE {

// ============================================================================
// NNUE Architecture Constants (HalfKP 256x2-32-32-1)
// ============================================================================
constexpr size_t TRANSFORMER_HALF_DIM  = 256;
constexpr size_t L1_DIM                 = 32;
constexpr size_t PIECE_SQUARE_FEATURES  = 64 * 10; // 10 non-king piece types * 64 squares = 640
constexpr size_t HALFKP_FEATURES        = 64 * PIECE_SQUARE_FEATURES; // 64 king squares * 640 = 40,960
constexpr uint32_t NNUE_VERSION         = 0x7AF32F16; // Standard HalfKP NNUE version magic

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

// Compute feature index for a piece and king square relative to perspective
[[nodiscard]] constexpr size_t feature_index(Color perspective, Square ksq, Piece pc, Square sq) {
    Color pc_color = color_of(pc);
    PieceType pt = type_of(pc);
    
    // Relative squares for perspective symmetry
    Square rel_ksq = relative_square(perspective, ksq);
    Square rel_sq  = relative_square(perspective, sq);
    
    size_t color_offset = (pc_color == perspective) ? 0 : 1;
    size_t piece_idx = color_offset * 5 + (to_index(pt) - 1);
    size_t piece_sq_idx = piece_idx * 64 + to_index(rel_sq);
    return to_index(rel_ksq) * PIECE_SQUARE_FEATURES + piece_sq_idx;
}

// Compute full accumulator refresh for a position
void refresh_accumulator(const Position& pos, Accumulator& acc);

// High-performance NNUE static evaluation function
[[nodiscard]] Value evaluate(const Position& pos);

} // namespace NNUE
} // namespace Bully
