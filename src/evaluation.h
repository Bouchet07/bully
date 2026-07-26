#pragma once

#include "types.h"
#include "position.h"

namespace Bully {
namespace Eval {

// High-performance static evaluation function returning score relative to the side to move
[[nodiscard]] Value evaluate(const Position& pos);

// Prints a detailed breakdown of the static evaluation terms (material, PSTs, pawns, phase)
void print_detailed_eval(const Position& pos, bool use_color);

} // namespace Eval
} // namespace Bully
