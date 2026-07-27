#include "bitboard.h"
#include "attacks.h"
#include "position.h"
#include "evaluation.h"
#include "uci.h"
#include "search.h"

#include <sstream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    // Initialize core engine tables
    Bully::init_bitboards();
    Bully::init_attacks();
    Bully::init_zobrist();
    Bully::Eval::init_eval();

    Bully::UCI uci;
    uci.init();

    if (argc > 1) {
        std::string first_arg = argv[1];
        if (first_arg == "help" || first_arg == "-h" || first_arg == "--help") {
            uci.print_arguments_help();
            return 0;
        }

        // Join command line arguments
        std::string full_command;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) full_command += " ";
            full_command += argv[i];
        }

        // Split by ';' and execute
        std::istringstream stream(full_command);
        std::string segment;
        while (std::getline(stream, segment, ';')) {
            // Trim leading/trailing whitespace
            size_t start = segment.find_first_not_of(" \t\r\n");
            std::string cmd = (start == std::string::npos) ? "" : segment.substr(start);
            size_t end = cmd.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                cmd = cmd.substr(0, end + 1);
            }
            
            if (!cmd.empty()) {
                if (!uci.execute_line(cmd)) {
                    break;
                }
            }
        }

        // Wait for background search thread to finish if running
        while (!Bully::Search::stopped.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // Ensure search thread is joined/cleaned up
        Bully::Search::stop_and_join();

        return 0;
    }

    // Start UCI communication loop
    uci.loop();

    // Ensure search thread is joined/cleaned up before exiting
    Bully::Search::stop_and_join();

    return 0;
}