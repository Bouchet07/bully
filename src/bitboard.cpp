#include <print>

#include "bitboard.h"

uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];

Bitboard RookTable[0x19000];   // To store rook attacks
Bitboard BishopTable[0x1480];  // To store bishop attacks

void init_bitboards() {
    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
        for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
            SquareDistance[s1][s2] = std::max(distance<File>(s1, s2), distance<Rank>(s1, s2));
    

}

void pretty_print(Bitboard bitboard, bool Use_UTF8) {
    std::print("\n");

    // Top border
    if (Use_UTF8) {
        std::println("   ┌───┬───┬───┬───┬───┬───┬───┬───┐");
    }
    else {
        std::println("   +---+---+---+---+---+---+---+---+");
    }

    for (Rank rank = RANK_8; rank >= RANK_1; --rank) {
        // Row number
        if (Use_UTF8) {
            std::print(" {} │", rank + 1);
        }
        else {
            std::print(" {} |", rank + 1);
        }

        for (File file = FILE_A; file <= FILE_H; ++file) {
            Square square = make_square(file, rank);
            if (Use_UTF8) {
                std::print(" {} │", (bitboard & square) ? "●" : " ");
            }
            else {
                std::print(" {} │", (bitboard & square) ? "1" : " ");
            }
        }

        std::print("\n");
        if (rank) {
            if (Use_UTF8) {
                std::println("   ├───┼───┼───┼───┼───┼───┼───┼───┤"); // Inner dividers
            }
            else {
                std::println("   +---+---+---+---+---+---+---+---+"); // Inner dividers
            }
        }
    }

    // Bottom border
    if (Use_UTF8) {
        std::println("   └───┴───┴───┴───┴───┴───┴───┴───┘");
    }
    else {
        std::println("   +---+---+---+---+---+---+---+---+");
    }
    std::println("     a   b   c   d   e   f   g   h\n");
    std::println("   Bitboard: {}d\n", bitboard);
}