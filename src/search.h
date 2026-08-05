#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include "types.h"
#include "position.h"
#include "movegen.h"

namespace Bully {
namespace Search {

// Limits sent by the chess GUI to guide the search duration
struct Limits {
    int wtime = -1;       // White remaining time (ms)
    int btime = -1;       // Black remaining time (ms)
    int winc = 0;         // White time increment per move (ms)
    int binc = 0;         // Black time increment per move (ms)
    int movestogo = 0;    // Moves remaining until next time control
    int depth = -1;       // Maximum search depth (ply)
    uint64_t nodes = 0;   // Maximum nodes to search
    int movetime = -1;    // Fixed search time per move (ms)
    bool infinite = false;// Search until explicit 'stop' command
    bool ponder = false;  // Search in background for anticipated opponent move
    bool silent = false;  // Suppress output (used in benchmarks / tests)

    [[nodiscard]] bool time_controlled() const {
        return wtime != -1 || btime != -1 || movetime != -1;
    }
};

// Global control flag to abort search immediately
extern std::atomic<bool> stopped;
extern std::atomic<bool> pondering;
extern std::atomic<int64_t> search_start_time_ms;
extern int num_threads;
extern int multipv_count;
// Search Configuration Options
struct SearchConfig {
    bool nmp = true;
    bool lmr = true;
    bool rfp = true;
    bool lmp = true;
    bool fp = true;
    bool check_extensions = true;
    bool aspiration_window = true;
    bool quiescence = true;
    bool tt = true;
    bool killers = true;
    bool history = true;
};

extern SearchConfig config;

// Backward-compatibility references
inline bool& use_nmp = config.nmp;
inline bool& use_lmr = config.lmr;
inline bool& use_rfp = config.rfp;
inline bool& use_lmp = config.lmp;
inline bool& use_fp = config.fp;
inline bool& use_check_extensions = config.check_extensions;
inline bool& use_aspiration_window = config.aspiration_window;
inline bool& use_quiescence = config.quiescence;
inline bool& use_tt = config.tt;
inline bool& use_killers = config.killers;
inline bool& use_history = config.history;

// Quiet & Capture Move Ordering heuristic tables shared across all search threads
struct SharedHeuristics {
    // 1D History: [Piece][ToSquare]
    std::array<std::array<std::atomic<int>, 64>, 16> history{};

    // 1-ply Continuation History: [PiecePrev * 64 + ToSqPrev][PieceCurr][ToSqCurr]
    std::array<std::array<std::array<std::atomic<int>, 64>, 16>, 1024> cont_history_1{};

    // 2-ply Continuation History: [PiecePrev2 * 64 + ToSqPrev2][PieceCurr][ToSqCurr]
    std::array<std::array<std::array<std::atomic<int>, 64>, 16>, 1024> cont_history_2{};

    // Capture History: [Piece][ToSquare][VictimPieceType]
    std::array<std::array<std::array<std::atomic<int>, 8>, 64>, 16> capture_history{};

    void clear() {
        for (auto& row : history) {
            for (auto& val : row) val.store(0, std::memory_order_relaxed);
        }
        for (auto& table : cont_history_1) {
            for (auto& row : table) {
                for (auto& val : row) val.store(0, std::memory_order_relaxed);
            }
        }
        for (auto& table : cont_history_2) {
            for (auto& row : table) {
                for (auto& val : row) val.store(0, std::memory_order_relaxed);
            }
        }
        for (auto& table : capture_history) {
            for (auto& row : table) {
                for (auto& val : row) val.store(0, std::memory_order_relaxed);
            }
        }
    }
};

struct Heuristics {
    std::array<Move, MAX_PLY> killer1 = {Move::none()};
    std::array<Move, MAX_PLY> killer2 = {Move::none()};
    std::array<std::array<Move, 64>, 64> countermoves = {};

    SharedHeuristics* shared = nullptr;

    void clear() {
        killer1.fill(Move::none());
        killer2.fill(Move::none());
        for (auto& row : countermoves) row.fill(Move::none());
    }
};

// Correction History table mapping pawn structures to static evaluation adjustments
constexpr size_t CORR_HIST_SIZE = 16384;
constexpr int CORR_HIST_MAX = 16384;

struct CorrectionHistory {
    std::array<std::array<int16_t, CORR_HIST_SIZE>, COLOR_NB> table{};

    void clear() {
        for (auto& t : table) t.fill(0);
    }

    [[nodiscard]] int get_correction(const Position& pos) const {
        Key k = pos.pawn_key();
        Color c = pos.side_to_move();
        int score = table[to_index(c)][k % CORR_HIST_SIZE];
        return score / 256;
    }

    void update(const Position& pos, int bonus) {
        Key k = pos.pawn_key();
        Color c = pos.side_to_move();
        size_t idx = k % CORR_HIST_SIZE;
        int16_t& entry = table[to_index(c)][idx];
        
        int clamped_bonus = std::clamp(bonus * 16, -CORR_HIST_MAX, CORR_HIST_MAX);
        int new_val = entry + clamped_bonus - (entry * std::abs(clamped_bonus)) / CORR_HIST_MAX;
        entry = static_cast<int16_t>(std::clamp(new_val, -CORR_HIST_MAX, CORR_HIST_MAX));
    }
};

// Statistics and timing tracking for the active search
struct SearchState {
    Limits limits;
    uint64_t nodes = 0;
    int seldepth = 0;
    int time_limit = -1;  // Calculated optimal search time (ms)
    int thread_id = 0;    // Thread ID (0 = main thread, >0 = helper threads)
    NNUE::Accumulator* accumulators = nullptr; // Pointer to worker accumulators array
    CorrectionHistory* correction_history = nullptr;
    
    // Triangular PV Table
    std::array<std::array<Move, MAX_PLY>, MAX_PLY> pv_table{};
    std::array<int, MAX_PLY> pv_length{};

    // Heap-allocated per-ply MoveList array to eliminate function call stack bloat
    std::array<MoveList, MAX_PLY> move_list{};
    std::array<std::array<ExtMove, MAX_MOVES>, MAX_PLY> bad_captures{};

    // Check if we should abort the search (e.g. time limit reached)
    void check_limits();
};

// Start searching a position on a separate thread
void start(const Position& pos, const Limits& limits, std::list<StateInfo>& history);

// Wait for running search thread to complete naturally without requesting premature stop
void wait_for_search();

// Gracefully stop any running search and block until all worker threads are joined
void stop_and_join();

// Get the total nodes searched in the last completed search run
uint64_t get_last_search_nodes();

} // namespace Search
} // namespace Bully
