#include <print>

#include "bitboard.h"

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