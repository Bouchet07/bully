#include "threadpool.h"

namespace Bully {
namespace Search {

    // Instantiate the global search thread pool
    ThreadPool Threads;

    // ============================================================================
    // WorkerThread Implementation
    // ============================================================================

    WorkerThread::WorkerThread(int id, SharedHeuristics* shared_heuristics) {
        ss.thread_id = id;
        ss.accumulators = accumulators.data();
        heuristics.shared = shared_heuristics;
        history_stack.reserve(MAX_PLY + 256);
        os_thread = std::thread(&WorkerThread::idle_loop, this);
    }

    WorkerThread::~WorkerThread() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            exit = true;
            searching = false;
        }
        cv.notify_all();
        if (os_thread.joinable()) {
            os_thread.join();
        }
    }

    void WorkerThread::reset_for_search(const Position& root_pos, const Limits& limits, const std::list<StateInfo>& history, int time_limit) {
        history_stack.clear();

        if (history.empty()) {
            history_stack.emplace_back();
            history_stack.back().accumulator = &accumulators[0];
            pos.set_fen(root_pos.get_fen(), history_stack.back());
        } else {
            for (const auto& si : history) {
                history_stack.push_back(si);
            }
            for (size_t k = 0; k < history_stack.size(); ++k) {
                history_stack[k].previous = (k == 0) ? nullptr : &history_stack[k - 1];
                history_stack[k].accumulator = (k < MAX_PLY) ? &accumulators[k] : nullptr;
            }
            pos = root_pos;
            pos.set_state_pointer(&history_stack.back());
        }

        ss.limits = limits;
        ss.nodes = 0;
        ss.seldepth = 0;
        ss.time_limit = time_limit;
        ss.pv_length.fill(0);
        heuristics.clear();
    }

    void WorkerThread::start() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            searching = true;
        }
        cv.notify_one();
    }

    void WorkerThread::wait_for_completion() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !searching; });
    }

    bool WorkerThread::is_searching() const {
        std::unique_lock<std::mutex> lock(mutex);
        return searching;
    }

    void WorkerThread::idle_loop() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mutex);

                // Sleep until a search is started or the engine shuts down
                cv.wait(lock, [this] { return searching || exit; });

                if (exit) {
                    break;
                }
            }

            // Thread 0 is the controller worker; threads > 0 are helper workers
            if (ss.thread_id == 0) {
                controller_worker(this);
            } else {
                helper_worker(this);
            }

            // Search complete, go back to sleep and notify
            {
                std::unique_lock<std::mutex> lock(mutex);
                searching = false;
            }
            cv.notify_all();
        }
    }

    // ============================================================================
    // ThreadPool Implementation
    // ============================================================================

    ThreadPool::ThreadPool() {
        resize(1);
    }

    ThreadPool::~ThreadPool() {
        std::unique_lock<std::mutex> lock(pool_mutex);
        workers.clear();
    }

    void ThreadPool::resize(int requested_threads) {
        std::unique_lock<std::mutex> lock(pool_mutex);

        if (requested_threads < 1) requested_threads = 1;

        // Grow the pool
        while (workers.size() < static_cast<size_t>(requested_threads)) {
            workers.push_back(std::make_unique<WorkerThread>(static_cast<int>(workers.size()), &shared_heuristics));
        }

        // Shrink the pool
        while (workers.size() > static_cast<size_t>(requested_threads)) {
            workers.pop_back();
        }
    }

    void ThreadPool::start_helpers() {
        std::unique_lock<std::mutex> lock(pool_mutex);
        for (size_t i = 1; i < workers.size(); ++i) {
            workers[i]->start();
        }
    }

    void ThreadPool::wait_for_helpers() {
        std::unique_lock<std::mutex> lock(pool_mutex);
        for (size_t i = 1; i < workers.size(); ++i) {
            workers[i]->wait_for_completion();
        }
    }

    size_t ThreadPool::size() const {
        std::unique_lock<std::mutex> lock(pool_mutex);
        return workers.size();
    }

    WorkerThread* ThreadPool::get_worker(size_t index) const {
        std::unique_lock<std::mutex> lock(pool_mutex);
        return workers[index].get();
    }

} // namespace Search
} // namespace Bully