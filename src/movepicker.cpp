#include "movepicker.h"
#include "search.h"
#include "evaluation.h"

namespace Bully {

MovePicker::MovePicker(const Position& pos, Move tt_move, int ply, const Search::Heuristics* heuristics, Move prev_move, Move prev_move_2, MoveList& list_ref, std::array<ExtMove, MAX_MOVES>& bad_caps_ref)
    : tt_move_(tt_move), ply_(ply), heuristics_(heuristics), prev_move_(prev_move), prev_move_2_(prev_move_2), stage_(Stage::MAIN_TT), list_(list_ref), bad_captures_(bad_caps_ref) {
    list_.clear();
    bad_capture_count_ = 0;
    bad_capture_idx_ = 0;
    if (!tt_move_.is_ok() || !pos.legal(tt_move_)) {
        tt_move_ = Move::none();
        stage_ = Stage::CAPTURE_INIT;
    }
}

static inline int score_move_picker(Move m, Move tt_move, Move prev_move, Move prev_move_2, const Position& pos, int ply, const Search::Heuristics* heuristics, Value& out_see) {
    out_see = VALUE_NONE;
    if (m == tt_move) return 1000000;

    Piece pc = pos.piece_on(m.from_sq());
    PieceType pt = type_of(pc);

    bool is_cap = (pos.piece_on(m.to_sq()) != NO_PIECE) || (m.type_of() == EN_PASSANT);
    if (is_cap) {
        out_see = pos.see(m);
        PieceType victim_pt = (m.type_of() == EN_PASSANT) ? PAWN : type_of(pos.piece_on(m.to_sq()));
        
        int cap_hist = 0;
        if (heuristics && Search::config.history) {
            cap_hist = heuristics->capture_history[to_index(pc)][to_index(m.to_sq())][to_index(victim_pt)] / 32;
        }

        if (out_see < 0) {
            return 10000 + out_see + cap_hist;
        }
        return 900000 + get_piece_value(victim_pt) * 10 - get_piece_value(pt) + cap_hist;
    }

    int score = 0;

    if (heuristics && Search::config.killers) {
        size_t ply_idx = to_index(ply);
        if (m == heuristics->killer1[ply_idx]) return 80000;
        if (m == heuristics->killer2[ply_idx]) return 70000;
        if (prev_move.is_ok() && m == heuristics->countermoves[to_index(prev_move.from_sq())][to_index(prev_move.to_sq())]) score += 65000;
    }

    if (heuristics && Search::config.history) {
        score += heuristics->history[to_index(pc)][to_index(m.to_sq())];

        if (prev_move.is_ok()) {
            Piece pc1 = pos.piece_on(prev_move.to_sq());
            if (pc1 != NO_PIECE) {
                size_t prev_idx1 = to_index(pc1) * 64 + to_index(prev_move.to_sq());
                if (prev_idx1 < 1024) {
                    score += heuristics->cont_history_1[prev_idx1][to_index(pc)][to_index(m.to_sq())];
                }
            }
        }

        if (prev_move_2.is_ok()) {
            Piece pc2 = pos.piece_on(prev_move_2.to_sq());
            if (pc2 != NO_PIECE) {
                size_t prev_idx2 = to_index(pc2) * 64 + to_index(prev_move_2.to_sq());
                if (prev_idx2 < 1024) {
                    score += heuristics->cont_history_2[prev_idx2][to_index(pc)][to_index(m.to_sq())];
                }
            }
        }
    }

    return score;
}

Move MovePicker::next_move(const Position& pos, bool skip_quiets) {
    while (true) {
        switch (stage_) {
            case Stage::MAIN_TT: {
                stage_ = Stage::CAPTURE_INIT;
                return tt_move_;
            }

            case Stage::CAPTURE_INIT: {
                list_.generate_captures(pos);
                for (size_t i = 0; i < list_.size(); ++i) {
                    list_[i].value = score_move_picker(list_[i].move, tt_move_, prev_move_, prev_move_2_, pos, ply_, heuristics_, list_[i].see_score);
                }
                current_idx_ = 0;
                stage_ = Stage::GOOD_CAPTURES;
                break;
            }

            case Stage::GOOD_CAPTURES: {
                while (current_idx_ < list_.size()) {
                    size_t best_idx = current_idx_;
                    for (size_t j = current_idx_ + 1; j < list_.size(); ++j) {
                        if (list_[j].value > list_[best_idx].value) {
                            best_idx = j;
                        }
                    }
                    std::swap(list_[current_idx_], list_[best_idx]);
                    ExtMove em = list_[current_idx_++];

                    if (em.move == tt_move_) continue;

                    if (em.see_score < 0) {
                        bad_captures_[bad_capture_count_++] = em;
                        continue;
                    }
                    return em.move;
                }
                stage_ = Stage::QUIET_INIT;
                break;
            }

            case Stage::QUIET_INIT: {
                if (skip_quiets) {
                    stage_ = Stage::DONE;
                    break;
                }
                list_.generate_quiets(pos);
                for (size_t i = 0; i < list_.size(); ++i) {
                    list_[i].value = score_move_picker(list_[i].move, tt_move_, prev_move_, prev_move_2_, pos, ply_, heuristics_, list_[i].see_score);
                }
                current_idx_ = 0;
                stage_ = Stage::QUIETS;
                break;
            }

            case Stage::QUIETS: {
                while (current_idx_ < list_.size()) {
                    size_t best_idx = current_idx_;
                    for (size_t j = current_idx_ + 1; j < list_.size(); ++j) {
                        if (list_[j].value > list_[best_idx].value) {
                            best_idx = j;
                        }
                    }
                    std::swap(list_[current_idx_], list_[best_idx]);
                    ExtMove em = list_[current_idx_++];

                    if (em.move == tt_move_) continue;
                    return em.move;
                }
                stage_ = Stage::BAD_CAPTURES;
                break;
            }

            case Stage::BAD_CAPTURES: {
                if (bad_capture_idx_ < bad_capture_count_) {
                    return bad_captures_[bad_capture_idx_++].move;
                }
                stage_ = Stage::DONE;
                break;
            }

            case Stage::DONE: {
                return Move::none();
            }
        }
    }
}

} // namespace Bully
