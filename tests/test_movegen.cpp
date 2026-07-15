#include <gtest/gtest.h>
#include "../src/types.h"
#include "../src/bitboard.h"
#include "../src/attacks.h"
#include "../src/position.h"
#include "../src/movegen.h"

using namespace Bully;

class MoveGenTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        init_bitboards();
        init_attacks();
        init_zobrist();
    }

    uint64_t perft(int depth, Position& pos) {
        if (depth == 0) return 1ULL;

        uint64_t nodes = 0;
        MoveList list;
        list.generate(pos);

        for (size_t i = 0; i < list.size(); ++i) {
            Move m = list[i].move;
            StateInfo next_si;
            
            // Only count legal moves
            if (pos.make_move(m, next_si)) {
                nodes += perft(depth - 1, pos);
            }
            pos.unmake_move(m);
        }
        return nodes;
    }
};

// 1. Initial Starting Position
TEST_F(MoveGenTest, PerftStartingPosition) {
    Position pos;
    StateInfo si;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", si);

    EXPECT_EQ(perft(1, pos), 20ULL);
    EXPECT_EQ(perft(2, pos), 400ULL);
    EXPECT_EQ(perft(3, pos), 8902ULL);
    EXPECT_EQ(perft(4, pos), 197281ULL);
}

// 2. Position 2 (Kiwipete) - Heavily tests castling and pins
TEST_F(MoveGenTest, PerftKiwipete) {
    Position pos;
    StateInfo si;
    pos.set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", si);

    EXPECT_EQ(perft(1, pos), 48ULL);
    EXPECT_EQ(perft(2, pos), 2039ULL);
    EXPECT_EQ(perft(3, pos), 97862ULL);
}

// 3. Position 3 - Tests king safety and blocked squares
TEST_F(MoveGenTest, PerftPosition3) {
    Position pos;
    StateInfo si;
    pos.set_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", si);

    EXPECT_EQ(perft(1, pos), 14ULL);
    EXPECT_EQ(perft(2, pos), 191ULL);
    EXPECT_EQ(perft(3, pos), 2812ULL);
}

std::string move_to_string(Move m) {
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
            case KNIGHT: p = 'n'; break;
            case BISHOP: p = 'b'; break;
            case ROOK: p = 'r'; break;
            case QUEEN: p = 'q'; break;
            default: break;
        }
        s += p;
    }
    return s;
}

// 4. Position 4 - Complex position containing en passant and promotions
TEST_F(MoveGenTest, PerftPosition4) {
    Position pos;
    StateInfo si;
    pos.set_fen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", si);

    EXPECT_EQ(perft(1, pos), 6ULL);
    EXPECT_EQ(perft(2, pos), 264ULL);
    EXPECT_EQ(perft(3, pos), 9467ULL);
}
