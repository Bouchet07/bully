#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include "types.h"
#include "position.h"

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
extern bool use_nmp;
extern bool use_lmr;
extern bool use_rfp;
extern bool use_lmp;
extern bool use_fp;
extern bool use_check_extensions;
extern bool use_aspiration_window;
extern bool use_quiescence;
extern bool use_tt;
extern bool use_killers;
extern bool use_history;

// Statistics and timing tracking for the active search
struct SearchState {
    Limits limits;
    uint64_t nodes = 0;
    int seldepth = 0;
    int time_limit = -1;  // Calculated optimal search time (ms)
    int thread_id = 0;    // Thread ID (0 = main thread, >0 = helper threads)
    
    // Check if we should abort the search (e.g. time limit reached)
    void check_limits();
};

// Start searching a position on a separate thread
void start(const Position& pos, const Limits& limits, std::list<StateInfo>& history);

// Gracefully stop any running search and block until all worker threads are joined
void stop_and_join();

} // namespace Search
} // namespace Bully
