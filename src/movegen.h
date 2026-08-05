/**
 * @file movegen.h
 * @brief Move Generation API and ExtMove / MoveList Containers.
 *
 * Implements high-performance pseudo-legal and legal move generation for all piece types,
 * including specialized capture generation for Quiescence Search. Uses the zero-allocation
 * `MoveList` container storing scored `ExtMove` elements for fast sorting in move ordering.
 */

#pragma once

#include <array>
#include "types.h"
#include "position.h"

namespace Bully {

// ExtMove contains a Move, a score value used for sorting, and cached SEE evaluation
struct ExtMove {
    Move move;
    int  value;
    Value see_score = VALUE_NONE;

    constexpr bool operator==(const ExtMove& other) const { return move == other.move; }
    constexpr bool operator!=(const ExtMove& other) const { return move != other.move; }
    constexpr bool operator<(const ExtMove& other) const { return value > other.value; } // Descending sort
};

enum GenType {
    GEN_ALL,
    GEN_CAPTURES,
    GEN_QUIETS
};

// Lightweight move list container with zero heap allocations
class MoveList {
public:
    MoveList() : last(list.data()) {}

    // Generate all pseudo-legal moves for a position
    void generate(const Position& pos);

    // Generate all strictly legal moves for a position
    void generate_legal(const Position& pos);

    // Generate only pseudo-legal captures (used for Quiescence Search & MovePicker)
    void generate_captures(const Position& pos);

    // Generate only pseudo-legal non-captures (used in MovePicker)
    void generate_quiets(const Position& pos);

    // Iterators for range-based for loops
    [[nodiscard]] const ExtMove* begin() const { return list.data(); }
    [[nodiscard]] const ExtMove* end() const { return last; }

    [[nodiscard]] ExtMove* begin() { return list.data(); }
    [[nodiscard]] ExtMove* end() { return last; }
    
    [[nodiscard]] size_t size() const { return static_cast<size_t>(last - list.data()); }
    [[nodiscard]] bool empty() const { return last == list.data(); }
    void clear() { last = list.data(); }

    [[nodiscard]] const ExtMove& operator[](size_t index) const { return list[index]; }
    [[nodiscard]] ExtMove& operator[](size_t index) { return list[index]; }

    // Helper to add a move directly (for testing or manual generation)
    void push(Move m) {
        last->move = m;
        last->value = 0;
        last++;
    }

private:
    std::array<ExtMove, MAX_MOVES> list;
    ExtMove* last;
};

} // namespace Bully
