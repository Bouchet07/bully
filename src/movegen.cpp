#include "movegen.h"
#include "bitboard.h"
#include "attacks.h"

namespace Bully {

template<Color Us, bool CapturesOnly>
ExtMove* generate_all(const Position& pos, ExtMove* list) {
    constexpr Color Them = ~Us;

    const Bitboard friendly_pieces = pos.pieces(Us);
    const Bitboard enemy_pieces    = pos.pieces(Them);
    const Bitboard occupied_squares = pos.occupied();
    const Bitboard target_squares   = CapturesOnly ? enemy_pieces : ~friendly_pieces;

    // ------------------------------------------------------------------------
    // 1. Pawn Moves
    // ------------------------------------------------------------------------
    Bitboard pawns = pos.pieces(Us, PAWN);
    constexpr Rank promo_rank  = relative_rank(Us, RANK_8);

    constexpr Direction Up     = pawn_push(Us);
    constexpr Direction UpUp   = Up + Up;
    constexpr Direction UpWest = Up + WEST;
    constexpr Direction UpEast = Up + EAST;

    Bitboard empty_squares = ~occupied_squares;

    if constexpr (!CapturesOnly) {
        Bitboard single_pushes = shift<Up>(pawns) & empty_squares;
        Bitboard promo_pushes = single_pushes & rank_bb(promo_rank);
        Bitboard quiet_pushes = single_pushes & ~rank_bb(promo_rank);

        while (quiet_pushes) {
            Square to = lsb(quiet_pushes);
            quiet_pushes &= quiet_pushes - 1;
            Square from = to - Up;
            *list++ = ExtMove{Move(from, to), 0};
        }

        while (promo_pushes) {
            Square to = lsb(promo_pushes);
            promo_pushes &= promo_pushes - 1;
            Square from = to - Up;
            *list++ = ExtMove{Move::make<PROMOTION>(from, to, QUEEN), 0};
            *list++ = ExtMove{Move::make<PROMOTION>(from, to, ROOK), 0};
            *list++ = ExtMove{Move::make<PROMOTION>(from, to, BISHOP), 0};
            *list++ = ExtMove{Move::make<PROMOTION>(from, to, KNIGHT), 0};
        }

        constexpr Rank double_push_rank = (Us == WHITE) ? RANK_3 : RANK_6;
        Bitboard double_pushes = shift<Up>(single_pushes & rank_bb(double_push_rank)) & empty_squares;

        while (double_pushes) {
            Square to = lsb(double_pushes);
            double_pushes &= double_pushes - 1;
            Square from = to - UpUp;
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    Bitboard cap_left  = shift<UpWest>(pawns) & enemy_pieces;
    Bitboard cap_right = shift<UpEast>(pawns) & enemy_pieces;

    Bitboard promo_left = cap_left & rank_bb(promo_rank);
    Bitboard quiet_left = cap_left & ~rank_bb(promo_rank);

    Bitboard promo_right = cap_right & rank_bb(promo_rank);
    Bitboard quiet_right = cap_right & ~rank_bb(promo_rank);

    while (quiet_left) {
        Square to = lsb(quiet_left);
        quiet_left &= quiet_left - 1;
        Square from = to - UpWest;
        *list++ = ExtMove{Move(from, to), 0};
    }

    while (promo_left) {
        Square to = lsb(promo_left);
        promo_left &= promo_left - 1;
        Square from = to - UpWest;
        *list++ = ExtMove{Move::make<PROMOTION>(from, to, QUEEN), 0};
        *list++ = ExtMove{Move::make<PROMOTION>(from, to, ROOK), 0};
        *list++ = ExtMove{Move::make<PROMOTION>(from, to, BISHOP), 0};
        *list++ = ExtMove{Move::make<PROMOTION>(from, to, KNIGHT), 0};
    }

    while (quiet_right) {
        Square to = lsb(quiet_right);
        quiet_right &= quiet_right - 1;
        Square from = to - UpEast;
        *list++ = ExtMove{Move(from, to), 0};
    }

    while (promo_right) {
        Square to = lsb(promo_right);
        promo_right &= promo_right - 1;
        Square from = to - UpEast;
        *list++ = ExtMove{Move::make<PROMOTION>(from, to, QUEEN), 0};
        *list++ = ExtMove{Move::make<PROMOTION>(from, to, ROOK), 0};
        *list++ = ExtMove{Move::make<PROMOTION>(from, to, BISHOP), 0};
        *list++ = ExtMove{Move::make<PROMOTION>(from, to, KNIGHT), 0};
    }

    Square ep_sq = pos.en_passant_square();
    if (ep_sq != SQ_NONE) {
        Bitboard ep_attackers = pawn_attacks(Them, ep_sq) & pawns;
        while (ep_attackers) {
            Square from = lsb(ep_attackers);
            ep_attackers &= ep_attackers - 1;
            *list++ = ExtMove{Move::make<EN_PASSANT>(from, ep_sq), 0};
        }
    }

    // ------------------------------------------------------------------------
    // 2. Knight Moves
    // ------------------------------------------------------------------------
    Bitboard knights = pos.pieces(Us, KNIGHT);
    while (knights) {
        Square from = lsb(knights);
        knights &= knights - 1;
        Bitboard targets = knight_attacks(from) & target_squares;
        while (targets) {
            Square to = lsb(targets);
            targets &= targets - 1;
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    // ------------------------------------------------------------------------
    // 3. Bishop & Queen Moves (Diagonal Sliders)
    // ------------------------------------------------------------------------
    Bitboard diagonal_sliders = pos.pieces(Us, BISHOP) | pos.pieces(Us, QUEEN);
    while (diagonal_sliders) {
        Square from = lsb(diagonal_sliders);
        diagonal_sliders &= diagonal_sliders - 1;
        Bitboard targets = bishop_attacks(from, occupied_squares) & target_squares;
        while (targets) {
            Square to = lsb(targets);
            targets &= targets - 1;
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    // ------------------------------------------------------------------------
    // 4. Rook & Queen Moves (Orthogonal Sliders)
    // ------------------------------------------------------------------------
    Bitboard straight_sliders = pos.pieces(Us, ROOK) | pos.pieces(Us, QUEEN);
    while (straight_sliders) {
        Square from = lsb(straight_sliders);
        straight_sliders &= straight_sliders - 1;
        Bitboard targets = rook_attacks(from, occupied_squares) & target_squares;
        while (targets) {
            Square to = lsb(targets);
            targets &= targets - 1;
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    // ------------------------------------------------------------------------
    // 5. King Moves & Castling
    // ------------------------------------------------------------------------
    Square king_from = pos.king_square(Us);
    Bitboard king_targets = king_attacks(king_from) & target_squares;
    while (king_targets) {
        Square to = lsb(king_targets);
        king_targets &= king_targets - 1;
        *list++ = ExtMove{Move(king_from, to), 0};
    }

    if constexpr (!CapturesOnly) {
        if (!pos.in_check()) {
            if constexpr (Us == WHITE) {
                if (pos.castling_rights() & WHITE_OO) {
                    if (pos.piece_on(SQ_F1) == NO_PIECE && pos.piece_on(SQ_G1) == NO_PIECE) {
                        if (!pos.attacked(SQ_F1, BLACK) && !pos.attacked(SQ_G1, BLACK)) {
                            *list++ = ExtMove{Move::make<CASTLING>(SQ_E1, SQ_G1), 0};
                        }
                    }
                }
                if (pos.castling_rights() & WHITE_OOO) {
                    if (pos.piece_on(SQ_B1) == NO_PIECE && pos.piece_on(SQ_C1) == NO_PIECE && pos.piece_on(SQ_D1) == NO_PIECE) {
                        if (!pos.attacked(SQ_C1, BLACK) && !pos.attacked(SQ_D1, BLACK)) {
                            *list++ = ExtMove{Move::make<CASTLING>(SQ_E1, SQ_C1), 0};
                        }
                    }
                }
            } else {
                if (pos.castling_rights() & BLACK_OO) {
                    if (pos.piece_on(SQ_F8) == NO_PIECE && pos.piece_on(SQ_G8) == NO_PIECE) {
                        if (!pos.attacked(SQ_F8, WHITE) && !pos.attacked(SQ_G8, WHITE)) {
                            *list++ = ExtMove{Move::make<CASTLING>(SQ_E8, SQ_G8), 0};
                        }
                    }
                }
                if (pos.castling_rights() & BLACK_OOO) {
                    if (pos.piece_on(SQ_B8) == NO_PIECE && pos.piece_on(SQ_C8) == NO_PIECE && pos.piece_on(SQ_D8) == NO_PIECE) {
                        if (!pos.attacked(SQ_C8, WHITE) && !pos.attacked(SQ_D8, WHITE)) {
                            *list++ = ExtMove{Move::make<CASTLING>(SQ_E8, SQ_C8), 0};
                        }
                    }
                }
            }
        }
    }

    return list;
}

void MoveList::generate(const Position& pos) {
    if (pos.side_to_move() == WHITE) {
        last = generate_all<WHITE, false>(pos, list.data());
    } else {
        last = generate_all<BLACK, false>(pos, list.data());
    }
}

void MoveList::generate_captures(const Position& pos) {
    if (pos.side_to_move() == WHITE) {
        last = generate_all<WHITE, true>(pos, list.data());
    } else {
        last = generate_all<BLACK, true>(pos, list.data());
    }
}

} // namespace Bully
