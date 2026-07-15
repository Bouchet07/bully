#include "movegen.h"
#include "bitboard.h"
#include "attacks.h"

namespace Bully {

template<Color Us, bool CapturesOnly>
void generate_all(const Position& pos, MoveList& move_list) {
    constexpr Color Them = ~Us;

    const Bitboard friendly_pieces = pos.pieces(Us);
    const Bitboard enemy_pieces    = pos.pieces(Them);
    const Bitboard occupied_squares = pos.occupied();
    const Bitboard target_squares   = CapturesOnly ? enemy_pieces : ~friendly_pieces;

    // ------------------------------------------------------------------------
    // 1. Pawn Moves
    // ------------------------------------------------------------------------
    constexpr Direction up        = pawn_push(Us);
    constexpr Rank start_rank     = (Us == WHITE) ? RANK_2 : RANK_7;
    constexpr Rank promo_rank     = (Us == WHITE) ? RANK_8 : RANK_1;

    Bitboard pawns = pos.pieces(Us, PAWN);
    while (pawns) {
        Square from = get_LSB(pawns);
        pawns ^= square_bb(from);

        // A. Single & Double pushes (Quiet Moves)
        if constexpr (!CapturesOnly) {
            Square to = from + up;
            if (pos.piece_on(to) == NO_PIECE) {
                if (rank_of(to) == promo_rank) {
                    move_list.push(Move::make<PROMOTION>(from, to, QUEEN));
                    move_list.push(Move::make<PROMOTION>(from, to, ROOK));
                    move_list.push(Move::make<PROMOTION>(from, to, BISHOP));
                    move_list.push(Move::make<PROMOTION>(from, to, KNIGHT));
                } else {
                    move_list.push(Move(from, to));
                    
                    // Double push
                    if (rank_of(from) == start_rank) {
                        Square double_to = to + up;
                        if (pos.piece_on(double_to) == NO_PIECE) {
                            move_list.push(Move(from, double_to));
                        }
                    }
                }
            }
        }

        // B. Pawn Captures (Diagonal Attacks)
        Bitboard attacks = pawn_attacks(Us, from);
        Bitboard captures = attacks & enemy_pieces;
        while (captures) {
            Square to = get_LSB(captures);
            captures ^= square_bb(to);

            if (rank_of(to) == promo_rank) {
                move_list.push(Move::make<PROMOTION>(from, to, QUEEN));
                move_list.push(Move::make<PROMOTION>(from, to, ROOK));
                move_list.push(Move::make<PROMOTION>(from, to, BISHOP));
                move_list.push(Move::make<PROMOTION>(from, to, KNIGHT));
            } else {
                move_list.push(Move(from, to));
            }
        }

        // C. En Passant Capture
        if (pos.en_passant_square() != SQ_NONE) {
            Bitboard ep_targets = attacks & pos.en_passant_square();
            while (ep_targets) {
                Square to = get_LSB(ep_targets);
                ep_targets ^= square_bb(to);
                move_list.push(Move::make<EN_PASSANT>(from, to));
            }
        }
    }

    // ------------------------------------------------------------------------
    // 2. Knight Moves
    // ------------------------------------------------------------------------
    Bitboard knights = pos.pieces(Us, KNIGHT);
    while (knights) {
        Square from = get_LSB(knights);
        knights ^= square_bb(from);

        Bitboard targets = knight_attacks(from) & target_squares;
        while (targets) {
            Square to = get_LSB(targets);
            targets ^= square_bb(to);
            move_list.push(Move(from, to));
        }
    }

    // ------------------------------------------------------------------------
    // 3. Bishop & Queen Moves (Diagonal Sliders)
    // ------------------------------------------------------------------------
    Bitboard diagonal_sliders = pos.pieces(Us, BISHOP) | pos.pieces(Us, QUEEN);
    while (diagonal_sliders) {
        Square from = get_LSB(diagonal_sliders);
        diagonal_sliders ^= square_bb(from);

        Bitboard targets = bishop_attacks(from, occupied_squares) & target_squares;
        while (targets) {
            Square to = get_LSB(targets);
            targets ^= square_bb(to);
            move_list.push(Move(from, to));
        }
    }

    // ------------------------------------------------------------------------
    // 4. Rook & Queen Moves (Orthogonal Sliders)
    // ------------------------------------------------------------------------
    Bitboard straight_sliders = pos.pieces(Us, ROOK) | pos.pieces(Us, QUEEN);
    while (straight_sliders) {
        Square from = get_LSB(straight_sliders);
        straight_sliders ^= square_bb(from);

        Bitboard targets = rook_attacks(from, occupied_squares) & target_squares;
        while (targets) {
            Square to = get_LSB(targets);
            targets ^= square_bb(to);
            move_list.push(Move(from, to));
        }
    }

    // ------------------------------------------------------------------------
    // 5. King Moves & Castling
    // ------------------------------------------------------------------------
    Square king_from = pos.king_square(Us);
    Bitboard king_targets = king_attacks(king_from) & target_squares;
    while (king_targets) {
        Square to = get_LSB(king_targets);
        king_targets ^= square_bb(to);
        move_list.push(Move(king_from, to));
    }

    // Castling moves (Only quiet/all moves search, never captures-only)
    if constexpr (!CapturesOnly) {
        if (!pos.in_check()) {
            if constexpr (Us == WHITE) {
                // White King-side castling (E1 -> G1)
                if (pos.castling_rights() & WHITE_OO) {
                    if (pos.piece_on(SQ_F1) == NO_PIECE && pos.piece_on(SQ_G1) == NO_PIECE) {
                        if (!pos.attacked(SQ_F1, BLACK) && !pos.attacked(SQ_G1, BLACK)) {
                            move_list.push(Move::make<CASTLING>(SQ_E1, SQ_G1));
                        }
                    }
                }
                // White Queen-side castling (E1 -> C1)
                if (pos.castling_rights() & WHITE_OOO) {
                    if (pos.piece_on(SQ_B1) == NO_PIECE && pos.piece_on(SQ_C1) == NO_PIECE && pos.piece_on(SQ_D1) == NO_PIECE) {
                        if (!pos.attacked(SQ_C1, BLACK) && !pos.attacked(SQ_D1, BLACK)) {
                            move_list.push(Move::make<CASTLING>(SQ_E1, SQ_C1));
                        }
                    }
                }
            } else {
                // Black King-side castling (E8 -> G8)
                if (pos.castling_rights() & BLACK_OO) {
                    if (pos.piece_on(SQ_F8) == NO_PIECE && pos.piece_on(SQ_G8) == NO_PIECE) {
                        if (!pos.attacked(SQ_F8, WHITE) && !pos.attacked(SQ_G8, WHITE)) {
                            move_list.push(Move::make<CASTLING>(SQ_E8, SQ_G8));
                        }
                    }
                }
                // Black Queen-side castling (E8 -> C8)
                if (pos.castling_rights() & BLACK_OOO) {
                    if (pos.piece_on(SQ_B8) == NO_PIECE && pos.piece_on(SQ_C8) == NO_PIECE && pos.piece_on(SQ_D8) == NO_PIECE) {
                        if (!pos.attacked(SQ_C8, WHITE) && !pos.attacked(SQ_D8, WHITE)) {
                            move_list.push(Move::make<CASTLING>(SQ_E8, SQ_C8));
                        }
                    }
                }
            }
        }
    }
}

void MoveList::generate(const Position& pos) {
    count = 0;
    if (pos.side_to_move() == WHITE) {
        generate_all<WHITE, false>(pos, *this);
    } else {
        generate_all<BLACK, false>(pos, *this);
    }
}

void MoveList::generate_captures(const Position& pos) {
    count = 0;
    if (pos.side_to_move() == WHITE) {
        generate_all<WHITE, true>(pos, *this);
    } else {
        generate_all<BLACK, true>(pos, *this);
    }
}

} // namespace Bully
