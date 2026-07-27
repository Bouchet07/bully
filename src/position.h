#pragma once

#include <string>
#include <string_view>
#include <array>
#include "types.h"
#include "bitboard.h"
#include "attacks.h"

namespace Bully {

// Zobrist keys for hashing positions
extern std::array<std::array<Key, SQUARE_NB>, PIECE_NB> PieceKeys;
extern std::array<Key, CASTLING_RIGHT_NB>               CastlingKeys;
extern std::array<Key, SQUARE_NB>                       EnPassantKeys;
extern Key                                              SideKey;

// Castling rights update mask
extern std::array<CastlingRights, SQUARE_NB> CastlingRightsMask;

// Initialize Zobrist hashing keys
void init_zobrist();

// Holds information about the non-reversible aspects of the game state.
struct StateInfo {
    CastlingRights castling_rights = NO_CASTLING;
    Square en_passant_square = SQ_NONE;
    int rule50 = 0;
    Key key = 0;
    Piece captured_piece = NO_PIECE;
    StateInfo* previous = nullptr;
};

class Position {
public:
    Position() = default;

    // Set board to match a FEN string
    void set_fen(const std::string& fen, StateInfo& si);

    // Get current FEN representation
    [[nodiscard]] std::string get_fen() const;

    // Check if the side to move's king is in check
    [[nodiscard]] bool in_check() const;

    // Fast check if a pseudo-legal move is strictly legal without mutating board state
    [[nodiscard]] bool legal(Move m) const;

    // Get bitboard of pieces pinning enemy/friendly pieces to the king
    [[nodiscard]] Bitboard blockers_for_king(Color c) const;

    // Get bitboard of pieces checking a given king
    [[nodiscard]] Bitboard checkers() const;
    [[nodiscard]] Bitboard checkers(Color c) const;

    // Get bitboard of all attackers to a square by a given color
    [[nodiscard]] Bitboard attackers_to(Square sq, Color c) const;
    [[nodiscard]] Bitboard attackers_to(Square sq, Color c, Bitboard occ) const;

    // Check if a square is attacked by pieces of a given color
    [[nodiscard]] bool attacked(Square sq, Color attacked_by) const;
    [[nodiscard]] bool attacked(Square sq, Color attacked_by, Bitboard occ) const;

    // Make a move. Returns false if the move was illegal (leaves King in check)
    bool make_move(Move m, StateInfo& new_state);

    // Unmake a move, restoring previous position state
    void unmake_move(Move m);

    // Make a null move (passes the turn to the opponent). Used in search for Null Move Pruning.
    void make_null_move(StateInfo& new_state);

    // Unmake a null move
    void unmake_null_move();

    // Static Exchange Evaluation (SEE)
    [[nodiscard]] Value see(Move m) const;

    // Getters for bitboards and properties
    [[nodiscard]] const StateInfo* state() const { return st; }
    [[nodiscard]] Bitboard pieces(PieceType pt) const { return pieces_by_type[to_index(pt)]; }
    [[nodiscard]] Bitboard pieces(Color c) const { return pieces_by_color[to_index(c)]; }
    [[nodiscard]] Bitboard pieces(Color c, PieceType pt) const { return pieces_by_color[to_index(c)] & pieces_by_type[to_index(pt)]; }
    [[nodiscard]] Bitboard occupied() const { return pieces_by_color[to_index(WHITE)] | pieces_by_color[to_index(BLACK)]; }

    [[nodiscard]] Piece piece_on(Square s) const { return board[to_index(s)]; }
    [[nodiscard]] Color side_to_move() const { return side_to_move_color; }
    [[nodiscard]] Square king_square(Color c) const { return get_LSB(pieces(c, KING)); }
    
    [[nodiscard]] Key key() const { return st->key; }
    [[nodiscard]] Square en_passant_square() const { return st->en_passant_square; }
    [[nodiscard]] CastlingRights castling_rights() const { return st->castling_rights; }
    [[nodiscard]] int rule50() const { return st->rule50; }
    void set_state_pointer(StateInfo* new_st) { st = new_st; }

    // Print the board representation to stdout
    void print(bool Use_UTF8 = true, bool Use_Color = false) const;

private:
    // Helper to add a piece to a square
    void add_piece(Piece pc, Square sq);

    // Helper to remove a piece from a square
    void remove_piece(Square sq);

    // Helper to move a piece from one square to another
    void move_piece_internal(Square from, Square to);

    // Recompute Zobrist key from scratch
    [[nodiscard]] Key compute_key() const;

    std::array<Piece, SQUARE_NB>               board = {NO_PIECE}; // mailbox array
    std::array<Bitboard, PIECE_TYPE_NB>        pieces_by_type = {0};
    std::array<Bitboard, COLOR_NB>             pieces_by_color = {0};
    Color side_to_move_color = WHITE;
    StateInfo* st = nullptr;
};

} // namespace Bully
