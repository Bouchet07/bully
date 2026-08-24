#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include <string>
#include <array>
#include "types.h"
#include "position.h"
#include "movegen.h"
#include "nnue.h"

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

// Search Configuration Options
struct SearchConfig {
    bool nmp = true;
    bool lmr = true;
    bool rfp = true;
    bool lmp = true;
    bool fp = true;
    bool check_extensions = true;
    bool singular_extensions = true;
    bool aspiration_window = true;
    bool quiescence = true;
    bool tt = true;
    bool killers = true;
    bool history = true;
};

extern SearchConfig config;

// Global control flags
extern std::atomic<bool> stopped;
extern std::atomic<bool> pondering;
extern std::atomic<int64_t> search_start_time_ms;

// Quiet & Capture Move Ordering heuristic tables shared across all search threads
struct SharedHeuristics {
    // 1D History: [Piece][ToSquare]
    std::array<std::array<int, 64>, 16> history;

    // 1-ply Continuation History: [PiecePrev * 64 + ToSqPrev][PieceCurr][ToSqCurr]
    std::array<std::array<std::array<int, 64>, 16>, 1024> cont_history_1;

    // 2-ply Continuation History: [PiecePrev2 * 64 + ToSqPrev2][PieceCurr][ToSqCurr]
    std::array<std::array<std::array<int, 64>, 16>, 1024> cont_history_2;

    // Capture History: [Piece][ToSquare][VictimPieceType]
    std::array<std::array<std::array<int, 8>, 64>, 16> capture_history;

    SharedHeuristics() {
        clear();
    }

    void clear() {
        for (auto& row : history) {
            for (auto& val : row) val = 0;
        }
        for (auto& table : cont_history_1) {
            for (auto& row : table) {
                for (auto& val : row) val = 0;
            }
        }
        for (auto& table : cont_history_2) {
            for (auto& row : table) {
                for (auto& val : row) val = 0;
            }
        }
        for (auto& table : capture_history) {
            for (auto& row : table) {
                for (auto& val : row) val = 0;
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

constexpr int HISTORY_MAX = 16384;

// Continuous gravity update formula for racy history values
inline void update_history(int& entry, int bonus) {
    int clamped_bonus = std::clamp(bonus, -HISTORY_MAX, HISTORY_MAX);
    int current = entry; // Racy read
    entry = current + clamped_bonus - (current * std::abs(clamped_bonus)) / HISTORY_MAX; // Racy write
}

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

    // Singular Extension Tracking
    Move excluded_move = Move::none();
    bool singular_search = false;

    // Check if we should abort the search (e.g. time limit reached)
    void check_limits();
};

// ============================================================================
// Unified Search API
// ============================================================================

// 1. Thread Management
void set_threads(int count);
[[nodiscard]] int get_threads();

// 2. MultiPV
void set_multipv(int count);
[[nodiscard]] int get_multipv();

// 3. Search Lifecycle
void start(const Position& pos, const Limits& limits, const std::list<StateInfo>& history);
void stop();
void wait();
[[nodiscard]] bool is_searching();

// 4. Statistics
[[nodiscard]] uint64_t get_last_search_nodes();

} // namespace Search
} // namespace Bully
