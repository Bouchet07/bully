#include "tbprobe.h"
#include "api.h"
#include "syzygy.h"
#include "movegen.h"
#include <bit>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <format>

namespace Bully {
namespace Syzygy {

std::string path = "syzygy";
int max_cardinality = 0;

void init(const std::string& tb_path) {
    path = tb_path;
    if (path.empty()) {
        tb_free();
        max_cardinality = 0;
        return;
    }

    if (tb_init(path.c_str())) {
        max_cardinality = static_cast<int>(TB_LARGEST);
        std::cout << std::format("info string Syzygy tablebases loaded (max {} pieces) from '{}'\n", max_cardinality, path);
    } else {
        max_cardinality = 0;
    }
}

Value probe_wdl(const Position& pos) {
    if (max_cardinality == 0) return VALUE_NONE;

    int pc_count = std::popcount(pos.occupied());
    if (pc_count > max_cardinality) return VALUE_NONE;

    uint64_t white = pos.pieces(WHITE);
    uint64_t black = pos.pieces(BLACK);
    uint64_t kings = pos.pieces(KING);
    uint64_t queens = pos.pieces(QUEEN);
    uint64_t rooks = pos.pieces(ROOK);
    uint64_t bishops = pos.pieces(BISHOP);
    uint64_t knights = pos.pieces(KNIGHT);
    uint64_t pawns = pos.pieces(PAWN);
    unsigned ep = (pos.en_passant_square() == SQ_NONE) ? 0 : static_cast<unsigned>(to_index(pos.en_passant_square()));
    bool turn = (pos.side_to_move() == WHITE);

    unsigned res = tb_probe_wdl(white, black, kings, queens, rooks, bishops, knights, pawns, ep, turn);
    if (res == TB_RESULT_FAILED) return VALUE_NONE;

    switch (res) {
        case TB_LOSS:
            return -VALUE_MATE_IN_MAX_PLY;
        case TB_BLESSED_LOSS:
            return -VALUE_DRAW - 1;
        case TB_DRAW:
            return VALUE_DRAW;
        case TB_CURSED_WIN:
            return VALUE_DRAW + 1;
        case TB_WIN:
            return VALUE_MATE_IN_MAX_PLY;
        default:
            return VALUE_NONE;
    }
}

bool probe_root(const Position& pos, Move& best_tb_move, Value& tb_score) {
    best_tb_move = Move::none();
    tb_score = VALUE_NONE;

    if (max_cardinality == 0) return false;

    int pc_count = std::popcount(pos.occupied());
    if (pc_count > max_cardinality) return false;

    uint64_t white = pos.pieces(WHITE);
    uint64_t black = pos.pieces(BLACK);
    uint64_t kings = pos.pieces(KING);
    uint64_t queens = pos.pieces(QUEEN);
    uint64_t rooks = pos.pieces(ROOK);
    uint64_t bishops = pos.pieces(BISHOP);
    uint64_t knights = pos.pieces(KNIGHT);
    uint64_t pawns = pos.pieces(PAWN);
    unsigned ep = (pos.en_passant_square() == SQ_NONE) ? 0 : static_cast<unsigned>(to_index(pos.en_passant_square()));
    bool turn = (pos.side_to_move() == WHITE);

    TbRootMoves results;
    int success = tb_probe_root_dtz(white, black, kings, queens, rooks, bishops, knights, pawns,
                                    static_cast<unsigned>(pos.rule50()), ep, turn, false, &results);

    if (!success || results.size == 0) return false;

    // Sort moves by rank descending to get the best move first
    std::sort(results.moves, results.moves + results.size, [](const TbRootMove& a, const TbRootMove& b) {
        return a.tbRank > b.tbRank;
    });

    PyrrhicMove pm = results.moves[0].move;
    Square from = static_cast<Square>(PYRRHIC_MOVE_FROM(pm));
    Square to = static_cast<Square>(PYRRHIC_MOVE_TO(pm));

    PieceType promo = KNIGHT;
    bool is_promo = false;
    if (PYRRHIC_MOVE_IS_QPROMO(pm)) { promo = QUEEN; is_promo = true; }
    else if (PYRRHIC_MOVE_IS_RPROMO(pm)) { promo = ROOK; is_promo = true; }
    else if (PYRRHIC_MOVE_IS_BPROMO(pm)) { promo = BISHOP; is_promo = true; }
    else if (PYRRHIC_MOVE_IS_NPROMO(pm)) { promo = KNIGHT; is_promo = true; }

    if (is_promo) {
        best_tb_move = Move::make<PROMOTION>(from, to, promo);
    } else if (PYRRHIC_MOVE_IS_ENPASS(pm)) {
        best_tb_move = Move::make<EN_PASSANT>(from, to);
    } else if (pos.piece_on(from) == make_piece(pos.side_to_move(), KING) &&
               (std::abs(static_cast<int>(to_index(from)) - static_cast<int>(to_index(to))) == 2)) {
        best_tb_move = Move::make<CASTLING>(from, to);
    } else {
        best_tb_move = Move(from, to);
    }

    int32_t rank = results.moves[0].tbRank;
    if (rank > 0) tb_score = VALUE_MATE_IN_MAX_PLY;
    else if (rank < 0) tb_score = -VALUE_MATE_IN_MAX_PLY;
    else tb_score = VALUE_DRAW;

    return true;
}

} // namespace Syzygy
} // namespace Bully
