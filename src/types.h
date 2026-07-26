#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <utility>
#include <algorithm>
#include <type_traits>

namespace Bully {

// ============================================================================
// Engine Metadata & Constants
// ============================================================================
constexpr std::string_view ENGINE_NAME    = "Bully";
constexpr std::string_view ENGINE_VERSION = "1.1-dev";
constexpr std::string_view ENGINE_AUTHOR  = "Diego Bouchet";
constexpr std::string_view UCI_OPTIONS    = "option name Hash type spin default 16 min 1 max 2048\noption name Clear Hash type button\noption name Threads type spin default 1 min -1 max 128\noption name SyzygyPath type string default syzygy\noption name MultiPV type spin default 1 min 1 max 128\noption name NullMovePruning type check default true\noption name LateMoveReduction type check default true\noption name ReverseFutilityPruning type check default true\noption name LateMovePruning type check default true\noption name FutilityPruning type check default true\noption name CheckExtensions type check default true\noption name AspirationWindow type check default true\noption name QuiescenceSearch type check default true\noption name UseTT type check default true\noption name KillerHeuristic type check default true\noption name HistoryHeuristic type check default true\noption name Ponder type check default false";

using Key      = uint64_t;
using Bitboard = uint64_t;

constexpr uint16_t MAX_MOVES         = 256;
constexpr uint8_t  MAX_PLY           = 246;
constexpr uint8_t  MAX_DEPTH         = 64;
constexpr uint8_t  FULL_DEPTH_MOVES  = 4;
constexpr uint8_t  REDUCTION_LIMIT   = 3;

// ============================================================================
// Core Chess Enums
// ============================================================================
enum Color : uint8_t {
    WHITE,
    BLACK,
    COLOR_NB,
    BOTH = COLOR_NB,
};

enum CastlingRights : uint8_t {
    NO_CASTLING     = 0,
    WHITE_OO        = 1,
    WHITE_OOO       = WHITE_OO << 1,
    BLACK_OO        = WHITE_OO << 2,
    BLACK_OOO       = WHITE_OO << 3,

    KING_SIDE       = WHITE_OO | BLACK_OO,
    QUEEN_SIDE      = WHITE_OOO | BLACK_OOO,
    WHITE_CASTLING  = WHITE_OO | WHITE_OOO,
    BLACK_CASTLING  = BLACK_OO | BLACK_OOO,
    ANY_CASTLING    = WHITE_CASTLING | BLACK_CASTLING,

    CASTLING_RIGHT_NB = 16
};

enum Bound : uint8_t {
    BOUND_NONE   = 0,
    BOUND_UPPER  = 1,
    BOUND_LOWER  = 2,
    BOUND_EXACT  = BOUND_UPPER | BOUND_LOWER
};

// ============================================================================
// Evaluation Scores & Piece Values
// ============================================================================
using Value = int16_t;

constexpr Value VALUE_ZERO             = 0;
constexpr Value VALUE_DRAW             = 0;
constexpr Value VALUE_NONE             = 32002;
constexpr Value VALUE_INFINITE         = 32001;

constexpr Value VALUE_MATE             = 32000;
constexpr Value VALUE_MATE_IN_MAX_PLY  = static_cast<Value>(VALUE_MATE - MAX_PLY);
constexpr Value VALUE_MATED_IN_MAX_PLY = static_cast<Value>(-VALUE_MATE_IN_MAX_PLY);

constexpr Value VALUE_TB                 = static_cast<Value>(VALUE_MATE_IN_MAX_PLY - 1);
constexpr Value VALUE_TB_WIN_IN_MAX_PLY  = static_cast<Value>(VALUE_TB - MAX_PLY);
constexpr Value VALUE_TB_LOSS_IN_MAX_PLY = static_cast<Value>(-VALUE_TB_WIN_IN_MAX_PLY);

constexpr Value PawnValue   = 100;
constexpr Value KnightValue = 320;
constexpr Value BishopValue = 330;
constexpr Value RookValue   = 500;
constexpr Value QueenValue  = 900;

enum PieceType : uint8_t {
    NO_PIECE_TYPE = 0,
    PAWN          = 1,
    KNIGHT        = 2,
    BISHOP        = 3,
    ROOK          = 4,
    QUEEN         = 5,
    KING          = 6,
    ALL_PIECES    = 0,
    PIECE_TYPE_NB = 8
};

enum Piece : uint8_t {
    NO_PIECE = 0,
    W_PAWN   = PAWN, 
    W_KNIGHT, 
    W_BISHOP, 
    W_ROOK, 
    W_QUEEN, 
    W_KING,
    B_PAWN   = PAWN + 8, 
    B_KNIGHT, 
    B_BISHOP, 
    B_ROOK, 
    B_QUEEN, 
    B_KING,
    PIECE_NB = 16
};

// ============================================================================
// Board Geometry (Squares, Files, Ranks, Directions)
// ============================================================================
enum Square : int8_t {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE,

    SQUARE_ZERO = 0,
    SQUARE_NB   = 64
};

enum Direction : int8_t {
    NORTH      = 8,
    EAST       = 1,
    SOUTH      = -NORTH,
    WEST       = -EAST,
    NORTH_EAST = NORTH + EAST,
    SOUTH_EAST = SOUTH + EAST,
    SOUTH_WEST = SOUTH + WEST,
    NORTH_WEST = NORTH + WEST
};

enum File : int8_t {
    FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H,
    FILE_NB
};

enum Rank : int8_t {
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8,
    RANK_NB
};

// ============================================================================
// Operator Overloads for strongly-typed enums
// ============================================================================

// Enable standard ++ and -- operators using C++23 std::to_underlying
#define ENABLE_INCR_OPERATORS_ON(T) \
    inline T& operator++(T& d) { return d = static_cast<T>(std::to_underlying(d) + 1); } \
    inline T& operator--(T& d) { return d = static_cast<T>(std::to_underlying(d) - 1); }

ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Color)
ENABLE_INCR_OPERATORS_ON(Piece)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

#undef ENABLE_INCR_OPERATORS_ON

[[nodiscard]] constexpr Direction operator+(Direction d1, Direction d2) { 
    return static_cast<Direction>(std::to_underlying(d1) + std::to_underlying(d2)); 
}

[[nodiscard]] constexpr Direction operator*(int i, Direction d) { 
    return static_cast<Direction>(i * std::to_underlying(d)); 
}

[[nodiscard]] constexpr Square operator+(Square s, Direction d) { 
    return static_cast<Square>(std::to_underlying(s) + std::to_underlying(d)); 
}

[[nodiscard]] constexpr Square operator-(Square s, Direction d) { 
    return static_cast<Square>(std::to_underlying(s) - std::to_underlying(d)); 
}

inline Square& operator+=(Square& s, Direction d) { return s = s + d; }
inline Square& operator-=(Square& s, Direction d) { return s = s - d; }

// Toggle color (WHITE <-> BLACK)
[[nodiscard]] constexpr Color operator~(Color c) { 
    return static_cast<Color>(std::to_underlying(c) ^ BLACK); 
}

// Swap Rank A1 <-> A8
[[nodiscard]] constexpr Square flip_rank(Square s) { 
    return static_cast<Square>(std::to_underlying(s) ^ SQ_A8); 
}

// Swap File A1 <-> H1
[[nodiscard]] constexpr Square flip_file(Square s) { 
    return static_cast<Square>(std::to_underlying(s) ^ SQ_H1); 
}

// Swap color of a piece (e.g. W_KNIGHT <-> B_KNIGHT)
[[nodiscard]] constexpr Piece operator~(Piece pc) { 
    return static_cast<Piece>(std::to_underlying(pc) ^ 8); 
}

[[nodiscard]] constexpr CastlingRights operator&(Color c, CastlingRights cr) {
    return static_cast<CastlingRights>((c == WHITE ? WHITE_CASTLING : BLACK_CASTLING) & cr);
}

// ============================================================================
// Core Helper Functions
// ============================================================================
[[nodiscard]] constexpr Value mate_in(int ply) { 
    return static_cast<Value>(VALUE_MATE - ply); 
}

[[nodiscard]] constexpr Value mated_in(int ply) { 
    return static_cast<Value>(-VALUE_MATE + ply); 
}

[[nodiscard]] constexpr Square make_square(File f, Rank r) { 
    return static_cast<Square>((std::to_underlying(r) << 3) + std::to_underlying(f)); 
}

[[nodiscard]] constexpr Piece make_piece(Color c, PieceType pt) { 
    return static_cast<Piece>((std::to_underlying(c) << 3) + std::to_underlying(pt)); 
}

[[nodiscard]] constexpr PieceType type_of(Piece pc) { 
    return static_cast<PieceType>(std::to_underlying(pc) & 7); 
}

[[nodiscard]] constexpr Color color_of(Piece pc) { 
    return static_cast<Color>(std::to_underlying(pc) < 8 ? WHITE : BLACK); 
}

[[nodiscard]] constexpr Value get_piece_value(PieceType pt) {
    switch (pt) {
        case PAWN:   return PawnValue;
        case KNIGHT: return KnightValue;
        case BISHOP: return BishopValue;
        case ROOK:   return RookValue;
        case QUEEN:  return QueenValue;
        case KING:   return 10000;
        default:     return VALUE_ZERO;
    }
}

[[nodiscard]] constexpr bool is_ok(Square s) { 
    return s >= SQ_A1 && s <= SQ_H8; 
}

[[nodiscard]] constexpr File file_of(Square s) { 
    return static_cast<File>(std::to_underlying(s) & 7); 
}

[[nodiscard]] constexpr Rank rank_of(Square s) { 
    return static_cast<Rank>(std::to_underlying(s) >> 3); 
}

[[nodiscard]] constexpr Square relative_square(Color c, Square s) { 
    return static_cast<Square>(std::to_underlying(s) ^ (std::to_underlying(c) * 56)); 
}

[[nodiscard]] constexpr Rank relative_rank(Color c, Rank r) { 
    return static_cast<Rank>(std::to_underlying(r) ^ (std::to_underlying(c) * 7)); 
}

[[nodiscard]] constexpr Rank relative_rank(Color c, Square s) { 
    return relative_rank(c, rank_of(s)); 
}

[[nodiscard]] constexpr Direction pawn_push(Color c) { 
    return c == WHITE ? NORTH : SOUTH; 
}

// Pseudo-random key generator seed multiplier
[[nodiscard]] constexpr Key make_key(uint64_t seed) {
    return seed * 6364136223846793005ULL + 1442695040888963407ULL;
}

// Convert strongly-typed enum values to size_t for safe array indexing (avoids sign-conversion warnings)
template<typename T> requires std::is_enum_v<T>
[[nodiscard]] constexpr size_t to_index(T val) {
    return static_cast<size_t>(std::to_underlying(val));
}

template<typename T> requires std::is_integral_v<T>
[[nodiscard]] constexpr size_t to_index(T val) {
    return static_cast<size_t>(val);
}

// ============================================================================
// Move Encoding Class
// ============================================================================
// Stored as a single 16-bit unsigned integer (uint16_t data).
//
// Memory / Bit layout:
// +-------------------+-------------------+-------------------+-------------------+
// |    Bits 15-14     |    Bits 13-12     |     Bits 11-6     |     Bits 5-0      |
// |     Move Type     |  Promotion Type   |   Origin Square   |    Dest Square    |
// |  (Special Flags)  |   (pt - KNIGHT)   |     (0 to 63)     |     (0 to 63)     |
// +-------------------+-------------------+-------------------+-------------------+
//
// - Bits 0-5:   Destination square index (0 to 63)
// - Bits 6-11:  Origin square index (0 to 63)
// - Bits 12-13: Promotion piece offset (0 = Knight, 1 = Bishop, 2 = Rook, 3 = Queen)
// - Bits 14-15: Move type flag (0 = Normal, 1 = Promotion, 2 = En Passant, 3 = Castling)
enum MoveType : uint16_t {
    NORMAL     = 0,
    PROMOTION  = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING   = 3 << 14
};

class Move {
public:
    Move() = default;
    
    constexpr explicit Move(uint16_t d) : data(d) {}

    constexpr Move(Square from, Square to) :
        data(static_cast<uint16_t>((std::to_underlying(from) << 6) + std::to_underlying(to))) {}

    template<MoveType T>
    [[nodiscard]] static constexpr Move make(Square from, Square to, PieceType pt = KNIGHT) {
        return Move(static_cast<uint16_t>(
            static_cast<uint16_t>(T) + 
            static_cast<uint16_t>((std::to_underlying(pt) - std::to_underlying(KNIGHT)) << 12) + 
            static_cast<uint16_t>(std::to_underlying(from) << 6) + 
            static_cast<uint16_t>(std::to_underlying(to))
        ));
    }

    [[nodiscard]] constexpr Square from_sq() const {
        return static_cast<Square>((data >> 6) & 0x3F);
    }

    [[nodiscard]] constexpr Square to_sq() const {
        return static_cast<Square>(data & 0x3F);
    }

    [[nodiscard]] constexpr int from_to() const { return data & 0xFFF; }

    [[nodiscard]] constexpr MoveType type_of() const { return static_cast<MoveType>(data & (3 << 14)); }

    [[nodiscard]] constexpr PieceType promotion_type() const { 
        return static_cast<PieceType>(((data >> 12) & 3) + KNIGHT); 
    }

    [[nodiscard]] constexpr bool is_ok() const { return none().data != data && null().data != data; }

    [[nodiscard]] static constexpr Move null() { return Move(65); }
    [[nodiscard]] static constexpr Move none() { return Move(0); }

    constexpr bool operator==(const Move& m) const { return data == m.data; }
    constexpr bool operator!=(const Move& m) const { return data != m.data; }

    constexpr explicit operator bool() const { return data != 0; }

    [[nodiscard]] constexpr uint16_t raw() const { return data; }

    [[nodiscard]] std::string to_string() const {
        if (*this == none()) return "none";
        if (*this == null()) return "0000";

        std::string s;
        s += static_cast<char>('a' + std::to_underlying(file_of(from_sq())));
        s += static_cast<char>('1' + std::to_underlying(rank_of(from_sq())));
        s += static_cast<char>('a' + std::to_underlying(file_of(to_sq())));
        s += static_cast<char>('1' + std::to_underlying(rank_of(to_sq())));

        if (type_of() == PROMOTION) {
            PieceType pt = promotion_type();
            switch (pt) {
                case KNIGHT: s += 'n'; break;
                case BISHOP: s += 'b'; break;
                case ROOK:   s += 'r'; break;
                case QUEEN:  s += 'q'; break;
                default: break;
            }
        }
        return s;
    }

    struct MoveHash {
        [[nodiscard]] size_t operator()(const Move& m) const { 
            return static_cast<size_t>(make_key(m.data)); 
        }
    };

protected:
    uint16_t data = 0;
};

} // namespace Bully
