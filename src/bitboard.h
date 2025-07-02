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

void pretty_print(Bitboard bitboard, bool Use_UTF8);

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

constexpr Bitboard square_bb(Square s) { return (1ULL << s); }

// Overloads of bitwise operators between a Bitboard and a Square for testing
// whether a given bit is set in a bitboard, and for setting and clearing bits.

constexpr Bitboard  operator&(Bitboard b, Square s) { return b & square_bb(s); }
constexpr Bitboard  operator|(Bitboard b, Square s) { return b | square_bb(s); }
constexpr Bitboard  operator^(Bitboard b, Square s) { return b ^ square_bb(s); }
constexpr Bitboard& operator|=(Bitboard& b, Square s) { return b |= square_bb(s); }
constexpr Bitboard& operator^=(Bitboard& b, Square s) { return b ^= square_bb(s); }

constexpr Bitboard operator&(Square s, Bitboard b) { return b & s; }
constexpr Bitboard operator|(Square s, Bitboard b) { return b | s; }
constexpr Bitboard operator^(Square s, Bitboard b) { return b ^ s; }

constexpr Bitboard operator|(Square s1, Square s2) { return square_bb(s1) | s2; }

constexpr bool more_than_one(Bitboard b) { return b & (b - 1); }

// rank_bb() and file_bb() return a bitboard representing all the squares on
// the given file or rank.

constexpr Bitboard rank_bb(Rank r) { return Rank1BB << (8 * r); }

constexpr Bitboard rank_bb(Square s) { return rank_bb(rank_of(s)); }

constexpr Bitboard file_bb(File f) { return FileABB << f; }

constexpr Bitboard file_bb(Square s) { return file_bb(file_of(s)); }


// Moves a bitboard one or two steps as specified by the direction D
template<Direction D>
constexpr Bitboard shift(Bitboard b) {
    return D == NORTH ? b << 8
        : D == SOUTH ? b >> 8
        : D == NORTH + NORTH ? b << 16
        : D == SOUTH + SOUTH ? b >> 16
        : D == EAST ? (b & ~FileHBB) << 1
        : D == WEST ? (b & ~FileABB) >> 1
        : D == NORTH_EAST ? (b & ~FileHBB) << 9
        : D == NORTH_WEST ? (b & ~FileABB) << 7
        : D == SOUTH_EAST ? (b & ~FileHBB) >> 7
        : D == SOUTH_WEST ? (b & ~FileABB) >> 9
        : 0;
}