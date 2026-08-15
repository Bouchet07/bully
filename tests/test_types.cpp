#include <gtest/gtest.h>
#include "../src/types.h"

using namespace Bully;


// Test ENABLE_INCR_OPERATORS_ON

// Color increment operators
TEST(ColorTest, IncrementOperators) {
	Color white = WHITE;
	Color black = BLACK;

	EXPECT_EQ(++white, BLACK);
	EXPECT_EQ(++black, BOTH);
}

// Color decrement operators
TEST(ColorTest, DecrementOperators) {
	Color both = BOTH;
	Color black = BLACK;
	EXPECT_EQ(--both, BLACK);
	EXPECT_EQ(--black, WHITE);
}

// CastlingRights
TEST(CastlingRightsTest, CastlingRights) {
	EXPECT_EQ(NO_CASTLING, 0);
	EXPECT_EQ(WHITE_OO, 1);
	EXPECT_EQ(WHITE_OOO, 2);
	EXPECT_EQ(BLACK_OO, 4);
	EXPECT_EQ(BLACK_OOO, 8);
}

// Bound
TEST(BoundTest, BoundValues) {
	EXPECT_EQ(BOUND_NONE, 0);
	EXPECT_EQ(BOUND_UPPER, 1);
	EXPECT_EQ(BOUND_LOWER, 2);
}

// Piecetype increment operators
TEST(PieceTypeTest, IncrementOperators) {
	PieceType no_piece = NO_PIECE_TYPE;
	PieceType pawn = PAWN;
	PieceType knight = KNIGHT;
	PieceType bishop = BISHOP;
	PieceType rook = ROOK;
	PieceType queen = QUEEN;

	EXPECT_EQ(++no_piece, PAWN);
	EXPECT_EQ(++pawn, KNIGHT);
	EXPECT_EQ(++knight, BISHOP);
	EXPECT_EQ(++bishop, ROOK);
	EXPECT_EQ(++rook, QUEEN);
	EXPECT_EQ(++queen, KING);
}

// PieceType decrement operators
TEST(PieceTypeTest, DecrementOperators) {
	PieceType king = KING;
	PieceType queen = QUEEN;
	PieceType rook = ROOK;
	PieceType bishop = BISHOP;
	PieceType knight = KNIGHT;
	PieceType pawn = PAWN;

	EXPECT_EQ(--king, QUEEN);
	EXPECT_EQ(--queen, ROOK);
	EXPECT_EQ(--rook, BISHOP);
	EXPECT_EQ(--bishop, KNIGHT);
	EXPECT_EQ(--knight, PAWN);
	
	EXPECT_EQ(--pawn, NO_PIECE_TYPE);
}

// Piece increment operators
TEST(PieceTest, IncrementOperators) {
	Piece w_pawn = W_PAWN;
	Piece w_knight = W_KNIGHT;
	Piece w_bishop = W_BISHOP;
	Piece w_rook = W_ROOK;
	Piece w_queen = W_QUEEN;

	EXPECT_EQ(++w_pawn, W_KNIGHT);
	EXPECT_EQ(++w_knight, W_BISHOP);
	EXPECT_EQ(++w_bishop, W_ROOK);
	EXPECT_EQ(++w_rook, W_QUEEN);
	EXPECT_EQ(++w_queen, W_KING);

	Piece b_pawn = B_PAWN;
	Piece b_knight = B_KNIGHT;
	Piece b_bishop = B_BISHOP;
	Piece b_rook = B_ROOK;
	Piece b_queen = B_QUEEN;

	EXPECT_EQ(++b_pawn, B_KNIGHT);
	EXPECT_EQ(++b_knight, B_BISHOP);
	EXPECT_EQ(++b_bishop, B_ROOK);
	EXPECT_EQ(++b_rook, B_QUEEN);
	EXPECT_EQ(++b_queen, B_KING);
}

// Piece decrement operators
TEST(PieceTest, DecrementOperators) {
	Piece b_king = B_KING;
	Piece b_queen = B_QUEEN;
	Piece b_rook = B_ROOK;
	Piece b_bishop = B_BISHOP;
	Piece b_knight = B_KNIGHT;

	EXPECT_EQ(--b_king, B_QUEEN);
	EXPECT_EQ(--b_queen, B_ROOK);
	EXPECT_EQ(--b_rook, B_BISHOP);
	EXPECT_EQ(--b_bishop, B_KNIGHT);
	EXPECT_EQ(--b_knight, B_PAWN);

	Piece w_king = W_KING;
	Piece w_queen = W_QUEEN;
	Piece w_rook = W_ROOK;
	Piece w_bishop = W_BISHOP;
	Piece w_knight = W_KNIGHT;

	EXPECT_EQ(--w_king, W_QUEEN);
	EXPECT_EQ(--w_queen, W_ROOK);
	EXPECT_EQ(--w_rook, W_BISHOP);
	EXPECT_EQ(--w_bishop, W_KNIGHT);
	EXPECT_EQ(--w_knight, W_PAWN);
}

// Square increment operators
TEST(SquareTest, IncrementOperators) {
	Square sq = SQ_A1;
	EXPECT_EQ(++sq, SQ_B1); EXPECT_EQ(++sq, SQ_C1); EXPECT_EQ(++sq, SQ_D1);
	EXPECT_EQ(++sq, SQ_E1); EXPECT_EQ(++sq, SQ_F1); EXPECT_EQ(++sq, SQ_G1); EXPECT_EQ(++sq, SQ_H1);
	EXPECT_EQ(++sq, SQ_A2); EXPECT_EQ(++sq, SQ_B2); EXPECT_EQ(++sq, SQ_C2); EXPECT_EQ(++sq, SQ_D2);
	EXPECT_EQ(++sq, SQ_E2); EXPECT_EQ(++sq, SQ_F2); EXPECT_EQ(++sq, SQ_G2); EXPECT_EQ(++sq, SQ_H2);
	EXPECT_EQ(++sq, SQ_A3); EXPECT_EQ(++sq, SQ_B3); EXPECT_EQ(++sq, SQ_C3); EXPECT_EQ(++sq, SQ_D3);
	EXPECT_EQ(++sq, SQ_E3); EXPECT_EQ(++sq, SQ_F3); EXPECT_EQ(++sq, SQ_G3); EXPECT_EQ(++sq, SQ_H3);
	EXPECT_EQ(++sq, SQ_A4); EXPECT_EQ(++sq, SQ_B4); EXPECT_EQ(++sq, SQ_C4); EXPECT_EQ(++sq, SQ_D4);
	EXPECT_EQ(++sq, SQ_E4); EXPECT_EQ(++sq, SQ_F4); EXPECT_EQ(++sq, SQ_G4); EXPECT_EQ(++sq, SQ_H4);
	EXPECT_EQ(++sq, SQ_A5); EXPECT_EQ(++sq, SQ_B5); EXPECT_EQ(++sq, SQ_C5); EXPECT_EQ(++sq, SQ_D5);
	EXPECT_EQ(++sq, SQ_E5); EXPECT_EQ(++sq, SQ_F5); EXPECT_EQ(++sq, SQ_G5); EXPECT_EQ(++sq, SQ_H5);
	EXPECT_EQ(++sq, SQ_A6); EXPECT_EQ(++sq, SQ_B6); EXPECT_EQ(++sq, SQ_C6); EXPECT_EQ(++sq, SQ_D6);
	EXPECT_EQ(++sq, SQ_E6); EXPECT_EQ(++sq, SQ_F6); EXPECT_EQ(++sq, SQ_G6); EXPECT_EQ(++sq, SQ_H6);
	EXPECT_EQ(++sq, SQ_A7); EXPECT_EQ(++sq, SQ_B7); EXPECT_EQ(++sq, SQ_C7); EXPECT_EQ(++sq, SQ_D7);
	EXPECT_EQ(++sq, SQ_E7); EXPECT_EQ(++sq, SQ_F7); EXPECT_EQ(++sq, SQ_G7); EXPECT_EQ(++sq, SQ_H7);
	EXPECT_EQ(++sq, SQ_A8); EXPECT_EQ(++sq, SQ_B8); EXPECT_EQ(++sq, SQ_C8); EXPECT_EQ(++sq, SQ_D8);
	EXPECT_EQ(++sq, SQ_E8); EXPECT_EQ(++sq, SQ_F8); EXPECT_EQ(++sq, SQ_G8); EXPECT_EQ(++sq, SQ_H8);
	EXPECT_EQ(++sq, SQ_NONE); // Should wrap around to SQ_NONE
}

// Square decrement operators
TEST(SquareTest, DecrementOperators) {
	Square sq = SQ_H8;
	EXPECT_EQ(--sq, SQ_G8); EXPECT_EQ(--sq, SQ_F8); EXPECT_EQ(--sq, SQ_E8);
	EXPECT_EQ(--sq, SQ_D8); EXPECT_EQ(--sq, SQ_C8); EXPECT_EQ(--sq, SQ_B8); EXPECT_EQ(--sq, SQ_A8);
	EXPECT_EQ(--sq, SQ_H7); EXPECT_EQ(--sq, SQ_G7); EXPECT_EQ(--sq, SQ_F7); EXPECT_EQ(--sq, SQ_E7);
	EXPECT_EQ(--sq, SQ_D7); EXPECT_EQ(--sq, SQ_C7); EXPECT_EQ(--sq, SQ_B7); EXPECT_EQ(--sq, SQ_A7);
	EXPECT_EQ(--sq, SQ_H6); EXPECT_EQ(--sq, SQ_G6); EXPECT_EQ(--sq, SQ_F6); EXPECT_EQ(--sq, SQ_E6);
	EXPECT_EQ(--sq, SQ_D6); EXPECT_EQ(--sq, SQ_C6); EXPECT_EQ(--sq, SQ_B6); EXPECT_EQ(--sq, SQ_A6);
	EXPECT_EQ(--sq, SQ_H5); EXPECT_EQ(--sq, SQ_G5); EXPECT_EQ(--sq, SQ_F5); EXPECT_EQ(--sq, SQ_E5);
	EXPECT_EQ(--sq, SQ_D5); EXPECT_EQ(--sq, SQ_C5); EXPECT_EQ(--sq, SQ_B5); EXPECT_EQ(--sq, SQ_A5);
	EXPECT_EQ(--sq, SQ_H4); EXPECT_EQ(--sq, SQ_G4); EXPECT_EQ(--sq, SQ_F4); EXPECT_EQ(--sq, SQ_E4);
	EXPECT_EQ(--sq, SQ_D4); EXPECT_EQ(--sq, SQ_C4); EXPECT_EQ(--sq, SQ_B4); EXPECT_EQ(--sq, SQ_A4);
	EXPECT_EQ(--sq, SQ_H3); EXPECT_EQ(--sq, SQ_G3); EXPECT_EQ(--sq, SQ_F3); EXPECT_EQ(--sq, SQ_E3);
	EXPECT_EQ(--sq, SQ_D3); EXPECT_EQ(--sq, SQ_C3); EXPECT_EQ(--sq, SQ_B3); EXPECT_EQ(--sq, SQ_A3);
	EXPECT_EQ(--sq, SQ_H2); EXPECT_EQ(--sq, SQ_G2); EXPECT_EQ(--sq, SQ_F2); EXPECT_EQ(--sq, SQ_E2);
	EXPECT_EQ(--sq, SQ_D2); EXPECT_EQ(--sq, SQ_C2); EXPECT_EQ(--sq, SQ_B2); EXPECT_EQ(--sq, SQ_A2);
	EXPECT_EQ(--sq, SQ_H1); EXPECT_EQ(--sq, SQ_G1); EXPECT_EQ(--sq, SQ_F1); EXPECT_EQ(--sq, SQ_E1);
	EXPECT_EQ(--sq, SQ_D1); EXPECT_EQ(--sq, SQ_C1); EXPECT_EQ(--sq, SQ_B1); EXPECT_EQ(--sq, SQ_A1);
}

// File increment operators
TEST(FileTest, IncrementOperators) {
	File file_a = FILE_A;
	File file_b = FILE_B;
	File file_c = FILE_C;
	File file_d = FILE_D;
	File file_e = FILE_E;
	File file_f = FILE_F;
	File file_g = FILE_G;
	File file_h = FILE_H;

	EXPECT_EQ(++file_a, FILE_B);
	EXPECT_EQ(++file_b, FILE_C);
	EXPECT_EQ(++file_c, FILE_D);
	EXPECT_EQ(++file_d, FILE_E);
	EXPECT_EQ(++file_e, FILE_F);
	EXPECT_EQ(++file_f, FILE_G);
	EXPECT_EQ(++file_g, FILE_H);
	EXPECT_EQ(++file_h, FILE_NB); // Should wrap around to FILE_NB
}

// File decrement operators
TEST(FileTest, DecrementOperators) {
	File file_h = FILE_H;
	File file_g = FILE_G;
	File file_f = FILE_F;
	File file_e = FILE_E;
	File file_d = FILE_D;
	File file_c = FILE_C;
	File file_b = FILE_B;

	EXPECT_EQ(--file_h, FILE_G);
	EXPECT_EQ(--file_g, FILE_F);
	EXPECT_EQ(--file_f, FILE_E);
	EXPECT_EQ(--file_e, FILE_D);
	EXPECT_EQ(--file_d, FILE_C);
	EXPECT_EQ(--file_c, FILE_B);
	EXPECT_EQ(--file_b, FILE_A);
}

// Rank increment operators
TEST(RankTest, IncrementOperators) {
	Rank rank_1 = RANK_1;
	Rank rank_2 = RANK_2;
	Rank rank_3 = RANK_3;
	Rank rank_4 = RANK_4;
	Rank rank_5 = RANK_5;
	Rank rank_6 = RANK_6;
	Rank rank_7 = RANK_7;
	Rank rank_8 = RANK_8;

	EXPECT_EQ(++rank_1, RANK_2);
	EXPECT_EQ(++rank_2, RANK_3);
	EXPECT_EQ(++rank_3, RANK_4);
	EXPECT_EQ(++rank_4, RANK_5);
	EXPECT_EQ(++rank_5, RANK_6);
	EXPECT_EQ(++rank_6, RANK_7);
	EXPECT_EQ(++rank_7, RANK_8);
	EXPECT_EQ(++rank_8, RANK_NB); // Should wrap around to RANK_NB
}

// Rank decrement operators
TEST(RankTest, DecrementOperators) {
	Rank rank_8 = RANK_8;
	Rank rank_7 = RANK_7;
	Rank rank_6 = RANK_6;
	Rank rank_5 = RANK_5;
	Rank rank_4 = RANK_4;
	Rank rank_3 = RANK_3;
	Rank rank_2 = RANK_2;

	EXPECT_EQ(--rank_8, RANK_7);
	EXPECT_EQ(--rank_7, RANK_6);
	EXPECT_EQ(--rank_6, RANK_5);
	EXPECT_EQ(--rank_5, RANK_4);
	EXPECT_EQ(--rank_4, RANK_3);
	EXPECT_EQ(--rank_3, RANK_2);
	EXPECT_EQ(--rank_2, RANK_1);
}

// Direction sum operator
TEST(DirectionTest, SumOperators) {
	Direction n = NORTH;
	Direction s = SOUTH;

	EXPECT_EQ(n + n, 16);
	EXPECT_EQ(s + s, -16);
}

// Direction multiplication operator
TEST(DirectionTest, MultiplicationOperators) {
	Direction n = NORTH;
	Direction s = SOUTH;

	EXPECT_EQ(n * 2, 16);
	EXPECT_EQ(s * 2, -16);
}

// Square sum direction operator
TEST(SquareTest, SumDirectionOperators) {
	Square sq_a1 = SQ_A1;
	Square sq_h1 = SQ_H1;
	Direction n = NORTH;
	Direction e = EAST;

	EXPECT_EQ(sq_a1 + n, SQ_A2);
	EXPECT_EQ(sq_a1 + e, SQ_B1);

	EXPECT_EQ(sq_h1 + n, SQ_H2);
	// Note: sq_h1 + e (7 + 1 = 8 = SQ_A2) is an unchecked raw integer coordinate step.
	// Chessboard file-wrapping boundary checks are performed separately where appropriate.
	EXPECT_EQ(sq_h1 + e, SQ_A2);
}

// Toggle color
TEST(ColorTest, ToggleColor) {
	Color white = WHITE;
	Color black = BLACK;

	EXPECT_EQ(~white, BLACK);
	EXPECT_EQ(~black, WHITE);
}

// Flip rank of square
TEST(SquareTest, FlipRank) {
	EXPECT_EQ(flip_rank(SQ_A1), SQ_A8);
	EXPECT_EQ(flip_rank(SQ_H8), SQ_H1);
	EXPECT_EQ(flip_rank(SQ_A2), SQ_A7);
	EXPECT_EQ(flip_rank(SQ_B3), SQ_B6);
	EXPECT_EQ(flip_rank(SQ_C4), SQ_C5);
	EXPECT_EQ(flip_rank(SQ_D5), SQ_D4);
	EXPECT_EQ(flip_rank(SQ_E6), SQ_E3);
	EXPECT_EQ(flip_rank(SQ_F7), SQ_F2);
	EXPECT_EQ(flip_rank(SQ_G8), SQ_G1);
}

// Flip file of square
TEST(SquareTest, FlipFile) {
	EXPECT_EQ(flip_file(SQ_A1), SQ_H1);
	EXPECT_EQ(flip_file(SQ_H8), SQ_A8);
	EXPECT_EQ(flip_file(SQ_B2), SQ_G2);
	EXPECT_EQ(flip_file(SQ_C3), SQ_F3);
	EXPECT_EQ(flip_file(SQ_D4), SQ_E4);
	EXPECT_EQ(flip_file(SQ_E5), SQ_D5);
	EXPECT_EQ(flip_file(SQ_F6), SQ_C6);
	EXPECT_EQ(flip_file(SQ_G7), SQ_B7);
	EXPECT_EQ(flip_file(SQ_H8), SQ_A8);
}

// Swap color of piece
TEST(PieceTest, SwapColor) {
	Piece w_pawn = W_PAWN;
	Piece b_pawn = B_PAWN;
	EXPECT_EQ(~w_pawn, b_pawn);
	EXPECT_EQ(~b_pawn, w_pawn);
	Piece w_knight = W_KNIGHT;
	Piece b_knight = B_KNIGHT;
	EXPECT_EQ(~w_knight, b_knight);
	EXPECT_EQ(~b_knight, w_knight);
}

// Castling rights & color
TEST(CastlingRightsTest, CastlingRightsAndColor) {
	EXPECT_EQ(WHITE & KING_SIDE, WHITE_OO);
	EXPECT_EQ(BLACK & KING_SIDE, BLACK_OO);
	EXPECT_EQ(BLACK & ANY_CASTLING, BLACK_CASTLING);
	EXPECT_EQ(WHITE & ANY_CASTLING, WHITE_CASTLING);
}

// Make square
TEST(SquareTest, MakeSquare) {
	EXPECT_EQ(make_square(FILE_A, RANK_1), SQ_A1);
	EXPECT_EQ(make_square(FILE_H, RANK_8), SQ_H8);
	EXPECT_EQ(make_square(FILE_C, RANK_5), SQ_C5);
	EXPECT_EQ(make_square(FILE_F, RANK_3), SQ_F3);
	EXPECT_EQ(make_square(FILE_D, RANK_4), SQ_D4);
	EXPECT_EQ(make_square(FILE_E, RANK_6), SQ_E6);
	EXPECT_EQ(make_square(FILE_B, RANK_2), SQ_B2);
	EXPECT_EQ(make_square(FILE_G, RANK_7), SQ_G7);
}

// Make piece
TEST(PieceTest, MakePiece) {
	EXPECT_EQ(make_piece(WHITE, PAWN), W_PAWN);
	EXPECT_EQ(make_piece(BLACK, PAWN), B_PAWN);
	EXPECT_EQ(make_piece(WHITE, KNIGHT), W_KNIGHT);
	EXPECT_EQ(make_piece(BLACK, KNIGHT), B_KNIGHT);
	EXPECT_EQ(make_piece(WHITE, BISHOP), W_BISHOP);
	EXPECT_EQ(make_piece(BLACK, BISHOP), B_BISHOP);
	EXPECT_EQ(make_piece(WHITE, ROOK), W_ROOK);
	EXPECT_EQ(make_piece(BLACK, ROOK), B_ROOK);
	EXPECT_EQ(make_piece(WHITE, QUEEN), W_QUEEN);
	EXPECT_EQ(make_piece(BLACK, QUEEN), B_QUEEN);
	EXPECT_EQ(make_piece(WHITE, KING), W_KING);
	EXPECT_EQ(make_piece(BLACK, KING), B_KING);
}

// Type of piece
TEST(PieceTest, TypeOfPiece) {
	EXPECT_EQ(type_of(W_PAWN), PAWN);
	EXPECT_EQ(type_of(B_PAWN), PAWN);
	EXPECT_EQ(type_of(W_KNIGHT), KNIGHT);
	EXPECT_EQ(type_of(B_KNIGHT), KNIGHT);
	EXPECT_EQ(type_of(W_BISHOP), BISHOP);
	EXPECT_EQ(type_of(B_BISHOP), BISHOP);
	EXPECT_EQ(type_of(W_ROOK), ROOK);
	EXPECT_EQ(type_of(B_ROOK), ROOK);
	EXPECT_EQ(type_of(W_QUEEN), QUEEN);
	EXPECT_EQ(type_of(B_QUEEN), QUEEN);
	EXPECT_EQ(type_of(W_KING), KING);
	EXPECT_EQ(type_of(B_KING), KING);
}

// Color of piece
TEST(PieceTest, ColorOfPiece) {
	EXPECT_EQ(color_of(W_PAWN), WHITE);
	EXPECT_EQ(color_of(B_PAWN), BLACK);
	EXPECT_EQ(color_of(W_KNIGHT), WHITE);
	EXPECT_EQ(color_of(B_KNIGHT), BLACK);
	EXPECT_EQ(color_of(W_BISHOP), WHITE);
	EXPECT_EQ(color_of(B_BISHOP), BLACK);
	EXPECT_EQ(color_of(W_ROOK), WHITE);
	EXPECT_EQ(color_of(B_ROOK), BLACK);
	EXPECT_EQ(color_of(W_QUEEN), WHITE);
	EXPECT_EQ(color_of(B_QUEEN), BLACK);
	EXPECT_EQ(color_of(W_KING), WHITE);
	EXPECT_EQ(color_of(B_KING), BLACK);
}

// Square is valid
TEST(SquareTest, IsValidSquare) {
	EXPECT_TRUE(is_ok(SQ_A1));
	EXPECT_TRUE(is_ok(SQ_H8));
	EXPECT_TRUE(is_ok(SQ_C5));
	EXPECT_TRUE(is_ok(SQ_F3));
	EXPECT_TRUE(is_ok(SQ_D4));
	EXPECT_TRUE(is_ok(SQ_E6));
	EXPECT_TRUE(is_ok(SQ_B2));
	EXPECT_TRUE(is_ok(SQ_G7));
	EXPECT_FALSE(is_ok(SQ_NONE)); // Should be invalid
}

// File of square
TEST(SquareTest, FileOfSquare) {
	EXPECT_EQ(file_of(SQ_A1), FILE_A);
	EXPECT_EQ(file_of(SQ_H8), FILE_H);
	EXPECT_EQ(file_of(SQ_C5), FILE_C);
	EXPECT_EQ(file_of(SQ_F3), FILE_F);
	EXPECT_EQ(file_of(SQ_D4), FILE_D);
	EXPECT_EQ(file_of(SQ_E6), FILE_E);
	EXPECT_EQ(file_of(SQ_B2), FILE_B);
	EXPECT_EQ(file_of(SQ_G7), FILE_G);
}

// Rank of square
TEST(SquareTest, RankOfSquare) {
	EXPECT_EQ(rank_of(SQ_A1), RANK_1);
	EXPECT_EQ(rank_of(SQ_H8), RANK_8);
	EXPECT_EQ(rank_of(SQ_C5), RANK_5);
	EXPECT_EQ(rank_of(SQ_F3), RANK_3);
	EXPECT_EQ(rank_of(SQ_D4), RANK_4);
	EXPECT_EQ(rank_of(SQ_E6), RANK_6);
	EXPECT_EQ(rank_of(SQ_B2), RANK_2);
	EXPECT_EQ(rank_of(SQ_G7), RANK_7);
}

// Relative square
TEST(SquareTest, RelativeSquare) {
	EXPECT_EQ(relative_square(WHITE, SQ_A1), SQ_A1);
	EXPECT_EQ(relative_square(BLACK, SQ_A1), SQ_A8);
	EXPECT_EQ(relative_square(WHITE, SQ_B2), SQ_B2);
	EXPECT_EQ(relative_square(BLACK, SQ_B2), SQ_B7);
	EXPECT_EQ(relative_square(WHITE, SQ_C3), SQ_C3);
	EXPECT_EQ(relative_square(BLACK, SQ_C3), SQ_C6);
}

// Relative rank
TEST(SquareTest, RelativeRank) {
	EXPECT_EQ(relative_rank(WHITE, RANK_1), RANK_1);
	EXPECT_EQ(relative_rank(WHITE, RANK_2), RANK_2);
	EXPECT_EQ(relative_rank(WHITE, RANK_3), RANK_3);
	EXPECT_EQ(relative_rank(WHITE, RANK_4), RANK_4);
	EXPECT_EQ(relative_rank(WHITE, RANK_5), RANK_5);
	EXPECT_EQ(relative_rank(WHITE, RANK_6), RANK_6);
	EXPECT_EQ(relative_rank(WHITE, RANK_7), RANK_7);
	EXPECT_EQ(relative_rank(WHITE, RANK_8), RANK_8);

	EXPECT_EQ(relative_rank(BLACK, RANK_1), RANK_8);
	EXPECT_EQ(relative_rank(BLACK, RANK_2), RANK_7);
	EXPECT_EQ(relative_rank(BLACK, RANK_3), RANK_6);
	EXPECT_EQ(relative_rank(BLACK, RANK_4), RANK_5);
	EXPECT_EQ(relative_rank(BLACK, RANK_5), RANK_4);
	EXPECT_EQ(relative_rank(BLACK, RANK_6), RANK_3);
	EXPECT_EQ(relative_rank(BLACK, RANK_7), RANK_2);
	EXPECT_EQ(relative_rank(BLACK, RANK_8), RANK_1);
}

// Pawn push direction
TEST(DirectionTest, PawnPushDirection) {
	EXPECT_EQ(pawn_push(WHITE), NORTH);
	EXPECT_EQ(pawn_push(BLACK), SOUTH);
}

// Move
TEST(MoveTest, FromTo) {
	Move m = Move(SQ_A1, SQ_B2);
	EXPECT_EQ(m.from_sq(), SQ_A1);
	EXPECT_EQ(m.to_sq(), SQ_B2);
	EXPECT_EQ(m.type_of(), NORMAL);
	m = Move::make<PROMOTION>(SQ_C3, SQ_D4, KNIGHT);
	EXPECT_EQ(m.from_sq(), SQ_C3);
	EXPECT_EQ(m.to_sq(), SQ_D4);
	EXPECT_EQ(m.type_of(), PROMOTION);
	m = Move::make<EN_PASSANT>(SQ_E5, SQ_F6);
	EXPECT_EQ(m.from_sq(), SQ_E5);
	EXPECT_EQ(m.to_sq(), SQ_F6);
	EXPECT_EQ(m.type_of(), EN_PASSANT);
	m = Move::make<CASTLING>(SQ_G7, SQ_H8);
	EXPECT_EQ(m.from_sq(), SQ_G7);
	EXPECT_EQ(m.to_sq(), SQ_H8);
	EXPECT_EQ(m.type_of(), CASTLING);
}

// Move equality
TEST(MoveTest, Equality) {
	Move m1 = Move(SQ_A1, SQ_B2);
	Move m2 = Move(SQ_A1, SQ_B2);
	Move m3 = Move(SQ_C3, SQ_D4);
	EXPECT_TRUE(m1 == m2);
	EXPECT_FALSE(m1 == m3);
	EXPECT_FALSE(m2 == m3);
	EXPECT_TRUE(m1 != m3);
	EXPECT_TRUE(m2 != m3);
}

// Move string and special moves
TEST(MoveTest, ToStringAndSpecial) {
	EXPECT_EQ(Move::none().to_string(), "none");
	EXPECT_EQ(Move::null().to_string(), "0000");

	Move m1 = Move(SQ_E2, SQ_E4);
	EXPECT_EQ(m1.to_string(), "e2e4");
	EXPECT_TRUE(m1.is_ok());

	Move m2 = Move::make<PROMOTION>(SQ_D7, SQ_D8, QUEEN);
	EXPECT_EQ(m2.to_string(), "d7d8q");
	EXPECT_TRUE(m2.is_ok());

	Move m3 = Move::make<PROMOTION>(SQ_C7, SQ_B8, KNIGHT);
	EXPECT_EQ(m3.to_string(), "c7b8n");

	EXPECT_FALSE(Move::none().is_ok());
	EXPECT_FALSE(Move::null().is_ok());
}

// to_index helper
TEST(MoveTest, ToIndexHelper) {
	EXPECT_EQ(to_index(SQ_A1), 0);
	EXPECT_EQ(to_index(SQ_H8), 63);
	EXPECT_EQ(to_index(WHITE), 0);
	EXPECT_EQ(to_index(BLACK), 1);
	EXPECT_EQ(to_index(PAWN), 1);
}
