#include <gtest/gtest.h>
#include "../src/types.h"
#include "../src/position.h"
#include "../src/syzygy.h"

using namespace Bully;

class SyzygyTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        init_bitboards();
        init_attacks();
        init_zobrist();
        // Initialize Syzygy tablebases
        Syzygy::init("syzygy");
    }
};

// 1. Verify initialization successfully loaded the 3-piece tablebases
TEST_F(SyzygyTest, LoadStatus) {
    // Should successfully recognize the 3-piece EGTBs in the syzygy directory
    EXPECT_GE(Syzygy::max_cardinality, 3);
    EXPECT_EQ(Syzygy::path, "syzygy");
}

// 2. Verify probing WDL scores at non-root nodes
TEST_F(SyzygyTest, ProbeWDL) {
    if (Syzygy::max_cardinality < 3) {
        GTEST_SKIP() << "Syzygy 3-piece tablebases not loaded.";
    }

    Position pos;
    StateInfo si;
    // KRvK position: White King on g6, White Rook on a7, Black King on h8 (White to move).
    // This is a forced win for White (mate in 1 with Ra7-a8).
    pos.set_fen("7k/R7/6K1/8/8/8/8/8 w - - 0 1", si);

    Value wdl = Syzygy::probe_wdl(pos);
    
    // WDL probing returns normalized tablebase scores
    // Since White is winning, the return value should be positive (VALUE_MATE or standard win score)
    EXPECT_GT(wdl, 0);
}

// 3. Verify root probing and best move selection
TEST_F(SyzygyTest, ProbeRoot) {
    if (Syzygy::max_cardinality < 3) {
        GTEST_SKIP() << "Syzygy 3-piece tablebases not loaded.";
    }

    Position pos;
    StateInfo si;
    // KRvK position: White King on g6, White Rook on a7, Black King on h8 (White to move).
    // The only winning move that mates in 1 is Ra7-a8.
    pos.set_fen("7k/R7/6K1/8/8/8/8/8 w - - 0 1", si);

    Move best_tb_move = Move::none();
    Value tb_score = VALUE_NONE;

    bool success = Syzygy::probe_root(pos, best_tb_move, tb_score);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(best_tb_move.is_ok());
    // The score should indicate a win (VALUE_MATE_IN_MAX_PLY)
    EXPECT_EQ(tb_score, VALUE_MATE_IN_MAX_PLY);
}
