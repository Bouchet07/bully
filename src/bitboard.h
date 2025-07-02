#pragma once

#include <array>
#include <bit>

#include "types.h"

// Returns the number of bits set in a bitboard.
constexpr uint8_t popcnt(Bitboard b) {
    return static_cast<uint8_t>(std::popcount(b));
}

// Returns the index of the least significant bit set in a bitboard.
// If the bitboard is empty, returns SQ_NONE.
constexpr Square get_LSB(Bitboard b) {
    return b ? Square(std::countr_zero(b)) : SQ_NONE;
}

