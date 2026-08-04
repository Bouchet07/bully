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

// Quiet Move Ordering heuristic tables local to each search thread
struct Heuristics {
    std::array<Move, MAX_PLY> killer1 = {Move::none()};
    std::array<Move, MAX_PLY> killer2 = {Move::none()};
    std::array<std::array<Move, 64>, 64> countermoves = {};
    // [PieceType][ToSquare]
    std::array<std::array<int, 64>, 16> history = {{{0}}};

    void clear() {
        killer1.fill(Move::none());
        killer2.fill(Move::none());
        for (auto& row : countermoves) row.fill(Move::none());
        for (auto& row : history) row.fill(0);
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
    
    // Triangular PV Table
    std::array<std::array<Move, MAX_PLY>, MAX_PLY> pv_table{};
    std::array<int, MAX_PLY> pv_length{};

    // Heap-allocated per-ply MoveList array to eliminate function call stack bloat
    std::array<MoveList, MAX_PLY> move_list{};

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
