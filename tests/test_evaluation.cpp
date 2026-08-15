#include <gtest/gtest.h>
#include "../src/types.h"
#include "../src/bitboard.h"
#include "../src/attacks.h"
#include "../src/position.h"
#include "../src/evaluation.h"

using namespace Bully;

class EvaluationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        init_bitboards();
        init_attacks();
        init_zobrist();
    }
};

// 1. Startpos Symmetry Test
TEST_F(EvaluationTest, StartposSymmetry) {
    Position pos;
    StateInfo si;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si);

    // Initial position evaluation from White's perspective
    Value score_w = Eval::evaluate(pos);

    // Mirror the starting position FEN for Black (side to move is Black)
    Position pos_b;
    StateInfo si_b;
    pos_b.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", si_b);
    Value score_b = Eval::evaluate(pos_b);

    // Symmetry check: evaluate() returns score relative to side to move.
    // Since the position is symmetric, both sides should evaluate it exactly the same.
    EXPECT_EQ(score_w, score_b);
}

// 2. Material Imbalance
TEST_F(EvaluationTest, MaterialImbalance) {
    Position pos;
    StateInfo si;
    // White is missing queen
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", si);
    Value score_no_q = Eval::evaluate(pos);
    
    Position pos2;
    StateInfo si2;
    pos2.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si2);
    Value score_normal = Eval::evaluate(pos2);
    
    // White is missing queen, so score_no_q should be much worse than score_normal
    EXPECT_LT(score_no_q, score_normal - 800);
}

// 3. Piece Square Table (PST) Advancements
TEST_F(EvaluationTest, PawnPSTAdvancement) {
    Position pos1;
    StateInfo si1;
    // Pawn on e2
    pos1.set_fen("k7/8/8/8/8/8/4P3/K7 w - - 0 1", si1);
    Value eval1 = Eval::evaluate(pos1);

    Position pos2;
    StateInfo si2;
    // Pawn on e4
    pos2.set_fen("k7/8/8/8/4P3/8/8/K7 w - - 0 1", si2);
    Value eval2 = Eval::evaluate(pos2);

    // E4 is a better square than E2 for white pawns in PST
    EXPECT_GT(eval2, eval1);
}

// 4. Doubled Pawn Penalty
TEST_F(EvaluationTest, DoubledPawnPenalty) {
    Position pos_normal;
    StateInfo si_normal;
    // Two pawns on adjacent files (A2 and B2)
    pos_normal.set_fen("k7/8/8/8/8/8/PP6/K7 w - - 0 1", si_normal);
    Value val_normal = Eval::evaluate(pos_normal);

    Position pos_doubled;
    StateInfo si_doubled;
    // Two pawns on same file (A2 and A3)
    pos_doubled.set_fen("k7/8/8/8/8/P7/P7/K7 w - - 0 1", si_doubled);
    Value val_doubled = Eval::evaluate(pos_doubled);

    // Doubled pawns should get a penalty (-15 points)
    EXPECT_LT(val_doubled, val_normal);
}

// 5. Isolated Pawn Penalty
TEST_F(EvaluationTest, IsolatedPawnPenalty) {
    Position pos_normal;
    StateInfo si_normal;
    // Pawns on A2 and B2 (supporting each other's adjacent files)
    pos_normal.set_fen("k7/8/8/8/8/8/PP6/K7 w - - 0 1", si_normal);
    Value val_normal = Eval::evaluate(pos_normal);

    Position pos_isolated;
    StateInfo si_isolated;
    // Pawns on A2 and C2 (B file is empty, so both are isolated!)
    pos_isolated.set_fen("k7/8/8/8/8/8/P1P5/K7 w - - 0 1", si_isolated);
    Value val_isolated = Eval::evaluate(pos_isolated);

    // Isolated pawns should get a penalty (-15 points per pawn)
    EXPECT_LT(val_isolated, val_normal);
}

// 6. Bishop Pair Bonus
TEST_F(EvaluationTest, BishopPairBonus) {
    Position pos_no_pair;
    StateInfo si_no_pair;
    // White has only one bishop
    pos_no_pair.set_fen("k7/8/8/8/8/8/3B4/K7 w - - 0 1", si_no_pair);
    Value val_no_pair = Eval::evaluate(pos_no_pair);

    Position pos_pair;
    StateInfo si_pair;
    // White has two bishops
    pos_pair.set_fen("k7/8/8/8/8/8/2BB4/K7 w - - 0 1", si_pair);
    Value val_pair = Eval::evaluate(pos_pair);

    // Having a bishop pair should give a bonus (+30 MG, +40 EG)
    EXPECT_GT(val_pair, val_no_pair + 350); // (Including the value of the second bishop + BishopPST + bishop pair bonus)
}

// 7. Tapered Phase Blending and detailed print execution verification
TEST_F(EvaluationTest, TaperedPhaseBlending) {
    Position pos_start;
    StateInfo si_start;
    pos_start.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si_start);
    
    // In starting position, phase should be 24 (maximum non-pawn material)
    // We verify print_detailed_eval executes without crashes
    testing::internal::CaptureStdout();
    Eval::print_detailed_eval(pos_start, false);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.find("Phase") != std::string::npos);
    EXPECT_TRUE(output.find("Phase : 24") != std::string::npos);

    Position pos_eg;
    StateInfo si_eg;
    // Endgame: only Kings and Pawns (phase should be 0)
    pos_eg.set_fen("8/k7/3p4/p2P4/P2P4/5K2/8/8 w - - 0 1", si_eg);
    testing::internal::CaptureStdout();
    Eval::print_detailed_eval(pos_eg, false);
    std::string eg_output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(eg_output.empty());
    EXPECT_TRUE(eg_output.find("Phase : 0") != std::string::npos);
}

// 8. Passed Pawn Rank-Scaled Bonuses
TEST_F(EvaluationTest, PassedPawnBonuses) {
    Position pos_e3;
    StateInfo si_e3;
    pos_e3.set_fen("k7/8/8/8/8/4P3/8/K7 w - - 0 1", si_e3);
    Value val_e3 = Eval::evaluate(pos_e3);

    Position pos_e6;
    StateInfo si_e6;
    pos_e6.set_fen("k7/8/4P3/8/8/8/8/K7 w - - 0 1", si_e6);
    Value val_e6 = Eval::evaluate(pos_e6);

    // Advanced passed pawn on e6 should score significantly higher than e3
    EXPECT_GT(val_e6, val_e3);
}

// 9. Endgame King Activity (Center vs Corner)
TEST_F(EvaluationTest, EndgameKingActivity) {
    Position pos_center;
    StateInfo si_center;
    // Endgame position (phase 0) with White King centralized on E4
    pos_center.set_fen("k7/8/8/8/4K3/8/8/8 w - - 0 1", si_center);
    Value val_center = Eval::evaluate(pos_center);

    Position pos_corner;
    StateInfo si_corner;
    // Endgame position (phase 0) with White King in corner A1
    pos_corner.set_fen("k7/8/8/8/8/8/8/K7 w - - 0 1", si_corner);
    Value val_corner = Eval::evaluate(pos_corner);

    // Centralized King in the endgame should be evaluated higher than cornered King
    EXPECT_GT(val_center, val_corner);
}

