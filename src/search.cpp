#include <iostream>
#include <format>
#include <thread>
#include <vector>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <cmath>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif defined(__ANDROID__)
    #include <sched.h>
#elif defined(__linux__)
    #include <pthread.h>
    #include <sched.h>
#endif

#include "search.h"
#include "threadpool.h"
#include "tt.h"
#include "movegen.h"
#include "movepicker.h"
#include "bitboard.h"
#include "evaluation.h"
#include "syzygy.h"

namespace Bully {
namespace Search {

static void bind_thread_affinity(int thread_id) {
#if defined(_WIN32)
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads > 0) {
        DWORD_PTR mask = 1ULL << (static_cast<unsigned int>(thread_id) % hardware_threads);
        SetThreadAffinityMask(GetCurrentThread(), mask);
    }
#elif defined(__ANDROID__)
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads > 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(static_cast<unsigned int>(thread_id) % hardware_threads, &cpuset);
        sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    }
#elif defined(__linux__)
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads > 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(static_cast<unsigned int>(thread_id) % hardware_threads, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }
#else
    (void)thread_id;
#endif
}

// Global variables
std::atomic<bool> stopped(true);
std::atomic<bool> pondering(false);
std::atomic<int64_t> search_start_time_ms(0);
std::atomic<uint64_t> last_search_nodes(0);
int num_threads = 1;
int multipv_count = 1;
SearchConfig config;

// LMR reductions table
static int Reductions[MAX_PLY][MAX_MOVES];

struct SearchInitializer {
    SearchInitializer() {
        for (int d = 0; d < MAX_PLY; ++d) {
            for (int m = 0; m < MAX_MOVES; ++m) {
                if (d == 0 || m == 0) {
                    Reductions[d][m] = 0;
                } else {
                    double r = std::log(d) * std::log(m) / 1.95;
                    Reductions[d][m] = static_cast<int>(r + 0.5);
                }
            }
        }
    }
} search_initializer;

static bool is_repetition(const Position& pos) {
    int rule50 = pos.rule50();
    if (rule50 < 4) {
        return false; // Fast O(1) short-circuit: Repetitions require at least 4 plies.
    }

    const StateInfo* state = pos.state();
    Key current_key = state->key;

    int limit = std::min(rule50, 100);
    for (int i = 2; i <= limit && state->previous && state->previous->previous; i += 2) {
        state = state->previous->previous;
        if (state->key == current_key) {
            return true;
        }
    }
    return false;
}

void SearchState::check_limits() {
    if (stopped.load(std::memory_order_relaxed)) {
        return;
    }

    if (limits.nodes > 0 && nodes >= limits.nodes) {
        stopped.store(true, std::memory_order_relaxed);
        return;
    }

    if (pondering.load(std::memory_order_relaxed)) {
        return;
    }

    if (time_limit != -1) {
        auto now = std::chrono::steady_clock::now();
        int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        int64_t start_ms = search_start_time_ms.load(std::memory_order_relaxed);
        int64_t elapsed = now_ms - start_ms;
        if (elapsed >= time_limit) {
            stopped.store(true, std::memory_order_relaxed);
        }
    }
}



static Value evaluate_position(const Position& pos, const SearchState& ss) {
    Value v = Eval::evaluate(pos);
    int corr = ss.correction_history ? ss.correction_history->get_correction(pos) : 0;
    return std::clamp(v + corr, -VALUE_TB + 1, VALUE_TB - 1);
}

static Value quiescence(Position& pos, Value alpha, Value beta, int ply, SearchState& ss, Heuristics& heuristics) {
    if (config.tt) TT.prefetch(pos.key());

    if ((ss.nodes & 1023) == 0) {
        ss.check_limits();
    }

    if (stopped.load(std::memory_order_relaxed)) {
        return VALUE_ZERO;
    }

    if (ply >= MAX_PLY - 1) {
        return evaluate_position(pos, ss);
    }

    if (is_repetition(pos) || pos.rule50() >= 100) {
        return VALUE_DRAW;
    }

    ss.seldepth = std::max(ss.seldepth, ply);

    bool in_check = pos.in_check();

    Value stand_pat = VALUE_NONE;
    if (!in_check) {
        stand_pat = evaluate_position(pos, ss);
        if (stand_pat >= beta) {
            return beta;
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
    }

    int legal_moves = 0;
    size_t p_idx = to_index(ply);
    Bitboard pinned = pos.blockers_for_king(pos.side_to_move());
    MovePicker picker(pos, Move::none(), ply, &heuristics, Move::none(), Move::none(), ss.move_list[p_idx], ss.bad_captures[p_idx]);
    Move m;
    while ((m = picker.next_move(pos, !in_check, !in_check)) != Move::none()) {
        if (!pos.legal(m, pinned)) continue;
        StateInfo next_si;
        if (to_index(ply + 1) < MAX_PLY && ss.accumulators) {
            next_si.accumulator = &ss.accumulators[to_index(ply + 1)];
        }
        if (!pos.make_move(m, next_si)) {
            continue;
        }
        legal_moves++;

        ss.nodes++;
        Value score = -quiescence(pos, -beta, -alpha, ply + 1, ss, heuristics);
        pos.unmake_move(m);

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    if (in_check && legal_moves == 0) {
        return mated_in(ply);
    }

    return alpha;
}

static Value pvs(Position& pos, Value alpha, Value beta, int depth, int ply, SearchState& ss, Heuristics& heuristics, Move prev_move = Move::none(), Move prev_move_2 = Move::none()) {
    if (config.tt) TT.prefetch(pos.key());

    if ((ss.nodes & 1023) == 0) {
        ss.check_limits();
    }

    if (stopped.load(std::memory_order_relaxed)) {
        return VALUE_ZERO;
    }

    if (ply > 0 && (is_repetition(pos) || pos.rule50() >= 100)) {
        return VALUE_DRAW;
    }

    if (ply > 0 && Syzygy::max_cardinality > 0) {
        Value tb_val = Syzygy::probe_wdl(pos);
        if (tb_val == VALUE_TB_WIN_IN_MAX_PLY) return tb_val - ply;
        if (tb_val == -VALUE_TB_WIN_IN_MAX_PLY) return tb_val + ply;
        if (tb_val != VALUE_NONE) return tb_val;
    }

    if (ply >= MAX_PLY - 1) {
        return evaluate_position(pos, ss);
    }

    if (depth <= 0) {
        return config.quiescence ? quiescence(pos, alpha, beta, ply, ss, heuristics) : evaluate_position(pos, ss);
    }

    ss.seldepth = std::max(ss.seldepth, ply);
    if (ply < MAX_PLY) {
        ss.pv_length[to_index(ply)] = 0;
    }

    // 1. Transposition Table Probe
    Move tt_move = Move::none();
    Value tt_score = VALUE_NONE;
    Value tt_eval = VALUE_NONE;
    int tt_depth = -1;
    Bound tt_bound = BOUND_NONE;

    if (config.tt && TT.probe(pos.key(), tt_move, tt_score, tt_eval, tt_depth, tt_bound, ply)) {
        if (tt_depth >= depth && !ss.singular_search) {
            if (tt_bound == BOUND_EXACT) return tt_score;
            if (tt_bound == BOUND_UPPER && tt_score <= alpha) return alpha;
            if (tt_bound == BOUND_LOWER && tt_score >= beta) return beta;
        }
    }

    Color us = pos.side_to_move();
    bool in_check = pos.in_check();
    int extension = (config.check_extensions && in_check && ply < MAX_PLY - 1) ? 1 : 0;

    if (config.singular_extensions
        && extension == 0
        && ply > 0
        && depth >= 7
        && tt_move.is_ok()
        && tt_depth >= depth - 3
        && (tt_bound == BOUND_LOWER || tt_bound == BOUND_EXACT)
        && std::abs(tt_score) < VALUE_MATE_IN_MAX_PLY
        && !ss.singular_search) {

        int singular_margin = depth * 2;
        Value singular_beta = tt_score - singular_margin;
        int singular_depth = (depth - 1) / 2;

        Move saved_excluded = ss.excluded_move;
        bool saved_singular = ss.singular_search;

        ss.excluded_move = tt_move;
        ss.singular_search = true;

        Value singular_score = pvs(pos, singular_beta - 1, singular_beta, singular_depth, ply, ss, heuristics, prev_move, prev_move_2);

        ss.excluded_move = saved_excluded;
        ss.singular_search = saved_singular;

        if (singular_score < singular_beta) {
            extension = 1;
        }
    }

    Value static_eval = evaluate_position(pos, ss);

    // 1.5. Reverse Futility Pruning (RFP) / Static Null Move Pruning
    if (config.rfp
        && !in_check
        && depth <= 3
        && beta - alpha <= 1
        && std::abs(beta) < VALUE_MATE_IN_MAX_PLY) {
        
        int margin = 80 * depth;
        if (static_eval - margin >= beta) {
            return static_eval;
        }
    }

    // 2. Null Move Pruning (NMP)
    if (config.nmp && !in_check && depth >= 3 && static_eval >= beta) {
        Bitboard major_pieces = pos.pieces(us) ^ pos.pieces(us, PAWN) ^ pos.pieces(us, KING);
        if (major_pieces != 0) {
            StateInfo next_si;
            pos.make_null_move(next_si);
            ss.nodes++;
            
            Value null_score = -pvs(pos, -beta, -beta + 1, depth - 1 - 3, ply + 1, ss, heuristics, Move::none(), prev_move);
            pos.unmake_null_move();

            if (null_score >= beta) {
                return beta;
            }
        }
    }

    int legal_moves = 0;
    int quiet_moves_searched = 0;
    std::array<Move, 64> quiet_moves;
    int quiet_count = 0;
    Move best_move = Move::none();
    Value best_score = -VALUE_INFINITE;
    Bound bound_type = BOUND_UPPER;

    size_t p_idx = to_index(ply);
    Bitboard pinned = pos.blockers_for_king(us);
    MovePicker picker(pos, tt_move, ply, &heuristics, prev_move, prev_move_2, ss.move_list[p_idx], ss.bad_captures[p_idx]);
    Move m;
    while ((m = picker.next_move(pos)) != Move::none()) {
        if (m == ss.excluded_move) continue;
        if (!pos.legal(m, pinned)) continue;
        bool is_cap = (m.type_of() != CASTLING) && ((pos.piece_on(m.to_sq()) != NO_PIECE) || (m.type_of() == EN_PASSANT) || (m.type_of() == PROMOTION));
        StateInfo next_si;
        if (to_index(ply + 1) < MAX_PLY && ss.accumulators) {
            next_si.accumulator = &ss.accumulators[to_index(ply + 1)];
        }
        if (!pos.make_move(m, next_si)) {
            continue;
        }

        legal_moves++;
        ss.nodes++;
        bool gives_check = pos.in_check();

        if (!is_cap) {
            quiet_moves_searched++;

            // Late Move Pruning (LMP): Prune quiet non-checking moves at low depths when count exceeds threshold
            if (config.lmp && !gives_check && !in_check && depth < 4) {
                int lmp_threshold = 3 + depth * depth;
                if (quiet_moves_searched > lmp_threshold) {
                    pos.unmake_move(m);
                    continue;
                }
            }
        }

        // Futility Pruning: Prune quiet non-checking moves at depth 1 when evaluation is far below alpha
        if (config.fp && !is_cap && !gives_check && !in_check && depth == 1) {
            int margin = 150;
            if (static_eval + margin < alpha) {
                pos.unmake_move(m);
                continue;
            }
        }

        if (!is_cap && quiet_count < 64) {
            quiet_moves[static_cast<size_t>(quiet_count++)] = m;
        }

        Value score;
        if (legal_moves == 1) {
            score = -pvs(pos, -beta, -alpha, depth - 1 + extension, ply + 1, ss, heuristics, m, prev_move);
        } else {
            int reduction = 0;
            if (config.lmr && depth >= 3 && legal_moves > 4 && !is_cap && !gives_check && !in_check) {
                int d_idx = std::min(static_cast<int>(depth), static_cast<int>(MAX_PLY - 1));
                int m_idx = std::min(legal_moves, static_cast<int>(MAX_MOVES - 1));
                reduction = Reductions[d_idx][m_idx];

                // Lazy SMP Diversification: apply slightly different LMR reduction based on thread_id
                if (ss.thread_id > 0 && ((legal_moves + ss.thread_id) & 1)) {
                    reduction++;
                }

                // Clamp reduction to prevent reducing below depth 0
                reduction = std::clamp(reduction, 0, depth - 1);
            }

            score = -pvs(pos, -(alpha + 1), -alpha, depth - 1 - reduction + extension, ply + 1, ss, heuristics, m, prev_move);

            if (score > alpha && reduction > 0) {
                score = -pvs(pos, -(alpha + 1), -alpha, depth - 1 + extension, ply + 1, ss, heuristics, m, prev_move);
            }

            if (score > alpha && score < beta) {
                score = -pvs(pos, -beta, -alpha, depth - 1 + extension, ply + 1, ss, heuristics, m, prev_move);
            }
        }

        pos.unmake_move(m);

        if (stopped.load(std::memory_order_relaxed)) {
            return VALUE_ZERO;
        }

        if (score > best_score) {
            best_score = score;
            best_move = m;
        }

        if (score > alpha) {
            alpha = score;
            bound_type = BOUND_EXACT;

            if (ply < MAX_PLY) {
                size_t p = to_index(ply);
                size_t p_next = to_index(ply + 1);
                ss.pv_table[p][0] = m;
                int child_len = (ply + 1 < MAX_PLY) ? ss.pv_length[p_next] : 0;
                int copy_len = std::min(child_len, MAX_PLY - 1);
                for (int j = 0; j < copy_len; ++j) {
                    ss.pv_table[p][static_cast<size_t>(j + 1)] = ss.pv_table[p_next][static_cast<size_t>(j)];
                }
                ss.pv_length[p] = copy_len + 1;
            }
        }

        if (score >= beta) {
            bound_type = BOUND_LOWER;
            
            if (config.history) {
                int bonus = std::clamp(depth * depth, 1, 1024);
                if (!is_cap) {
                    if (config.killers) {
                        size_t k_idx = static_cast<size_t>(ply);
                        if (heuristics.killer1[k_idx] != m) {
                            heuristics.killer2[k_idx] = heuristics.killer1[k_idx];
                            heuristics.killer1[k_idx] = m;
                        }
                        if (prev_move.is_ok()) {
                            heuristics.countermoves[to_index(prev_move.from_sq())][to_index(prev_move.to_sq())] = m;
                        }
                    }

                    Piece pc = pos.piece_on(m.from_sq());
                    if (heuristics.shared) {
                        update_history(heuristics.shared->history[to_index(pc)][to_index(m.to_sq())], bonus);

                        if (prev_move.is_ok()) {
                            Piece pc1 = pos.piece_on(prev_move.to_sq());
                            if (pc1 != NO_PIECE) {
                                size_t prev_idx1 = to_index(pc1) * 64 + to_index(prev_move.to_sq());
                                if (prev_idx1 < 1024) {
                                    update_history(heuristics.shared->cont_history_1[prev_idx1][to_index(pc)][to_index(m.to_sq())], bonus);
                                }
                            }
                        }

                        if (prev_move_2.is_ok()) {
                            Piece pc2 = pos.piece_on(prev_move_2.to_sq());
                            if (pc2 != NO_PIECE) {
                                size_t prev_idx2 = to_index(pc2) * 64 + to_index(prev_move_2.to_sq());
                                if (prev_idx2 < 1024) {
                                    update_history(heuristics.shared->cont_history_2[prev_idx2][to_index(pc)][to_index(m.to_sq())], bonus);
                                }
                            }
                        }

                        // Penalize other quiet moves that failed to cause a cutoff
                        for (int q = 0; q < quiet_count; ++q) {
                            Move qm = quiet_moves[static_cast<size_t>(q)];
                            if (qm != m) {
                                Piece qpc = pos.piece_on(qm.from_sq());
                                update_history(heuristics.shared->history[to_index(qpc)][to_index(qm.to_sq())], -bonus);

                                if (prev_move.is_ok()) {
                                    Piece pc1 = pos.piece_on(prev_move.to_sq());
                                    if (pc1 != NO_PIECE) {
                                        size_t prev_idx1 = to_index(pc1) * 64 + to_index(prev_move.to_sq());
                                        if (prev_idx1 < 1024) {
                                            update_history(heuristics.shared->cont_history_1[prev_idx1][to_index(qpc)][to_index(qm.to_sq())], -bonus);
                                        }
                                    }
                                }

                                if (prev_move_2.is_ok()) {
                                    Piece pc2 = pos.piece_on(prev_move_2.to_sq());
                                    if (pc2 != NO_PIECE) {
                                        size_t prev_idx2 = to_index(pc2) * 64 + to_index(prev_move_2.to_sq());
                                        if (prev_idx2 < 1024) {
                                            update_history(heuristics.shared->cont_history_2[prev_idx2][to_index(qpc)][to_index(qm.to_sq())], -bonus);
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    Piece pc = pos.piece_on(m.from_sq());
                    PieceType victim_pt = (m.type_of() == EN_PASSANT) ? PAWN : type_of(pos.piece_on(m.to_sq()));
                    if (heuristics.shared) {
                        update_history(heuristics.shared->capture_history[to_index(pc)][to_index(m.to_sq())][to_index(victim_pt)], bonus);
                    }
                }
            }
            break;
        }
    }

    if (legal_moves == 0) {
        if (in_check) {
            return mated_in(ply);
        } else {
            return VALUE_DRAW;
        }
    }

    if (ss.correction_history && !in_check && depth >= 2) {
        if (best_score >= beta || (best_score > -VALUE_INFINITE && best_score <= alpha)) {
            int bonus = std::clamp((best_score - static_eval) * depth, -1024, 1024);
            ss.correction_history->update(pos, bonus);
        }
    }

    if (config.tt) {
        TT.save(pos.key(), best_move, best_score, static_eval, depth, bound_type, ply);
    }

    return best_score;
}

// Single Worker thread entry point for helper threads (id > 0)
void helper_worker(WorkerThread* w) {
    bind_thread_affinity(w->get_id());
    w->heuristics.clear();
    int start_depth = 1 + (w->get_id() & 1); // Alternating starting depths

    for (int d = start_depth; d <= MAX_PLY; ++d) {
        pvs(w->pos, -VALUE_INFINITE, VALUE_INFINITE, d, 0, w->ss, w->heuristics);
        if (stopped.load(std::memory_order_relaxed)) {
            break;
        }
    }
}

// The controller worker (Thread 0) that coordinates helper threads, runs iterative deepening, and prints bestmove
void controller_worker(WorkerThread* root_worker) {
    stopped.store(false, std::memory_order_relaxed);
    TT.new_search();

    auto start_time = std::chrono::steady_clock::now();
    search_start_time_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count(), std::memory_order_relaxed);
    Move best_move = Move::none();

    // Syzygy Root DTZ Probing
    Move best_tb_move = Move::none();
    Value tb_score = VALUE_NONE;
    if (Syzygy::probe_root(root_worker->pos, best_tb_move, tb_score)) {
        best_move = best_tb_move;
        if (!root_worker->ss.limits.silent) {
            int cp_score = (tb_score > 0 ? 10000 : (tb_score < 0 ? -10000 : 0));
            std::cout << std::format("info depth 1 seldepth 1 score cp {} nodes 1 nps 0 time 0 pv {}\n", cp_score, best_move.to_string());
            std::cout << std::format("bestmove {}\n", best_move.to_string()) << std::flush;
        }
        stopped.store(true, std::memory_order_relaxed);
        return;
    }

    stopped.store(false, std::memory_order_relaxed);

    // Wake up all helper workers (threads 1 to N-1)
    Threads.start_helpers();

    // Run root worker (Thread 0)
    bind_thread_affinity(0);
    int max_search_depth = (root_worker->ss.limits.depth != -1) ? root_worker->ss.limits.depth : MAX_PLY;

    struct RootMove {
        Move move = Move::none();
        Value score = -VALUE_INFINITE;
        std::string pv_str;
    };

    std::vector<RootMove> root_moves;
    {
        MoveList list;
        list.generate(root_worker->pos);
        Bitboard root_pinned = root_worker->pos.blockers_for_king(root_worker->pos.side_to_move());
        for (size_t i = 0; i < list.size(); ++i) {
            if (!root_worker->pos.legal(list[i].move, root_pinned)) continue;
            StateInfo si;
            si.accumulator = &root_worker->accumulators[0];
            if (root_worker->pos.make_move(list[i].move, si)) {
                RootMove rm;
                rm.move = list[i].move;
                rm.score = -VALUE_INFINITE;
                root_moves.push_back(rm);
                root_worker->pos.unmake_move(list[i].move);
            }
        }
    }

    if (root_moves.empty()) {
        if (!root_worker->ss.limits.silent) {
            std::cout << "bestmove none\n" << std::flush;
        }
        stopped.store(true, std::memory_order_relaxed);
        Threads.wait_for_helpers();
        return;
    }

    Value last_score = VALUE_ZERO;
    for (int d = 1; d <= max_search_depth; ++d) {
        root_worker->ss.seldepth = 0;

        if (multipv_count > 1) {
            for (size_t i = 0; i < root_moves.size(); ++i) {
                if (stopped.load(std::memory_order_relaxed)) {
                    break;
                }

                Move m = root_moves[i].move;
                StateInfo si;
                si.accumulator = &root_worker->accumulators[0];
                if (!root_worker->pos.make_move(m, si)) {
                    continue;
                }
                root_worker->ss.nodes++;

                Value score;
                int n_pv = std::min(multipv_count, static_cast<int>(root_moves.size()));
                if (static_cast<int>(i) < n_pv) {
                    score = -pvs(root_worker->pos, -VALUE_INFINITE, VALUE_INFINITE, d - 1, 1, root_worker->ss, root_worker->heuristics);
                } else {
                    Value alpha = root_moves[static_cast<size_t>(n_pv - 1)].score;
                    if (alpha == -VALUE_INFINITE) {
                        score = -pvs(root_worker->pos, -VALUE_INFINITE, VALUE_INFINITE, d - 1, 1, root_worker->ss, root_worker->heuristics);
                    } else {
                        score = -pvs(root_worker->pos, -alpha - 1, -alpha, d - 1, 1, root_worker->ss, root_worker->heuristics);
                        if (score > alpha) {
                            score = -pvs(root_worker->pos, -VALUE_INFINITE, -alpha, d - 1, 1, root_worker->ss, root_worker->heuristics);
                        }
                    }
                }

                root_worker->pos.unmake_move(m);

                if (stopped.load(std::memory_order_relaxed)) {
                    break;
                }

                root_moves[i].score = score;

                std::string pv_str = m.to_string();
                Position pv_pos = root_worker->pos;
                std::vector<StateInfo> pv_history(MAX_PLY);
                size_t pv_idx = 0;
                pv_pos.make_move(m, pv_history[pv_idx++]);

                int child_len = root_worker->ss.pv_length[1];
                int current_depth = 1;

                for (int j = 0; j < child_len && current_depth < d; ++j) {
                    Move child_m = root_worker->ss.pv_table[1][static_cast<size_t>(j)];
                    Bitboard pv_pinned = pv_pos.blockers_for_king(pv_pos.side_to_move());
                    if (child_m == Move::none() || !child_m.is_ok() || !pv_pos.pseudo_legal(child_m) || !pv_pos.legal(child_m, pv_pinned)) break;

                    pv_str += " " + child_m.to_string();
                    if (!pv_pos.make_move(child_m, pv_history[pv_idx++])) break;
                    current_depth++;
                }

                while (current_depth < d && pv_idx < MAX_PLY) {
                    Move next_pv_move = Move::none();
                    Value dummy_score, dummy_eval;
                    int dummy_depth;
                    Bound dummy_bound;

                    if (!TT.probe(pv_pos.key(), next_pv_move, dummy_score, dummy_eval, dummy_depth, dummy_bound, 0)
                        || next_pv_move == Move::none() || !next_pv_move.is_ok()) {
                        break;
                    }

                    MoveList legal_list;
                    legal_list.generate(pv_pos);
                    bool legal = false;
                    for (size_t l = 0; l < legal_list.size(); ++l) {
                        if (legal_list[l].move == next_pv_move) {
                            legal = true;
                            break;
                        }
                    }
                    if (!legal) break;

                    pv_str += " " + next_pv_move.to_string();
                    if (!pv_pos.make_move(next_pv_move, pv_history[pv_idx++])) break;
                    current_depth++;
                }

                root_moves[i].pv_str = pv_str;

                std::sort(root_moves.begin(), root_moves.begin() + static_cast<ptrdiff_t>(i + 1), [](const RootMove& a, const RootMove& b) {
                    return a.score > b.score;
                });
            }

            if (stopped.load(std::memory_order_relaxed)) {
                break;
            }

            uint64_t total_nodes = 0;
            int max_seldepth = 0;
            for (size_t w = 0; w < Threads.size(); ++w) {
                total_nodes += Threads.get_worker(w)->ss.nodes;
                max_seldepth = std::max(max_seldepth, Threads.get_worker(w)->ss.seldepth);
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time
            ).count();
            double secs = static_cast<double>(elapsed) / 1000.0;
            double nps = (secs > 0.0) ? (static_cast<double>(total_nodes) / secs) : 0.0;

            if (!root_worker->ss.limits.silent) {
                int n_pv = std::min(multipv_count, static_cast<int>(root_moves.size()));
                for (int i = 0; i < n_pv; ++i) {
                    Value score = root_moves[static_cast<size_t>(i)].score;
                    if (std::abs(score) >= VALUE_MATE_IN_MAX_PLY) {
                        int mate_plies = VALUE_MATE - std::abs(score);
                        int mate_moves = (mate_plies + 1) / 2;
                        if (score < 0) mate_moves = -mate_moves;
                        std::cout << std::format("info depth {} seldepth {} multipv {} score mate {} nodes {} nps {:.0f} time {} pv {}\n",
                                                 d, max_seldepth, i + 1, mate_moves, total_nodes, nps, elapsed, root_moves[static_cast<size_t>(i)].pv_str);
                    } else {
                        std::cout << std::format("info depth {} seldepth {} multipv {} score cp {} nodes {} nps {:.0f} time {} pv {}\n",
                                                 d, max_seldepth, i + 1, score, total_nodes, nps, elapsed, root_moves[static_cast<size_t>(i)].pv_str);
                    }
                }
                std::cout << std::flush;
            }

            if (root_moves[0].move != Move::none()) {
                best_move = root_moves[0].move;
            }

            if (root_worker->ss.limits.time_controlled() && elapsed >= root_worker->ss.time_limit) {
                break;
            }
            last_score = root_moves[0].score;

        } else {
            Value alpha = -VALUE_INFINITE;
            Value beta = VALUE_INFINITE;
            int delta = 35;

            if (config.aspiration_window && d >= 5) {
                alpha = std::max(last_score - delta, -VALUE_INFINITE);
                beta = std::min(last_score + delta, VALUE_INFINITE);
            }

            Value score = VALUE_ZERO;
            while (true) {
                score = pvs(root_worker->pos, alpha, beta, d, 0, root_worker->ss, root_worker->heuristics);

                if (stopped.load(std::memory_order_relaxed)) {
                    break;
                }

                if (score <= alpha) {
                    // Fail Low: Widen the lower bound.
                    alpha = std::max(alpha - delta, -VALUE_INFINITE);
                    delta += delta / 2;
                }
                else if (score >= beta) {
                    // Fail High: Widen the upper bound.
                    beta = std::min(beta + delta, VALUE_INFINITE);
                    delta += delta / 2;
                }
                else {
                    break;
                }
            }

            if (stopped.load(std::memory_order_relaxed)) {
                break;
            }

            if (root_worker->ss.pv_length[0] > 0) {
                best_move = root_worker->ss.pv_table[0][0];
            } else {
                Move depth_best_move = Move::none();
                Value dummy_score, dummy_eval;
                int dummy_depth;
                Bound dummy_bound;
                if (TT.probe(root_worker->pos.key(), depth_best_move, dummy_score, dummy_eval, dummy_depth, dummy_bound, 0)) {
                    if (depth_best_move != Move::none()) {
                        best_move = depth_best_move;
                    }
                }
            }

            uint64_t total_nodes = 0;
            int max_seldepth = 0;
            for (size_t w = 0; w < Threads.size(); ++w) {
                total_nodes += Threads.get_worker(w)->ss.nodes;
                max_seldepth = std::max(max_seldepth, Threads.get_worker(w)->ss.seldepth);
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time
            ).count();
            double secs = static_cast<double>(elapsed) / 1000.0;
            double nps = (secs > 0.0) ? (static_cast<double>(total_nodes) / secs) : 0.0;

            std::string pv_str;
            Position pv_pos = root_worker->pos;
            std::vector<StateInfo> pv_history(MAX_PLY);
            size_t pv_idx = 0;

            int pv_len = root_worker->ss.pv_length[0];
            int current_depth = 0;

            for (int j = 0; j < pv_len && current_depth < d && pv_idx < MAX_PLY; ++j) {
                Move m = root_worker->ss.pv_table[0][static_cast<size_t>(j)];
                Bitboard pv_pinned = pv_pos.blockers_for_king(pv_pos.side_to_move());
                if (m == Move::none() || !m.is_ok() || !pv_pos.pseudo_legal(m) || !pv_pos.legal(m, pv_pinned)) break;

                pv_str += (pv_str.empty() ? "" : " ") + m.to_string();
                if (!pv_pos.make_move(m, pv_history[pv_idx++])) break;
                current_depth++;
            }

            while (current_depth < d && pv_idx < MAX_PLY) {
                Move next_pv_move = Move::none();
                Value dummy_score, dummy_eval;
                int dummy_depth;
                Bound dummy_bound;

                if (!TT.probe(pv_pos.key(), next_pv_move, dummy_score, dummy_eval, dummy_depth, dummy_bound, 0)
                    || next_pv_move == Move::none() || !next_pv_move.is_ok()) {
                    break;
                }

                MoveList legal_list;
                legal_list.generate(pv_pos);
                bool legal = false;
                for (size_t l = 0; l < legal_list.size(); ++l) {
                    if (legal_list[l].move == next_pv_move) {
                        legal = true;
                        break;
                    }
                }
                if (!legal) break;

                pv_str += (pv_str.empty() ? "" : " ") + next_pv_move.to_string();
                if (!pv_pos.make_move(next_pv_move, pv_history[pv_idx++])) break;
                current_depth++;
            }

            if (!root_worker->ss.limits.silent) {
                if (std::abs(score) >= VALUE_MATE_IN_MAX_PLY) {
                    int mate_plies = VALUE_MATE - std::abs(score);
                    int mate_moves = (mate_plies + 1) / 2;
                    if (score < 0) mate_moves = -mate_moves;
                    std::cout << std::format("info depth {} seldepth {} score mate {} nodes {} nps {:.0f} time {} pv {}\n",
                                             d, max_seldepth, mate_moves, total_nodes, nps, elapsed, pv_str);
                } else {
                    std::cout << std::format("info depth {} seldepth {} score cp {} nodes {} nps {:.0f} time {} pv {}\n",
                                             d, max_seldepth, score, total_nodes, nps, elapsed, pv_str);
                }
                std::cout << std::flush;
            }

            if (root_worker->ss.limits.time_controlled() && elapsed >= root_worker->ss.time_limit) {
                break;
            }

            if (root_worker->ss.limits.depth == -1 && std::abs(score) >= VALUE_MATE_IN_MAX_PLY) {
                break;
            }

            last_score = score;
        }
    }

    // 4. Search is complete: Stop all helper threads and wait for them to finish
    stopped.store(true, std::memory_order_relaxed);
    Threads.wait_for_helpers();

    // If main thread failed to retrieve move or best_move is illegal, fallback check PV table, TT, or legal movegen
    Bitboard fallback_pinned = root_worker->pos.blockers_for_king(root_worker->pos.side_to_move());
    if (best_move == Move::none() || !root_worker->pos.legal(best_move, fallback_pinned)) {
        if (root_worker->ss.pv_length[0] > 0 && root_worker->pos.legal(root_worker->ss.pv_table[0][0], fallback_pinned)) {
            best_move = root_worker->ss.pv_table[0][0];
        } else {
            Value dummy_score, dummy_eval;
            int dummy_depth;
            Bound dummy_bound;
            Move tt_m = Move::none();
            if (TT.probe(root_worker->pos.key(), tt_m, dummy_score, dummy_eval, dummy_depth, dummy_bound, 0) && root_worker->pos.pseudo_legal(tt_m) && root_worker->pos.legal(tt_m, fallback_pinned)) {
                best_move = tt_m;
            } else {
                MoveList list;
                list.generate(root_worker->pos);
                for (size_t i = 0; i < list.size(); ++i) {
                    if (!root_worker->pos.legal(list[i].move, fallback_pinned)) continue;
                    StateInfo si;
                    si.accumulator = &root_worker->accumulators[0];
                    if (root_worker->pos.make_move(list[i].move, si)) {
                        best_move = list[i].move;
                        root_worker->pos.unmake_move(list[i].move);
                        break;
                    }
                }
            }
        }
    }

    if (config.tt && best_move.is_ok()) {
        TT.save(root_worker->pos.key(), best_move, last_score, VALUE_NONE, max_search_depth, BOUND_EXACT, 0);
    }

    uint64_t total = 0;
    for (size_t w = 0; w < Threads.size(); ++w) {
        total += Threads.get_worker(w)->ss.nodes;
    }
    last_search_nodes.store(total, std::memory_order_relaxed);

    if (!root_worker->ss.limits.silent) {
        std::cout << std::format("bestmove {}\n", best_move.to_string()) << std::flush;
    }
}

// ============================================================================
// Unified Search API Implementation
// ============================================================================

void set_threads(int count) {
    int requested = count <= 0 ? static_cast<int>(std::thread::hardware_concurrency()) : count;
    if (requested < 1) requested = 1;
    Threads.resize(requested);
}

int get_threads() {
    return static_cast<int>(Threads.size());
}

void set_multipv(int count) {
    multipv_count = std::max(1, count);
}

int get_multipv() {
    return multipv_count;
}

uint64_t get_last_search_nodes() {
    return last_search_nodes.load(std::memory_order_relaxed);
}

void wait() {
    if (Threads.size() > 0 && Threads.get_worker(0)) {
        Threads.get_worker(0)->wait_for_completion();
    }
}

void stop() {
    stopped.store(true, std::memory_order_relaxed);
    if (Threads.size() > 0 && Threads.get_worker(0)) {
        Threads.get_worker(0)->wait_for_completion();
    }
}

bool is_searching() {
    return Threads.size() > 0 && Threads.get_worker(0) && Threads.get_worker(0)->is_searching();
}

void start(const Position& pos, const Limits& limits, const std::list<StateInfo>& history) {
    stop();

    // Time budget allocation
    int time_limit = -1;
    if (limits.time_controlled()) {
        if (limits.movetime != -1) {
            time_limit = limits.movetime;
        } else {
            int time_left = (pos.side_to_move() == WHITE) ? limits.wtime : limits.btime;
            int inc = (pos.side_to_move() == WHITE) ? limits.winc : limits.binc;

            if (limits.movestogo > 0) {
                time_limit = time_left / limits.movestogo + inc;
            } else {
                time_limit = time_left / 20 + inc / 2;
            }

            if (time_limit > time_left) {
                time_limit = time_left - 20;
            }
            if (time_limit < 10) {
                time_limit = 10;
            }
        }
    }

    // Reset all persistent workers for the new search
    size_t thread_count = Threads.size();
    for (size_t i = 0; i < thread_count; ++i) {
        Threads.get_worker(i)->reset_for_search(pos, limits, history, time_limit);
    }

    // Start controller worker (Thread 0)
    Threads.get_worker(0)->start();
}

} // namespace Search
} // namespace Bully
