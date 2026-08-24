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
    Search::stop();

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
    Search::stop();

    // Confirm it terminated
    EXPECT_TRUE(Search::stopped.load());
}

// 4. Tactical Mate Resolution (Fool's Mate & Scholar's Mate in 1)
TEST_F(SearchTest, TacticalMateResolution) {
    TT.clear();

    // Fool's Mate in 1 (Black to move: Qh4#)
    Position pos_fools;
    StateInfo si_fools;
    pos_fools.set_fen("rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2", si_fools);
    std::list<StateInfo> hist_fools;
    hist_fools.push_back(si_fools);

    Search::Limits limits_fools;
    limits_fools.depth = 2;
    limits_fools.silent = true;

    Search::start(pos_fools, limits_fools, hist_fools);
    Search::wait();

    Move move_fools;
    Value score_fools, eval_fools;
    int depth_fools;
    Bound bound_fools;
    bool hit_fools = TT.probe(pos_fools.key(), move_fools, score_fools, eval_fools, depth_fools, bound_fools, 0);
    EXPECT_TRUE(hit_fools);
    EXPECT_EQ(move_fools, Move(SQ_D8, SQ_H4));

    // Scholar's Mate in 1 (White to move: Qxf7#)
    TT.clear();
    Position pos_scholars;
    StateInfo si_scholars;
    pos_scholars.set_fen("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", si_scholars);
    std::list<StateInfo> hist_scholars;
    hist_scholars.push_back(si_scholars);

    Search::Limits limits_scholars;
    limits_scholars.depth = 2;
    limits_scholars.silent = true;

    Search::start(pos_scholars, limits_scholars, hist_scholars);
    Search::wait();

    Move move_scholars;
    Value score_scholars, eval_scholars;
    int depth_scholars;
    Bound bound_scholars;
    bool hit_scholars = TT.probe(pos_scholars.key(), move_scholars, score_scholars, eval_scholars, depth_scholars, bound_scholars, 0);
    EXPECT_TRUE(hit_scholars);
    EXPECT_EQ(move_scholars, Move(SQ_H5, SQ_F7));
}

// 5. 50-Move Rule Draw Verification
TEST_F(SearchTest, FiftyMoveRuleDraw) {
    TT.clear();
    Position pos;
    StateInfo si;
    // 50-move rule counter at 100 plies (50 full moves)
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 100 50", si);
    std::list<StateInfo> history;
    history.push_back(si);

    Search::Limits limits;
    limits.depth = 2;
    limits.silent = true;

    Search::start(pos, limits, history);
    Search::wait();

    Move move;
    Value score, eval;
    int depth;
    Bound bound;
    bool hit = TT.probe(pos.key(), move, score, eval, depth, bound, 0);
    EXPECT_TRUE(hit);
    EXPECT_EQ(score, VALUE_DRAW);
}
