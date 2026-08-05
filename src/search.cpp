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
    #include <windows.h>
#elif defined(__linux__)
    #include <pthread.h>
    #include <sched.h>
#endif

#include "search.h"
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
static std::thread controller_thread;

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

// Thread Worker structure
struct SearchWorker {
    int id = 0;
    Position pos;
    std::vector<StateInfo> history_stack;
    std::array<NNUE::Accumulator, MAX_PLY> accumulators;
    SearchState ss;
    Heuristics heuristics;
};

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
    return static_cast<Value>(std::clamp(v + corr, -VALUE_TB + 1, VALUE_TB - 1));
}

static Value quiescence(Position& pos, Value alpha, Value beta, int ply, SearchState& ss, Heuristics& heuristics) {
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
    MovePicker picker(pos, Move::none(), ply, &heuristics, Move::none(), Move::none(), ss.move_list[p_idx], ss.bad_captures[p_idx]);
    Move m;
    while ((m = picker.next_move(pos, !in_check)) != Move::none()) {
        if (!in_check && pos.see(m) < 0) {
            continue;
        }
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
        if (tb_val == VALUE_TB_WIN_IN_MAX_PLY) return static_cast<Value>(tb_val - ply);
        if (tb_val == -VALUE_TB_WIN_IN_MAX_PLY) return static_cast<Value>(tb_val + ply);
        if (tb_val != VALUE_NONE) return tb_val;
    }

    if (ply >= MAX_PLY - 1) {
        return evaluate_position(pos, ss);
    }

    if (depth <= 0) {
        return use_quiescence ? quiescence(pos, alpha, beta, ply, ss, heuristics) : evaluate_position(pos, ss);
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

    if (use_tt && TT.probe(pos.key(), tt_move, tt_score, tt_eval, tt_depth, tt_bound, ply)) {
        if (tt_depth >= depth && !ss.singular_search) {
            if (tt_bound == BOUND_EXACT) return tt_score;
            if (tt_bound == BOUND_UPPER && tt_score <= alpha) return alpha;
            if (tt_bound == BOUND_LOWER && tt_score >= beta) return beta;
        }
    }

    Color us = pos.side_to_move();
    bool in_check = pos.in_check();
    int extension = (use_check_extensions && in_check && ply < MAX_PLY - 1) ? 1 : 0;

    if (use_singular_extensions
        && extension == 0
        && ply > 0
        && depth >= 7
        && tt_move.is_ok()
        && tt_depth >= depth - 3
        && (tt_bound == BOUND_LOWER || tt_bound == BOUND_EXACT)
        && std::abs(tt_score) < VALUE_MATE_IN_MAX_PLY
        && !ss.singular_search) {

        int singular_margin = depth * 2;
        Value singular_beta = static_cast<Value>(tt_score - singular_margin);
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
    if (use_rfp
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
    if (use_nmp && !in_check && depth >= 3 && static_eval >= beta) {
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
    MovePicker picker(pos, tt_move, ply, &heuristics, prev_move, prev_move_2, ss.move_list[p_idx], ss.bad_captures[p_idx]);
    Move m;
    while ((m = picker.next_move(pos)) != Move::none()) {
        if (m == ss.excluded_move) {
            continue;
        }
        bool is_cap = (pos.piece_on(m.to_sq()) != NO_PIECE) || (m.type_of() == EN_PASSANT) || (m.type_of() == PROMOTION);
        if (!is_cap) {
            quiet_moves_searched++;

            // Late Move Pruning (LMP): Prune quiet moves at low depths when count exceeds threshold
            if (use_lmp && !in_check && depth < 4) {
                int lmp_threshold = 3 + depth * depth;
                if (quiet_moves_searched > lmp_threshold) {
                    continue;
                }
            }

            // Futility Pruning: Prune quiet moves at depth 1 when evaluation is far below alpha
            if (use_fp && !in_check && depth == 1) {
                int margin = 150;
                if (static_eval + margin < alpha) {
                    continue;
                }
            }
        }

        StateInfo next_si;
        if (to_index(ply + 1) < MAX_PLY && ss.accumulators) {
            next_si.accumulator = &ss.accumulators[to_index(ply + 1)];
        }
        if (!pos.make_move(m, next_si)) {
            continue;
        }

        legal_moves++;
        ss.nodes++;

        if (!is_cap && quiet_count < 64) {
            quiet_moves[static_cast<size_t>(quiet_count++)] = m;
        }

        Value score;
        if (legal_moves == 1) {
            score = -pvs(pos, -beta, -alpha, depth - 1 + extension, ply + 1, ss, heuristics, m, prev_move);
        } else {
            int reduction = 0;
            if (use_lmr && depth >= 3 && legal_moves > 4 && !is_cap && !in_check) {
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
            
            if (use_history) {
                int bonus = std::clamp(depth * depth, 1, 1024);
                if (!is_cap) {
                    if (use_killers) {
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

    if (use_tt) {
        TT.save(pos.key(), best_move, best_score, static_eval, depth, bound_type, ply);
    }

    return best_score;
}

// Single Worker thread entry point
static void worker_run(SearchWorker* w) {
    bind_thread_affinity(w->id);
    w->heuristics.clear();
    int start_depth = 1 + (w->id & 1); // Alternating starting depths
    int max_search_depth = (w->ss.limits.depth != -1) ? w->ss.limits.depth : MAX_PLY;

    // --- HELPER LAZY SMP THREAD ---
    for (int d = start_depth; d <= max_search_depth; ++d) {
        pvs(w->pos, -VALUE_INFINITE, VALUE_INFINITE, d, 0, w->ss, w->heuristics);
        if (stopped.load(std::memory_order_relaxed)) {
            break;
        }
    }
}

// Global list of active workers (populated by controller)
static std::vector<SearchWorker*> active_workers;

static void search_thread_run(SearchWorker* w) {
    worker_run(w);
}

// The controller thread that spawns all workers, monitors limits, joins them, and prints bestmove
static void controller_worker(Position pos, Limits limits, std::list<StateInfo> history_copy) {
    stopped.store(false, std::memory_order_relaxed);
    TT.new_search();

    auto start_time = std::chrono::steady_clock::now();
    search_start_time_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count(), std::memory_order_relaxed);
    int time_limit = -1;
    Move best_move = Move::none();

    // Time budget allocation
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

    // Syzygy Root DTZ Probing
    Move best_tb_move = Move::none();
    Value tb_score = VALUE_NONE;
    if (Syzygy::probe_root(pos, best_tb_move, tb_score)) {
        best_move = best_tb_move;
        if (!limits.silent) {
            int cp_score = (tb_score > 0 ? 10000 : (tb_score < 0 ? -10000 : 0));
            std::cout << std::format("info depth 1 seldepth 1 score cp {} nodes 1 nps 0 time 0 pv {}\n", cp_score, best_move.to_string());
            std::cout << std::format("bestmove {}\n", best_move.to_string()) << std::flush;
        }
        stopped.store(true, std::memory_order_relaxed);
        return;
    }

    stopped.store(false, std::memory_order_relaxed);

    int threads_to_spawn = num_threads;
    if (threads_to_spawn < 1) threads_to_spawn = 1;

    std::vector<std::unique_ptr<SearchWorker>> workers;
    active_workers.clear();

    auto shared_heuristics = std::make_unique<SharedHeuristics>();
    shared_heuristics->clear();

    // 1. Initialize all workers (cloning board positions and stack histories)
    for (int i = 0; i < threads_to_spawn; ++i) {
        auto w = std::make_unique<SearchWorker>();
        w->id = i;
        w->heuristics.shared = shared_heuristics.get();
        w->heuristics.clear();
        w->ss.limits = limits;
        w->ss.thread_id = i;
        w->ss.time_limit = time_limit;
        w->ss.accumulators = w->accumulators.data();
        
        // Clone stack history with guaranteed capacity reserved
        w->history_stack.clear();
        w->history_stack.reserve(history_copy.size() + MAX_PLY + 256);
        if (history_copy.empty()) {
            w->history_stack.emplace_back();
            w->history_stack.back().accumulator = &w->accumulators[0];
            w->pos.set_fen(pos.get_fen(), w->history_stack.back());
        } else {
            for (const auto& si : history_copy) {
                w->history_stack.push_back(si);
            }
            for (size_t k = 0; k < w->history_stack.size(); ++k) {
                w->history_stack[k].previous = (k == 0) ? nullptr : &w->history_stack[k-1];
                w->history_stack[k].accumulator = (k < MAX_PLY) ? &w->accumulators[k] : nullptr;
            }
            w->pos = pos;
            w->pos.set_state_pointer(&w->history_stack.back());
        }

        active_workers.push_back(w.get());
        workers.push_back(std::move(w));
    }

    std::vector<std::thread> helper_threads;

    // 2. Spawn helper threads (id > 0)
    for (int i = 1; i < threads_to_spawn; ++i) {
        helper_threads.emplace_back(search_thread_run, workers[static_cast<size_t>(i)].get());
    }

    // 3. Run main thread (id = 0) in the controller thread
    SearchWorker* main_worker = workers[0].get();
    int max_search_depth = (limits.depth != -1) ? limits.depth : MAX_PLY;

    struct RootMove {
        Move move = Move::none();
        Value score = -VALUE_INFINITE;
        std::string pv_str;
    };

    std::vector<RootMove> root_moves;
    {
        MoveList list;
        list.generate(pos);
        for (size_t i = 0; i < list.size(); ++i) {
            StateInfo si;
            si.accumulator = &main_worker->accumulators[0];
            if (main_worker->pos.make_move(list[i].move, si)) {
                RootMove rm;
                rm.move = list[i].move;
                rm.score = -VALUE_INFINITE;
                root_moves.push_back(rm);
                main_worker->pos.unmake_move(list[i].move);
            }
        }
    }

    if (root_moves.empty()) {
        if (!limits.silent) {
            std::cout << "bestmove none\n" << std::flush;
        }
        stopped.store(true, std::memory_order_relaxed);
        return;
    }

    Value last_score = VALUE_ZERO;
    for (int d = 1; d <= max_search_depth; ++d) {
        main_worker->ss.seldepth = 0;

        if (multipv_count > 1) {
            for (size_t i = 0; i < root_moves.size(); ++i) {
                if (stopped.load(std::memory_order_relaxed)) {
                    break;
                }

                Move m = root_moves[i].move;
                StateInfo si;
                si.accumulator = &main_worker->accumulators[0];
                if (!main_worker->pos.make_move(m, si)) {
                    continue;
                }
                main_worker->ss.nodes++;

                Value score;
                int n_pv = std::min(multipv_count, static_cast<int>(root_moves.size()));
                if (static_cast<int>(i) < n_pv) {
                    score = -pvs(main_worker->pos, -VALUE_INFINITE, VALUE_INFINITE, d - 1, 1, main_worker->ss, main_worker->heuristics);
                } else {
                    Value alpha = root_moves[static_cast<size_t>(n_pv - 1)].score;
                    if (alpha == -VALUE_INFINITE) {
                        score = -pvs(main_worker->pos, -VALUE_INFINITE, VALUE_INFINITE, d - 1, 1, main_worker->ss, main_worker->heuristics);
                    } else {
                        score = -pvs(main_worker->pos, -alpha - 1, -alpha, d - 1, 1, main_worker->ss, main_worker->heuristics);
                        if (score > alpha) {
                            score = -pvs(main_worker->pos, -VALUE_INFINITE, -alpha, d - 1, 1, main_worker->ss, main_worker->heuristics);
                        }
                    }
                }

                main_worker->pos.unmake_move(m);

                if (stopped.load(std::memory_order_relaxed)) {
                    break;
                }

                root_moves[i].score = score;

                std::string pv_str = m.to_string();
                Position pv_pos = pos;
                std::vector<StateInfo> pv_history(MAX_PLY);
                size_t pv_idx = 0;
                pv_pos.make_move(m, pv_history[pv_idx++]);

                int child_len = main_worker->ss.pv_length[1];
                int current_depth = 1;

                for (int j = 0; j < child_len && current_depth < d; ++j) {
                    Move child_m = main_worker->ss.pv_table[1][static_cast<size_t>(j)];
                    if (child_m == Move::none() || !child_m.is_ok() || !pv_pos.pseudo_legal(child_m) || !pv_pos.legal(child_m)) break;

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
            for (const auto& w : workers) {
                total_nodes += w->ss.nodes;
                max_seldepth = std::max(max_seldepth, w->ss.seldepth);
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time
            ).count();
            double secs = static_cast<double>(elapsed) / 1000.0;
            double nps = (secs > 0.0) ? (static_cast<double>(total_nodes) / secs) : 0.0;

            if (!limits.silent) {
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

            if (limits.time_controlled() && elapsed >= time_limit) {
                break;
            }
            last_score = root_moves[0].score;

        } else {
            Value alpha = -VALUE_INFINITE;
            Value beta = VALUE_INFINITE;
            int delta = 35;

            if (use_aspiration_window && d >= 5) {
                alpha = static_cast<Value>(std::max(static_cast<int>(last_score) - delta, -static_cast<int>(VALUE_INFINITE)));
                beta = static_cast<Value>(std::min(static_cast<int>(last_score) + delta, static_cast<int>(VALUE_INFINITE)));
            }

            Value score = VALUE_ZERO;
            while (true) {
                score = pvs(main_worker->pos, alpha, beta, d, 0, main_worker->ss, main_worker->heuristics);

                if (stopped.load(std::memory_order_relaxed)) {
                    break;
                }

                if (score <= alpha) {
                    beta = alpha;
                    alpha = static_cast<Value>(std::max(static_cast<int>(alpha) - delta, -static_cast<int>(VALUE_INFINITE)));
                    delta += delta / 2;
                }
                else if (score >= beta) {
                    alpha = beta;
                    beta = static_cast<Value>(std::min(static_cast<int>(beta) + delta, static_cast<int>(VALUE_INFINITE)));
                    delta += delta / 2;
                }
                else {
                    break;
                }
            }

            if (stopped.load(std::memory_order_relaxed)) {
                break;
            }

            if (main_worker->ss.pv_length[0] > 0) {
                best_move = main_worker->ss.pv_table[0][0];
            } else {
                Move depth_best_move = Move::none();
                Value dummy_score, dummy_eval;
                int dummy_depth;
                Bound dummy_bound;
                if (TT.probe(main_worker->pos.key(), depth_best_move, dummy_score, dummy_eval, dummy_depth, dummy_bound, 0)) {
                    if (depth_best_move != Move::none()) {
                        best_move = depth_best_move;
                    }
                }
            }

            uint64_t total_nodes = 0;
            int max_seldepth = 0;
            for (const auto& w : workers) {
                total_nodes += w->ss.nodes;
                max_seldepth = std::max(max_seldepth, w->ss.seldepth);
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time
            ).count();
            double secs = static_cast<double>(elapsed) / 1000.0;
            double nps = (secs > 0.0) ? (static_cast<double>(total_nodes) / secs) : 0.0;

            std::string pv_str;
            Position pv_pos = pos;
            std::vector<StateInfo> pv_history(MAX_PLY);
            size_t pv_idx = 0;

            int pv_len = main_worker->ss.pv_length[0];
            int current_depth = 0;

            for (int j = 0; j < pv_len && current_depth < d && pv_idx < MAX_PLY; ++j) {
                Move m = main_worker->ss.pv_table[0][static_cast<size_t>(j)];
                if (m == Move::none() || !m.is_ok() || !pv_pos.pseudo_legal(m) || !pv_pos.legal(m)) break;

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

            if (!limits.silent) {
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

            if (limits.time_controlled() && elapsed >= time_limit) {
                break;
            }

            if (limits.depth == -1 && std::abs(score) >= VALUE_MATE_IN_MAX_PLY) {
                break;
            }

            last_score = score;
        }
    }

    // 4. Search is complete: Stop all threads and join them
    stopped.store(true, std::memory_order_relaxed);

    for (auto& t : helper_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // If main thread failed to retrieve move or best_move is illegal, fallback check PV table, TT, or legal movegen
    if (best_move == Move::none() || !pos.legal(best_move)) {
        if (main_worker->ss.pv_length[0] > 0 && pos.legal(main_worker->ss.pv_table[0][0])) {
            best_move = main_worker->ss.pv_table[0][0];
        } else {
            Value dummy_score, dummy_eval;
            int dummy_depth;
            Bound dummy_bound;
            Move tt_m = Move::none();
            if (TT.probe(pos.key(), tt_m, dummy_score, dummy_eval, dummy_depth, dummy_bound, 0) && pos.pseudo_legal(tt_m) && pos.legal(tt_m)) {
                best_move = tt_m;
            } else {
                MoveList list;
                list.generate(pos);
                for (size_t i = 0; i < list.size(); ++i) {
                    StateInfo si;
                    si.accumulator = &main_worker->accumulators[0];
                    if (main_worker->pos.make_move(list[i].move, si)) {
                        best_move = list[i].move;
                        main_worker->pos.unmake_move(list[i].move);
                        break;
                    }
                }
            }
        }
    }

    uint64_t total = 0;
    for (const auto& w : workers) {
        total += w->ss.nodes;
    }
    last_search_nodes.store(total, std::memory_order_relaxed);

    if (!limits.silent) {
        std::cout << std::format("bestmove {}\n", best_move.to_string()) << std::flush;
    }
}

uint64_t get_last_search_nodes() {
    return last_search_nodes.load(std::memory_order_relaxed);
}

void wait_for_search() {
    if (controller_thread.joinable()) {
        controller_thread.join();
    }
}

void stop_and_join() {
    stopped.store(true, std::memory_order_relaxed);
    if (controller_thread.joinable()) {
        controller_thread.join();
    }
}

void start(const Position& pos, const Limits& limits, std::list<StateInfo>& history) {
    stop_and_join();
    
    // Spawn controller thread running background tasks
    controller_thread = std::thread(controller_worker, pos, limits, history);
}

} // namespace Search
} // namespace Bully
