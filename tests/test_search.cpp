#include <gtest/gtest.h>
#include <list>
#include <chrono>
#include <thread>
#include "../src/types.h"
#include "../src/position.h"
#include "../src/search.h"
#include "../src/tt.h"

using namespace Bully;

class SearchTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        init_bitboards();
        init_attacks();
        init_zobrist();
        TT.resize(16);
    }
};

// 1. Verify Limits structure properties
TEST_F(SearchTest, LimitsCheck) {
    Search::Limits limits;
    EXPECT_FALSE(limits.time_controlled());
    EXPECT_FALSE(limits.infinite);
    EXPECT_FALSE(limits.ponder);

    limits.wtime = 1000;
    EXPECT_TRUE(limits.time_controlled());

    limits.wtime = -1;
    limits.movetime = 500;
    EXPECT_TRUE(limits.time_controlled());
}

// 2. Verify basic start, join, and stopped state transitions
TEST_F(SearchTest, SearchLifecycle) {
    Position pos;
    StateInfo si;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si);
    std::list<StateInfo> history;
    history.push_back(si);

    Search::Limits limits;
    limits.depth = 2;

    // Start background search thread
    Search::start(pos, limits, history);

    // Join and block until complete
    Search::stop_and_join();

    // Verification
    EXPECT_TRUE(Search::stopped.load());
}

// 3. Verify infinite search and explicit stopping
TEST_F(SearchTest, InfiniteSearchStopping) {
    Position pos;
    StateInfo si;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si);
    std::list<StateInfo> history;
    history.push_back(si);

    Search::Limits limits;
    limits.infinite = true;

    // Start infinite search
    Search::start(pos, limits, history);

    // Sleep briefly to let threads initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Confirm it's running
    EXPECT_FALSE(Search::stopped.load());

    // Abruptly stop search
    Search::stop_and_join();

    // Confirm it terminated
    EXPECT_TRUE(Search::stopped.load());
}
