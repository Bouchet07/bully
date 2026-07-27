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
    EXPECT_LT(idx_w, NNUE::HALFKP_FEATURES);

    // Feature index for e7 pawn from Black's perspective
    size_t idx_b = NNUE::feature_index(BLACK, SQ_E8, B_PAWN, SQ_E7);
    EXPECT_LT(idx_b, NNUE::HALFKP_FEATURES);
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
