#include <iostream>
#include <format>
#include <algorithm>
#include <cmath>

#include "bitboard.h"

namespace Bully {

DistanceTable      SquareDistance;
SquareTable        LineBB;
SquareTable        BetweenBB;

void init_bitboards() {
    // 1. Initialize SquareDistance
    for (Square s1 = SQ_A1; s1 < SQUARE_NB; ++s1) {
        for (Square s2 = SQ_A1; s2 < SQUARE_NB; ++s2) {
            SquareDistance[to_index(s1)][to_index(s2)] = static_cast<uint8_t>(
                std::max(distance(file_of(s1), file_of(s2)), distance(rank_of(s1), rank_of(s2)))
            );
        }
    }

    // 2. Initialize LineBB and BetweenBB
    for (Square s1 = SQ_A1; s1 < SQUARE_NB; ++s1) {
        for (Square s2 = SQ_A1; s2 < SQUARE_NB; ++s2) {
            LineBB[to_index(s1)][to_index(s2)] = 0;
            BetweenBB[to_index(s1)][to_index(s2)] = 0;

            if (s1 == s2) {
                continue;
            }

            int f1 = std::to_underlying(file_of(s1)), r1 = std::to_underlying(rank_of(s1));
            int f2 = std::to_underlying(file_of(s2)), r2 = std::to_underlying(rank_of(s2));
            int df = f2 - f1;
            int dr = r2 - r1;

            bool aligned_squares = (df == 0) || (dr == 0) || (std::abs(df) == std::abs(dr));
            if (!aligned_squares) {
                BetweenBB[to_index(s1)][to_index(s2)] = square_bb(s2);
                continue;
            }

            // Determine direction step
            int step_f = (df > 0) ? 1 : ((df < 0) ? -1 : 0);
            int step_r = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
            Direction step = static_cast<Direction>((step_r * 8) + step_f);

            // Compute LineBB: extend infinitely in both directions from s1
            Bitboard line = 0;
            
            // forward
            Square s = s1;
            while (true) {
                line |= square_bb(s);
                int next_f = std::to_underlying(file_of(s)) + step_f;
                int next_r = std::to_underlying(rank_of(s)) + step_r;
                if (next_f < 0 || next_f >= 8 || next_r < 0 || next_r >= 8) break;
                s = s + step;
            }
            
            // backward
            s = s1;
            while (true) {
                line |= square_bb(s);
                int next_f = std::to_underlying(file_of(s)) - step_f;
                int next_r = std::to_underlying(rank_of(s)) - step_r;
                if (next_f < 0 || next_f >= 8 || next_r < 0 || next_r >= 8) break;
                s = s - step;
            }
            LineBB[to_index(s1)][to_index(s2)] = line;

            // Compute BetweenBB: strictly between s1 and s2 (excluding s1, including s2)
            Bitboard between = 0;
            s = s1 + step;
            while (s != s2) {
                between |= square_bb(s);
                s = s + step;
            }
            between |= square_bb(s2); // include s2
            BetweenBB[to_index(s1)][to_index(s2)] = between;
        }
    }
}

void pretty_print(Bitboard bitboard, bool Use_UTF8) {
    std::cout << "\n";

    // Top border
    if (Use_UTF8) {
        std::cout << "   ┌───┬───┬───┬───┬───┬───┬───┬───┐\n";
    }
    else {
        std::cout << "   +---+---+---+---+---+---+---+---+\n";
    }

    for (Rank rank = RANK_8; rank >= RANK_1; --rank) {
        // Row number
        if (Use_UTF8) {
            std::cout << std::format(" {} │", std::to_underlying(rank) + 1);
        }
        else {
            std::cout << std::format(" {} |", std::to_underlying(rank) + 1);
        }

        for (File file = FILE_A; file < FILE_NB; ++file) {
            Square square = make_square(file, rank);
            if (Use_UTF8) {
                std::cout << std::format(" {} │", (bitboard & square) ? "●" : " ");
            }
            else {
                std::cout << std::format(" {} |", (bitboard & square) ? "1" : " ");
            }
        }

        std::cout << "\n";
        if (rank > RANK_1) {
            if (Use_UTF8) {
                std::cout << "   ├───┼───┼───┼───┼───┼───┼───┼───┤\n"; // Inner dividers
            }
            else {
                std::cout << "   +---+---+---+---+---+---+---+---+\n"; // Inner dividers
            }
        }
    }

    // Bottom border
    if (Use_UTF8) {
        std::cout << "   └───┴───┴───┴───┴───┴───┴───┴───┘\n";
    }
    else {
        std::cout << "   +---+---+---+---+---+---+---+---+\n";
    }
    std::cout << "     a   b   c   d   e   f   g   h\n\n";
    std::cout << std::format("   Bitboard: {:#018x}ULL\n\n", bitboard);
}

} // namespace Bully