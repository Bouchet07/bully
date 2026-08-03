/**
 * @file attacks.h
 * @brief Precomputed Attack Tables and Slider Attack Generators.
 *
 * Efficient precomputed move attack lookups for non-sliding pieces (Pawns, Knights, Kings)
 * and sliding pieces (Rooks, Bishops, Queens). Sliding piece attacks utilize BMI2 PEXT
 * (Parallel Bit Extract) instructions when hardware support is enabled (`USE_PEXT`), falling
 * back to portable Magic Bitboards on non-BMI2 architectures (e.g. ARM64).
 */

#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
#endif
#include <array>
#include <utility>
#include "types.h"
#include "bitboard.h"

// Check if BMI2 PEXT is supported
#if !defined(USE_PEXT) && defined(__BMI2__)
    #define USE_PEXT
#endif

namespace Bully {

// Non-sliding attack tables
extern std::array<Bitboard, SQUARE_NB> KnightAttacks;
extern std::array<Bitboard, SQUARE_NB> KingAttacks;
extern std::array<std::array<Bitboard, SQUARE_NB>, COLOR_NB> PawnAttacks;

// Masks used for occupancy extraction
extern std::array<Bitboard, SQUARE_NB> RookMasks;
extern std::array<Bitboard, SQUARE_NB> BishopMasks;

// Initialize all attack tables (sliding and non-sliding)
void init_attacks();

// Non-sliding attacks inline accessors
[[nodiscard]] inline Bitboard knight_attacks(Square s) { 
    return KnightAttacks[to_index(s)]; 
}

[[nodiscard]] inline Bitboard king_attacks(Square s) { 
    return KingAttacks[to_index(s)]; 
}

[[nodiscard]] inline Bitboard pawn_attacks(Color c, Square s) { 
    return PawnAttacks[to_index(c)][to_index(s)]; 
}

#ifdef USE_PEXT
// PEXT-based sliding attacks (ultra-fast)
extern std::array<std::array<Bitboard, 4096>, SQUARE_NB> RookAttacks;
extern std::array<std::array<Bitboard, 512>, SQUARE_NB> BishopAttacks;

[[nodiscard]] inline Bitboard rook_attacks(Square s, Bitboard occ) {
    size_t idx = to_index(s);
    return RookAttacks[idx][_pext_u64(occ, RookMasks[idx])];
}

[[nodiscard]] inline Bitboard bishop_attacks(Square s, Bitboard occ) {
    size_t idx = to_index(s);
    return BishopAttacks[idx][_pext_u64(occ, BishopMasks[idx])];
}

#else
// Magic-based sliding attacks (highly portable fallback)
extern std::array<Bitboard, 0x19000> RookAttackTable;
extern std::array<Bitboard, 0x1480> BishopAttackTable;

extern std::array<uint64_t, SQUARE_NB> RookMagics;
extern std::array<uint64_t, SQUARE_NB> BishopMagics;

extern std::array<uint8_t, SQUARE_NB> RookShifts;
extern std::array<uint8_t, SQUARE_NB> BishopShifts;

extern std::array<int, SQUARE_NB> RookOffsets;
extern std::array<int, SQUARE_NB> BishopOffsets;

[[nodiscard]] inline Bitboard rook_attacks(Square s, Bitboard occ) {
    size_t idx = to_index(s);
    Bitboard m = RookMasks[idx];
    uint64_t key = ((occ & m) * RookMagics[idx]) >> RookShifts[idx];
    return RookAttackTable[static_cast<size_t>(RookOffsets[idx]) + key];
}

[[nodiscard]] inline Bitboard bishop_attacks(Square s, Bitboard occ) {
    size_t idx = to_index(s);
    Bitboard m = BishopMasks[idx];
    uint64_t key = ((occ & m) * BishopMagics[idx]) >> BishopShifts[idx];
    return BishopAttackTable[static_cast<size_t>(BishopOffsets[idx]) + key];
}
#endif

// Queen attacks are simply rook + bishop attacks
[[nodiscard]] inline Bitboard queen_attacks(Square s, Bitboard occ) {
    return rook_attacks(s, occ) | bishop_attacks(s, occ);
}

} // namespace Bully
