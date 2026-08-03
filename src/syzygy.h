/**
 * @file syzygy.h
 * @brief Syzygy Endgame Tablebase Probing Wrapper (Pyrrhic C++20 Integration).
 *
 * Provides root DTZ (Distance To Zero) move probing for instant perfect move execution
 * in endgames (3-5 pieces), and WDL (Win/Draw/Loss) probing during tree search for early node pruning.
 */

#ifndef BULLY_SYZYGY_H
#define BULLY_SYZYGY_H

#include <string>
#include "types.h"
#include "position.h"

namespace Bully {
namespace Syzygy {

extern std::string path;
extern int max_cardinality; // Maximum piece count available in loaded tablebases (0 if none)

// Initialize Syzygy tablebases with given directory path
void init(const std::string& tb_path);

// Probe WDL (Win/Draw/Loss) score at non-root nodes
// Returns VALUE_NONE if probing fails or position not in tablebases
[[nodiscard]] Value probe_wdl(const Position& pos);

// Probe DTZ / Root move at root position
// Returns true if a tablebase move was found, populating best_tb_move and tb_score
[[nodiscard]] bool probe_root(const Position& pos, Move& best_tb_move, Value& tb_score);

} // namespace Syzygy
} // namespace Bully

#endif // BULLY_SYZYGY_H
