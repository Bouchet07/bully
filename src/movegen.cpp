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
    // We use a high-performance parallel shift-based move generation algorithm.
    // Instead of looping over individual pawn squares one by one, we shift the
    // entire pawn bitboard at once in all directions, masking out occupied or
    // empty squares to generate target squares in bulk. This completely avoids
    // individual square checks and conditional branching.
    Bitboard pawns = pos.pieces(Us, PAWN);
    constexpr Rank promo_rank  = relative_rank(Us, RANK_8);

    // Dynamic color-dependent vector directions
    constexpr Direction Up     = pawn_push(Us);
    constexpr Direction UpUp   = Up + Up;
    constexpr Direction UpWest = Up + WEST;
    constexpr Direction UpEast = Up + EAST;

    Bitboard empty_squares = ~occupied_squares;

    if constexpr (!CapturesOnly) {
        // Quiet moves: Single pushes (shifted forward by 1 rank onto empty squares)
        Bitboard single_pushes = shift<Up>(pawns) & empty_squares;

        // Partition single pushes into promotions (reaching the 8th rank) 
        // and quiet pushes to avoid branchy checks inside the serialization loops.
        Bitboard promo_pushes = single_pushes & rank_bb(promo_rank);
        Bitboard quiet_pushes = single_pushes & ~rank_bb(promo_rank);

        // Process quiet pushes: serialize target squares using hardware LSB popping.
        // We use value-based lsb(b) and inline b &= b - 1 (which compiles directly 
        // to single-cycle TZCNT and BLSR instructions) to prevent compiler register-spills.
        while (quiet_pushes) {
            Square to = lsb(quiet_pushes);
            quiet_pushes &= quiet_pushes - 1;
            Square from = to - Up;
            move_list.push(Move(from, to));
        }

        // Process promo pushes: expand each single push reaching the 8th rank 
        // into four distinct promotion moves (Queen, Rook, Bishop, Knight).
        while (promo_pushes) {
            Square to = lsb(promo_pushes);
            promo_pushes &= promo_pushes - 1;
            Square from = to - Up;
            move_list.push(Move::make<PROMOTION>(from, to, QUEEN));
            move_list.push(Move::make<PROMOTION>(from, to, ROOK));
            move_list.push(Move::make<PROMOTION>(from, to, BISHOP));
            move_list.push(Move::make<PROMOTION>(from, to, KNIGHT));
        }

        // Process double pushes: pawns on the start rank that can push forward by 
        // 2 ranks onto empty squares (using the single pushes as the step-wise basis).
        constexpr Rank double_push_rank = (Us == WHITE) ? RANK_3 : RANK_6;
        Bitboard double_pushes = shift<Up>(single_pushes & rank_bb(double_push_rank)) & empty_squares;

        while (double_pushes) {
            Square to = lsb(double_pushes);
            double_pushes &= double_pushes - 1;
            Square from = to - UpUp;
            move_list.push(Move(from, to));
        }
    }

    // Capture moves: Shift pawns diagonally to intersect with enemy pieces.
    // Wrap-around checks (preventing pawns on file A capturing onto file H) 
    // are automatically handled by the shift<> template using file masks.
    Bitboard cap_left  = shift<UpWest>(pawns) & enemy_pieces;
    Bitboard cap_right = shift<UpEast>(pawns) & enemy_pieces;

    // Partition left captures into promotions and normal captures
    Bitboard promo_left = cap_left & rank_bb(promo_rank);
    Bitboard quiet_left = cap_left & ~rank_bb(promo_rank);

    // Partition right captures into promotions and normal captures
    Bitboard promo_right = cap_right & rank_bb(promo_rank);
    Bitboard quiet_right = cap_right & ~rank_bb(promo_rank);

    // Process Left Quiet Captures
    while (quiet_left) {
        Square to = lsb(quiet_left);
        quiet_left &= quiet_left - 1;
        Square from = to - UpWest;
        move_list.push(Move(from, to));
    }

    // Process Left Promo Captures
    while (promo_left) {
        Square to = lsb(promo_left);
        promo_left &= promo_left - 1;
        Square from = to - UpWest;
        move_list.push(Move::make<PROMOTION>(from, to, QUEEN));
        move_list.push(Move::make<PROMOTION>(from, to, ROOK));
        move_list.push(Move::make<PROMOTION>(from, to, BISHOP));
        move_list.push(Move::make<PROMOTION>(from, to, KNIGHT));
    }

    // Process Right Quiet Captures
    while (quiet_right) {
        Square to = lsb(quiet_right);
        quiet_right &= quiet_right - 1;
        Square from = to - UpEast;
        move_list.push(Move(from, to));
    }

    // Process Right Promo Captures
    while (promo_right) {
        Square to = lsb(promo_right);
        promo_right &= promo_right - 1;
        Square from = to - UpEast;
        move_list.push(Move::make<PROMOTION>(from, to, QUEEN));
        move_list.push(Move::make<PROMOTION>(from, to, ROOK));
        move_list.push(Move::make<PROMOTION>(from, to, BISHOP));
        move_list.push(Move::make<PROMOTION>(from, to, KNIGHT));
    }

    // En Passant Capture
    Square ep_sq = pos.en_passant_square();
    if (ep_sq != SQ_NONE) {
        Bitboard ep_attackers = pawn_attacks(Them, ep_sq) & pawns;
        while (ep_attackers) {
            Square from = lsb(ep_attackers);
            ep_attackers &= ep_attackers - 1;
            move_list.push(Move::make<EN_PASSANT>(from, ep_sq));
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
            move_list.push(Move(from, to));
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
            move_list.push(Move(from, to));
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
            move_list.push(Move(from, to));
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
