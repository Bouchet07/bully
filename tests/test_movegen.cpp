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
        list.generate_legal(pos);

        for (size_t i = 0; i < list.size(); ++i) {
            Move m = list[i].move;
            StateInfo next_si;
            
            pos.make_move(m, next_si);
            nodes += perft(depth - 1, pos);
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
    pos.set_fen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 21", si);

    EXPECT_EQ(perft(1, pos), 6ULL);
    EXPECT_EQ(perft(2, pos), 264ULL);
    EXPECT_EQ(perft(3, pos), 9467ULL);
}

// 5. Test captures-only move generation (generate_captures)
TEST_F(MoveGenTest, GenerateCaptures) {
    Position pos;
    StateInfo si;
    // 1. e4 d5 setup
    pos.set_fen("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", si);

    MoveList list;
    list.generate_captures(pos);

    // There should only be exactly 1 capture (e4xd5) and no quiet moves (e.g. e4e5, d2d3)
    EXPECT_EQ(list.size(), 1);
    EXPECT_EQ(list[0].move, Move(SQ_E4, SQ_D5));
}

// 6. Position 5 - Pinned pieces and pawn promotions
TEST_F(MoveGenTest, PerftPosition5) {
    Position pos;
    StateInfo si;
    pos.set_fen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", si);

    EXPECT_EQ(perft(1, pos), 44ULL);
    EXPECT_EQ(perft(2, pos), 1486ULL);
    EXPECT_EQ(perft(3, pos), 62379ULL);
}

// 7. Position 6 - Middlegame tactical pins
TEST_F(MoveGenTest, PerftPosition6) {
    Position pos;
    StateInfo si;
    pos.set_fen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", si);

    EXPECT_EQ(perft(1, pos), 46ULL);
    EXPECT_EQ(perft(2, pos), 2079ULL);
    EXPECT_EQ(perft(3, pos), 89890ULL);
}

// 8. Horizontal En Passant Pin test
TEST_F(MoveGenTest, HorizontalEnPassantPin) {
    Position pos;
    StateInfo si;
    // King on A5, White Pawn on E5, Black Pawn on D5 (just pushed d7-d5 -> e.p. sq d6), Black Rook on H5
    pos.set_fen("8/8/8/K2pP2r/8/8/8/8 w - d6 0 1", si);

    Move ep_move = Move::make<EN_PASSANT>(SQ_E5, SQ_D6);
    EXPECT_FALSE(pos.legal(ep_move, pos.blockers_for_king(pos.side_to_move())));
}
