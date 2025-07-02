#include <gtest/gtest.h>
#include "../src/types.h"
#include "../src/bitboard.h"

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