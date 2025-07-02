#include <print>
#include <iostream>
#include <string>
#include <sstream>

#include "types.h"
#include "uci.h"

void UCI::init() {
	std::println("id name {} {}", ENGINE_NAME, ENGINE_VERSION);	
	std::println("id author {}", ENGINE_AUTHOR);
}

void UCI::loop() {
	

    std::string line;
    std::string token;
    while (std::getline(std::cin, line)) {
        std::istringstream is(line);
        token.clear();
        is >> std::skipws >> token;
        if (token == "uci") {
            std::println("id name {} {}", ENGINE_NAME, ENGINE_VERSION);
            std::println("id author {}", ENGINE_AUTHOR);
            std::println("{}", UCI_OPTIONS);
            std::println("uciok");
        }
        else if (token == "isready") {
            std::println("readyok");
        }
        else if (token == "ucinewgame") {
        }
        else if (token == "go") {
        }
        else if (token == "stop") {
        }
        else if (token == "position") {
        }
        else if (token == "d") {
        }
        else if (token == "bench") {
        }
        else if (token == "utf8") {
        }
        else if (token == "setoption") {
        }
        else if (token == "eval") {
        }
        else if (token == "info") {
        }
        else if (token == "quit") {
            break;
        }
    }
}