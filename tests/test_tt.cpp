#include <gtest/gtest.h>
#include "../src/types.h"
#include "../src/tt.h"

using namespace Bully;

class TTTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Initialize TT with 16MB
        TT.resize(16);
    }
};

// 1. Check basic resizing and clearing
TEST_F(TTTest, ResizeAndClear) {
    TT.resize(8); // 8MB
    TT.clear();
    
    Move m;
    Value score, eval;
    int depth;
    Bound bound;
    
    // Probing should fail since the table is empty
    EXPECT_FALSE(TT.probe(0x123456789ABCDEULL, m, score, eval, depth, bound, 1));
}

// 2. Test saving and probing simple entries
TEST_F(TTTest, SaveAndProbe) {
    TT.resize(16);
    TT.clear();

    Key key = 0xabcdef123456ULL;
    Move move(SQ_E2, SQ_E4);
    Value score = 100;
    Value eval = 95;
    int depth = 8;
    Bound bound = BOUND_EXACT;

    TT.save(key, move, score, eval, depth, bound, 1);

    Move probed_move;
    Value probed_score, probed_eval;
    int probed_depth;
    Bound probed_bound;

    // Probing with correct key should succeed
    EXPECT_TRUE(TT.probe(key, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 1));
    EXPECT_EQ(probed_move, move);
    EXPECT_EQ(probed_score, score);
    EXPECT_EQ(probed_eval, eval);
    EXPECT_EQ(probed_depth, depth);
    EXPECT_EQ(probed_bound, bound);

    // Probing with wrong key should fail (key verification check)
    EXPECT_FALSE(TT.probe(key ^ 1, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 1));
}

// 3. Test mate score normalization
TEST_F(TTTest, MateScoreNormalization) {
    TT.resize(16);
    TT.clear();

    Key key = 0x9876543210ULL;
    Move move = Move::none();
    Value score = VALUE_MATE - 3; // Mate in 3
    Value eval = VALUE_ZERO;
    int depth = 5;
    Bound bound = BOUND_EXACT;
    int write_ply = 2;
    int read_ply = 4;

    // Write at ply 2
    TT.save(key, move, score, eval, depth, bound, write_ply);

    Move probed_move;
    Value probed_score, probed_eval;
    int probed_depth;
    Bound probed_bound;

    // Read back at ply 4
    // Expected: score is adjusted from mate-in-3 at ply 2 to mate-in-5 at ply 4
    // Absolute distance: VALUE_MATE - 1
    // Read at ply 4: (VALUE_MATE - 1) - 4 = VALUE_MATE - 5
    EXPECT_TRUE(TT.probe(key, probed_move, probed_score, probed_eval, probed_depth, probed_bound, read_ply));
    EXPECT_EQ(probed_score, VALUE_MATE - 5);
}

// 4. Test TT replacement strategy (depth-preferred & aging)
TEST_F(TTTest, ReplacementStrategy) {
    TT.resize(1); // Set very small size (1MB) to force clusters to overwrite
    TT.clear();

    // Generate keys that hash to the same cluster index
    // Using simple keys. Since indexing uses bitwise AND, keys with matching lower bits hit same index.
    // Generate keys that hash to the same cluster index (lower bits identical, upper bits different)
    Key base_key = 0x1000;
    Key k0 = base_key | (0ULL << 48);
    Key k1 = base_key | (1ULL << 48);
    Key k2 = base_key | (2ULL << 48);
    Key k3 = base_key | (3ULL << 48);
    
    // Write 3 entries to completely fill the cluster
    TT.save(k0, Move(SQ_A1, SQ_A2), 10, 10, 5, BOUND_EXACT, 0);
    TT.save(k1, Move(SQ_B1, SQ_B2), 20, 20, 10, BOUND_EXACT, 0);
    TT.save(k2, Move(SQ_C1, SQ_C2), 30, 30, 15, BOUND_EXACT, 0);

    Move probed_move;
    Value probed_score, probed_eval;
    int probed_depth;
    Bound probed_bound;

    // All 3 should be stored
    EXPECT_TRUE(TT.probe(k0, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 0));
    EXPECT_TRUE(TT.probe(k1, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 0));
    EXPECT_TRUE(TT.probe(k2, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 0));

    // Write a 4th entry with higher depth. It should replace the one with the lowest depth (which is k0, depth 5)
    TT.save(k3, Move(SQ_D1, SQ_D2), 40, 40, 20, BOUND_EXACT, 0);

    // k0 should be gone (overwritten by k3)
    EXPECT_FALSE(TT.probe(k0, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 0));
    
    // k1, k2, k3 should be present
    EXPECT_TRUE(TT.probe(k1, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 0));
    EXPECT_TRUE(TT.probe(k2, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 0));
    EXPECT_TRUE(TT.probe(k3, probed_move, probed_score, probed_eval, probed_depth, probed_bound, 0));
}
