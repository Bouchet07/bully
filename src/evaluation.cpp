#include "evaluation.h"
#include "bitboard.h"
#include "nnue.h"
#include <utility>
#include <iostream>
#include <format>

namespace Bully {
namespace Eval {

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

// Precomputed file spans and adjacent files
constexpr std::array<Bitboard, FILE_NB> AdjacentFiles = []() {
    std::array<Bitboard, FILE_NB> arr{};
    for (File f = FILE_A; f < FILE_NB; ++f) {
        Bitboard mask = 0;
        if (f > FILE_A) mask |= file_bb(f - 1);
        if (f < FILE_H) mask |= file_bb(f + 1);
        arr[to_index(f)] = mask;
    }
    return arr;
}();

constexpr std::array<Bitboard, SQUARE_NB> FrontSpansWhite = []() {
    std::array<Bitboard, SQUARE_NB> arr{};
    for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq) {
        int shift_amt = 8 * (std::to_underlying(rank_of(sq)) + 1);
        arr[to_index(sq)] = (shift_amt >= 64) ? 0ULL : (~0ULL << shift_amt);
    }
    return arr;
}();

constexpr std::array<Bitboard, SQUARE_NB> FrontSpansBlack = []() {
    std::array<Bitboard, SQUARE_NB> arr{};
    for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq) {
        arr[to_index(sq)] = (1ULL << (8 * std::to_underlying(rank_of(sq)))) - 1;
    }
    return arr;
}();

// Compute the game phase (24 = full middlegame, 0 = pure endgame)
[[nodiscard]] static int calculate_phase(const Position& pos) {
    int phase = 24;
    
    int knights = popcnt(pos.pieces(KNIGHT));
    int bishops = popcnt(pos.pieces(BISHOP));
    int rooks   = popcnt(pos.pieces(ROOK));
    int queens  = popcnt(pos.pieces(QUEEN));

    phase -= (4 - knights) * 1;
    phase -= (4 - bishops) * 1;
    phase -= (4 - rooks) * 2;
    phase -= (2 - queens) * 4;

    return std::clamp(phase, 0, 24);
}

template<Color Us>
[[nodiscard]] static std::pair<Value, Value> evaluate_pawns(const Position& pos) {
    constexpr Color Them = ~Us;
    const Bitboard our_pawns = pos.pieces(Us, PAWN);
    const Bitboard opponent_pawns = pos.pieces(Them, PAWN);
    
    Value mg = 0;
    Value eg = 0;
    
    Bitboard pawns = our_pawns;
    while (pawns) {
        Square sq = pop_lsb(pawns);
        
        Square rel_sq = relative_square(Us, sq);
        size_t idx = to_index(rel_sq);
        
        Value p_mg = PawnPST[idx];
        Value p_eg = PawnPST[idx];
        
        File f = file_of(sq);
        Bitboard adjacent = AdjacentFiles[to_index(f)];
        
        // Isolated Pawn Penalty
        if ((our_pawns & adjacent) == 0) {
            p_mg -= 15;
            p_eg -= 15;
        }
        
        // Doubled Pawn Penalty (once per file, when sq is the LSB on that file)
        Bitboard same_file = our_pawns & file_bb(sq);
        if (more_than_one(same_file) && sq == lsb(same_file)) {
            p_mg -= 15;
            p_eg -= 15;
        }
        
        // Passed Pawn Bonus
        Bitboard front_span = (Us == WHITE) ? FrontSpansWhite[to_index(sq)] : FrontSpansBlack[to_index(sq)];
        Bitboard files_to_check = file_bb(sq) | adjacent;
        if ((opponent_pawns & files_to_check & front_span) == 0) {
            int rel_rank = std::to_underlying(relative_rank(Us, rank_of(sq)));
            p_mg += 10 * rel_rank;
            p_eg += 20 * rel_rank;
        }
        
        mg += PawnValue + p_mg;
        eg += PawnValue + p_eg;
    }
    
    return { mg, eg };
}

template<Color Us>
[[nodiscard]] static std::pair<Value, Value> evaluate_pieces(const Position& pos) {
    Value mg = 0;
    Value eg = 0;
    
    Bitboard knights = pos.pieces(Us, KNIGHT);
    while (knights) {
        Square sq = pop_lsb(knights);
        size_t idx = to_index(relative_square(Us, sq));
        mg += KnightValue + KnightPST[idx];
        eg += KnightValue + KnightPST[idx];
    }
    
    Bitboard bishops = pos.pieces(Us, BISHOP);
    while (bishops) {
        Square sq = pop_lsb(bishops);
        size_t idx = to_index(relative_square(Us, sq));
        mg += BishopValue + BishopPST[idx];
        eg += BishopValue + BishopPST[idx];
    }
    
    Bitboard rooks = pos.pieces(Us, ROOK);
    while (rooks) {
        Square sq = pop_lsb(rooks);
        size_t idx = to_index(relative_square(Us, sq));
        mg += RookValue + RookPST[idx];
        eg += RookValue + RookPST[idx];
    }
    
    Bitboard queens = pos.pieces(Us, QUEEN);
    while (queens) {
        Square sq = pop_lsb(queens);
        size_t idx = to_index(relative_square(Us, sq));
        mg += QueenValue + QueenPST[idx];
        eg += QueenValue + QueenPST[idx];
    }
    
    Bitboard king = pos.pieces(Us, KING);
    if (king) {
        Square sq = lsb(king);
        size_t idx = to_index(relative_square(Us, sq));
        mg += KingMiddlegamePST[idx];
        eg += KingEndgamePST[idx];
    }
    
    // Bishop pair bonus
    int bishop_count = popcnt(pos.pieces(Us, BISHOP));
    if (bishop_count >= 2) {
        mg += 30;
        eg += 40;
    }
    
    return { mg, eg };
}

Value evaluate(const Position& pos) {
    if (NNUE::use_nnue && NNUE::is_ready()) {
        return NNUE::evaluate(pos);
    }

    auto [mg_white_pawns, eg_white_pawns] = evaluate_pawns<WHITE>(pos);
    auto [mg_white_pieces, eg_white_pieces] = evaluate_pieces<WHITE>(pos);
    
    auto [mg_black_pawns, eg_black_pawns] = evaluate_pawns<BLACK>(pos);
    auto [mg_black_pieces, eg_black_pieces] = evaluate_pieces<BLACK>(pos);
    
    Value mg_white = mg_white_pawns + mg_white_pieces;
    Value eg_white = eg_white_pawns + eg_white_pieces;
    
    Value mg_black = mg_black_pawns + mg_black_pieces;
    Value eg_black = eg_black_pawns + eg_black_pieces;
    
    int phase = calculate_phase(pos);
    
    Value mg_score = mg_white - mg_black;
    Value eg_score = eg_white - eg_black;
    
    Value score = (mg_score * phase + eg_score * (24 - phase)) / 24;
    
    return (pos.side_to_move() == WHITE) ? score : -score;
}

void print_detailed_eval(const Position& pos, bool use_color) {
    auto [mg_white_pawns, eg_white_pawns] = evaluate_pawns<WHITE>(pos);
    auto [mg_white_pieces, eg_white_pieces] = evaluate_pieces<WHITE>(pos);
    
    auto [mg_black_pawns, eg_black_pawns] = evaluate_pawns<BLACK>(pos);
    auto [mg_black_pieces, eg_black_pieces] = evaluate_pieces<BLACK>(pos);
    
    Value mg_white = mg_white_pawns + mg_white_pieces;
    Value eg_white = eg_white_pawns + eg_white_pieces;
    
    Value mg_black = mg_black_pawns + mg_black_pieces;
    Value eg_black = eg_black_pawns + eg_black_pieces;
    
    int phase = calculate_phase(pos);
    
    Value mg_score = mg_white - mg_black;
    Value eg_score = eg_white - eg_black;
    
    Value score = (mg_score * phase + eg_score * (24 - phase)) / 24;
    Value relative_score = (pos.side_to_move() == WHITE) ? score : -score;

    std::string reset   = use_color ? "\033[0m" : "";
    std::string yellow  = use_color ? "\033[1;33m" : "";
    std::string blue    = use_color ? "\033[1;34m" : "";
    std::string green   = use_color ? "\033[1;32m" : "";
    std::string magenta = use_color ? "\033[1;35m" : "";

    std::cout << blue << "========================================================\n" << reset;
    std::cout << yellow << "                 Bully Static Evaluation                 \n" << reset;
    std::cout << blue << "========================================================\n" << reset;
    std::cout << std::format("  {}Phase{} : {}{} (24 = Middlegame, 0 = Endgame)\n", 
                 green, reset, magenta, phase, reset);
    std::cout << blue << "--------------------------------------------------------\n" << reset;
    std::cout << "  Category           |  White (MG/EG)  |  Black (MG/EG)   \n";
    std::cout << blue << "--------------------------------------------------------\n" << reset;
    std::cout << std::format("  Pawns & PST        |   {:4} / {:4}   |   {:4} / {:4}   \n", 
                 mg_white_pawns, eg_white_pawns, mg_black_pawns, eg_black_pawns);
    std::cout << std::format("  Pieces & PST       |   {:4} / {:4}   |   {:4} / {:4}   \n", 
                 mg_white_pieces, eg_white_pieces, mg_black_pieces, eg_black_pieces);
    std::cout << blue << "--------------------------------------------------------\n" << reset;
    std::cout << std::format("  Total Term         |   {:4} / {:4}   |   {:4} / {:4}   \n", 
                 mg_white, eg_white, mg_black, eg_black);
    std::cout << blue << "--------------------------------------------------------\n" << reset;
    std::cout << std::format("  {}Middlegame Score{}    : {}{:+6}{}\n", green, reset, magenta, mg_score, reset);
    std::cout << std::format("  {}Endgame Score{}       : {}{:+6}{}\n", green, reset, magenta, eg_score, reset);
    std::cout << std::format("  {}Interpolated Score{}  : {}{:+6}{}\n", green, reset, magenta, score, reset);
    std::cout << std::format("  {}Classical Rel Score{} : {}{:+6}{} (relative to {})\n", 
                 green, reset, magenta, relative_score, reset, pos.side_to_move() == WHITE ? "White" : "Black");
    std::cout << blue << "--------------------------------------------------------\n" << reset;
    if (NNUE::use_nnue && NNUE::is_ready()) {
        Value nnue_val = NNUE::evaluate(pos);
        std::cout << std::format("  {}NNUE Score{}          : {}{:+6}{} (active evaluation mode)\n",
                     green, reset, magenta, nnue_val, reset);
    } else {
        std::cout << std::format("  {}NNUE Evaluation{}    : {}OFF{} (load .nnue net & set 'options nnue on')\n",
                     green, reset, magenta, reset);
    }
    std::cout << blue << "========================================================\n" << reset;
}

} // namespace Eval
} // namespace Bully
