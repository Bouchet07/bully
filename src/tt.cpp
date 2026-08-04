#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
    #include <windows.h>
    #include <malloc.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

#include "tt.h"

namespace Bully {

// Instantiate global Transposition Table
TranspositionTable TT;

static void* aligned_large_pages_alloc(size_t bytes) {
    if (bytes == 0) return nullptr;
#if defined(_WIN32)
    // 1. Try Windows Large Pages (2MB Huge Pages)
    void* mem = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
    if (mem) return mem;

    // 2. Fallback to VirtualAlloc (standard 4KB pages)
    mem = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (mem) return mem;

    // 3. Fallback to _aligned_malloc
    return _aligned_malloc(bytes, 64);
#elif defined(__linux__)
    // 1. Try Linux Hugetlbfs MAP_HUGETLB
    void* mem = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (mem != MAP_FAILED) return mem;

    // 2. Fallback to posix_memalign
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 64, bytes) == 0) return ptr;
    return nullptr;
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 64, bytes) == 0) return ptr;
    return nullptr;
#endif
}

static void aligned_large_pages_free(void* ptr, size_t bytes) {
    if (!ptr) return;
#if defined(_WIN32)
    (void)bytes;
    if (!VirtualFree(ptr, 0, MEM_RELEASE)) {
        _aligned_free(ptr);
    }
#elif defined(__linux__)
    if (munmap(ptr, bytes) != 0) {
        free(ptr);
    }
#else
    (void)bytes;
    free(ptr);
#endif
}

TranspositionTable::~TranspositionTable() {
    if (table) {
        aligned_large_pages_free(table, cluster_count * sizeof(Cluster));
        table = nullptr;
    }
}

void TranspositionTable::resize(size_t mb_size) {
    if (table) {
        aligned_large_pages_free(table, cluster_count * sizeof(Cluster));
        table = nullptr;
    }

    if (mb_size == 0) {
        cluster_count = 0;
        return;
    }

    size_t size_bytes = mb_size * 1024 * 1024;
    
    // Find the largest power of 2 clusters that fits within requested size
    cluster_count = 1;
    while (cluster_count * sizeof(Cluster) <= size_bytes) {
        cluster_count <<= 1;
    }
    cluster_count >>= 1;

    if (cluster_count == 0) {
        cluster_count = 1;
    }

    size_t alloc_bytes = cluster_count * sizeof(Cluster);

    // Allocate memory using OS Large Pages allocator
    table = static_cast<Cluster*>(aligned_large_pages_alloc(alloc_bytes));

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

    // Fast bitwise AND index calculation (power of 2 cluster count)
    size_t idx = key & (cluster_count - 1);
    uint16_t key16 = static_cast<uint16_t>(key >> 48);
    Cluster& c = table[idx];

    for (size_t i = 0; i < 3; ++i) {
        TTEntry& entry = c.entries[i];
        if (entry.key16 == key16 && entry.bound() != BOUND_NONE) {
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

    TTEntry* replace = nullptr;
    int min_val = 100000;

    for (size_t i = 0; i < 3; ++i) {
        TTEntry& entry = c.entries[i];
        
        if (entry.key16 == key16 || entry.bound() == BOUND_NONE) {
            replace = &entry;
            break;
        }

        int score_val = entry.depth8 - (entry.age() * 2);
        if (score_val < min_val) {
            min_val = score_val;
            replace = &entry;
        }
    }

    if (replace) {
        replace->save(key16, move, score_to_tt(score, ply), eval, static_cast<uint8_t>(depth), bound, age_count);
    }
}

} // namespace Bully
