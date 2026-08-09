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
constexpr size_t TransformerHalfDim  = 256;
constexpr size_t L1Dim               = 32;
constexpr size_t PieceSquareFeatures = 64 * 10; // 10 non-king piece types * 64 squares = 640
constexpr size_t HalfKPFeatures      = 64 * PieceSquareFeatures; // 64 king squares * 640 = 40,960
constexpr uint32_t NnueVersionMagic  = 0x7AF32F16; // Standard HalfKP NNUE version magic

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

// SIMD 64-byte Cache-Aligned Accumulator for 256 16-bit values per perspective
struct alignas(64) Accumulator {
    std::array<int16_t, TransformerHalfDim> white;
    std::array<int16_t, TransformerHalfDim> black;
    std::array<bool, 2> computed = {false, false};
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

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
    return to_index(rel_ksq) * PieceSquareFeatures + piece_sq_idx;
}

// Compute full accumulator refresh for a position
void refresh_accumulator(const Position& pos, Accumulator& acc);

// High-performance NNUE static evaluation function
[[nodiscard]] Value evaluate(const Position& pos);

} // namespace NNUE
} // namespace Bully