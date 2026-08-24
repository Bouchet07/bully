#include "cli.h"
#include "search.h"
#include "nnue.h"
#include "types.h"

#include <iostream>
#include <format>
#include <chrono>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <io.h>
    #include <windows.h>
    #define ISATTY _isatty
    #define FILENO _fileno
#else
    #include <unistd.h>
    #include <cstdlib>
    #define ISATTY isatty
    #define FILENO fileno
#endif

namespace Bully {

CLIStyle style;

void CLIStyle::init(bool interactive) {
    if (interactive) {
        reset   = "\033[0m";
        bold    = "\033[1m";
        red     = "\033[1;31m";
        green   = "\033[1;32m";
        yellow  = "\033[1;33m";
        blue    = "\033[1;34m";
        magenta = "\033[1;35m";
        cyan    = "\033[1;36m";
    } else {
        reset = bold = red = green = yellow = blue = magenta = cyan = "";
    }
}

bool is_interactive() {
    return ISATTY(FILENO(stdout)) != 0;
}

bool detect_utf8() {
#if defined(_WIN32)
    return GetConsoleOutputCP() == 65001;
#else
    const char* lang = std::getenv("LANG");
    if (!lang) return false;
    std::string s(lang);
    return s.find("UTF-8") != std::string::npos || s.find("utf-8") != std::string::npos;
#endif
}

#if defined(_WIN32)
static UINT original_cp = 0;
static UINT original_icp = 0;
static DWORD original_mode = 0;
#endif

void init_terminal() {
#if defined(_WIN32)
    // Save the user's original code pages
    original_cp = GetConsoleOutputCP();
    original_icp = GetConsoleCP();

    // Force UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (is_interactive()) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            // Save the original console mode
            GetConsoleMode(hOut, &original_mode);

            // Enable ANSI colors
            DWORD dwMode = original_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

void restore_terminal() {
#if defined(_WIN32)
    // Put everything back exactly how we found it
    SetConsoleOutputCP(original_cp);
    SetConsoleCP(original_icp);

    if (is_interactive()) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            SetConsoleMode(hOut, original_mode);
        }
    }
#endif
}

void print_banner(bool use_utf8) {
    std::cout << style.blue << "========================================================\n" << style.reset;
    if (use_utf8) {
        std::cout << style.cyan << " ██████╗  ██╗   ██╗ ██╗      ██╗   ██╗   ██╗\n";
        std::cout << " ██╔══██╗ ██║   ██║ ██║      ██║   ╚██╗ ██╔╝\n";
        std::cout << " ██████╔╝ ██║   ██║ ██║      ██║    ╚████╔╝ \n";
        std::cout << " ██╔══██╗ ██║   ██║ ██║      ██║     ╚██╔╝  \n";
        std::cout << " ██████╔╝ ╚██████╔╝ ███████╗ ███████╗ ██║   \n";
        std::cout << " ╚═════╝   ╚═════╝  ╚══════╝ ╚══════╝ ╚═╝   \n" << style.reset;
    } else {
        std::cout << style.cyan << "  ____  _   _ _     _    __   __ \n";
        std::cout << " |  _ \\| | | | |   | |   \\ \\ / / \n";
        std::cout << " | |_) | | | | |   | |    \\ V /  \n";
        std::cout << " |  _ <| |_| | |___| |___  | |   \n";
        std::cout << " |____/ \\___/|_____|_____| |_|   \n\n" << style.reset;
    }
    std::cout << style.blue << "========================================================\n" << style.reset;
    std::cout << std::format("  {}Version{}      : {}{}{}\n", style.green, style.reset, style.magenta, EngineVersion, style.reset);
    std::cout << std::format("  {}Author{}       : {}{}{}\n", style.green, style.reset, style.magenta, EngineAuthor, style.reset);

    std::cout << std::format("  {}Compiler{}     : {}{}{}\n", style.green, style.reset, style.magenta, CompilerName, style.reset);
    std::cout << std::format("  {}Build{}        : {}{}{}\n", style.green, style.reset, style.magenta, BuildArchName, style.reset);
    std::cout << style.blue << "========================================================\n" << style.reset;
    std::cout << std::format("  Type '{}help{}' for custom CLI commands, or '{}uci{}' for GUI.\n", style.yellow, style.reset, style.yellow, style.reset);
    std::cout << style.blue << "========================================================\n\n" << style.reset;
}

void print_interactive_help() {
    std::cout << "\n" << style.blue << "============= Bully Interactive CLI Guide =============" << style.reset << "\n";
    std::cout << "  " << style.yellow << "d" << style.reset << " / " << style.yellow << "display" << style.reset
              << "                : Visual representation of the active position.\n";
    std::cout << "  " << style.yellow << "position" << style.reset << " " << style.green << "startpos" << style.reset
              << "          : Load standard chess starting position.\n";
    std::cout << "  " << style.yellow << "position" << style.reset << " " << style.green << "fen" << style.reset << " "
              << style.magenta << "<FEN>" << style.reset
              << "         : Load a FEN string position.\n";
    std::cout << "                               (Add '" << style.green << "moves" << style.reset << " " << style.magenta << "e2e4 ..." << style.reset << "' to play moves on top).\n";
    std::cout << "  " << style.yellow << "move" << style.reset << " "
              << style.magenta << "<e2e4>" << style.reset << " [" << style.magenta << "e7e5 ..." << style.reset << "]     : Play one or more moves on the active board.\n";
    std::cout << "  " << style.yellow << "go" << style.reset << " [" << style.green << "depth" << style.reset << " "
              << style.magenta << "<D>" << style.reset << "] [" << style.green << "ponder" << style.reset << "]    : Search (optionally ponder in background) the active position.\n";
    std::cout << "  " << style.yellow << "ponderhit" << style.reset << "                  : Transition a background ponder search into active search.\n";
    std::cout << "  " << style.yellow << "stop" << style.reset << "                       : Abort a running search.\n";
    std::cout << "  " << style.yellow << "eval" << style.reset << "                       : Print detailed static evaluation breakdown.\n";
    std::cout << "  " << style.yellow << "perft" << style.reset << " "
              << style.magenta << "<depth>" << style.reset
              << "              : Measure speed & count leaf nodes recursively.\n";
    std::cout << "  " << style.yellow << "divide" << style.reset << " "
              << style.magenta << "<depth>" << style.reset
              << "             : Print move-by-move node counts (divide test).\n";
    std::cout << "  " << style.yellow << "bench" << style.reset << " ["
              << style.magenta << "<depth>" << style.reset
              << "]            : Run standard search benchmark suite.\n";
    std::cout << "  " << style.yellow << "hash" << style.reset << " "
              << style.magenta << "<MB>" << style.reset
              << "                  : Resize transposition table (in Megabytes).\n";
    std::cout << "  " << style.yellow << "threads" << style.reset << " "
              << style.magenta << "<count>" << style.reset
              << "            : Set the number of search threads.\n";
    std::cout << "  " << style.yellow << "multipv" << style.reset << " "
              << style.magenta << "<count>" << style.reset
              << "            : Set the number of PV lines to show in search.\n";
    std::cout << "  " << style.yellow << "utf8" << style.reset << " [" << style.green << "on" << style.reset << " | " << style.green << "off" << style.reset << "]            : Toggle UTF-8 grid graphics.\n";
    std::cout << "  " << style.yellow << "color" << style.reset << " [" << style.green << "on" << style.reset << " | " << style.green << "off" << style.reset << "]           : Toggle ANSI terminal colors.\n";
    std::cout << "  " << style.yellow << "autoprint" << style.reset << " [" << style.green << "on" << style.reset << " | " << style.green << "off" << style.reset << "]       : Toggle board auto-printing after moves.\n";
    std::cout << "  " << style.yellow << "options" << style.reset << " [" << style.magenta << "name" << style.reset << " [" << style.green << "on" << style.reset << " | " << style.green << "off" << style.reset << "]]  : View or toggle search heuristic options.\n";
    std::cout << "  " << style.yellow << "info" << style.reset
              << "                       : Print detailed build, compiler, OS, flags, and SIMD info.\n";
    std::cout << "  " << style.yellow << "syzygy" << style.reset << " [" << style.magenta << "<path>" << style.reset << "|" << style.green << "on" << style.reset << "|" << style.green << "off" << style.reset << "]     : View or set Syzygy tablebases path & state.\n";
    std::cout << "  " << style.yellow << "variation" << style.reset << " ["
              << style.magenta << "<name>" << style.reset
              << "]         : View or set active chess variation (standard / chess960).\n";
    std::cout << "  " << style.yellow << "uci" << style.reset << "                        : Switch to UCI engine mode.\n";
    std::cout << "  " << style.yellow << "quit" << style.reset << " / " << style.yellow << "exit" << style.reset
              << "                : Terminate Bully.\n";
    std::cout << style.blue << "========================================================" << style.reset << "\n\n";
}

void print_interactive_options() {
    std::cout << "\n" << style.blue << "============ Bully Search Heuristics Options ============" << style.reset << "\n";
    std::cout << "  " << style.green << "NullMovePruning" << style.reset << " (" << style.magenta << "nmp" << style.reset << ")          : "
              << style.magenta << (Search::config.nmp ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "LateMoveReduction" << style.reset << " (" << style.magenta << "lmr" << style.reset << ")        : "
              << style.magenta << (Search::config.lmr ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "ReverseFutilityPruning" << style.reset << " (" << style.magenta << "rfp" << style.reset << ")   : "
              << style.magenta << (Search::config.rfp ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "LateMovePruning" << style.reset << " (" << style.magenta << "lmp" << style.reset << ")          : "
              << style.magenta << (Search::config.lmp ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "FutilityPruning" << style.reset << " (" << style.magenta << "fp" << style.reset << ")           : "
              << style.magenta << (Search::config.fp ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "CheckExtensions" << style.reset << " (" << style.magenta << "ce" << style.reset << ")           : "
              << style.magenta << (Search::config.check_extensions ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "SingularExtensions" << style.reset << " (" << style.magenta << "se" << style.reset << ")        : "
              << style.magenta << (Search::config.singular_extensions ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "AspirationWindow" << style.reset << " (" << style.magenta << "aw" << style.reset << ")          : "
              << style.magenta << (Search::config.aspiration_window ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "QuiescenceSearch" << style.reset << " (" << style.magenta << "qs" << style.reset << ")          : "
              << style.magenta << (Search::config.quiescence ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "UseTT" << style.reset << " (" << style.magenta << "tt" << style.reset << ")                     : "
              << style.magenta << (Search::config.tt ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "KillerHeuristic" << style.reset << " (" << style.magenta << "kh" << style.reset << ")           : "
              << style.magenta << (Search::config.killers ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "HistoryHeuristic" << style.reset << " (" << style.magenta << "hh" << style.reset << ")          : "
              << style.magenta << (Search::config.history ? "ON" : "OFF") << style.reset << "\n";
    std::cout << "  " << style.green << "UseNNUE" << style.reset << " (" << style.magenta << "nnue" << style.reset << ")                 : "
              << style.magenta << (NNUE::use_nnue ? "ON" : "OFF") << style.reset << "\n\n";
    std::cout << "  " << style.green << "Usage" << style.reset << ": "
              << style.yellow << "options" << style.reset << " [" << style.magenta << "name" << style.reset << " [" << style.green << "on" << style.reset << " | " << style.green << "off" << style.reset << "]]"
              << "  (e.g., " << style.yellow << "options" << style.reset << " " << style.magenta << "nnue" << style.reset << " " << style.green << "on" << style.reset << ")\n";
    std::cout << style.blue << "========================================================" << style.reset << "\n\n";
}

void print_arguments_guide() {
    std::string bin(BinaryName);
    int p1 = std::max(1, 38 - static_cast<int>(bin.length()));
    int p2 = std::max(1, 28 - static_cast<int>(bin.length()));
    int p3 = std::max(1, 22 - static_cast<int>(bin.length()));
    int p4 = std::max(1, 30 - static_cast<int>(bin.length()));

    std::cout << style.blue << "========================================================\n" << style.reset;
    std::cout << style.cyan << "Bully Chess Engine - Command Line Argument Guide\n" << style.reset;
    std::cout << style.blue << "========================================================\n" << style.reset;
    std::cout << style.yellow << "Usage:\n" << style.reset;
    std::cout << std::format("  {}{}{}{:>{}} : Starts in interactive/UCI loop mode.\n", style.yellow, bin, style.reset, "", p1);
    std::cout << std::format("  {}{}{} {}<command>{}{:>{}} : Executes a single command and exits.\n", style.yellow, bin, style.reset, style.magenta, style.reset, "", p2);
    std::cout << std::format("  {}{}{} {}\"<cmd1>; <cmd2>; ...\"{}{:>{}} : Executes multiple commands in sequence and exits.\n", style.yellow, bin, style.reset, style.magenta, style.reset, "", p3);
    std::cout << std::format("  {}{}{} {}help / -h / --help{}{:>{}} : Prints this command line argument guide.\n\n", style.yellow, bin, style.reset, style.magenta, style.reset, "", p4);
    std::cout << style.yellow << "Examples:\n" << style.reset;
    std::cout << std::format("  {} \"position startpos; go depth 8; quit\"\n", bin);
    std::cout << std::format("  {} \"position fen 8/8/8/8/8/4k3/4P3/4K3 w - - 0 1; go depth 12; quit\"\n", bin);
    std::cout << style.blue << "========================================================\n" << style.reset;
}

} // namespace Bully
