#include "evaluation.h"
#include "bitboard.h"
#include <utility>
#include <iostream>
#include <format>

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

// Precomputed file spans and adjacent files
constexpr std::array<Bitboard, 8> AdjacentFiles = []() {
    std::array<Bitboard, 8> arr{};
    for (int i = 0; i < 8; ++i) {
        File f = static_cast<File>(i);
        Bitboard mask = 0;
        if (f > FILE_A) mask |= file_bb(static_cast<File>(std::to_underlying(f) - 1));
        if (f < FILE_H) mask |= file_bb(static_cast<File>(std::to_underlying(f) + 1));
        arr[to_index(f)] = mask;
    }
    return arr;
}();

constexpr std::array<Bitboard, 64> FrontSpansWhite = []() {
    std::array<Bitboard, 64> arr{};
    for (int i = 0; i < 64; ++i) {
        Square sq = static_cast<Square>(i);
        int shift_amt = 8 * (std::to_underlying(rank_of(sq)) + 1);
        arr[to_index(sq)] = (shift_amt >= 64) ? 0ULL : (~0ULL << shift_amt);
    }
    return arr;
}();

constexpr std::array<Bitboard, 64> FrontSpansBlack = []() {
    std::array<Bitboard, 64> arr{};
    for (int i = 0; i < 64; ++i) {
        Square sq = static_cast<Square>(i);
        arr[to_index(sq)] = (1ULL << (8 * std::to_underlying(rank_of(sq)))) - 1;
    }
    return arr;
}();

std::array<std::array<int, 64>, PIECE_NB> PST_MG{};
std::array<std::array<int, 64>, PIECE_NB> PST_EG{};
std::array<int, PIECE_NB>                 PhaseWeight{};

void init_eval() {
    PhaseWeight.fill(0);
    PhaseWeight[to_index(W_KNIGHT)] = 1;
    PhaseWeight[to_index(B_KNIGHT)] = 1;
    PhaseWeight[to_index(W_BISHOP)] = 1;
    PhaseWeight[to_index(B_BISHOP)] = 1;
    PhaseWeight[to_index(W_ROOK)]   = 2;
    PhaseWeight[to_index(B_ROOK)]   = 2;
    PhaseWeight[to_index(W_QUEEN)]  = 4;
    PhaseWeight[to_index(B_QUEEN)]  = 4;

    for (size_t p = 0; p < PIECE_NB; ++p) {
        PST_MG[p].fill(0);
        PST_EG[p].fill(0);
    }

    auto set_pst = [](Piece pc, int val, const std::array<int, 64>& pst_mg, const std::array<int, 64>& pst_eg) {
        Color c = color_of(pc);
        size_t p_idx = to_index(pc);
        for (int s = 0; s < 64; ++s) {
            Square sq = static_cast<Square>(s);
            Square rel_sq = relative_square(c, sq);
            size_t idx = to_index(rel_sq);
            int mg = val + pst_mg[idx];
            int eg = val + pst_eg[idx];
            PST_MG[p_idx][to_index(sq)] = (c == WHITE) ? mg : -mg;
            PST_EG[p_idx][to_index(sq)] = (c == WHITE) ? eg : -eg;
        }
    };

    set_pst(W_PAWN, PawnValue, PawnPST, PawnPST);
    set_pst(B_PAWN, PawnValue, PawnPST, PawnPST);

    set_pst(W_KNIGHT, KnightValue, KnightPST, KnightPST);
    set_pst(B_KNIGHT, KnightValue, KnightPST, KnightPST);

    set_pst(W_BISHOP, BishopValue, BishopPST, BishopPST);
    set_pst(B_BISHOP, BishopValue, BishopPST, BishopPST);

    set_pst(W_ROOK, RookValue, RookPST, RookPST);
    set_pst(B_ROOK, RookValue, RookPST, RookPST);

    set_pst(W_QUEEN, QueenValue, QueenPST, QueenPST);
    set_pst(B_QUEEN, QueenValue, QueenPST, QueenPST);

    set_pst(W_KING, 0, KingMiddlegamePST, KingEndgamePST);
    set_pst(B_KING, 0, KingMiddlegamePST, KingEndgamePST);
}

template<Color Us>
[[nodiscard]] static std::pair<Value, Value> evaluate_pawn_structure(const Position& pos) {
    constexpr Color Them = ~Us;
    const Bitboard our_pawns = pos.pieces(Us, PAWN);
    const Bitboard opponent_pawns = pos.pieces(Them, PAWN);
    
    Value mg = 0;
    Value eg = 0;
    
    Bitboard pawns = our_pawns;
    while (pawns) {
        Square sq = lsb(pawns);
        pawns &= pawns - 1;
        
        int p_mg = 0;
        int p_eg = 0;
        
        File f = file_of(sq);
        Bitboard adjacent = AdjacentFiles[to_index(f)];
        
        // Isolated Pawn Penalty
        if ((our_pawns & adjacent) == 0) {
            p_mg -= 15;
            p_eg -= 15;
        }
        
        // Doubled Pawn Penalty (once per file, when sq is the LSB on that file)
        Bitboard same_file = our_pawns & file_bb(sq);
        if (more_than_one(same_file) && sq == get_LSB(same_file)) {
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
        
        mg = static_cast<Value>(mg + p_mg);
        eg = static_cast<Value>(eg + p_eg);
    }
    
    return { mg, eg };
}

Value evaluate(const Position& pos) {
    const StateInfo* st = pos.state();
    int mg = st->psq_mg;
    int eg = st->psq_eg;
    int phase = std::clamp(static_cast<int>(st->game_phase), 0, 24);

    auto [white_pawn_mg, white_pawn_eg] = evaluate_pawn_structure<WHITE>(pos);
    auto [black_pawn_mg, black_pawn_eg] = evaluate_pawn_structure<BLACK>(pos);

    mg += white_pawn_mg - black_pawn_mg;
    eg += white_pawn_eg - black_pawn_eg;

    int white_bishops = popcnt(pos.pieces(WHITE, BISHOP));
    int black_bishops = popcnt(pos.pieces(BLACK, BISHOP));
    if (white_bishops >= 2) { mg += 30; eg += 40; }
    if (black_bishops >= 2) { mg -= 30; eg -= 40; }

    int score = (mg * phase + eg * (24 - phase)) / 24;
    return (pos.side_to_move() == WHITE) ? static_cast<Value>(score) : static_cast<Value>(-score);
}

void print_detailed_eval(const Position& pos, bool use_color) {
    const StateInfo* st = pos.state();
    int mg_base = st->psq_mg;
    int eg_base = st->psq_eg;

    auto [mg_white_pawns, eg_white_pawns] = evaluate_pawn_structure<WHITE>(pos);
    auto [mg_black_pawns, eg_black_pawns] = evaluate_pawn_structure<BLACK>(pos);

    int mg_score = mg_base + mg_white_pawns - mg_black_pawns;
    int eg_score = eg_base + eg_white_pawns - eg_black_pawns;

    int white_bishops = popcnt(pos.pieces(WHITE, BISHOP));
    int black_bishops = popcnt(pos.pieces(BLACK, BISHOP));
    if (white_bishops >= 2) { mg_score += 30; eg_score += 40; }
    if (black_bishops >= 2) { mg_score -= 30; eg_score -= 40; }

    int phase = std::clamp(static_cast<int>(st->game_phase), 0, 24);
    int score = (mg_score * phase + eg_score * (24 - phase)) / 24;
    int relative_score = (pos.side_to_move() == WHITE) ? score : -score;

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
    std::cout << std::format("  {}Middlegame Score{}   : {}{:+6}{}\n", green, reset, magenta, mg_score, reset);
    std::cout << std::format("  {}Endgame Score{}      : {}{:+6}{}\n", green, reset, magenta, eg_score, reset);
    std::cout << std::format("  {}Interpolated Score{} : {}{:+6}{}\n", green, reset, magenta, score, reset);
    std::cout << std::format("  {}Relative Score{}     : {}{:+6}{} (relative to {})\n", 
                 green, reset, magenta, relative_score, reset, pos.side_to_move() == WHITE ? "White" : "Black");
    std::cout << blue << "========================================================\n" << reset;
}

} // namespace Eval
} // namespace Bully
