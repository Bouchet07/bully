#pragma once

#include "types.h"
#include "position.h"

namespace Bully {
namespace Eval {

// High-performance static evaluation function returning score relative to the side to move
[[nodiscard]] Value evaluate(const Position& pos);

} // namespace Eval
} // namespace Bully
