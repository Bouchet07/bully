#pragma once

#include <list>
#include <string>
#include "types.h"
#include "position.h"

namespace Bully {

class UCI {
public:
    UCI() = default;

    // Checks if running in terminal and prints welcome banner if so
    void init();

    // Core execution loop
    void loop();

    // Executes a single UCI command line. Returns false if 'quit' or 'exit' is executed.
    bool execute_line(const std::string& line);

    // Prints command line arguments usage guide
    void print_arguments_help();

private:
    // Run recursive Perft from the active position
    uint64_t run_perft(int depth);

    // Run divide test from the active position
    void run_divide(int depth, bool is_go_cmd);

    // Parse move string (e.g. "e2e4") to a legal Move object
    Move parse_move(const std::string& move_str);

    Position pos;
    std::list<StateInfo> history;
    bool use_utf8 = true;
    bool use_color = true;
    bool autoprint = true;
};

} // namespace Bully