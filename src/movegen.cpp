#include "movegen.h"
#include "bitboard.h"
#include "attacks.h"

namespace Bully {

template<Color Us, GenType Type>
ExtMove* generate_all(const Position& pos, ExtMove* list) {
    constexpr Color Them = ~Us;

    const Bitboard friendly_pieces  = pos.pieces(Us);
    const Bitboard enemy_pieces     = pos.pieces(Them);
    const Bitboard occupied_squares = pos.occupied();

    Bitboard check_mask = ~0ULL;
    Bitboard chk = pos.checkers(Us);
    if (chk) {
        if (more_than_one(chk)) {
            check_mask = 0ULL; // Double check requires King move
        } else {
            Square checker_sq = lsb(chk);
            Square ksq = pos.king_square(Us);
            check_mask = square_bb(checker_sq) | (BetweenBB[to_index(ksq)][to_index(checker_sq)] ^ square_bb(checker_sq));
        }
    }

    constexpr bool AllowQuiets   = (Type == GEN_ALL || Type == GEN_QUIETS);
    constexpr bool AllowCaptures = (Type == GEN_ALL || Type == GEN_CAPTURES);

    const Bitboard target_squares = ((Type == GEN_CAPTURES) ? enemy_pieces :
                                     (Type == GEN_QUIETS)   ? ~occupied_squares :
                                                              ~friendly_pieces) & check_mask;

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

    if constexpr (AllowQuiets) {
        Bitboard single_pushes = shift<Up>(pawns) & empty_squares;
        Bitboard promo_pushes = single_pushes & rank_bb(promo_rank) & check_mask;
        Bitboard quiet_pushes = single_pushes & ~rank_bb(promo_rank) & check_mask;

        while (quiet_pushes) {
            Square to = pop_lsb(quiet_pushes);
            Square from = to - Up;
            *list++ = ExtMove{Move(from, to), 0};
        }

        if constexpr (AllowCaptures) {
            while (promo_pushes) {
                Square to = lsb(promo_pushes);
                promo_pushes &= promo_pushes - 1;
                Square from = to - Up;
                *list++ = ExtMove{Move::make<PROMOTION>(from, to, QUEEN), 0};
                *list++ = ExtMove{Move::make<PROMOTION>(from, to, ROOK), 0};
                *list++ = ExtMove{Move::make<PROMOTION>(from, to, BISHOP), 0};
                *list++ = ExtMove{Move::make<PROMOTION>(from, to, KNIGHT), 0};
            }
        }

        constexpr Rank double_push_rank = (Us == WHITE) ? RANK_3 : RANK_6;
        Bitboard double_pushes = shift<Up>(single_pushes & rank_bb(double_push_rank)) & empty_squares & check_mask;

        while (double_pushes) {
            Square to = pop_lsb(double_pushes);
            Square from = to - UpUp;
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    if constexpr (AllowCaptures) {
        Bitboard cap_left  = shift<UpWest>(pawns) & enemy_pieces & check_mask;
        Bitboard cap_right = shift<UpEast>(pawns) & enemy_pieces & check_mask;

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
            Square to = pop_lsb(quiet_right);
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

        // En Passant Captures
        Square ep_sq = pos.en_passant_square();
        if (ep_sq != SQ_NONE) {
            Square cap_sq = ep_sq - Up;
            Bitboard ep_pawns = pawns & pawn_attacks(~Us, ep_sq);
            while (ep_pawns) {
                Square from = pop_lsb(ep_pawns);

                if ((square_bb(cap_sq) & check_mask) || (square_bb(ep_sq) & check_mask)) {
                    *list++ = ExtMove{Move::make<EN_PASSANT>(from, ep_sq), 0};
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // 2. Piece Moves (Knights, Bishops, Rooks, Queens)
    // ------------------------------------------------------------------------
    Bitboard knights = pos.pieces(Us, KNIGHT);
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard targets = knight_attacks(from) & target_squares;
        while (targets) {
            Square to = pop_lsb(targets);
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    Bitboard bishops = pos.pieces(Us, BISHOP);
    while (bishops) {
        Square from = pop_lsb(bishops);
        Bitboard targets = bishop_attacks(from, occupied_squares) & target_squares;
        while (targets) {
            Square to = pop_lsb(targets);
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    Bitboard rooks = pos.pieces(Us, ROOK);
    while (rooks) {
        Square from = pop_lsb(rooks);
        Bitboard targets = rook_attacks(from, occupied_squares) & target_squares;
        while (targets) {
            Square to = pop_lsb(targets);
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    Bitboard queens = pos.pieces(Us, QUEEN);
    while (queens) {
        Square from = pop_lsb(queens);
        Bitboard targets = queen_attacks(from, occupied_squares) & target_squares;
        while (targets) {
            Square to = pop_lsb(targets);
            *list++ = ExtMove{Move(from, to), 0};
        }
    }

    // ------------------------------------------------------------------------
    // 3. King Moves & Castling
    // ------------------------------------------------------------------------
    Square king_from = pos.king_square(Us);
    Bitboard king_targets = king_attacks(king_from) & (Type == GEN_CAPTURES ? enemy_pieces : Type == GEN_QUIETS ? ~occupied_squares : ~friendly_pieces);
    while (king_targets) {
        Square to = lsb(king_targets);
        king_targets &= king_targets - 1;
        *list++ = ExtMove{Move(king_from, to), 0};
    }

    if constexpr (AllowQuiets) {
        if (!pos.in_check()) {
            CastlingRights oo = (Us == WHITE) ? WHITE_OO : BLACK_OO;
            CastlingRights ooo = (Us == WHITE) ? WHITE_OOO : BLACK_OOO;

            if (pos.castling_rights() & oo) {
                Square rook_sq = pos.castling_rook_square(oo);
                if (rook_sq != SQ_NONE && !pos.castling_impeded(oo)) {
                    *list++ = ExtMove{Move::make<CASTLING>(king_from, rook_sq), 0};
                }
            }
            if (pos.castling_rights() & ooo) {
                Square rook_sq = pos.castling_rook_square(ooo);
                if (rook_sq != SQ_NONE && !pos.castling_impeded(ooo)) {
                    *list++ = ExtMove{Move::make<CASTLING>(king_from, rook_sq), 0};
                }
            }
        }
    }

    return list;
}

void MoveList::generate(const Position& pos) {
    if (pos.side_to_move() == WHITE) {
        last = generate_all<WHITE, GEN_ALL>(pos, list.data());
    } else {
        last = generate_all<BLACK, GEN_ALL>(pos, list.data());
    }
}

void MoveList::generate_legal(const Position& pos) {
    generate(pos);
    ExtMove* cur = list.data();
    ExtMove* end = last;
    last = list.data();

    Color us = pos.side_to_move();
    Bitboard pinned = pos.blockers_for_king(us);
    Square ksq = pos.king_square(us);

    for (ExtMove* p = cur; p < end; ++p) {
        Move m = p->move;
        MoveType type = m.type_of();
        Square from = m.from_sq();

        if (type == EN_PASSANT || type == CASTLING || type_of(pos.piece_on(from)) == KING) {
            if (pos.legal(m, pinned)) {
                *last++ = *p;
            }
        } else {
            if (!(pinned & square_bb(from)) || aligned(from, m.to_sq(), ksq)) {
                *last++ = *p;
            }
        }
    }
}

void MoveList::generate_captures(const Position& pos) {
    if (pos.side_to_move() == WHITE) {
        last = generate_all<WHITE, GEN_CAPTURES>(pos, list.data());
    } else {
        last = generate_all<BLACK, GEN_CAPTURES>(pos, list.data());
    }
}

void MoveList::generate_quiets(const Position& pos) {
    if (pos.side_to_move() == WHITE) {
        last = generate_all<WHITE, GEN_QUIETS>(pos, list.data());
    } else {
        last = generate_all<BLACK, GEN_QUIETS>(pos, list.data());
    }
}

} // namespace Bully
