#include <gtest/gtest.h>
#include "../src/types.h"
#include "../src/bitboard.h"
#include "../src/attacks.h"
#include "../src/position.h"

using namespace Bully;

class PositionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        init_bitboards();
        init_attacks();
        init_zobrist();
    }
};

TEST_F(PositionTest, StartingPositionFEN) {
    Position pos;
    StateInfo si;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si);

    EXPECT_EQ(pos.side_to_move(), WHITE);
    EXPECT_EQ(pos.castling_rights(), ANY_CASTLING);
    EXPECT_EQ(pos.en_passant_square(), SQ_NONE);
    EXPECT_EQ(pos.rule50(), 0);

    // Verify some piece placements
    EXPECT_EQ(pos.piece_on(SQ_A1), W_ROOK);
    EXPECT_EQ(pos.piece_on(SQ_E1), W_KING);
    EXPECT_EQ(pos.piece_on(SQ_E8), B_KING);
    EXPECT_EQ(pos.piece_on(SQ_D4), NO_PIECE);
}

TEST_F(PositionTest, IncrementalZobristKey) {
    Position pos;
    StateInfo si;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si);

    // Initial hash should match computed hash
    Key initial_key = pos.key();
    
    // Make move e2-e4
    Move m(SQ_E2, SQ_E4); // Double pawn push
    StateInfo next_si;
    
    bool legal = pos.make_move(m, next_si);
    EXPECT_TRUE(legal);

    // After move, incremental key should match computed key from FEN representation
    std::string new_fen = pos.get_fen();
    Position pos2;
    StateInfo si2;
    pos2.set_fen(new_fen, si2);

    EXPECT_EQ(pos.key(), pos2.key());

    // Unmake the move
    pos.unmake_move(m);
    EXPECT_EQ(pos.key(), initial_key);
}

TEST_F(PositionTest, AttackAndCheckDetections) {
    Position pos;
    StateInfo si;
    
    // Standard Scholar's Mate position
    // 1. e4 e5 2. Qh5 Nc6 3. Bc4 Nf6 4. Qxf7#
    pos.set_fen("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4", si);

    // Black King is on E8
    EXPECT_TRUE(pos.in_check());
    EXPECT_TRUE(pos.attacked(SQ_E8, WHITE)); // attacked by White Queen
}

TEST_F(PositionTest, CastlingRightsUpdates) {
    Position pos;
    StateInfo si;
    // Starting position
    pos.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", si);

    EXPECT_EQ(pos.castling_rights(), ANY_CASTLING);

    // Move White Rook from A1 to A2. Should lose WHITE_OOO
    Move m(SQ_A1, SQ_A2);
    StateInfo si2;
    pos.make_move(m, si2);
    EXPECT_EQ(pos.castling_rights(), static_cast<CastlingRights>(ANY_CASTLING & ~WHITE_OOO));

    // Move it back. unmake should restore
    pos.unmake_move(m);
    EXPECT_EQ(pos.castling_rights(), ANY_CASTLING);

    // Move White King from E1 to E2. Should lose all White castling
    Move m2(SQ_E1, SQ_E2);
    StateInfo si3;
    pos.make_move(m2, si3);
    EXPECT_EQ(pos.castling_rights(), static_cast<CastlingRights>(ANY_CASTLING & ~WHITE_CASTLING));

    pos.unmake_move(m2);
}
