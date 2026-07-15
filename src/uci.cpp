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

// Convert a Move object to UCI format string
static std::string move_to_string(Move m) {
    if (!m) return "none";
    std::string s;
    Square from = m.from_sq();
    Square to = m.to_sq();
    s += static_cast<char>('a' + std::to_underlying(file_of(from)));
    s += static_cast<char>('1' + std::to_underlying(rank_of(from)));
    s += static_cast<char>('a' + std::to_underlying(file_of(to)));
    s += static_cast<char>('1' + std::to_underlying(rank_of(to)));
    if (m.type_of() == PROMOTION) {
        char p = '?';
        switch (m.promotion_type()) {
            case KNIGHT:  p = 'n'; break;
            case BISHOP:  p = 'b'; break;
            case ROOK:    p = 'r'; break;
            case QUEEN:   p = 'q'; break;
            default: break;
        }
        s += p;
    }
    return s;
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

    // Initialize starting position and TT first so sizes are resolved
    TT.resize(16);
    history.emplace_back();
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", history.back());

    use_utf8 = detect_utf8();
    use_color = is_interactive();
    style.init(use_color);

    if (is_interactive()) {
        std::cout << style.blue << "========================================================\n" << style.reset;
        std::cout << style.cyan << " ██████╗  ██╗   ██╗ ██╗      ██╗   ██╗   ██╗\n";
        std::cout << " ██╔══██╗ ██║   ██║ ██║      ██║   ╚██╗ ██╔╝\n";
        std::cout << " ██████╔╝ ██║   ██║ ██║      ██║    ╚████╔╝ \n";
        std::cout << " ██╔══██╗ ██║   ██║ ██║      ██║     ╚██╔╝  \n";
        std::cout << " ██████╔╝ ╚██████╔╝ ███████╗ ███████╗ ██║   \n";
        std::cout << " ╚═════╝   ╚═════╝  ╚══════╝ ╚══════╝ ╚═╝   \n" << style.reset;
        std::cout << style.blue << "========================================================\n" << style.reset;
        std::cout << std::format("  {}Version{}      : {}{}{}\n", style.green, style.reset, style.magenta, ENGINE_VERSION, style.reset);
        std::cout << std::format("  {}Author{}       : {}{}{}\n", style.green, style.reset, style.magenta, ENGINE_AUTHOR, style.reset);
        std::cout << std::format("  {}Compiler{}     : {}GCC {}{}\n", style.green, style.reset, style.magenta, __VERSION__, style.reset);
        
        #if defined(__BMI2__) && defined(USE_PEXT)
        std::cout << std::format("  {}Hardware{}     : {}BMI2 / PEXT (Hardware Accelerated){}\n", style.green, style.reset, style.magenta, style.reset);
        #elif defined(__AVX2__)
        std::cout << std::format("  {}Hardware{}     : {}AVX2 / Magic Bitboards{}\n", style.green, style.reset, style.magenta, style.reset);
        #else
        std::cout << std::format("  {}Hardware{}     : {}Generic x86-64 / Magic Bitboards{}\n", style.green, style.reset, style.magenta, style.reset);
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
        if (move_to_string(m) == move_str) {
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
                        std::cout << std::format("  {}position startpos{}               : Load standard starting chess position.\n", style.yellow, style.reset);
                        std::cout << std::format("  {}position fen{} {}<FEN>{}            : Load a custom FEN string.\n", style.yellow, style.reset, style.magenta, style.reset);
                        std::cout << std::format("  {}position{} {}<preset>{}             : Load one of the famous preset positions.\n\n", style.yellow, style.reset, style.green, style.reset);
                        std::cout << std::format("Available Preset Positions:\n");
                        std::cout << std::format("  {}kiwipete{} / {}pos2{} : Standard complex test/perft position (KiwiPete).\n", style.green, style.reset, style.green, style.reset);
                        std::cout << std::format("  {}lasker{}            : Lasker-Reichhelm pawn endgame study.\n", style.green, style.reset);
                        std::cout << std::format("  {}fools{}             : Fool's Mate setup.\n", style.green, style.reset);
                        std::cout << std::format("  {}scholars{}          : Scholar's Mate setup.\n", style.green, style.reset);
                        std::cout << std::format("  {}pos1{}              : Starting position (alias for startpos).\n", style.green, style.reset);
                        std::cout << std::format("  {}pos3{}              : Perft test position #3.\n", style.green, style.reset);
                        std::cout << std::format("  {}pos4{}              : Perft test position #4.\n", style.green, style.reset);
                        std::cout << std::format("  {}pos5{}              : Perft test position #5.\n", style.green, style.reset);
                        std::cout << std::format("  {}pos6{}              : Perft test position #6.\n", style.green, style.reset);
                        std::cout << std::format("\nNote: You can append '{}moves{} {}e2e4 ...{}' to play moves on top of any position.\n",
                                                 style.green, style.reset, style.magenta, style.reset);
                        std::cout << std::format("{}================================{}\n\n", style.blue, style.reset);
                    } else {
                        std::cout << "Usage:\n";
                        std::cout << "  position startpos               : Load standard starting chess position.\n";
                        std::cout << "  position fen <FEN>            : Load a custom FEN string.\n";
                        std::cout << "  position <preset>             : Load one of the famous preset positions.\n\n";
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
                        std::cout << std::format(" {}{}{}", style.yellow, move_to_string(m), style.reset);
                    }
                    std::cout << "\n";
                    
                    if (autoprint) {
                        pos.print(use_utf8, use_color);
                    }
                } else {
                    std::cout << "Played moves:";
                    for (Move m : played_moves) {
                        std::cout << std::format(" {}", move_to_string(m));
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
                        std::cout << std::format("{}: {}\n", move_to_string(m), subnodes);
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

                if (is_interactive() && !limits.time_controlled() && limits.depth == -1 && !limits.infinite) {
                    limits.depth = 6;
                }

                Search::start(pos, limits, history);
            }
        }
        else if (token == "stop") {
            Search::stop_and_join();
        }
        else if (token == "d" || token == "display") {
            pos.print(use_utf8, use_color);
        }
        else if (token == "hash") {
            int val = 0;
            if (is >> val) {
                TT.resize(static_cast<size_t>(val));
                if (is_interactive()) {
                    std::cout << std::format("Hash size resized to {}{}{} MB.\n", style.magenta, val, style.reset);
                } else {
                    std::cout << std::format("Hash size resized to {} MB.\n", val);
                }
            } else {
                if (is_interactive()) {
                    std::cout << std::format("{}Current Hash size{}: {}{} MB{}\nUsage: {}hash{} {}<size_in_MB>{} (e.g., 'hash 64')\n",
                                             style.green, style.reset, style.magenta, TT.get_size_mb(), style.reset, style.yellow, style.reset, style.magenta, style.reset);
                } else {
                    std::cout << std::format("Current Hash size: {} MB\nUsage: hash <size_in_MB> (e.g., 'hash 64')\n", TT.get_size_mb());
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
                        std::cout << std::format("  {}{}{}: {}{}{}\n", style.yellow, move_to_string(m), style.reset, style.magenta, subnodes, style.reset);
                    } else {
                        std::cout << std::format("  {}: {}\n", move_to_string(m), subnodes);
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
        else if (token == "help") {
            std::cout << std::format("\n{}=== Bully Interactive CLI Guide ==={}\n", style.blue, style.reset);
            std::cout << std::format("  {}d{} / {}display{}         : Visual representation of the active position.\n",
                                     style.yellow, style.reset, style.yellow, style.reset);
            std::cout << std::format("  {}position{} {}startpos{}   : Load standard chess starting position.\n",
                                     style.yellow, style.reset, style.green, style.reset);
            std::cout << std::format("  {}position{} {}fen{} {}<FEN>{}  : Load a FEN string position.\n",
                                     style.yellow, style.reset, style.green, style.reset, style.magenta, style.reset);
            std::cout << std::format("                        (Add '{}moves{} {}e2e4 ...{}' to play moves on top).\n",
                                     style.green, style.reset, style.magenta, style.reset);
            std::cout << std::format("  {}move{} {}<e2e4> [e7e5 ...]{} : Play one or more moves on the active board.\n",
                                     style.yellow, style.reset, style.magenta, style.reset);
            std::cout << std::format("  {}go{} [{}depth{} {}<D>{}]      : Search the active position for the best move.\n",
                                     style.yellow, style.reset, style.green, style.reset, style.magenta, style.reset);
            std::cout << std::format("  {}stop{}                : Abort a running search.\n",
                                     style.yellow, style.reset);
            std::cout << std::format("  {}perft{} {}<depth>{}       : Measure speed & count leaf nodes recursively.\n",
                                     style.yellow, style.reset, style.magenta, style.reset);
            std::cout << std::format("  {}divide{} {}<depth>{}      : Print move-by-move node counts (divide test).\n",
                                     style.yellow, style.reset, style.magenta, style.reset);
            std::cout << std::format("  {}hash{} {}<MB>{}           : Resize transposition table (in Megabytes).\n",
                                     style.yellow, style.reset, style.magenta, style.reset);
            std::cout << std::format("  {}threads{} {}<count>{}     : Set the number of search threads.\n",
                                     style.yellow, style.reset, style.magenta, style.reset);
            std::cout << std::format("  {}multipv{} {}<count>{}     : Set the number of PV lines to show in search.\n",
                                     style.yellow, style.reset, style.magenta, style.reset);
            std::cout << std::format("  {}utf8{} [{}on{} | {}off{}]     : Toggle UTF-8 grid graphics.\n",
                                     style.yellow, style.reset, style.green, style.reset, style.green, style.reset);
            std::cout << std::format("  {}color{} [{}on{} | {}off{}]    : Toggle ANSI terminal colors.\n",
                                     style.yellow, style.reset, style.green, style.reset, style.green, style.reset);
            std::cout << std::format("  {}autoprint{} [{}on{} | {}off{}]: Toggle board auto-printing after moves.\n",
                                     style.yellow, style.reset, style.green, style.reset, style.green, style.reset);
            std::cout << std::format("  {}uci{}                 : Switch to UCI engine mode.\n",
                                     style.yellow, style.reset);
            std::cout << std::format("  {}quit{} / {}exit{}         : Terminate Bully.\n",
                                     style.yellow, style.reset, style.yellow, style.reset);
            std::cout << std::format("{}===================================={}\n\n", style.blue, style.reset);
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

void UCI::print_arguments_help() {
    if (use_color) {
        std::cout << style.blue << "========================================================\n" << style.reset;
        std::cout << style.cyan << "Bully Chess Engine - Command Line Argument Guide\n" << style.reset;
        std::cout << style.blue << "========================================================\n" << style.reset;
        std::cout << style.yellow << "Usage:\n" << style.reset;
        std::cout << std::format("  {}bully-native.exe{}                      : Starts in interactive/UCI loop mode.\n", style.yellow, style.reset);
        std::cout << std::format("  {}bully-native.exe{} {}<command>{}            : Executes a single command and exits.\n", style.yellow, style.reset, style.magenta, style.reset);
        std::cout << std::format("  {}bully-native.exe{} {}\"<cmd1>; <cmd2>\"{}     : Executes multiple commands and exits.\n\n", style.yellow, style.reset, style.magenta, style.reset);
        std::cout << style.yellow << "Examples:\n" << style.reset;
        std::cout << std::format("  {}bully-native.exe perft 5{}              : Run perft depth 5 and exit.\n", style.green, style.reset);
        std::cout << std::format("  {}bully-native.exe \"position startpos moves e2e4 e7e5; go depth 10\"{}\n", style.green, style.reset);
        std::cout << "                                        : Setup board, play moves, search and exit.\n";
        std::cout << style.blue << "========================================================\n\n" << style.reset;
    } else {
        std::cout << "========================================================\n";
        std::cout << "Bully Chess Engine - Command Line Argument Guide\n";
        std::cout << "========================================================\n";
        std::cout << "Usage:\n";
        std::cout << "  bully-native.exe                      : Starts in interactive/UCI loop mode.\n";
        std::cout << "  bully-native.exe <command>            : Executes a single command and exits.\n";
        std::cout << "  bully-native.exe \"<cmd1>; <cmd2>\"     : Executes multiple commands and exits.\n\n";
        std::cout << "Examples:\n";
        std::cout << "  bully-native.exe perft 5              : Run perft depth 5 and exit.\n";
        std::cout << "  bully-native.exe \"position startpos moves e2e4 e7e5; go depth 10\"\n";
        std::cout << "                                        : Setup board, play moves, search and exit.\n";
        std::cout << "========================================================\n\n";
    }
}

} // namespace Bully