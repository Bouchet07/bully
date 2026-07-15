#include <gtest/gtest.h>
#include "../src/types.h"
#include "../src/bitboard.h"
#include "../src/attacks.h"

using namespace Bully;

// popcnt
TEST(BitboardTest, Popcnt) {
	Bitboard b = 0b10101010;
	EXPECT_EQ(popcnt(b), 4);
	
	b = 0b11111111;
	EXPECT_EQ(popcnt(b), 8);
	
	b = 0b00000000;
	EXPECT_EQ(popcnt(b), 0);

	b = ~b;
	EXPECT_EQ(popcnt(b), 64); // All bits set
}

// get_LSB
TEST(BitboardTest, GetLSB) {
	Bitboard b = 1; // LSB is set
	EXPECT_EQ(get_LSB(b), SQ_A1);
	
	b = 2; // Second LSB is set
	EXPECT_EQ(get_LSB(b), SQ_B1);
	
	b = 0; // No bits set
	EXPECT_EQ(get_LSB(b), SQ_NONE);
	
	b = 0x8000000000000000ULL; // MSB is set
	EXPECT_EQ(get_LSB(b), SQ_H8);
	
	b = 0b11111111; // Bunch of bits set
	EXPECT_EQ(get_LSB(b), SQ_A1); // First LSB should be returned
}

// Knight attacks validation
TEST(AttacksTest, KnightAttacks) {
    init_attacks(); // Ensure initialized for tests
    Bitboard att = knight_attacks(SQ_E4);
    
    // SQ_E4 is at (file=4, rank=3)
    // Knight targets: D6, F6, C5, G5, C3, G3, D2, F2
    Bitboard expected = square_bb(SQ_D6) | square_bb(SQ_F6) |
                        square_bb(SQ_C5) | square_bb(SQ_G5) |
                        square_bb(SQ_C3) | square_bb(SQ_G3) |
                        square_bb(SQ_D2) | square_bb(SQ_F2);
                        
    EXPECT_EQ(att, expected);
}

// King attacks validation
TEST(AttacksTest, KingAttacks) {
    init_attacks();
    Bitboard att = king_attacks(SQ_E4);
    
    // King targets: D5, E5, F5, D4, F4, D3, E3, F3
    Bitboard expected = square_bb(SQ_D5) | square_bb(SQ_E5) | square_bb(SQ_F5) |
                        square_bb(SQ_D4) |                    square_bb(SQ_F4) |
                        square_bb(SQ_D3) | square_bb(SQ_E3) | square_bb(SQ_F3);
                        
    EXPECT_EQ(att, expected);
}

// Pawn attacks validation
TEST(AttacksTest, PawnAttacks) {
    init_attacks();
    // White pawn on E4 attacks D5 and F5
    Bitboard w_att = pawn_attacks(WHITE, SQ_E4);
    Bitboard w_expected = square_bb(SQ_D5) | square_bb(SQ_F5);
    EXPECT_EQ(w_att, w_expected);

    // Black pawn on E4 attacks D3 and F3
    Bitboard b_att = pawn_attacks(BLACK, SQ_E4);
    Bitboard b_expected = square_bb(SQ_D3) | square_bb(SQ_F3);
    EXPECT_EQ(b_att, b_expected);
}

// Rook attacks validation (sliding)
TEST(AttacksTest, RookAttacks) {
    init_attacks();
    // E4 is square 28. Let's put blockers on E6 and C4
    Bitboard occ = square_bb(SQ_E6) | square_bb(SQ_C4);
    Bitboard att = rook_attacks(SQ_E4, occ);

    // Should include:
    // Vertical: E5, E6 (blocked, stop), E3, E2, E1
    // Horizontal: F4, G4, H4, D4, C4 (blocked, stop)
    Bitboard expected = square_bb(SQ_E5) | square_bb(SQ_E6) |
                        square_bb(SQ_E3) | square_bb(SQ_E2) | square_bb(SQ_E1) |
                        square_bb(SQ_F4) | square_bb(SQ_G4) | square_bb(SQ_H4) |
                        square_bb(SQ_D4) | square_bb(SQ_C4);

    EXPECT_EQ(att, expected);
}

// Bishop attacks validation (sliding)
TEST(AttacksTest, BishopAttacks) {
    init_attacks();
    // E4 square. Let's put blockers on G6 and C2
    Bitboard occ = square_bb(SQ_G6) | square_bb(SQ_C2);
    Bitboard att = bishop_attacks(SQ_E4, occ);

    // Should include:
    // NE: F5, G6 (blocked, stop)
    // NW: D5, C6, B7, A8
    // SE: F3, G2, H1
    // SW: D3, C2 (blocked, stop)
    Bitboard expected = square_bb(SQ_F5) | square_bb(SQ_G6) |
                        square_bb(SQ_D5) | square_bb(SQ_C6) | square_bb(SQ_B7) | square_bb(SQ_A8) |
                        square_bb(SQ_F3) | square_bb(SQ_G2) | square_bb(SQ_H1) |
                        square_bb(SQ_D3) | square_bb(SQ_C2);

    EXPECT_EQ(att, expected);
}