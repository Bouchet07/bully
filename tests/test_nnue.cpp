#include <gtest/gtest.h>
#include <list>
#include "nnue.h"
#include "position.h"

using namespace Bully;

class NNUETest : public ::testing::Test {
protected:
    void SetUp() override {
        NNUE::init();
        history.clear();
        history.emplace_back();
    }

    Position pos;
    std::list<StateInfo> history;
};

TEST_F(NNUETest, FeatureIndexBounds) {
    // Feature index for e2 pawn from White's perspective
    size_t idx_w = NNUE::feature_index(WHITE, SQ_E1, W_PAWN, SQ_E2);
    EXPECT_LT(idx_w, NNUE::HalfKPFeatures);

    // Feature index for e7 pawn from Black's perspective
    size_t idx_b = NNUE::feature_index(BLACK, SQ_E8, B_PAWN, SQ_E7);
    EXPECT_LT(idx_b, NNUE::HalfKPFeatures);
}

TEST_F(NNUETest, LoadNonExistentFile) {
    bool loaded = NNUE::load_net("non_existent_net_file_12345.nnue");
    EXPECT_FALSE(loaded);
    EXPECT_FALSE(NNUE::is_ready());
}

TEST_F(NNUETest, AccumulatorRefreshStartpos) {
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", history.back());
    NNUE::Accumulator acc;
    NNUE::refresh_accumulator(pos, acc);

    EXPECT_TRUE(acc.computed[WHITE]);
    EXPECT_TRUE(acc.computed[BLACK]);
}

TEST_F(NNUETest, DifferentialVsFreshAccumulatorParity) {
    NNUE::use_nnue = true;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", history.back());

    NNUE::Accumulator root_acc;
    NNUE::refresh_accumulator(pos, root_acc);
    history.back().accumulator = &root_acc;

    // Make move 1: e2e4
    StateInfo si1;
    NNUE::Accumulator acc1;
    si1.accumulator = &acc1;
    Move m1(SQ_E2, SQ_E4);
    pos.make_move(m1, si1);
    history.push_back(si1);

    // Make move 2: e7e5
    StateInfo si2;
    NNUE::Accumulator acc2;
    si2.accumulator = &acc2;
    Move m2(SQ_E7, SQ_E5);
    pos.make_move(m2, si2);
    history.push_back(si2);

    // Evaluate position (which triggers incremental update_accumulator on acc2)
    (void)NNUE::evaluate(pos);

    // Fresh accumulator on final position
    NNUE::Accumulator fresh_acc;
    NNUE::refresh_accumulator(pos, fresh_acc);

    EXPECT_EQ(history.back().accumulator->white, fresh_acc.white);
    EXPECT_EQ(history.back().accumulator->black, fresh_acc.black);
    NNUE::use_nnue = false;
}
