#pragma once

#include <array>
#include "types.h"
#include "position.h"

namespace Bully {

// ExtMove contains a Move and a score value used for sorting/move ordering
struct ExtMove {
    Move move;
    int  value;

    constexpr bool operator==(const ExtMove& other) const { return move == other.move; }
    constexpr bool operator!=(const ExtMove& other) const { return move != other.move; }
    constexpr bool operator<(const ExtMove& other) const { return value > other.value; } // Descending sort
};

// Lightweight move list container with zero heap allocations
class MoveList {
public:
    MoveList() = default;

    // Generate all pseudo-legal moves for a position
    void generate(const Position& pos);

    // Generate only pseudo-legal captures (used for Quiescence Search)
    void generate_captures(const Position& pos);

    // Iterators for range-based for loops
    [[nodiscard]] const ExtMove* begin() const { return list.data(); }
    [[nodiscard]] const ExtMove* end() const { return list.data() + count; }

    [[nodiscard]] ExtMove* begin() { return list.data(); }
    [[nodiscard]] ExtMove* end() { return list.data() + count; }
    
    [[nodiscard]] size_t size() const { return count; }
    [[nodiscard]] bool empty() const { return count == 0; }

    [[nodiscard]] const ExtMove& operator[](size_t index) const { return list[index]; }
    [[nodiscard]] ExtMove& operator[](size_t index) { return list[index]; }

    // Helper to add a move directly (for testing or manual generation)
    void push(Move m) {
        if (count < MAX_MOVES) {
            list[count].move = m;
            list[count].value = 0;
            count++;
        }
    }

private:
    std::array<ExtMove, MAX_MOVES> list;
    size_t count = 0;
};

} // namespace Bully
