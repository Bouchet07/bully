#pragma once

#include <bit>
#include <cstdint>
#include "types.h"
#include "attacks.h"

#define PYRRHIC_POPCOUNT(x)              (std::popcount(static_cast<uint64_t>(x)))
#define PYRRHIC_LSB(x)                   (std::countr_zero(static_cast<uint64_t>(x)))
#define PYRRHIC_POPLSB(x)                (pyrrhic_poplsb(x))

inline int pyrrhic_poplsb(uint64_t* bb) {
    int lsb = std::countr_zero(*bb);
    *bb &= *bb - 1;
    return lsb;
}

#define PYRRHIC_PAWN_ATTACKS(sq, c)      (Bully::pawn_attacks(static_cast<Bully::Color>(1 - (c)), static_cast<Bully::Square>(sq)))
#define PYRRHIC_KNIGHT_ATTACKS(sq)       (Bully::knight_attacks(static_cast<Bully::Square>(sq)))
#define PYRRHIC_BISHOP_ATTACKS(sq, occ)  (Bully::bishop_attacks(static_cast<Bully::Square>(sq), occ))
#define PYRRHIC_ROOK_ATTACKS(sq, occ)    (Bully::rook_attacks(static_cast<Bully::Square>(sq), occ))
#define PYRRHIC_QUEEN_ATTACKS(sq, occ)   (Bully::queen_attacks(static_cast<Bully::Square>(sq), occ))
#define PYRRHIC_KING_ATTACKS(sq)         (Bully::king_attacks(static_cast<Bully::Square>(sq)))
