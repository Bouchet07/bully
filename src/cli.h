/**
 * @file cli.h
 * @brief Interactive CLI Formatting, ANSI Styling, and UTF-8 Terminal Utilities.
 *
 * Provides terminal environment detection (`is_interactive`, `detect_utf8`), Windows virtual terminal
 * sequence initialization, ANSI color styling tokens, formatted help displays, and banner rendering.
 */

#pragma once

#include <string>

namespace Bully {

struct CLIStyle {
    std::string reset;
    std::string bold;
    std::string red;
    std::string green;
    std::string yellow;
    std::string blue;
    std::string magenta;
    std::string cyan;

    void init(bool interactive);
};

extern CLIStyle style;

// Initialize Windows virtual terminal processing and style
void init_terminal();

// Check if stdout is an interactive terminal
[[nodiscard]] bool is_interactive();

// Auto-detect UTF-8 support
[[nodiscard]] bool detect_utf8();

// Print welcome banner in interactive mode
void print_banner(bool use_utf8);

// Print interactive help guide
void print_interactive_help();

// Print non-interactive plain help guide
void print_plain_help();

// Print interactive CLI guide for options
void print_interactive_options();

// Print plain text options list
void print_plain_options();

// Print command-line argument help guide
void print_arguments_guide(bool use_color);

} // namespace Bully
