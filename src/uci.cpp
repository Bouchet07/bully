#include <iostream>
#include <format>
#include <string>
#include <sstream>
#include <chrono>
#include <list>
#include <thread>

#if defined(_WIN32)
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

#include "types.h"
#include "uci.h"
#include "movegen.h"
#include "tt.h"
#include "search.h"
#include "evaluation.h"
#include "syzygy.h"

namespace Bully {

// Helper function to check if the program is running in an interactive terminal (cross-platform)
static bool is_interactive() {
    return ISATTY(FILENO(stdout)) != 0;
}

// Struct containing styles for colors in terminal CLI
struct CLIStyle {
    std::string reset;
    std::string bold;
    std::string red;
    std::string green;
    std::string yellow;
    std::string blue;
    std::string magenta;
    std::string cyan;

    void init(bool interactive) {
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
};

static CLIStyle style;

// Auto-detect if the current terminal environment supports UTF-8 characters
static bool detect_utf8() {
#if defined(_WIN32)
    return GetConsoleOutputCP() == 65001;
#else
    const char* lang = std::getenv("LANG");
    if (!lang) return false;
    std::string s(lang);
    return s.find("UTF-8") != std::string::npos || s.find("utf-8") != std::string::npos;
#endif
}


void UCI::init() {
#if defined(_WIN32)
    // Enable ANSI escape sequences on Windows Console (if running in terminal)
    if (is_interactive()) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
    }
#endif

    // Initialize starting position, TT, and Syzygy tablebases
    TT.resize(16);
    Syzygy::init("syzygy");
    history.emplace_back();
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", history.back());

    use_utf8 = detect_utf8();
    use_color = is_interactive();
    style.init(use_color);

    if (is_interactive()) {
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
        std::cout << std::format("  {}Version{}      : {}{}{}\n", style.green, style.reset, style.magenta, ENGINE_VERSION, style.reset);
        std::cout << std::format("  {}Author{}       : {}{}{}\n", style.green, style.reset, style.magenta, ENGINE_AUTHOR, style.reset);
        std::cout << std::format("  {}Compiler{}     : {}GCC {}{}\n", style.green, style.reset, style.magenta, __VERSION__, style.reset);
        
        #if defined(USE_PEXT)
        std::cout << std::format("  {}Build{}        : {}BMI2 (PEXT){}\n", style.green, style.reset, style.magenta, style.reset);
        #elif defined(__AVX2__)
        std::cout << std::format("  {}Build{}        : {}AVX2{}\n", style.green, style.reset, style.magenta, style.reset);
        #elif defined(__SSE4_2__) || defined(__POPCNT__)
        std::cout << std::format("  {}Build{}        : {}Modern (SSE4.2/POPCNT){}\n", style.green, style.reset, style.magenta, style.reset);
        #else
        std::cout << std::format("  {}Build{}        : {}Generic (x86-64){}\n", style.green, style.reset, style.magenta, style.reset);
        #endif
        std::cout << style.blue << "========================================================\n" << style.reset;
        std::cout << std::format("  Type '{}help{}' for custom CLI commands, or '{}uci{}' for GUI.\n", style.yellow, style.reset, style.yellow, style.reset);
        std::cout << style.blue << "========================================================\n\n" << style.reset;
    }
}

uint64_t UCI::run_perft(int depth) {
    if (depth == 0) return 1ULL;

    uint64_t nodes = 0;
    MoveList list;
    list.generate(pos);

    for (size_t i = 0; i < list.size(); ++i) {
        Move m = list[i].move;
        StateInfo next_si;
        if (pos.make_move(m, next_si)) {
            nodes += run_perft(depth - 1);
        }
        pos.unmake_move(m);
    }
    return nodes;
}

Move UCI::parse_move(const std::string& move_str) {
    MoveList list;
    list.generate(pos);
    for (size_t i = 0; i < list.size(); ++i) {
        Move m = list[i].move;
        if (m.to_string() == move_str) {
            return m;
        }
    }
    return Move::none();
}

void UCI::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!execute_line(line)) {
            break;
        }
    }
}

bool UCI::execute_line(const std::string& line) {
    std::istringstream is(line);
    std::string token;
    is >> std::skipws >> token;
    
    if (token.empty()) {
        return true;
    }

        if (token == "uci") {
            std::cout << std::format("id name {} {}\n", ENGINE_NAME, ENGINE_VERSION);
            std::cout << std::format("id author {}\n", ENGINE_AUTHOR);
            std::cout << std::format("{}\n", UCI_OPTIONS);
            std::cout << "uciok\n" << std::flush;
        }
        else if (token == "isready") {
            std::cout << "readyok\n" << std::flush;
        }
        else if (token == "ucinewgame") {
            Search::stop_and_join();
            TT.clear();
        }
        else if (token == "setoption") {
            std::string name_keyword;
            is >> name_keyword;
            if (name_keyword == "name") {
                std::string option_name;
                is >> option_name;
                
                if (option_name == "Clear") {
                    std::string part2;
                    is >> part2;
                    option_name += " " + part2;
                }

                if (option_name == "Hash") {
                    std::string value_keyword;
                    is >> value_keyword;
                    int val = 16;
                    if (is >> val) {
                        TT.resize(static_cast<size_t>(val));
                        if (is_interactive()) {
                            std::cout << std::format("Hash size resized to {} MB.\n", val);
                        }
                    }
                }
                else if (option_name == "Clear Hash") {
                    TT.clear();
                    if (is_interactive()) {
                        std::cout << "Hash table cleared.\n";
                    }
                }
                else if (option_name == "Threads") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    int val = 1;
                    if (is >> val) {
                        int threads = val;
                        if (threads <= 0) {
                            threads = static_cast<int>(std::thread::hardware_concurrency());
                            if (threads <= 0) threads = 1;
                        }
                        Search::num_threads = threads;
                        if (is_interactive()) {
                            std::cout << std::format("Threads count set to {} (input: {}).\n", threads, val);
                        }
                    }
                }
                else if (option_name == "SyzygyPath") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string path_val;
                    if (is >> path_val) {
                        Syzygy::init(path_val);
                    }
                }
                else if (option_name == "MultiPV") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    int val = 1;
                    if (is >> val) {
                        Search::multipv_count = std::max(1, val);
                        if (is_interactive()) {
                            std::cout << std::format("MultiPV count set to {}.\n", Search::multipv_count);
                        }
                    }
                }
                else if (option_name == "NullMovePruning") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_nmp = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("NullMovePruning set to {}.\n", Search::use_nmp);
                        }
                    }
                }
                else if (option_name == "LateMoveReduction") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_lmr = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("LateMoveReduction set to {}.\n", Search::use_lmr);
                        }
                    }
                }
                else if (option_name == "ReverseFutilityPruning") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_rfp = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("ReverseFutilityPruning set to {}.\n", Search::use_rfp);
                        }
                    }
                }
                else if (option_name == "LateMovePruning") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_lmp = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("LateMovePruning set to {}.\n", Search::use_lmp);
                        }
                    }
                }
                else if (option_name == "FutilityPruning") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_fp = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("FutilityPruning set to {}.\n", Search::use_fp);
                        }
                    }
                }
                else if (option_name == "CheckExtensions") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_check_extensions = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("CheckExtensions set to {}.\n", Search::use_check_extensions);
                        }
                    }
                }
                else if (option_name == "AspirationWindow") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_aspiration_window = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("AspirationWindow set to {}.\n", Search::use_aspiration_window);
                        }
                    }
                }
                else if (option_name == "QuiescenceSearch") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_quiescence = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("QuiescenceSearch set to {}.\n", Search::use_quiescence);
                        }
                    }
                }
                else if (option_name == "UseTT") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_tt = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("UseTT set to {}.\n", Search::use_tt);
                        }
                    }
                }
                else if (option_name == "KillerHeuristic") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_killers = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("KillerHeuristic set to {}.\n", Search::use_killers);
                        }
                    }
                }
                else if (option_name == "HistoryHeuristic") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        Search::use_history = (val == "true");
                        if (is_interactive()) {
                            std::cout << std::format("HistoryHeuristic set to {}.\n", Search::use_history);
                        }
                    }
                }
                else if (option_name == "Ponder") {
                    std::string value_keyword;
                    is >> value_keyword; // "value"
                    std::string val;
                    if (is >> val) {
                        if (is_interactive()) {
                            std::cout << std::format("Ponder set to {}.\n", val);
                        }
                    }
                }
            }
        }
        else if (token == "position") {
            std::string subtoken;
            is >> subtoken;
            
            bool parsed_pos = false;
            std::string part;
            if (subtoken == "startpos" || subtoken == "pos1") {
                history.clear();
                history.emplace_back();
                pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", history.back());
                parsed_pos = true;
            } 
            else if (subtoken == "kiwipete" || subtoken == "pos2") {
                history.clear();
                history.emplace_back();
                pos.set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", history.back());
                parsed_pos = true;
            }
            else if (subtoken == "lasker") {
                history.clear();
                history.emplace_back();
                pos.set_fen("8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1", history.back());
                parsed_pos = true;
            }
            else if (subtoken == "fools") {
                history.clear();
                history.emplace_back();
                pos.set_fen("rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2", history.back());
                parsed_pos = true;
            }
            else if (subtoken == "scholars") {
                history.clear();
                history.emplace_back();
                pos.set_fen("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 4 4", history.back());
                parsed_pos = true;
            }
            else if (subtoken == "pos3") {
                history.clear();
                history.emplace_back();
                pos.set_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", history.back());
                parsed_pos = true;
            }
            else if (subtoken == "pos4") {
                history.clear();
                history.emplace_back();
                pos.set_fen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 21", history.back());
                parsed_pos = true;
            }
            else if (subtoken == "pos5") {
                history.clear();
                history.emplace_back();
                pos.set_fen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", history.back());
                parsed_pos = true;
            }
            else if (subtoken == "pos6") {
                history.clear();
                history.emplace_back();
                pos.set_fen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", history.back());
                parsed_pos = true;
            }
            else if (subtoken == "fen") {
                std::string fen_str;
                for (int i = 0; i < 6; ++i) {
                    if (!(is >> part)) {
                        break;
                    }
                    if (part == "moves") {
                        break;
                    }
                    if (i > 0) fen_str += " ";
                    fen_str += part;
                }
                history.clear();
                history.emplace_back();
                pos.set_fen(fen_str, history.back());
                parsed_pos = true;
            }
            
            if (parsed_pos) {
                std::string moves_token;
                if (subtoken == "fen" && part == "moves") {
                    moves_token = "moves";
                } else {
                    is >> moves_token;
                }
                
                if (moves_token == "moves") {
                    std::string move_str;
                    while (is >> move_str) {
                        Move m = parse_move(move_str);
                        if (m != Move::none()) {
                            history.emplace_back();
                            pos.make_move(m, history.back());
                        }
                    }
                }
            } else {
                if (subtoken.empty()) {
                    if (is_interactive()) {
                        std::cout << std::format("\n{}=== Position Command Helper ==={}\n", style.blue, style.reset);
                        std::cout << std::format("Usage:\n");
                        std::cout << std::format("  {}position startpos{}       : Load standard starting chess position.\n", style.yellow, style.reset);
                        std::cout << std::format("  {}position fen{} {}<FEN>{}      : Load a custom FEN string.\n", style.yellow, style.reset, style.magenta, style.reset);
                        std::cout << std::format("  {}position{} {}<preset>{}       : Load one of the famous preset positions.\n\n", style.yellow, style.reset, style.green, style.reset);
                        std::cout << std::format("Available Preset Positions:\n");
                        std::cout << std::format("  {}kiwipete{} / {}pos2{}     : Standard complex test/perft position (KiwiPete).\n", style.green, style.reset, style.green, style.reset);
                        std::cout << std::format("  {}lasker{}              : Lasker-Reichhelm pawn endgame study.\n", style.green, style.reset);
                        std::cout << std::format("  {}fools{}               : Fool's Mate setup.\n", style.green, style.reset);
                        std::cout << std::format("  {}scholars{}            : Scholar's Mate setup.\n", style.green, style.reset);
                        std::cout << std::format("  {}pos1{}                : Starting position (alias for startpos).\n", style.green, style.reset);
                        std::cout << std::format("  {}pos3{}                : Perft test position #3.\n", style.green, style.reset);
                        std::cout << std::format("  {}pos4{}                : Perft test position #4.\n", style.green, style.reset);
                        std::cout << std::format("  {}pos5{}                : Perft test position #5.\n", style.green, style.reset);
                        std::cout << std::format("  {}pos6{}                : Perft test position #6.\n", style.green, style.reset);
                        std::cout << std::format("\nNote: You can append '{}moves{} {}e2e4 ...{}' to play moves on top of any position.\n",
                                                 style.green, style.reset, style.magenta, style.reset);
                        std::cout << std::format("{}================================{}\n\n", style.blue, style.reset);
                    } else {
                        std::cout << "Usage:\n";
                        std::cout << "  position startpos       : Load standard starting chess position.\n";
                        std::cout << "  position fen <FEN>      : Load a custom FEN string.\n";
                        std::cout << "  position <preset>       : Load one of the famous preset positions.\n\n";
                        std::cout << "Available Presets: startpos/pos1, kiwipete/pos2, lasker, fools, scholars, pos3, pos4, pos5, pos6\n";
                    }
                } else {
                    if (is_interactive()) {
                        std::cout << std::format("{}Unknown position preset or command: '{}{}{}'. Type '{}position{}' for help.{}\n",
                                                 style.red, style.magenta, subtoken, style.reset, style.yellow, style.reset, style.reset);
                    } else {
                        std::cout << std::format("Unknown position preset or command: '{}'. Type 'position' for help.\n", subtoken);
                    }
                }
            }
        }
        else if (token == "move") {
            std::string move_str;
            std::vector<Move> played_moves;
            bool success = true;
            std::string error_msg;
            
            while (is >> move_str) {
                Move m = parse_move(move_str);
                if (m != Move::none()) {
                    StateInfo next_si;
                    if (pos.make_move(m, next_si)) {
                        history.push_back(next_si);
                        pos.set_state_pointer(&history.back());
                        played_moves.push_back(m);
                    } else {
                        pos.unmake_move(m);
                        success = false;
                        error_msg = std::format("Illegal move (leaves King in check): {}", move_str);
                        break;
                    }
                } else {
                    success = false;
                    error_msg = std::format("Invalid or illegal move: '{}'", move_str);
                    break;
                }
            }
            
            if (!success) {
                // Rollback all moves played in this batch
                for (auto it = played_moves.rbegin(); it != played_moves.rend(); ++it) {
                    pos.unmake_move(*it);
                    history.pop_back();
                }
                if (!history.empty()) {
                    pos.set_state_pointer(&history.back());
                }
                
                if (is_interactive()) {
                    std::cout << std::format("{}{}{}\n", style.red, error_msg, style.reset);
                } else {
                    std::cout << error_msg << "\n";
                }
            } else if (!played_moves.empty()) {
                // All moves in the batch played successfully!
                if (is_interactive()) {
                    std::cout << "Played moves:";
                    for (Move m : played_moves) {
                        std::cout << std::format(" {}{}{}", style.yellow, m.to_string(), style.reset);
                    }
                    std::cout << "\n";
                    
                    if (autoprint) {
                        pos.print(use_utf8, use_color);
                    }
                } else {
                    std::cout << "Played moves:";
                    for (Move m : played_moves) {
                        std::cout << std::format(" {}", m.to_string());
                    }
                    std::cout << "\n";
                }
            } else {
                // Empty move command
                if (is_interactive()) {
                    std::cout << std::format("{}Usage: move <e2e4> [e7e5 ...]{} (e.g., 'move e2e4 e7e5')\n", style.yellow, style.reset);
                } else {
                    std::cout << "Usage: move <e2e4> [e7e5 ...] (e.g., 'move e2e4 e7e5')\n";
                }
            }
        }
        else if (token == "go") {
            Search::Limits limits;
            std::string arg;
            bool is_go_perft = false;
            int perft_depth = 1;
            
            while (is >> arg) {
                if (arg == "wtime")          is >> limits.wtime;
                else if (arg == "btime")     is >> limits.btime;
                else if (arg == "winc")      is >> limits.winc;
                else if (arg == "binc")      is >> limits.binc;
                else if (arg == "movestogo") is >> limits.movestogo;
                else if (arg == "depth")     is >> limits.depth;
                else if (arg == "nodes")     is >> limits.nodes;
                else if (arg == "movetime")  is >> limits.movetime;
                else if (arg == "infinite")  limits.infinite = true;
                else if (arg == "ponder")    limits.ponder = true;
                else if (arg == "perft") {
                    is_go_perft = true;
                    is >> perft_depth;
                }
            }

            if (is_go_perft) {
                if (perft_depth < 1) perft_depth = 1;
                MoveList list;
                list.generate(pos);
                uint64_t total = 0;
                for (size_t i = 0; i < list.size(); ++i) {
                    Move m = list[i].move;
                    StateInfo next_si;
                    if (pos.make_move(m, next_si)) {
                        uint64_t subnodes = run_perft(perft_depth - 1);
                        std::cout << std::format("{}: {}\n", m.to_string(), subnodes);
                        total += subnodes;
                    }
                    pos.unmake_move(m);
                }
                std::cout << std::format("\nNodes searched: {}\n", total);
            } else {
                // Print execution metadata
                int cores = static_cast<int>(std::thread::hardware_concurrency());
                if (cores <= 0) cores = 1;
                std::cout << std::format("info string Available processors: 0-{}\n", cores - 1);
                std::cout << std::format("info string Using {} thread{}\n", Search::num_threads, Search::num_threads > 1 ? "s" : "");
                std::cout << std::format("info string Hash size: {} MB\n", TT.get_size_mb());
                std::cout << "info string Evaluation: Tapered Classical Static Evaluation\n";
                std::cout << std::flush;

                if (is_interactive() && !limits.time_controlled() && limits.depth == -1 && !limits.infinite && !limits.ponder) {
                    limits.depth = 6;
                }

                Search::start(pos, limits, history);
            }
        }
        else if (token == "stop") {
            Search::stop_and_join();
        }
        else if (token == "ponderhit") {
            Search::pondering.store(false, std::memory_order_relaxed);
            auto now = std::chrono::steady_clock::now();
            Search::search_start_time_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count(), std::memory_order_relaxed);
        }
        else if (token == "d" || token == "display") {
            pos.print(use_utf8, use_color);
        }
        else if (token == "hash") {
            std::string sub;
            if (is >> sub) {
                std::string lower_sub = sub;
                for (char &c : lower_sub) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                if (lower_sub == "clear") {
                    TT.clear();
                    if (is_interactive()) {
                        std::cout << std::format("{}Hash table cleared.{}\n", style.green, style.reset);
                    } else {
                        std::cout << "Hash table cleared.\n";
                    }
                } else {
                    try {
                        size_t val = std::stoull(sub);
                        TT.resize(val);
                        if (is_interactive()) {
                            std::cout << std::format("Hash size resized to {}{}{} MB.\n", style.magenta, val, style.reset);
                        } else {
                            std::cout << std::format("Hash size resized to {} MB.\n", val);
                        }
                    } catch (...) {
                        if (is_interactive()) {
                            std::cout << std::format("{}Invalid hash argument: '{}'. Usage: {}hash{} {}<MB>{} | {}hash clear{}\n",
                                                     style.red, sub, style.yellow, style.reset, style.magenta, style.reset, style.yellow, style.reset);
                        } else {
                            std::cout << std::format("Invalid hash argument: '{}'. Usage: hash <MB> | hash clear\n", sub);
                        }
                    }
                }
            } else {
                if (is_interactive()) {
                    std::cout << std::format("{}Current Hash size{}: {}{} MB{}\nUsage: {}hash{} {}<MB>{} | {}hash clear{}\n",
                                             style.green, style.reset, style.magenta, TT.get_size_mb(), style.reset,
                                             style.yellow, style.reset, style.magenta, style.reset, style.yellow, style.reset);
                } else {
                    std::cout << std::format("Current Hash size: {} MB\nUsage: hash <MB> | hash clear\n", TT.get_size_mb());
                }
            }
        }
        else if (token == "threads") {
            int val = 0;
            if (is >> val) {
                int threads = val;
                if (threads <= 0) {
                    threads = static_cast<int>(std::thread::hardware_concurrency());
                    if (threads <= 0) threads = 1;
                }
                Search::num_threads = threads;
                if (is_interactive()) {
                    std::cout << std::format("Threads count set to {}{}{} (input: {}{}{}).\n", style.magenta, threads, style.reset, style.magenta, val, style.reset);
                } else {
                    std::cout << std::format("Threads count set to {} (input: {}).\n", threads, val);
                }
            } else {
                if (is_interactive()) {
                    std::cout << std::format("{}Current Thread count{}: {}{}{}\nUsage: {}threads{} {}<count>{} (e.g., 'threads 4'). Use -1/0 for auto-detect.\n",
                                             style.green, style.reset, style.magenta, Search::num_threads, style.reset, style.yellow, style.reset, style.magenta, style.reset);
                } else {
                    std::cout << std::format("Current Thread count: {}\nUsage: threads <count> (e.g., 'threads 4'). Use -1/0 for auto-detect.\n", Search::num_threads);
                }
            }
        }
        else if (token == "multipv") {
            int val = 0;
            if (is >> val) {
                Search::multipv_count = std::max(1, val);
                if (is_interactive()) {
                    std::cout << std::format("MultiPV count set to {}{}{}.\n", style.magenta, Search::multipv_count, style.reset);
                } else {
                    std::cout << std::format("MultiPV count set to {}.\n", Search::multipv_count);
                }
            } else {
                if (is_interactive()) {
                    std::cout << std::format("{}Current MultiPV count{}: {}{}{}\nUsage: {}multipv{} {}<count>{} (e.g., 'multipv 3')\n",
                                             style.green, style.reset, style.magenta, Search::multipv_count, style.reset, style.yellow, style.reset, style.magenta, style.reset);
                } else {
                    std::cout << std::format("Current MultiPV count: {}\nUsage: multipv <count> (e.g., 'multipv 3')\n", Search::multipv_count);
                }
            }
        }
        else if (token == "utf8") {
            std::string subtoken;
            is >> subtoken;
            if (subtoken == "on") {
                use_utf8 = true;
                if (is_interactive()) {
                    std::cout << std::format("UTF-8 board drawing enabled ({}on{}).\n", style.green, style.reset);
                } else {
                    std::cout << "UTF-8 board drawing enabled.\n";
                }
            } else if (subtoken == "off") {
                use_utf8 = false;
                if (is_interactive()) {
                    std::cout << std::format("UTF-8 board drawing disabled ({}off{} - standard ASCII grid active).\n", style.green, style.reset);
                } else {
                    std::cout << "UTF-8 board drawing disabled (standard ASCII grid active).\n";
                }
            } else {
                if (is_interactive()) {
                    std::cout << std::format("UTF-8 board drawing is currently {}{}{}\n", style.green, use_utf8 ? "enabled" : "disabled", style.reset);
                } else {
                    std::cout << std::format("UTF-8 board drawing is currently {}\n", use_utf8 ? "enabled" : "disabled");
                }
            }
        }
        else if (token == "color") {
            std::string subtoken;
            is >> subtoken;
            if (subtoken == "on") {
                use_color = true;
                style.init(use_color);
                std::cout << std::format("Color output enabled ({}on{}).\n", style.green, style.reset);
            } else if (subtoken == "off") {
                if (is_interactive()) {
                    std::cout << std::format("Color output disabled ({}off{}).\n", style.green, style.reset);
                } else {
                    std::cout << "Color output disabled.\n";
                }
                use_color = false;
                style.init(use_color);
            } else {
                if (is_interactive()) {
                    std::cout << std::format("Color output is currently {}{}{}\n", style.green, use_color ? "enabled" : "disabled", style.reset);
                } else {
                    std::cout << std::format("Color output is currently {}\n", use_color ? "enabled" : "disabled");
                }
            }
        }
        else if (token == "autoprint") {
            std::string subtoken;
            is >> subtoken;
            if (subtoken == "on") {
                autoprint = true;
                if (is_interactive()) {
                    std::cout << std::format("Auto-print board after moves enabled ({}on{}).\n", style.green, style.reset);
                } else {
                    std::cout << "Auto-print board after moves enabled.\n";
                }
            } else if (subtoken == "off") {
                autoprint = false;
                if (is_interactive()) {
                    std::cout << std::format("Auto-print board after moves disabled ({}off{}).\n", style.green, style.reset);
                } else {
                    std::cout << "Auto-print board after moves disabled.\n";
                }
            } else {
                if (is_interactive()) {
                    std::cout << std::format("Auto-print board after moves is currently {}{}{}\n", style.green, autoprint ? "enabled" : "disabled", style.reset);
                } else {
                    std::cout << std::format("Auto-print board after moves is currently {}\n", autoprint ? "enabled" : "disabled");
                }
            }
        }
        else if (token == "options") {
            std::string opt_name;
            if (is >> opt_name) {
                std::string val;
                if (is >> val) {
                    if (val != "on" && val != "off") {
                        if (is_interactive()) {
                            std::cout << std::format("{}Invalid value: {}{}{}. Use '{}on{}' or '{}off{}'.\n", 
                                                     style.red, style.magenta, val, style.reset, style.green, style.reset, style.green, style.reset);
                        } else {
                            std::cout << std::format("Invalid value: {}. Use 'on' or 'off'\n", val);
                        }
                    } else {
                        bool enabled = (val == "on");
                        bool valid = true;
                        
                        if (opt_name == "nmp") Search::use_nmp = enabled;
                        else if (opt_name == "lmr") Search::use_lmr = enabled;
                        else if (opt_name == "rfp") Search::use_rfp = enabled;
                        else if (opt_name == "lmp") Search::use_lmp = enabled;
                        else if (opt_name == "fp")  Search::use_fp  = enabled;
                        else if (opt_name == "ce")  Search::use_check_extensions = enabled;
                        else if (opt_name == "aw")  Search::use_aspiration_window = enabled;
                        else if (opt_name == "qs")  Search::use_quiescence = enabled;
                        else if (opt_name == "tt")  Search::use_tt = enabled;
                        else if (opt_name == "kh")  Search::use_killers = enabled;
                        else if (opt_name == "hh")  Search::use_history = enabled;
                        else valid = false;

                        if (valid) {
                            if (is_interactive()) {
                                std::cout << std::format("Search option {}{}{} set to {}{}{}.\n", 
                                                         style.green, opt_name, style.reset, style.magenta, enabled ? "on" : "off", style.reset);
                            } else {
                                std::cout << std::format("options {} {}\n", opt_name, enabled ? "on" : "off");
                            }
                        } else {
                            if (is_interactive()) {
                                std::cout << std::format("{}Unknown option: {}{}{}\n", style.red, style.magenta, opt_name, style.reset);
                            } else {
                                std::cout << std::format("Unknown option: {}\n", opt_name);
                            }
                        }
                    }
                } else {
                    bool enabled = false;
                    bool valid = true;
                    
                    if (opt_name == "nmp") enabled = Search::use_nmp;
                    else if (opt_name == "lmr") enabled = Search::use_lmr;
                    else if (opt_name == "rfp") enabled = Search::use_rfp;
                    else if (opt_name == "lmp") enabled = Search::use_lmp;
                    else if (opt_name == "fp")  enabled = Search::use_fp;
                    else if (opt_name == "ce")  enabled = Search::use_check_extensions;
                    else if (opt_name == "aw")  enabled = Search::use_aspiration_window;
                    else if (opt_name == "qs")  enabled = Search::use_quiescence;
                    else if (opt_name == "tt")  enabled = Search::use_tt;
                    else if (opt_name == "kh")  enabled = Search::use_killers;
                    else if (opt_name == "hh")  enabled = Search::use_history;
                    else valid = false;

                    if (valid) {
                        if (is_interactive()) {
                            std::cout << std::format("Search option {}{}{} is currently {}{}{}.\n", 
                                                     style.green, opt_name, style.reset, style.magenta, enabled ? "on" : "off", style.reset);
                        } else {
                            std::cout << std::format("options {} {}\n", opt_name, enabled ? "on" : "off");
                        }
                    } else {
                        if (is_interactive()) {
                            std::cout << std::format("{}Unknown option: {}{}{}\n", style.red, style.magenta, opt_name, style.reset);
                        } else {
                            std::cout << std::format("Unknown option: {}\n", opt_name);
                        }
                    }
                }
            } else {
                if (is_interactive()) {
                    std::cout << "\n" << style.blue << "============ Bully Search Heuristics Options ============" << style.reset << "\n";
                    std::cout << "  " << style.green << "NullMovePruning" << style.reset << " (" << style.magenta << "nmp" << style.reset << ")          : "
                              << style.magenta << (Search::use_nmp ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "LateMoveReduction" << style.reset << " (" << style.magenta << "lmr" << style.reset << ")        : "
                              << style.magenta << (Search::use_lmr ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "ReverseFutilityPruning" << style.reset << " (" << style.magenta << "rfp" << style.reset << ")   : "
                              << style.magenta << (Search::use_rfp ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "LateMovePruning" << style.reset << " (" << style.magenta << "lmp" << style.reset << ")          : "
                              << style.magenta << (Search::use_lmp ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "FutilityPruning" << style.reset << " (" << style.magenta << "fp" << style.reset << ")           : "
                              << style.magenta << (Search::use_fp ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "CheckExtensions" << style.reset << " (" << style.magenta << "ce" << style.reset << ")           : "
                              << style.magenta << (Search::use_check_extensions ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "AspirationWindow" << style.reset << " (" << style.magenta << "aw" << style.reset << ")          : "
                              << style.magenta << (Search::use_aspiration_window ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "QuiescenceSearch" << style.reset << " (" << style.magenta << "qs" << style.reset << ")          : "
                              << style.magenta << (Search::use_quiescence ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "UseTT" << style.reset << " (" << style.magenta << "tt" << style.reset << ")                     : "
                              << style.magenta << (Search::use_tt ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "KillerHeuristic" << style.reset << " (" << style.magenta << "kh" << style.reset << ")           : "
                              << style.magenta << (Search::use_killers ? "ON" : "OFF") << style.reset << "\n";
                    std::cout << "  " << style.green << "HistoryHeuristic" << style.reset << " (" << style.magenta << "hh" << style.reset << ")          : "
                              << style.magenta << (Search::use_history ? "ON" : "OFF") << style.reset << "\n\n";
                    std::cout << "  " << style.green << "Usage" << style.reset << ": "
                              << style.yellow << "options" << style.reset << " [" << style.magenta << "name" << style.reset << " [" << style.green << "on" << style.reset << " | " << style.green << "off" << style.reset << "]]"
                              << "  (e.g., " << style.yellow << "options" << style.reset << " " << style.magenta << "lmr" << style.reset << " " << style.green << "off" << style.reset << ")\n";
                    std::cout << style.blue << "========================================================" << style.reset << "\n\n";
                } else {
                    std::cout << "\n=== Bully Search Heuristics Options ===\n";
                    std::cout << "  NullMovePruning (nmp)          : " << (Search::use_nmp ? "ON" : "OFF") << "\n";
                    std::cout << "  LateMoveReduction (lmr)        : " << (Search::use_lmr ? "ON" : "OFF") << "\n";
                    std::cout << "  ReverseFutilityPruning (rfp)   : " << (Search::use_rfp ? "ON" : "OFF") << "\n";
                    std::cout << "  LateMovePruning (lmp)          : " << (Search::use_lmp ? "ON" : "OFF") << "\n";
                    std::cout << "  FutilityPruning (fp)           : " << (Search::use_fp ? "ON" : "OFF") << "\n";
                    std::cout << "  CheckExtensions (ce)           : " << (Search::use_check_extensions ? "ON" : "OFF") << "\n";
                    std::cout << "  AspirationWindow (aw)          : " << (Search::use_aspiration_window ? "ON" : "OFF") << "\n";
                    std::cout << "  QuiescenceSearch (qs)          : " << (Search::use_quiescence ? "ON" : "OFF") << "\n";
                    std::cout << "  UseTT (tt)                     : " << (Search::use_tt ? "ON" : "OFF") << "\n";
                    std::cout << "  KillerHeuristic (kh)           : " << (Search::use_killers ? "ON" : "OFF") << "\n";
                    std::cout << "  HistoryHeuristic (hh)          : " << (Search::use_history ? "ON" : "OFF") << "\n";
                    std::cout << "  \nUsage: options [name [on|off]] (e.g., options lmr off)\n";
                    std::cout << "=======================================\n\n";
                }
            }
        }
        else if (token == "perft") {
            int depth = 1;
            is >> depth;
            if (depth < 1) depth = 1;

            if (is_interactive()) {
                std::cout << std::format("Running Perft depth {}{}{}...\n", style.magenta, depth, style.reset);
            } else {
                std::cout << std::format("Running Perft depth {}...\n", depth);
            }
            auto start = std::chrono::high_resolution_clock::now();
            uint64_t nodes = run_perft(depth);
            auto end = std::chrono::high_resolution_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            double secs = static_cast<double>(diff) / 1000.0;
            double nps = (secs > 0.0) ? (static_cast<double>(nodes) / secs) : 0.0;
            
            if (is_interactive()) {
                std::cout << std::format("  {}Nodes{}: {}{}{}\n", style.green, style.reset, style.magenta, nodes, style.reset);
                std::cout << std::format("  {}Time{}:  {}{} ms{}\n", style.green, style.reset, style.magenta, diff, style.reset);
                std::cout << std::format("  {}Speed{}: {}{:.0f} NPS{}\n", style.green, style.reset, style.magenta, nps, style.reset);
            } else {
                std::cout << std::format("Nodes: {}\n", nodes);
                std::cout << std::format("Time : {} ms\n", diff);
                std::cout << std::format("Speed: {:.0f} NPS\n", nps);
            }
        }
        else if (token == "divide") {
            int depth = 1;
            is >> depth;
            if (depth < 1) depth = 1;

            if (is_interactive()) {
                std::cout << std::format("Divide depth {}{}{}...\n", style.magenta, depth, style.reset);
            } else {
                std::cout << std::format("Divide depth {}...\n", depth);
            }
            MoveList list;
            list.generate(pos);
            uint64_t total = 0;
            for (size_t i = 0; i < list.size(); ++i) {
                Move m = list[i].move;
                StateInfo next_si;
                if (pos.make_move(m, next_si)) {
                    uint64_t subnodes = run_perft(depth - 1);
                    if (is_interactive()) {
                        std::cout << std::format("  {}{}{}: {}{}{}\n", style.yellow, m.to_string(), style.reset, style.magenta, subnodes, style.reset);
                    } else {
                        std::cout << std::format("  {}: {}\n", m.to_string(), subnodes);
                    }
                    total += subnodes;
                }
                pos.unmake_move(m);
            }
            if (is_interactive()) {
                std::cout << std::format("{}Total{}: {}{}{}\n", style.green, style.reset, style.magenta, total, style.reset);
            } else {
                std::cout << std::format("Total: {}\n", total);
            }
        }
        else if (token == "eval") {
            Eval::print_detailed_eval(pos, use_color);
        }
        else if (token == "help") {
            if (is_interactive()) {
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
                std::cout << "  " << style.yellow << "syzygy" << style.reset << " [" << style.magenta << "<path>" << style.reset << "|" << style.green << "on" << style.reset << "|" << style.green << "off" << style.reset << "]        : View or set Syzygy tablebases path & state.\n";
                std::cout << "  " << style.yellow << "uci" << style.reset << "                        : Switch to UCI engine mode.\n";
                std::cout << "  " << style.yellow << "quit" << style.reset << " / " << style.yellow << "exit" << style.reset
                          << "                : Terminate Bully.\n";
                std::cout << style.blue << "========================================================" << style.reset << "\n\n";
            } else {
                std::cout << "\n=== Bully Interactive CLI Guide ===\n";
                std::cout << "  d / display                         : Visual representation of the active position.\n";
                std::cout << "  position startpos                   : Load standard chess starting position.\n";
                std::cout << "  position fen <FEN>                  : Load a FEN string position.\n";
                std::cout << "                                        (Add 'moves e2e4 ...' to play moves on top).\n";
                std::cout << "  move <e2e4> [e7e5 ...]              : Play one or more moves on the active board.\n";
                std::cout << "  go [depth <D>] [ponder]             : Search (optionally ponder in background) the active position.\n";
                std::cout << "  ponderhit                           : Transition a background ponder search into active search.\n";
                std::cout << "  stop                                : Abort a running search.\n";
                std::cout << "  eval                                : Print detailed static evaluation breakdown.\n";
                std::cout << "  perft <depth>                       : Measure speed & count leaf nodes recursively.\n";
                std::cout << "  divide <depth>                      : Print move-by-move node counts (divide test).\n";
                std::cout << "  hash <MB>                           : Resize transposition table (in Megabytes).\n";
                std::cout << "  threads <count>                     : Set the number of search threads.\n";
                std::cout << "  multipv <count>                     : Set the number of PV lines to show in search.\n";
                std::cout << "  utf8 [on | off]                     : Toggle UTF-8 grid graphics.\n";
                std::cout << "  color [on | off]                    : Toggle ANSI terminal colors.\n";
                std::cout << "  autoprint [on | off]                : Toggle board auto-printing after moves.\n";
                std::cout << "  options [name [on | off]]           : View or toggle search heuristic options.\n";
                std::cout << "  syzygy [<path>|on|off]              : View or set Syzygy tablebases path & state.\n";
                std::cout << "  uci                                 : Switch to UCI engine mode.\n";
                std::cout << "  quit / exit                         : Terminate Bully.\n";
                std::cout << "====================================\n\n";
            }
        }
        else if (token == "quit" || token == "exit") {
            Search::stop_and_join();
            return false;
        }
        else if (is_interactive()) {
            std::cout << std::format("Unknown command: '{}{}{}'. Type '{}{}help{}{}' for options.\n",
                                     style.magenta, token, style.reset,
                                     style.bold, style.yellow, style.reset, style.bold);
        }
        return true;
}

#ifdef _WIN32
constexpr std::string_view BINARY_NAME = "bully.exe";
#else
constexpr std::string_view BINARY_NAME = "bully";
#endif

void UCI::print_arguments_help() {
    std::string bin(BINARY_NAME);
    int p1 = std::max(1, 38 - static_cast<int>(bin.length()));
    int p2 = std::max(1, 28 - static_cast<int>(bin.length()));
    int p3 = std::max(1, 22 - static_cast<int>(bin.length()));
    int p4 = std::max(1, 30 - static_cast<int>(bin.length()));

    if (use_color) {
        std::cout << style.blue << "========================================================\n" << style.reset;
        std::cout << style.cyan << "Bully Chess Engine - Command Line Argument Guide\n" << style.reset;
        std::cout << style.blue << "========================================================\n" << style.reset;
        std::cout << style.yellow << "Usage:\n" << style.reset;
        std::cout << std::format("  {}{}{}{:>{}} : Starts in interactive/UCI loop mode.\n", style.yellow, bin, style.reset, "", p1);
        std::cout << std::format("  {}{}{} {}<command>{}{:>{}} : Executes a single command and exits.\n", style.yellow, bin, style.reset, style.magenta, style.reset, "", p2);
        std::cout << std::format("  {}{}{} {}\"<cmd1>; <cmd2>\"{}{:>{}} : Executes multiple commands and exits.\n\n", style.yellow, bin, style.reset, style.magenta, style.reset, "", p3);
        std::cout << style.yellow << "Examples:\n" << style.reset;
        std::cout << std::format("  {}{} perft 5{}{:>{}} : Run perft depth 5 and exit.\n", style.green, bin, style.reset, "", p4);
        std::cout << std::format("  {}{} \"position startpos moves e2e4 e7e5; go depth 10\"{}\n", style.green, bin, style.reset);
        std::cout << "                                        : Setup board, play moves, search and exit.\n";
        std::cout << style.blue << "========================================================\n\n" << style.reset;
    } else {
        std::cout << "========================================================\n";
        std::cout << "Bully Chess Engine - Command Line Argument Guide\n";
        std::cout << "========================================================\n";
        std::cout << "Usage:\n";
        std::cout << std::format("  {: <38} : Starts in interactive/UCI loop mode.\n", bin);
        std::cout << std::format("  {: <38} : Executes a single command and exits.\n", bin + " <command>");
        std::cout << std::format("  {: <38} : Executes multiple commands and exits.\n\n", bin + " \"<cmd1>; <cmd2>\"");
        std::cout << "Examples:\n";
        std::cout << std::format("  {: <38} : Run perft depth 5 and exit.\n", bin + " perft 5");
        std::cout << std::format("  {}\n", bin + " \"position startpos moves e2e4 e7e5; go depth 10\"");
        std::cout << "                                        : Setup board, play moves, search and exit.\n";
        std::cout << "========================================================\n\n";
    }
}

} // namespace Bully