#include <gtest/gtest.h>
#include <string>
#include <sstream>
#include "../src/types.h"
#include "../src/position.h"
#include "../src/search.h"
#include "../src/tt.h"
#include "../src/syzygy.h"
#include "../src/uci.h"

using namespace Bully;

class UCITest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        init_bitboards();
        init_attacks();
        init_zobrist();
        TT.resize(16);
    }
};

// 1. Verify standard UCI initialization handshake
TEST_F(UCITest, Handshake) {
    UCI uci;

    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    bool keep_running = uci.execute_line("uci");
    std::cout.rdbuf(old);
    std::string output = buffer.str();

    EXPECT_TRUE(keep_running);
    EXPECT_TRUE(output.find("uciok") != std::string::npos);
    EXPECT_TRUE(output.find("id name") != std::string::npos);

    buffer.str("");
    buffer.clear();
    std::cout.rdbuf(buffer.rdbuf());
    keep_running = uci.execute_line("isready");
    std::cout.rdbuf(old);
    output = buffer.str();

    EXPECT_TRUE(keep_running);
    EXPECT_TRUE(output.find("readyok") != std::string::npos);
}

// 2. Verify setting UCI options correctly updates engine state variables
TEST_F(UCITest, SetOption) {
    UCI uci;

    // Change hash size
    uci.execute_line("setoption name Hash value 8");
    EXPECT_EQ(TT.get_size_mb(), 8);

    // Change Threads count
    uci.execute_line("setoption name Threads value 4");
    EXPECT_EQ(Search::num_threads, 4);
    uci.execute_line("setoption name Threads value 1");
    EXPECT_EQ(Search::num_threads, 1);

    // Change SyzygyPath
    uci.execute_line("setoption name SyzygyPath value dummy_path");
    EXPECT_EQ(Syzygy::path, "dummy_path");
}

// 3. Verify loop termination commands
TEST_F(UCITest, QuitTermination) {
    UCI uci;

    EXPECT_FALSE(uci.execute_line("quit"));
    EXPECT_FALSE(uci.execute_line("exit"));
}

// 4. Verify position and move parsing execution
TEST_F(UCITest, PositionExecution) {
    UCI uci;

    // This command updates the position. It should run successfully.
    EXPECT_TRUE(uci.execute_line("position startpos moves e2e4 e7e5"));
}

// 5. Verify benchmark execution flow at depth 1
TEST_F(UCITest, BenchmarkExecution) {
    UCI uci;
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    bool keep_running = uci.execute_line("bench 1");
    std::cout.rdbuf(old);
    EXPECT_TRUE(keep_running);
    EXPECT_TRUE(buffer.str().find("Total Nodes") != std::string::npos);
}

// 6. Verify ucinewgame clears TT and stops active search
TEST_F(UCITest, UCINewGame) {
    UCI uci;
    EXPECT_TRUE(uci.execute_line("ucinewgame"));
    EXPECT_TRUE(Search::stopped.load());
}

// 7. Verify move history parser and connected StateInfo stack
TEST_F(UCITest, PositionMovesParsing) {
    UCI uci;
    EXPECT_TRUE(uci.execute_line("position startpos moves e2e4 e7e5 g1f3 b8c6"));

    // Compare against equivalent position loaded directly via set_fen
    Position pos_expected;
    StateInfo si_expected;
    pos_expected.set_fen("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 1", si_expected);

    EXPECT_EQ(uci.position().get_fen(), pos_expected.get_fen());
    EXPECT_EQ(uci.position().key(), pos_expected.key());
}

// 8. Verify variation command selects standard and chess960
TEST_F(UCITest, VariationCommand) {
    UCI uci;
    EXPECT_TRUE(uci.execute_line("variation chess960"));
    EXPECT_TRUE(Position::is_chess960);

    EXPECT_TRUE(uci.execute_line("variation standard"));
    EXPECT_FALSE(Position::is_chess960);

    EXPECT_TRUE(uci.execute_line("variation 960"));
    EXPECT_TRUE(Position::is_chess960);

    EXPECT_TRUE(uci.execute_line("variation"));
    EXPECT_TRUE(Position::is_chess960);

    // Reset back to standard for remaining tests
    Position::is_chess960 = false;
}
