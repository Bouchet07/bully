/**
 * @file bitboard.h
 * @brief Bitboard Operations, Hardware Intrinsics, and Geometric Lookups.
 *
 * Provides low-level bitboard manipulation tools utilizing C++20 standard hardware
 * intrinsics (<bit>), including POPCNT and TZCNT. Also defines file/rank bitmasks,
 * direction shift templates, and array-based lookup tables (LineBB, BetweenBB, SquareDistance).
 */

#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <utility>

#include "types.h"

namespace Bully {

// ============================================================================
// Core Bitboard Operations (C++20/C++23 Hardware Intrinsics)
// ============================================================================

// Returns the number of bits set in a bitboard (Hardware POPCNT).
[[nodiscard]] constexpr uint8_t popcnt(Bitboard b) {
    return static_cast<uint8_t>(std::popcount(b));
}

// Returns the index of the least significant bit set (Hardware TZCNT/BSF).
// If the bitboard is empty, returns SQ_NONE.
[[nodiscard]] constexpr Square get_LSB(Bitboard b) {
    return b ? static_cast<Square>(std::countr_zero(b)) : SQ_NONE;
}

// Returns the index of the least significant bit set (Hardware TZCNT/BSF), assuming the bitboard is not empty.
[[nodiscard]] constexpr Square lsb(Bitboard b) {
    return static_cast<Square>(std::countr_zero(b));
}

// Returns the index of the most significant bit set (Hardware LZCNT/BSR), assuming the bitboard is not empty.
[[nodiscard]] constexpr Square msb(Bitboard b) {
    return static_cast<Square>(63 - std::countl_zero(b));
}

// Returns the index of the least significant bit set and clears it from the bitboard.
[[nodiscard]] constexpr inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// ============================================================================
// File & Rank Bitboard Masks
// ============================================================================
constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB = 0xFF;
constexpr Bitboard Rank2BB = Rank1BB << (8 * 1);
constexpr Bitboard Rank3BB = Rank1BB << (8 * 2);
constexpr Bitboard Rank4BB = Rank1BB << (8 * 3);
constexpr Bitboard Rank5BB = Rank1BB << (8 * 4);
constexpr Bitboard Rank6BB = Rank1BB << (8 * 5);
constexpr Bitboard Rank7BB = Rank1BB << (8 * 6);
constexpr Bitboard Rank8BB = Rank1BB << (8 * 7);

// ============================================================================
// Global Lookup Tables (encapsulated in std::array)
// ============================================================================
using SquareTable        = std::array<std::array<Bitboard, SQUARE_NB>, SQUARE_NB>;
using DistanceTable      = std::array<std::array<uint8_t, SQUARE_NB>, SQUARE_NB>;
extern DistanceTable      SquareDistance;
extern SquareTable        LineBB;
extern SquareTable        BetweenBB;

// Initialize core bitboard tables at startup
void init_bitboards();

// Diagnostic utility to print bitboard grid to console
void pretty_print(Bitboard bitboard, bool Use_UTF8);

// ============================================================================
// Bitboard Arithmetic & Conversions
// ============================================================================
[[nodiscard]] constexpr Bitboard square_bb(Square s) { 
    return (1ULL << std::to_underlying(s)); 
}

// Overloads between Bitboard and Square for clean bit manipulation
[[nodiscard]] constexpr Bitboard  operator&(Bitboard b, Square s) { return b & square_bb(s); }
[[nodiscard]] constexpr Bitboard  operator|(Bitboard b, Square s) { return b | square_bb(s); }
[[nodiscard]] constexpr Bitboard  operator^(Bitboard b, Square s) { return b ^ square_bb(s); }
inline Bitboard& operator|=(Bitboard& b, Square s) { return b |= square_bb(s); }
inline Bitboard& operator^=(Bitboard& b, Square s) { return b ^= square_bb(s); }

[[nodiscard]] constexpr Bitboard operator&(Square s, Bitboard b) { return b & s; }
[[nodiscard]] constexpr Bitboard operator|(Square s, Bitboard b) { return b | s; }
[[nodiscard]] constexpr Bitboard operator^(Square s, Bitboard b) { return b ^ s; }

[[nodiscard]] constexpr Bitboard operator|(Square s1, Square s2) { return square_bb(s1) | s2; }

[[nodiscard]] constexpr bool more_than_one(Bitboard b) { return (b & (b - 1)) != 0; }

[[nodiscard]] constexpr Bitboard rank_bb(Rank r) { return Rank1BB << (8 * std::to_underlying(r)); }
[[nodiscard]] constexpr Bitboard rank_bb(Square s) { return rank_bb(rank_of(s)); }

[[nodiscard]] constexpr Bitboard file_bb(File f) { return FileABB << std::to_underlying(f); }
[[nodiscard]] constexpr Bitboard file_bb(Square s) { return file_bb(file_of(s)); }

// Shifts a bitboard in direction D (with wrapping checks)
template<Direction D>
[[nodiscard]] constexpr Bitboard shift(Bitboard b) {
    if constexpr (D == NORTH) return b << 8;
    else if constexpr (D == SOUTH) return b >> 8;
    else if constexpr (D == NORTH + NORTH) return b << 16;
    else if constexpr (D == SOUTH + SOUTH) return b >> 16;
    else if constexpr (D == EAST) return (b & ~FileHBB) << 1;
    else if constexpr (D == WEST) return (b & ~FileABB) >> 1;
    else if constexpr (D == NORTH_EAST) return (b & ~FileHBB) << 9;
    else if constexpr (D == NORTH_WEST) return (b & ~FileABB) << 7;
    else if constexpr (D == SOUTH_EAST) return (b & ~FileHBB) >> 7;
    else if constexpr (D == SOUTH_WEST) return (b & ~FileABB) >> 9;
    
    // Knight moves
    else if constexpr (D == NORTH + NORTH + EAST) return (b & ~FileHBB) << 17;
    else if constexpr (D == NORTH + NORTH + WEST) return (b & ~FileABB) << 15;
    else if constexpr (D == SOUTH + SOUTH + EAST) return (b & ~FileHBB) >> 15;
    else if constexpr (D == SOUTH + SOUTH + WEST) return (b & ~FileABB) >> 17;
    else if constexpr (D == NORTH + EAST + EAST)  return (b & ~(FileGBB | FileHBB)) << 10;
    else if constexpr (D == NORTH + WEST + WEST)  return (b & ~(FileABB | FileBBB)) << 6;
    else if constexpr (D == SOUTH + EAST + EAST)  return (b & ~(FileGBB | FileHBB)) >> 6;
    else if constexpr (D == SOUTH + WEST + WEST)  return (b & ~(FileABB | FileBBB)) >> 10;
    
    else return 0;
}

// ============================================================================
// Distance & Intersection Functions
// ============================================================================

[[nodiscard]] inline int distance(File f1, File f2) {
    return std::abs(std::to_underlying(f1) - std::to_underlying(f2));
}

[[nodiscard]] inline int distance(Rank r1, Rank r2) {
    return std::abs(std::to_underlying(r1) - std::to_underlying(r2));
}

[[nodiscard]] inline int distance(Square s1, Square s2) {
    return SquareDistance[to_index(s1)][to_index(s2)];
}

[[nodiscard]] inline uint8_t edge_distance(File f) {
    return static_cast<uint8_t>(std::min(
        static_cast<int>(std::to_underlying(f)),
        static_cast<int>(std::to_underlying(FILE_H)) - static_cast<int>(std::to_underlying(f))
    ));
}

[[nodiscard]] inline Bitboard line_bb(Square s1, Square s2) { 
    return LineBB[to_index(s1)][to_index(s2)]; 
}

[[nodiscard]] inline Bitboard between_bb(Square s1, Square s2) { 
    return BetweenBB[to_index(s1)][to_index(s2)]; 
}

[[nodiscard]] inline bool aligned(Square s1, Square s2, Square s3) { 
    return (line_bb(s1, s2) & square_bb(s3)) != 0; 
}

} // namespace Bully