#include <print>

#include "bitboard.h"
#include "uci.h"

int main(int argc, char** argv) {
	UCI uci;
	uci.init();
	uci.loop();
	return 0;
}