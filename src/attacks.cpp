#include <random>
#include <vector>
#include <algorithm>
#include "attacks.h"

namespace Bully {

// Non-sliding attack tables
std::array<Bitboard, SQUARE_NB> KnightAttacks;
std::array<Bitboard, SQUARE_NB> KingAttacks;
std::array<std::array<Bitboard, SQUARE_NB>, COLOR_NB> PawnAttacks;

// Masks
std::array<Bitboard, SQUARE_NB> RookMasks;
std::array<Bitboard, SQUARE_NB> BishopMasks;

#ifdef USE_PEXT
// PEXT sliding attack tables
std::array<std::array<Bitboard, 4096>, SQUARE_NB> RookAttacks;
std::array<std::array<Bitboard, 512>, SQUARE_NB> BishopAttacks;
#else
// Magic sliding attack tables fallback
std::array<Bitboard, 0x19000> RookAttackTable;
std::array<Bitboard, 0x1480> BishopAttackTable;

std::array<uint64_t, SQUARE_NB> RookMagics;
std::array<uint64_t, SQUARE_NB> BishopMagics;

std::array<uint8_t, SQUARE_NB> RookShifts;
std::array<uint8_t, SQUARE_NB> BishopShifts;

std::array<int, SQUARE_NB> RookOffsets;
std::array<int, SQUARE_NB> BishopOffsets;
#endif

// On-the-fly generators for initialization
Bitboard rook_mask(Square s) {
    Bitboard mask = 0;
    int r = std::to_underlying(rank_of(s));
    int f = std::to_underlying(file_of(s));
    for (int nr = r + 1; nr < 7; ++nr) mask |= square_bb(make_square(static_cast<File>(f), static_cast<Rank>(nr)));
    for (int nr = r - 1; nr > 0; --nr) mask |= square_bb(make_square(static_cast<File>(f), static_cast<Rank>(nr)));
    for (int nf = f + 1; nf < 7; ++nf) mask |= square_bb(make_square(static_cast<File>(nf), static_cast<Rank>(r)));
    for (int nf = f - 1; nf > 0; --nf) mask |= square_bb(make_square(static_cast<File>(nf), static_cast<Rank>(r)));
    return mask;
}

Bitboard bishop_mask(Square s) {
    Bitboard mask = 0;
    int r = std::to_underlying(rank_of(s));
    int f = std::to_underlying(file_of(s));
    for (int nr = r + 1, nf = f + 1; nr < 7 && nf < 7; ++nr, ++nf) mask |= square_bb(make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
    for (int nr = r - 1, nf = f + 1; nr > 0 && nf < 7; --nr, ++nf) mask |= square_bb(make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
    for (int nr = r + 1, nf = f - 1; nr < 7 && nf > 0; ++nr, --nf) mask |= square_bb(make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
    for (int nr = r - 1, nf = f - 1; nr > 0 && nf > 0; --nr, --nf) mask |= square_bb(make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
    return mask;
}

Bitboard rook_attacks_on_the_fly(Square s, Bitboard occ) {
    Bitboard attacks = 0;
    int r = std::to_underlying(rank_of(s));
    int f = std::to_underlying(file_of(s));
    
    // North
    for (int nr = r + 1; nr < 8; ++nr) {
        Square sq = make_square(static_cast<File>(f), static_cast<Rank>(nr));
        attacks |= square_bb(sq);
        if (occ & sq) break;
    }
    // South
    for (int nr = r - 1; nr >= 0; --nr) {
        Square sq = make_square(static_cast<File>(f), static_cast<Rank>(nr));
        attacks |= square_bb(sq);
        if (occ & sq) break;
    }
    // East
    for (int nf = f + 1; nf < 8; ++nf) {
        Square sq = make_square(static_cast<File>(nf), static_cast<Rank>(r));
        attacks |= square_bb(sq);
        if (occ & sq) break;
    }
    // West
    for (int nf = f - 1; nf >= 0; --nf) {
        Square sq = make_square(static_cast<File>(nf), static_cast<Rank>(r));
        attacks |= square_bb(sq);
        if (occ & sq) break;
    }
    return attacks;
}

Bitboard bishop_attacks_on_the_fly(Square s, Bitboard occ) {
    Bitboard attacks = 0;
    int r = std::to_underlying(rank_of(s));
    int f = std::to_underlying(file_of(s));
    
    // NE
    for (int nr = r + 1, nf = f + 1; nr < 8 && nf < 8; ++nr, ++nf) {
        Square sq = make_square(static_cast<File>(nf), static_cast<Rank>(nr));
        attacks |= square_bb(sq);
        if (occ & sq) break;
    }
    // SE
    for (int nr = r - 1, nf = f + 1; nr >= 0 && nf < 8; --nr, ++nf) {
        Square sq = make_square(static_cast<File>(nf), static_cast<Rank>(nr));
        attacks |= square_bb(sq);
        if (occ & sq) break;
    }
    // NW
    for (int nr = r + 1, nf = f - 1; nr < 8 && nf >= 0; ++nr, --nf) {
        Square sq = make_square(static_cast<File>(nf), static_cast<Rank>(nr));
        attacks |= square_bb(sq);
        if (occ & sq) break;
    }
    // SW
    for (int nr = r - 1, nf = f - 1; nr >= 0 && nf >= 0; --nr, --nf) {
        Square sq = make_square(static_cast<File>(nf), static_cast<Rank>(nr));
        attacks |= square_bb(sq);
        if (occ & sq) break;
    }
    return attacks;
}

Bitboard reconstruct_occupancy(int index, Bitboard mask) {
    Bitboard occ = 0;
    int bit_count = popcnt(mask);
    for (int j = 0; j < bit_count; ++j) {
        Square sq = get_LSB(mask);
        mask ^= sq;
        if (index & (1 << j)) {
            occ |= sq;
        }
    }
    return occ;
}

// Generate a random 64-bit integer with a low number of bits set (good for magic candidates)
uint64_t random_uint64_sparse(std::mt19937_64& rng) {
    return rng() & rng() & rng();
}

#ifndef USE_PEXT
// Find a magic number for a square and piece type at startup
uint64_t find_magic(Square s, bool is_rook, Bitboard mask, int shift, Bitboard* attack_table_dest) {
    std::mt19937_64 rng(10283921ULL ^ to_index(s) ^ (is_rook ? 1ULL : 0ULL));
    int num_bits = popcnt(mask);
    int num_occupancies = 1 << num_bits;

    std::vector<Bitboard> occupancies(static_cast<size_t>(num_occupancies));
    std::vector<Bitboard> attacks(static_cast<size_t>(num_occupancies));
    for (int i = 0; i < num_occupancies; ++i) {
        occupancies[static_cast<size_t>(i)] = reconstruct_occupancy(i, mask);
        attacks[static_cast<size_t>(i)] = is_rook ? rook_attacks_on_the_fly(s, occupancies[static_cast<size_t>(i)]) 
                                                  : bishop_attacks_on_the_fly(s, occupancies[static_cast<size_t>(i)]);
    }

    std::vector<Bitboard> test_table(static_cast<size_t>(num_occupancies));
    while (true) {
        uint64_t candidate = random_uint64_sparse(rng);
        if (popcnt((candidate * mask) & 0xFF00000000000000ULL) < 6) continue;

        std::fill(test_table.begin(), test_table.end(), 0);
        bool fail = false;
        for (int i = 0; i < num_occupancies; ++i) {
            uint64_t key = ((occupancies[static_cast<size_t>(i)] & mask) * candidate) >> shift;
            if (test_table[static_cast<size_t>(key)] != 0 && test_table[static_cast<size_t>(key)] != attacks[static_cast<size_t>(i)]) {
                fail = true;
                break;
            }
            test_table[static_cast<size_t>(key)] = attacks[static_cast<size_t>(i)];
        }

        if (!fail) {
            for (int i = 0; i < num_occupancies; ++i) {
                uint64_t key = ((occupancies[static_cast<size_t>(i)] & mask) * candidate) >> shift;
                attack_table_dest[key] = attacks[static_cast<size_t>(i)];
            }
            return candidate;
        }
    }
}
#endif

void init_attacks() {
    // 1. Initialize Pawns, Knights, Kings
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        Bitboard b = square_bb(s);

        // Knight attacks
        Bitboard knight = 0;
        knight |= shift<NORTH + NORTH + EAST>(b);
        knight |= shift<NORTH + NORTH + WEST>(b);
        knight |= shift<SOUTH + SOUTH + EAST>(b);
        knight |= shift<SOUTH + SOUTH + WEST>(b);
        knight |= shift<NORTH + EAST + EAST>(b);
        knight |= shift<NORTH + WEST + WEST>(b);
        knight |= shift<SOUTH + EAST + EAST>(b);
        knight |= shift<SOUTH + WEST + WEST>(b);
        KnightAttacks[to_index(s)] = knight;

        // King attacks
        Bitboard king = 0;
        king |= shift<NORTH>(b) | shift<SOUTH>(b) | shift<EAST>(b) | shift<WEST>(b);
        king |= shift<NORTH_EAST>(b) | shift<NORTH_WEST>(b) | shift<SOUTH_EAST>(b) | shift<SOUTH_WEST>(b);
        KingAttacks[to_index(s)] = king;

        // Pawn attacks
        PawnAttacks[WHITE][to_index(s)] = shift<NORTH_EAST>(b) | shift<NORTH_WEST>(b);
        PawnAttacks[BLACK][to_index(s)] = shift<SOUTH_EAST>(b) | shift<SOUTH_WEST>(b);
    }

    // 2. Precompute masks
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        RookMasks[to_index(s)] = rook_mask(s);
        BishopMasks[to_index(s)] = bishop_mask(s);
    }

    // 3. Precompute sliding attacks
#ifdef USE_PEXT
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        size_t idx = to_index(s);
        Bitboard r_mask = RookMasks[idx];
        int r_bits = popcnt(r_mask);
        for (int i = 0; i < (1 << r_bits); ++i) {
            Bitboard occ = reconstruct_occupancy(i, r_mask);
            RookAttacks[idx][static_cast<size_t>(i)] = rook_attacks_on_the_fly(s, occ);
        }

        Bitboard b_mask = BishopMasks[idx];
        int b_bits = popcnt(b_mask);
        for (int i = 0; i < (1 << b_bits); ++i) {
            Bitboard occ = reconstruct_occupancy(i, b_mask);
            BishopAttacks[idx][static_cast<size_t>(i)] = bishop_attacks_on_the_fly(s, occ);
        }
    }
#else
    size_t rook_offset = 0;
    size_t bishop_offset = 0;

    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        size_t idx = to_index(s);
        RookOffsets[idx] = static_cast<int>(rook_offset);
        int r_bits = popcnt(RookMasks[idx]);
        RookShifts[idx] = static_cast<uint8_t>(64 - r_bits);
        RookMagics[idx] = find_magic(s, true, RookMasks[idx], RookShifts[idx], &RookAttackTable[rook_offset]);
        rook_offset += (1ULL << r_bits);

        BishopOffsets[idx] = static_cast<int>(bishop_offset);
        int b_bits = popcnt(BishopMasks[idx]);
        BishopShifts[idx] = static_cast<uint8_t>(64 - b_bits);
        BishopMagics[idx] = find_magic(s, false, BishopMasks[idx], BishopShifts[idx], &BishopAttackTable[bishop_offset]);
        bishop_offset += (1ULL << b_bits);
    }
#endif
}

} // namespace Bully
