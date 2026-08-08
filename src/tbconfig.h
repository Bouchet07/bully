#pragma once

#include <cstdint>
#include <utility>
#include "types.h"
#include "bitboard.h"
#include "attacks.h"

#define PYRRHIC_POPCOUNT(x)              (Bully::popcnt(x))
#define PYRRHIC_LSB(x)                   (std::to_underlying(Bully::lsb(x)))
#define PYRRHIC_POPLSB(x)                (std::to_underlying(Bully::pop_lsb(*(x))))

#define PYRRHIC_PAWN_ATTACKS(sq, c)      (Bully::pawn_attacks(~static_cast<Bully::Color>(c), static_cast<Bully::Square>(sq)))
#define PYRRHIC_KNIGHT_ATTACKS(sq)       (Bully::knight_attacks(static_cast<Bully::Square>(sq)))
#define PYRRHIC_BISHOP_ATTACKS(sq, occ)  (Bully::bishop_attacks(static_cast<Bully::Square>(sq), occ))
#define PYRRHIC_ROOK_ATTACKS(sq, occ)    (Bully::rook_attacks(static_cast<Bully::Square>(sq), occ))
#define PYRRHIC_QUEEN_ATTACKS(sq, occ)   (Bully::queen_attacks(static_cast<Bully::Square>(sq), occ))
#define PYRRHIC_KING_ATTACKS(sq)         (Bully::king_attacks(static_cast<Bully::Square>(sq)))
