#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
    #include <malloc.h>
#endif

#include "tt.h"

namespace Bully {

// Instantiate the global Transposition Table
TranspositionTable TT;

TranspositionTable::~TranspositionTable() {
    if (table) {
#if defined(_WIN32)
        _aligned_free(table);
#else
        free(table);
#endif
    }
}

void TranspositionTable::resize(size_t mb_size) {
    if (table) {
#if defined(_WIN32)
        _aligned_free(table);
#else
        free(table);
#endif
        table = nullptr;
    }

    if (mb_size == 0) {
        cluster_count = 0;
        return;
    }

    size_t size_bytes = mb_size * 1024 * 1024;
    
    // Find the largest power of 2 clusters that fits within the requested size
    cluster_count = 1;
    while (cluster_count * sizeof(Cluster) <= size_bytes) {
        cluster_count <<= 1;
    }
    cluster_count >>= 1;

    if (cluster_count == 0) {
        cluster_count = 1;
    }

    size_t alloc_bytes = cluster_count * sizeof(Cluster);

    // Allocate memory aligned to 64-byte boundary (cache line aligned)
#if defined(_WIN32)
    table = static_cast<Cluster*>(_aligned_malloc(alloc_bytes, 64));
#else
    void* mem = nullptr;
    if (posix_memalign(&mem, 64, alloc_bytes) == 0) {
        table = static_cast<Cluster*>(mem);
    } else {
        table = nullptr;
    }
#endif

    clear();
}

void TranspositionTable::clear() {
    if (table && cluster_count > 0) {
        std::memset(table, 0, cluster_count * sizeof(Cluster));
    }
}

bool TranspositionTable::probe(Key key, Move& move, Value& score, Value& eval, int& depth, Bound& bound, int ply) {
    if (!table || cluster_count == 0) {
        return false;
    }

    // Fast bitwise AND index calculation (since cluster_count is a power of 2)
    size_t idx = key & (cluster_count - 1);
    uint16_t key16 = static_cast<uint16_t>(key >> 48);
    Cluster& c = table[idx];

    for (size_t i = 0; i < 3; ++i) {
        TTEntry& entry = c.entries[i];
        if (entry.key16 == key16 && entry.bound() != BOUND_NONE) {
            // Hit! Extract variables using XOR decryption
            move = entry.move(key16);
            score = score_from_tt(entry.score(key16), ply);
            eval = entry.eval(key16);
            depth = entry.depth8;
            bound = entry.bound();
            return true;
        }
    }

    return false;
}

void TranspositionTable::save(Key key, Move move, Value score, Value eval, int depth, Bound bound, int ply) {
    if (!table || cluster_count == 0) {
        return;
    }

    size_t idx = key & (cluster_count - 1);
    uint16_t key16 = static_cast<uint16_t>(key >> 48);
    Cluster& c = table[idx];

    TTEntry* replace_entry = nullptr;
    int best_score = -99999; // Value heuristic score for entry replacement

    for (size_t i = 0; i < 3; ++i) {
        TTEntry& entry = c.entries[i];
        
        // Scenario 1: Same position. Overwrite it.
        if (entry.key16 == key16) {
            replace_entry = &entry;
            break;
        }

        // Scenario 2: Unused slot. Highest priority replacement.
        if (entry.bound() == BOUND_NONE) {
            replace_entry = &entry;
            break;
        }

        // Calculate replacement priority heuristic score
        // We prefer keeping deep searches and recent (young) entries.
        int entry_age_diff = (age_count >= entry.age()) ? (age_count - entry.age()) : (64 - (entry.age() - age_count));
        
        // Heuristic formula: older entries are easier to replace, deep entries are harder to replace
        int score_priority = entry_age_diff * 4 - entry.depth8;
        if (score_priority > best_score) {
            best_score = score_priority;
            replace_entry = &entry;
        }
    }

    // Preserve the existing move if the new move is null/none
    Move final_move = move;
    if (final_move == Move::none() && replace_entry->key16 == key16) {
        final_move = replace_entry->move(key16);
    }

    // Save normalization score (normalize checkmates)
    Value normalized_score = score_to_tt(score, ply);

    replace_entry->save(key16, final_move, normalized_score, eval, static_cast<uint8_t>(depth), bound, age_count);
}

} // namespace Bully
