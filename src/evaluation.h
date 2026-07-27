#pragma once

#include "types.h"
#include "position.h"

namespace Bully {
namespace Eval {

// Precomputed incremental Piece-Square Tables (PST) and Phase Weights
extern std::array<std::array<int, 64>, PIECE_NB> PST_MG;
extern std::array<std::array<int, 64>, PIECE_NB> PST_EG;
extern std::array<int, PIECE_NB>                 PhaseWeight;

// Initialize PST evaluation tables
void init_eval();

// High-performance static evaluation function returning score relative to the side to move
[[nodiscard]] Value evaluate(const Position& pos);

// Prints a detailed breakdown of the static evaluation terms (material, PSTs, pawns, phase)
void print_detailed_eval(const Position& pos, bool use_color);

} // namespace Eval
} // namespace Bully
