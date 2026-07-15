#include "evaluation.h"
#include "bitboard.h"

namespace Bully {
namespace Eval {

// Pops and returns the least significant bit (LSB) from a bitboard
static inline Square pop_LSB(Bitboard& b) {
    Square s = get_LSB(b);
    b ^= s;
    return s;
}

// ============================================================================
// Piece-Square Tables (PST) for White
// Evaluated from Rank 1 (indices 0-7) to Rank 8 (indices 56-63)
// ============================================================================

// Pawns want to advance, especially in the center
constexpr std::array<int, 64> PawnPST = {
     0,   0,   0,   0,   0,   0,   0,   0, // Rank 1
     5,   5,   5,   5,   5,   5,   5,   5, // Rank 2
    10,  10,  15,  20,  20,  15,  10,  10, // Rank 3
    15,  15,  20,  30,  30,  20,  15,  15, // Rank 4
    20,  20,  25,  35,  35,  25,  20,  20, // Rank 5
    30,  30,  35,  40,  40,  35,  30,  30, // Rank 6
    50,  50,  50,  50,  50,  50,  50,  50, // Rank 7
     0,   0,   0,   0,   0,   0,   0,   0  // Rank 8
};

// Knights want to sit in the center, not on the edges
constexpr std::array<int, 64> KnightPST = {
    -50, -40, -30, -30, -30, -30, -40, -50, // Rank 1
    -40, -20,   0,   0,   0,   0, -20, -40, // Rank 2
    -30,   0,  10,  15,  15,  10,   0, -30, // Rank 3
    -30,   5,  15,  20,  20,  15,   5, -30, // Rank 4
    -30,   0,  15,  20,  20,  15,   0, -30, // Rank 5
    -30,   5,  10,  15,  15,  10,   5, -30, // Rank 6
    -40, -20,   0,   5,   5,   0, -20, -40, // Rank 7
    -50, -40, -30, -30, -30, -30, -40, -50  // Rank 8
};

// Bishops control long diagonals and want active center squares
constexpr std::array<int, 64> BishopPST = {
    -20, -10, -10, -10, -10, -10, -10, -20, // Rank 1
    -10,   0,   0,   5,   5,   0,   0, -10, // Rank 2
    -10,   0,   5,  10,  10,   5,   0, -10, // Rank 3
    -10,   5,   5,  10,  10,   5,   5, -10, // Rank 4
    -10,   0,  10,  10,  10,  10,   0, -10, // Rank 5
    -10,  10,  10,  10,  10,  10,  10, -10, // Rank 6
    -10,   5,   0,   0,   0,   0,   5, -10, // Rank 7
    -20, -10, -10, -10, -10, -10, -10, -20  // Rank 8
};

// Rooks want open files and get a major bonus on the 7th rank
constexpr std::array<int, 64> RookPST = {
      0,   0,   0,   5,   5,   0,   0,   0, // Rank 1
     -5,   0,   0,   0,   0,   0,   0,  -5, // Rank 2
     -5,   0,   0,   0,   0,   0,   0,  -5, // Rank 3
     -5,   0,   0,   0,   0,   0,   0,  -5, // Rank 4
     -5,   0,   0,   0,   0,   0,   0,  -5, // Rank 5
     -5,   0,   0,   0,   0,   0,   0,  -5, // Rank 6
     20,  20,  20,  20,  20,  20,  20,  20, // Rank 7 (Strong attack)
      0,   0,   0,   5,   5,   0,   0,   0  // Rank 8
};

// Queens should not jump out too early, but control the center in the middlegame
constexpr std::array<int, 64> QueenPST = {
    -20, -10, -10,  -5,  -5, -10, -10, -20, // Rank 1
    -10,   0,   0,   0,   0,   0,   0, -10, // Rank 2
    -10,   0,   5,   5,   5,   5,   0, -10, // Rank 3
     -5,   0,   5,   5,   5,   5,   0,  -5, // Rank 4
      0,   0,   5,   5,   5,   5,   0,  -5, // Rank 5
    -10,   5,   5,   5,   5,   5,   0, -10, // Rank 6
    -10,   0,   5,   0,   0,   5,   0, -10, // Rank 7
    -20, -10, -10,  -5,  -5, -10, -10, -20  // Rank 8
};

// King Safety: Kings want to castle in the Middlegame
constexpr std::array<int, 64> KingMiddlegamePST = {
     20,  30,  10,   0,   0,  10,  30,  20, // Rank 1
     20,  20,   0,   0,   0,   0,  20,  20, // Rank 2
    -10, -20, -20, -20, -20, -20, -20, -10, // Rank 3
    -20, -30, -30, -40, -40, -30, -30, -20, // Rank 4
    -30, -40, -40, -50, -50, -40, -40, -30, // Rank 5
    -30, -40, -40, -50, -50, -40, -40, -30, // Rank 6
    -30, -40, -40, -50, -50, -40, -40, -30, // Rank 7
    -40, -50, -50, -50, -50, -50, -50, -40  // Rank 8
};

// Active King: Kings must move to the center in the Endgame
constexpr std::array<int, 64> KingEndgamePST = {
    -50, -40, -30, -20, -20, -30, -40, -50, // Rank 1
    -30, -20, -10,   0,   0, -10, -20, -30, // Rank 2
    -30, -10,  20,  30,  30,  20, -10, -30, // Rank 3
    -30, -10,  30,  40,  40,  30, -10, -30, // Rank 4
    -30, -10,  30,  40,  40,  30, -10, -30, // Rank 5
    -30, -10,  20,  30,  30,  20, -10, -30, // Rank 6
    -30, -30,   0,   0,   0,   0, -30, -30, // Rank 7
    -50, -30, -30, -30, -30, -30, -30, -50  // Rank 8
};

// Compute the game phase (24 = full middlegame, 0 = pure endgame)
[[nodiscard]] static int calculate_phase(const Position& pos) {
    int phase = 24;
    
    // Starting material counts:
    // 4 Knights * 1 phase point
    // 4 Bishops * 1 phase point
    // 4 Rooks * 2 phase points
    // 2 Queens * 4 phase points
    // Total starting phase points = 24
    
    int knights = popcnt(pos.pieces(KNIGHT));
    int bishops = popcnt(pos.pieces(BISHOP));
    int rooks   = popcnt(pos.pieces(ROOK));
    int queens  = popcnt(pos.pieces(QUEEN));

    // Remaining phase points:
    phase -= (4 - knights) * 1;
    phase -= (4 - bishops) * 1;
    phase -= (4 - rooks) * 2;
    phase -= (2 - queens) * 4;

    if (phase < 0)  phase = 0;
    if (phase > 24) phase = 24;
    
    return phase;
}

Value evaluate(const Position& pos) {
    Value mg_white = 0;
    Value mg_black = 0;
    Value eg_white = 0;
    Value eg_black = 0;

    // --- 1. WHITE PIECES EVALUATION ---
    Bitboard white_pawns_all = pos.pieces(WHITE, PAWN);
    Bitboard pawns = white_pawns_all;
    while (pawns) {
        Square sq = pop_LSB(pawns);
        size_t idx = to_index(sq);
        
        int mg = PawnPST[idx];
        int eg = PawnPST[idx];
        
        // Adjacent files mask
        Bitboard adjacent_files = 0;
        File f = file_of(sq);
        if (f > FILE_A) adjacent_files |= file_bb(static_cast<File>(std::to_underlying(f) - 1));
        if (f < FILE_H) adjacent_files |= file_bb(static_cast<File>(std::to_underlying(f) + 1));
        
        // Isolated Pawn Penalty
        if ((white_pawns_all & adjacent_files) == 0) {
            mg -= 15;
            eg -= 15;
        }
        
        // Doubled Pawn Penalty (once per file, when sq is the LSB on that file)
        Bitboard same_file = white_pawns_all & file_bb(sq);
        if (more_than_one(same_file) && sq == get_LSB(same_file)) {
            mg -= 15;
            eg -= 15;
        }
        
        // Passed Pawn Bonus
        Bitboard front_span = ~0ULL << (8 * (std::to_underlying(rank_of(sq)) + 1));
        Bitboard files_to_check = file_bb(sq) | adjacent_files;
        if ((pos.pieces(BLACK, PAWN) & files_to_check & front_span) == 0) {
            int relative_rank = std::to_underlying(rank_of(sq)); // 0-7 (1st to 8th rank)
            mg += 10 * relative_rank;
            eg += 20 * relative_rank;
        }
        
        mg_white = static_cast<Value>(mg_white + PawnValue + mg);
        eg_white = static_cast<Value>(eg_white + PawnValue + eg);
    }

    Bitboard knights = pos.pieces(WHITE, KNIGHT);
    while (knights) {
        Square sq = pop_LSB(knights);
        size_t idx = to_index(sq);
        mg_white = static_cast<Value>(mg_white + KnightValue + KnightPST[idx]);
        eg_white = static_cast<Value>(eg_white + KnightValue + KnightPST[idx]);
    }

    Bitboard bishops = pos.pieces(WHITE, BISHOP);
    while (bishops) {
        Square sq = pop_LSB(bishops);
        size_t idx = to_index(sq);
        mg_white = static_cast<Value>(mg_white + BishopValue + BishopPST[idx]);
        eg_white = static_cast<Value>(eg_white + BishopValue + BishopPST[idx]);
    }

    Bitboard rooks = pos.pieces(WHITE, ROOK);
    while (rooks) {
        Square sq = pop_LSB(rooks);
        size_t idx = to_index(sq);
        mg_white = static_cast<Value>(mg_white + RookValue + RookPST[idx]);
        eg_white = static_cast<Value>(eg_white + RookValue + RookPST[idx]);
    }

    Bitboard queens = pos.pieces(WHITE, QUEEN);
    while (queens) {
        Square sq = pop_LSB(queens);
        size_t idx = to_index(sq);
        mg_white = static_cast<Value>(mg_white + QueenValue + QueenPST[idx]);
        eg_white = static_cast<Value>(eg_white + QueenValue + QueenPST[idx]);
    }

    Bitboard king = pos.pieces(WHITE, KING);
    if (king) {
        Square sq = get_LSB(king);
        size_t idx = to_index(sq);
        mg_white = static_cast<Value>(mg_white + KingMiddlegamePST[idx]);
        eg_white = static_cast<Value>(eg_white + KingEndgamePST[idx]);
    }

    // --- 2. BLACK PIECES EVALUATION ---
    Bitboard black_pawns_all = pos.pieces(BLACK, PAWN);
    pawns = black_pawns_all;
    while (pawns) {
        Square sq = pop_LSB(pawns);
        size_t idx = to_index(flip_rank(sq));
        
        int mg = PawnPST[idx];
        int eg = PawnPST[idx];
        
        // Adjacent files mask
        Bitboard adjacent_files = 0;
        File f = file_of(sq);
        if (f > FILE_A) adjacent_files |= file_bb(static_cast<File>(std::to_underlying(f) - 1));
        if (f < FILE_H) adjacent_files |= file_bb(static_cast<File>(std::to_underlying(f) + 1));
        
        // Isolated Pawn Penalty
        if ((black_pawns_all & adjacent_files) == 0) {
            mg -= 15;
            eg -= 15;
        }
        
        // Doubled Pawn Penalty (once per file, when sq is the LSB on that file)
        Bitboard same_file = black_pawns_all & file_bb(sq);
        if (more_than_one(same_file) && sq == get_LSB(same_file)) {
            mg -= 15;
            eg -= 15;
        }
        
        // Passed Pawn Bonus
        Bitboard front_span = (1ULL << (8 * std::to_underlying(rank_of(sq)))) - 1;
        Bitboard files_to_check = file_bb(sq) | adjacent_files;
        if ((pos.pieces(WHITE, PAWN) & files_to_check & front_span) == 0) {
            int relative_rank = 7 - std::to_underlying(rank_of(sq)); // 0-7 (8th to 1st rank relative to black)
            mg += 10 * relative_rank;
            eg += 20 * relative_rank;
        }
        
        mg_black = static_cast<Value>(mg_black + PawnValue + mg);
        eg_black = static_cast<Value>(eg_black + PawnValue + eg);
    }

    knights = pos.pieces(BLACK, KNIGHT);
    while (knights) {
        Square sq = pop_LSB(knights);
        size_t idx = to_index(flip_rank(sq));
        mg_black = static_cast<Value>(mg_black + KnightValue + KnightPST[idx]);
        eg_black = static_cast<Value>(eg_black + KnightValue + KnightPST[idx]);
    }

    bishops = pos.pieces(BLACK, BISHOP);
    while (bishops) {
        Square sq = pop_LSB(bishops);
        size_t idx = to_index(flip_rank(sq));
        mg_black = static_cast<Value>(mg_black + BishopValue + BishopPST[idx]);
        eg_black = static_cast<Value>(eg_black + BishopValue + BishopPST[idx]);
    }

    rooks = pos.pieces(BLACK, ROOK);
    while (rooks) {
        Square sq = pop_LSB(rooks);
        size_t idx = to_index(flip_rank(sq));
        mg_black = static_cast<Value>(mg_black + RookValue + RookPST[idx]);
        eg_black = static_cast<Value>(eg_black + RookValue + RookPST[idx]);
    }

    queens = pos.pieces(BLACK, QUEEN);
    while (queens) {
        Square sq = pop_LSB(queens);
        size_t idx = to_index(flip_rank(sq));
        mg_black = static_cast<Value>(mg_black + QueenValue + QueenPST[idx]);
        eg_black = static_cast<Value>(eg_black + QueenValue + QueenPST[idx]);
    }

    king = pos.pieces(BLACK, KING);
    if (king) {
        Square sq = get_LSB(king);
        size_t idx = to_index(flip_rank(sq));
        mg_black = static_cast<Value>(mg_black + KingMiddlegamePST[idx]);
        eg_black = static_cast<Value>(eg_black + KingEndgamePST[idx]);
    }

    // --- 2.5. BISHOP PAIR BONUS ---
    int white_bishops = popcnt(pos.pieces(WHITE, BISHOP));
    if (white_bishops >= 2) {
        mg_white = static_cast<Value>(mg_white + 30);
        eg_white = static_cast<Value>(eg_white + 40);
    }
    int black_bishops = popcnt(pos.pieces(BLACK, BISHOP));
    if (black_bishops >= 2) {
        mg_black = static_cast<Value>(mg_black + 30);
        eg_black = static_cast<Value>(eg_black + 40);
    }

    // --- 3. TAPERED INTERPOLATION ---
    int phase = calculate_phase(pos);
    
    Value mg_score = static_cast<Value>(mg_white - mg_black);
    Value eg_score = static_cast<Value>(eg_white - eg_black);

    // Interpolate score between Middlegame and Endgame phase points
    Value score = static_cast<Value>((mg_score * phase + eg_score * (24 - phase)) / 24);

    // Return evaluation relative to the side to move
    return (pos.side_to_move() == WHITE) ? score : static_cast<Value>(-score);
}

} // namespace Eval
} // namespace Bully
