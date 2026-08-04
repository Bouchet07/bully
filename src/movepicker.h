/**
 * @file movepicker.h
 * @brief Staged Move Picker Architecture for Alpha-Beta / PVS Search.
 *
 * Implements a high-performance staged state machine yielding moves incrementally:
 *  Stage 1: Main TT move (yielded before generating any moves; returns instantly on beta cutoff)
 *  Stage 2: Capture move generation & scoring (good captures vs bad captures)
 *  Stage 3: Good captures sorting & picking
 *  Stage 4: Quiet move generation & scoring (killers, countermoves, history heuristic)
 *  Stage 5: Quiet moves sorting & picking
 *  Stage 6: Bad captures fallback
 */

#pragma once

#include <array>
#include "types.h"
#include "position.h"
#include "movegen.h"

namespace Bully {

namespace Search {
    struct Heuristics;
}

enum class Stage : uint8_t {
    MAIN_TT,
    CAPTURE_INIT,
    GOOD_CAPTURES,
    QUIET_INIT,
    QUIETS,
    BAD_CAPTURES,
    DONE
};

class MovePicker {
public:
    MovePicker(const Position& pos, Move tt_move, int ply, const Search::Heuristics* heuristics, Move prev_move);

    // Yields the next best move in order, or Move::none() when moves are exhausted
    [[nodiscard]] Move next_move(const Position& pos, bool skip_quiets = false);

private:
    Move tt_move_ = Move::none();
    int ply_ = 0;
    const Search::Heuristics* heuristics_ = nullptr;
    Move prev_move_ = Move::none();
    Stage stage_ = Stage::MAIN_TT;

    MoveList list_;
    size_t current_idx_ = 0;

    std::array<ExtMove, MAX_MOVES> bad_captures_{};
    size_t bad_capture_count_ = 0;
    size_t bad_capture_idx_ = 0;
};

} // namespace Bully
