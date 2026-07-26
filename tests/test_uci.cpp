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

    testing::internal::CaptureStdout();
    bool keep_running = uci.execute_line("uci");
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(keep_running);
    EXPECT_TRUE(output.find("uciok") != std::string::npos);
    EXPECT_TRUE(output.find("id name") != std::string::npos);

    testing::internal::CaptureStdout();
    keep_running = uci.execute_line("isready");
    output = testing::internal::GetCapturedStdout();

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
