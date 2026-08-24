/**
 * @file threadpool.h
 * @brief Zero-overhead persistent thread pool and worker state for Lazy SMP Search.
 *
 * Merges search workers with persistent sleeping threads, eliminating heap fragmentation
 * and per-search memory allocations.
 */

#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <list>

#include "types.h"
#include "position.h"
#include "nnue.h"
#include "search.h"

namespace Bully {
namespace Search {

class WorkerThread;

// Forward declare search entry points defined in search.cpp
void controller_worker(WorkerThread* root_worker);
void helper_worker(WorkerThread* helper_worker);

class WorkerThread {
public:
    WorkerThread(int id, SharedHeuristics* shared_heuristics);
    ~WorkerThread();

    // Search memory must not be duplicated
    WorkerThread(const WorkerThread&) = delete; // no copy
    WorkerThread& operator=(const WorkerThread&) = delete; // no assignment

    // Reset worker state for a new search without heap reallocations
    void reset_for_search(const Position& root_pos, const Limits& limits, const std::list<StateInfo>& history, int time_limit);

    // Wakes up this worker to start searching
    void start();

    // Blocks caller until this worker finishes its current search task
    void wait_for_completion();

    [[nodiscard]] int get_id() const { return ss.thread_id; }
    [[nodiscard]] bool is_searching() const;

    // Persistent search data structures allocated once per thread
    Position pos;
    std::vector<StateInfo> history_stack;
    std::array<NNUE::Accumulator, MAX_PLY> accumulators;
    SearchState ss;
    Heuristics heuristics;

private:
    void idle_loop();

    std::thread os_thread;
    mutable std::mutex mutex;
    std::condition_variable cv;

    bool searching = false;
    bool exit = false;
};

class ThreadPool {
public:
    ThreadPool();
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Resize pool of persistent workers
    void resize(int requested_threads);

    // Wakes up all helper workers (indices 1 to N-1)
    void start_helpers();

    // Waits for all helper workers (indices 1 to N-1) to complete
    void wait_for_helpers();

    [[nodiscard]] size_t size() const;
    [[nodiscard]] WorkerThread* get_worker(size_t index) const;
    [[nodiscard]] SharedHeuristics* get_shared_heuristics() { return &shared_heuristics; }

private:
    std::vector<std::unique_ptr<WorkerThread>> workers;
    SharedHeuristics shared_heuristics;
    mutable std::mutex pool_mutex;
};

// Global internal search thread pool instance
extern ThreadPool Threads;

} // namespace Search
} // namespace Bully