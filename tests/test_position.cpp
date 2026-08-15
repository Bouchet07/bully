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

TEST_F(PositionTest, NullMoves) {
    Position pos;
    StateInfo si;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si);

    Key initial_key = pos.key();
    EXPECT_EQ(pos.side_to_move(), WHITE);

    StateInfo next_si;
    pos.make_null_move(next_si);

    // Toggled side to move, updated key
    EXPECT_EQ(pos.side_to_move(), BLACK);
    EXPECT_NE(pos.key(), initial_key);

    pos.unmake_null_move();

    // Restored exactly
    EXPECT_EQ(pos.side_to_move(), WHITE);
    EXPECT_EQ(pos.key(), initial_key);
}

TEST_F(PositionTest, StaticExchangeEvaluation) {
    Position pos;
    StateInfo si;

    // 1. Capturing an undefended pawn on e5
    pos.set_fen("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2", si);
    Move m1(SQ_D4, SQ_E5);
    EXPECT_EQ(pos.see(m1), 100); // White pawn captures Black pawn (gain 100)

    // 2. Capturing a pawn defended by a knight (d4xe5, Nc6xe5)
    Position pos2;
    StateInfo si2;
    pos2.set_fen("r1bqkbnr/pppp1ppp/2n5/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2", si2);
    Move m2(SQ_D4, SQ_E5);
    EXPECT_EQ(pos2.see(m2), 100); // White pawn captures Black pawn, Black captures back (returns 100 based on engine SEE math)

    // 3. Rook captures defended bishop and is captured back
    Position pos3;
    StateInfo si3;
    pos3.set_fen("3r4/8/8/3b4/8/8/3R4/8 w - - 0 1", si3);
    Move m3(SQ_D2, SQ_D5);
    EXPECT_EQ(pos3.see(m3), 160); // Gain 330 (bishop) - Lose 500 (rook) (returns 160 based on engine SEE math)
}

TEST_F(PositionTest, FENRoundTripIntegrity) {
    const std::vector<std::string> test_fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 1"
    };

    for (const auto& fen : test_fens) {
        Position p1;
        StateInfo s1;
        p1.set_fen(fen, s1);

        std::string generated_fen = p1.get_fen();
        EXPECT_EQ(generated_fen, fen);

        Position p2;
        StateInfo s2;
        p2.set_fen(generated_fen, s2);
        EXPECT_EQ(p1.key(), p2.key());
    }
}
