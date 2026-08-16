/**
 * @file tt.h
 * @brief Cache-Aligned Lock-Free Transposition Table Architecture.
 *
 * Implements a high-performance Transposition Table stored in 32-byte cache-aligned clusters.
 * Each 32-byte cluster fits 3 10-byte entries with XOR key encryption to prevent torn reads
 * across parallel threads without needing mutex locks. Supports age-based and depth-preferred
 * replacement strategies and mate score normalization.
 */

#pragma once

#include <array>
#include <memory>
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    #include <xmmintrin.h>
#endif
#include "types.h"

namespace Bully {

// Group of entries packed into a cache-friendly 32-byte bucket
struct TTEntry {
    uint16_t key16;       // 16 bits of the 64-bit Zobrist key
    uint16_t move16;      // Best move (XORed with key16)
    int16_t  score16;     // Search evaluation score (XORed with key16)
    int16_t  eval16;      // Static evaluation score (XORed with key16)
    uint8_t  depth8;      // Remaining search depth
    uint8_t  gen_bound8;  // Bound type (2 bits) + Generation/Age (6 bits)

    // Save data into the entry, combining bound and age, XORing with key signature
    void save(uint16_t key, Move move, Value score, Value eval, uint8_t depth, Bound bound, uint8_t age) {
        key16 = key;
        move16 = static_cast<uint16_t>(move.raw() ^ key);
        score16 = static_cast<int16_t>(static_cast<uint16_t>(score) ^ key);
        eval16 = static_cast<int16_t>(static_cast<uint16_t>(eval) ^ key);
        depth8 = depth;
        gen_bound8 = static_cast<uint8_t>(bound | (age << 2));
    }

    // Accessors
    [[nodiscard]] Bound bound() const { return static_cast<Bound>(gen_bound8 & 3); }
    [[nodiscard]] uint8_t age() const { return static_cast<uint8_t>(gen_bound8 >> 2); }
    [[nodiscard]] Move move(uint16_t key) const { return Move(static_cast<uint16_t>(move16 ^ key)); }
    [[nodiscard]] Value score(uint16_t key) const { return static_cast<Value>(static_cast<uint16_t>(score16) ^ key); }
    [[nodiscard]] Value eval(uint16_t key) const { return static_cast<Value>(static_cast<uint16_t>(eval16) ^ key); }
};

// Ensure that a Cluster fits exactly in 32 bytes (half a standard 64-byte CPU cache line)
struct alignas(32) Cluster {
    std::array<TTEntry, 3> entries; // 3 * 10 bytes = 30 bytes
    uint16_t padding;               // 2 bytes padding -> 32 bytes
};

// Convert scores to/from transposition table representation (normalizes mate scores)
[[nodiscard]] inline Value score_to_tt(Value v, int ply) {
    if (v >= VALUE_MATE_IN_MAX_PLY)  return v + ply;
    if (v <= VALUE_MATED_IN_MAX_PLY) return v - ply;
    return v;
}

[[nodiscard]] inline Value score_from_tt(Value v, int ply) {
    if (v >= VALUE_MATE_IN_MAX_PLY)  return v - ply;
    if (v <= VALUE_MATED_IN_MAX_PLY) return v + ply;
    return v;
}

class TranspositionTable {
public:
    TranspositionTable() = default;
    ~TranspositionTable();

    // Disable copy constructors
    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;

    // Resize the table to a specified size in Megabytes (allocates cache-aligned memory)
    void resize(size_t mb_size);

    // Clear all entries (resets the table)
    void clear();

    // Issue a hardware prefetch instruction for a specific key's cluster
    inline void prefetch(Key key) const {
        if (!table || cluster_count == 0) return;
        size_t idx = key & (cluster_count - 1);
        
#if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(&table[idx]);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_prefetch(reinterpret_cast<const char*>(&table[idx]), _MM_HINT_T0);
#endif
    }

    // Probe the table for a position. Returns true if hit and satisfies verification
    bool probe(Key key, Move& move, Value& score, Value& eval, int& depth, Bound& bound, int ply);

    // Save a new entry to the table, using depth-preferred/aging replacement rules
    void save(Key key, Move move, Value score, Value eval, int depth, Bound bound, int ply);

    // Increment age at the start of a new search
    void new_search() { age_count = static_cast<uint8_t>((age_count + 1) & 63); }

    // Get the current table size in Megabytes
    [[nodiscard]] size_t get_size_mb() const {
        return (cluster_count * sizeof(Cluster)) / (1024 * 1024);
    }

    // Get the raw pointer of the allocated table for alignment testing
    [[nodiscard]] const void* get_raw_table() const { return table; }

private:
    Cluster* table = nullptr;
    size_t   cluster_count = 0;
    uint8_t  age_count = 0;
};

// Global Transposition Table instance shared across search threads
extern TranspositionTable TT;

} // namespace Bully
