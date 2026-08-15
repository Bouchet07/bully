#include <random>
#include <sstream>
#include <vector>
#include <iostream>
#include <format>
#include <algorithm>
#include "position.h"
#include "attacks.h"

namespace Bully {

// Keys definitions
std::array<std::array<Key, SQUARE_NB>, PIECE_NB> PieceKeys;
std::array<Key, CASTLING_RIGHT_NB>               CastlingKeys;
std::array<Key, SQUARE_NB>                       EnPassantKeys;
Key SideKey;

// Cuckoo table definitions
std::array<Key, 8192>  CuckooKeys{};
std::array<Move, 8192> CuckooMoves{};

// Castling rights update mask
std::array<CastlingRights, SQUARE_NB> CastlingRightsMask;

static inline size_t cuckoo_h1(Key key) { return static_cast<size_t>(key & 0x1FFF); }
static inline size_t cuckoo_h2(Key key) { return static_cast<size_t>((key >> 16) & 0x1FFF); }

void init_cuckoo() {
    CuckooKeys.fill(0);
    CuckooMoves.fill(Move::none());

    for (Piece pc = W_PAWN; pc < PIECE_NB; ++pc) {
        if (type_of(pc) == PAWN) continue;

        for (Square s1 = SQ_A1; s1 < SQUARE_NB; ++s1) {
            for (Square s2 = s1 + 1; s2 < SQUARE_NB; ++s2) {
                Bitboard attacks = 0;
                PieceType pt = type_of(pc);
                if (pt == KNIGHT) attacks = knight_attacks(s1);
                else if (pt == BISHOP) attacks = bishop_attacks(s1, 0);
                else if (pt == ROOK) attacks = rook_attacks(s1, 0);
                else if (pt == QUEEN) attacks = queen_attacks(s1, 0);
                else if (pt == KING) attacks = king_attacks(s1);

                if (!(attacks & square_bb(s2))) continue;

                Move move = Move(s1, s2);
                Key key = PieceKeys[to_index(pc)][to_index(s1)]
                        ^ PieceKeys[to_index(pc)][to_index(s2)]
                        ^ SideKey;

                size_t i = cuckoo_h1(key);

                int count = 0;
                while (count < 64) {
                    std::swap(CuckooKeys[i], key);
                    std::swap(CuckooMoves[i], move);

                    if (move == Move::none()) break;

                    i = (i == cuckoo_h1(key)) ? cuckoo_h2(key) : cuckoo_h1(key);
                    count++;
                }
            }
        }
    }
}

void init_zobrist() {
    std::mt19937_64 rng(7821938ULL);
    for (size_t pc = 0; pc < PIECE_NB; ++pc) {
        for (size_t sq = 0; sq < SQUARE_NB; ++sq) {
            PieceKeys[pc][sq] = rng();
        }
    }
    for (size_t cr = 0; cr < CASTLING_RIGHT_NB; ++cr) {
        CastlingKeys[cr] = rng();
    }
    for (size_t sq = 0; sq < SQUARE_NB; ++sq) {
        EnPassantKeys[sq] = rng();
    }
    SideKey = rng();

    // Initialize CastlingRightsMask
    for (size_t sq = 0; sq < SQUARE_NB; ++sq) {
        CastlingRightsMask[sq] = ANY_CASTLING;
    }
    CastlingRightsMask[to_index(SQ_A1)] = static_cast<CastlingRights>(ANY_CASTLING & ~WHITE_OOO);
    CastlingRightsMask[to_index(SQ_H1)] = static_cast<CastlingRights>(ANY_CASTLING & ~WHITE_OO);
    CastlingRightsMask[to_index(SQ_E1)] = static_cast<CastlingRights>(ANY_CASTLING & ~(WHITE_OO | WHITE_OOO));
    CastlingRightsMask[to_index(SQ_A8)] = static_cast<CastlingRights>(ANY_CASTLING & ~BLACK_OOO);
    CastlingRightsMask[to_index(SQ_H8)] = static_cast<CastlingRights>(ANY_CASTLING & ~BLACK_OO);
    CastlingRightsMask[to_index(SQ_E8)] = static_cast<CastlingRights>(ANY_CASTLING & ~(BLACK_OO | BLACK_OOO));

    init_cuckoo();
}

void Position::add_piece(Piece pc, Square sq) {
    size_t idx = to_index(sq);
    board[idx] = pc;
    PieceType pt = type_of(pc);
    Color c = color_of(pc);
    pieces_by_type[to_index(pt)] |= square_bb(sq);
    pieces_by_color[to_index(c)] |= square_bb(sq);
}

void Position::remove_piece(Square sq) {
    size_t idx = to_index(sq);
    Piece pc = board[idx];
    PieceType pt = type_of(pc);
    Color c = color_of(pc);
    pieces_by_type[to_index(pt)] ^= square_bb(sq);
    pieces_by_color[to_index(c)] ^= square_bb(sq);
    board[idx] = NO_PIECE;
}

void Position::move_piece_internal(Square from, Square to) {
    size_t from_idx = to_index(from);
    size_t to_idx = to_index(to);
    Piece pc = board[from_idx];
    PieceType pt = type_of(pc);
    Color c = color_of(pc);
    
    Bitboard from_to_bb = square_bb(from) | square_bb(to);
    pieces_by_type[to_index(pt)] ^= from_to_bb;
    pieces_by_color[to_index(c)] ^= from_to_bb;
    board[from_idx] = NO_PIECE;
    board[to_idx] = pc;
}

Key Position::pawn_key() const {
    Key k = 0;
    Bitboard pawns = pieces(PAWN);
    while (pawns) {
        Square sq = lsb(pawns);
        pawns &= pawns - 1;
        k ^= PieceKeys[to_index(board[to_index(sq)])][to_index(sq)];
    }
    return k;
}

Key Position::compute_key() const {
    Key k = 0;
    for (size_t sq = 0; sq < SQUARE_NB; ++sq) {
        Piece pc = board[sq];
        if (pc != NO_PIECE) {
            k ^= PieceKeys[to_index(pc)][sq];
        }
    }
    k ^= CastlingKeys[to_index(st->castling_rights)];
    if (st->en_passant_square != SQ_NONE) {
        k ^= EnPassantKeys[to_index(st->en_passant_square)];
    }
    if (side_to_move_color == BLACK) {
        k ^= SideKey;
    }
    return k;
}

void Position::set_fen(const std::string& fen, StateInfo& si) {
    std::fill(board.begin(), board.end(), NO_PIECE);
    std::fill(pieces_by_type.begin(), pieces_by_type.end(), 0ULL);
    std::fill(pieces_by_color.begin(), pieces_by_color.end(), 0ULL);

    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(fen);
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) return;

    // 1. Piece placement
    std::string placement = tokens[0];
    int rank = 7;
    int file = 0;
    for (char c : placement) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += (c - '0');
        } else {
            Piece pc = NO_PIECE;
            switch (c) {
                case 'P': pc = W_PAWN; break;
                case 'N': pc = W_KNIGHT; break;
                case 'B': pc = W_BISHOP; break;
                case 'R': pc = W_ROOK; break;
                case 'Q': pc = W_QUEEN; break;
                case 'K': pc = W_KING; break;
                case 'p': pc = B_PAWN; break;
                case 'n': pc = B_KNIGHT; break;
                case 'b': pc = B_BISHOP; break;
                case 'r': pc = B_ROOK; break;
                case 'q': pc = B_QUEEN; break;
                case 'k': pc = B_KING; break;
            }
            if (pc != NO_PIECE) {
                add_piece(pc, make_square(static_cast<File>(file), static_cast<Rank>(rank)));
                file++;
            }
        }
    }

    // 2. Active color
    side_to_move_color = WHITE;
    if (tokens.size() > 1 && tokens[1] == "b") {
        side_to_move_color = BLACK;
    }

    // 3. Castling rights
    si.castling_rights = NO_CASTLING;
    std::fill(castling_rook_sq.begin(), castling_rook_sq.end(), SQ_NONE);
    std::fill(castling_rights_mask.begin(), castling_rights_mask.end(), ANY_CASTLING);

    if (tokens.size() > 2) {
        std::string castling = tokens[2];
        if (castling != "-") {
            Square w_ksq = king_square(WHITE);
            Square b_ksq = king_square(BLACK);

            for (char c : castling) {
                if (c == 'K') {
                    Bitboard rooks = pieces(WHITE, ROOK) & rank_bb(RANK_1);
                    Square r_sq = is_chess960 ? (rooks ? msb(rooks) : SQ_H1) : SQ_H1;
                    si.castling_rights = static_cast<CastlingRights>(si.castling_rights | WHITE_OO);
                    castling_rook_sq[to_index(WHITE_OO)] = r_sq;
                    if (w_ksq != SQ_NONE) castling_rights_mask[to_index(w_ksq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(w_ksq)] & ~WHITE_OO);
                    castling_rights_mask[to_index(r_sq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(r_sq)] & ~WHITE_OO);
                } else if (c == 'Q') {
                    Bitboard rooks = pieces(WHITE, ROOK) & rank_bb(RANK_1);
                    Square r_sq = is_chess960 ? (rooks ? lsb(rooks) : SQ_A1) : SQ_A1;
                    si.castling_rights = static_cast<CastlingRights>(si.castling_rights | WHITE_OOO);
                    castling_rook_sq[to_index(WHITE_OOO)] = r_sq;
                    if (w_ksq != SQ_NONE) castling_rights_mask[to_index(w_ksq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(w_ksq)] & ~WHITE_OOO);
                    castling_rights_mask[to_index(r_sq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(r_sq)] & ~WHITE_OOO);
                } else if (c >= 'A' && c <= 'H') {
                    File f = static_cast<File>(c - 'A');
                    Square r_sq = make_square(f, RANK_1);
                    CastlingRights cr = (w_ksq != SQ_NONE && r_sq > w_ksq) ? WHITE_OO : WHITE_OOO;
                    si.castling_rights = static_cast<CastlingRights>(si.castling_rights | cr);
                    castling_rook_sq[to_index(cr)] = r_sq;
                    if (w_ksq != SQ_NONE) castling_rights_mask[to_index(w_ksq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(w_ksq)] & ~cr);
                    castling_rights_mask[to_index(r_sq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(r_sq)] & ~cr);
                } else if (c == 'k') {
                    Bitboard rooks = pieces(BLACK, ROOK) & rank_bb(RANK_8);
                    Square r_sq = is_chess960 ? (rooks ? msb(rooks) : SQ_H8) : SQ_H8;
                    si.castling_rights = static_cast<CastlingRights>(si.castling_rights | BLACK_OO);
                    castling_rook_sq[to_index(BLACK_OO)] = r_sq;
                    if (b_ksq != SQ_NONE) castling_rights_mask[to_index(b_ksq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(b_ksq)] & ~BLACK_OO);
                    castling_rights_mask[to_index(r_sq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(r_sq)] & ~BLACK_OO);
                } else if (c == 'q') {
                    Bitboard rooks = pieces(BLACK, ROOK) & rank_bb(RANK_8);
                    Square r_sq = is_chess960 ? (rooks ? lsb(rooks) : SQ_A8) : SQ_A8;
                    si.castling_rights = static_cast<CastlingRights>(si.castling_rights | BLACK_OOO);
                    castling_rook_sq[to_index(BLACK_OOO)] = r_sq;
                    if (b_ksq != SQ_NONE) castling_rights_mask[to_index(b_ksq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(b_ksq)] & ~BLACK_OOO);
                    castling_rights_mask[to_index(r_sq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(r_sq)] & ~BLACK_OOO);
                } else if (c >= 'a' && c <= 'h') {
                    File f = static_cast<File>(c - 'a');
                    Square r_sq = make_square(f, RANK_8);
                    CastlingRights cr = (b_ksq != SQ_NONE && r_sq > b_ksq) ? BLACK_OO : BLACK_OOO;
                    si.castling_rights = static_cast<CastlingRights>(si.castling_rights | cr);
                    castling_rook_sq[to_index(cr)] = r_sq;
                    if (b_ksq != SQ_NONE) castling_rights_mask[to_index(b_ksq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(b_ksq)] & ~cr);
                    castling_rights_mask[to_index(r_sq)] = static_cast<CastlingRights>(castling_rights_mask[to_index(r_sq)] & ~cr);
                }
            }
        }
    }

    // 4. En passant
    si.en_passant_square = SQ_NONE;
    if (tokens.size() > 3) {
        std::string ep = tokens[3];
        if (ep.length() == 2 && ep[0] >= 'a' && ep[0] <= 'h' && ep[1] >= '1' && ep[1] <= '8') {
            File f = static_cast<File>(ep[0] - 'a');
            Rank r = static_cast<Rank>(ep[1] - '1');
            si.en_passant_square = make_square(f, r);
        }
    }

    // 5. Halfmove clock (50-move rule plies counter)
    si.rule50 = 0;
    if (tokens.size() > 4) {
        si.rule50 = std::stoi(tokens[4]);
    }

    st = &si;
    st->previous = nullptr;
    st->captured_piece = NO_PIECE;
    st->key = compute_key();
}

std::string Position::get_fen() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Square sq = make_square(static_cast<File>(file), static_cast<Rank>(rank));
            Piece pc = board[to_index(sq)];
            if (pc == NO_PIECE) {
                empty++;
            } else {
                if (empty > 0) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                char c = '?';
                switch (pc) {
                    case W_PAWN: c = 'P'; break;
                    case W_KNIGHT: c = 'N'; break;
                    case W_BISHOP: c = 'B'; break;
                    case W_ROOK: c = 'R'; break;
                    case W_QUEEN: c = 'Q'; break;
                    case W_KING: c = 'K'; break;
                    case B_PAWN: c = 'p'; break;
                    case B_KNIGHT: c = 'n'; break;
                    case B_BISHOP: c = 'b'; break;
                    case B_ROOK: c = 'r'; break;
                    case B_QUEEN: c = 'q'; break;
                    case B_KING: c = 'k'; break;
                    default: break;
                }
                fen += c;
            }
        }
        if (empty > 0) {
            fen += std::to_string(empty);
        }
        if (rank > 0) {
            fen += '/';
        }
    }

    fen += (side_to_move_color == WHITE) ? " w " : " b ";

    std::string castling;
    if (st->castling_rights & WHITE_OO) castling += 'K';
    if (st->castling_rights & WHITE_OOO) castling += 'Q';
    if (st->castling_rights & BLACK_OO) castling += 'k';
    if (st->castling_rights & BLACK_OOO) castling += 'q';
    if (castling.empty()) castling = "-";
    fen += castling + " ";

    if (st->en_passant_square == SQ_NONE) {
        fen += "-";
    } else {
        char file_char = static_cast<char>('a' + std::to_underlying(file_of(st->en_passant_square)));
        char rank_char = static_cast<char>('1' + std::to_underlying(rank_of(st->en_passant_square)));
        fen += file_char;
        fen += rank_char;
    }

    fen += " " + std::to_string(st->rule50) + " 1";
    return fen;
}

bool Position::castling_impeded(CastlingRights cr) const {
    Color us = (cr & WHITE_CASTLING) ? WHITE : BLACK;
    Square king_from = king_square(us);
    Square rook_from = castling_rook_sq[to_index(cr)];
    if (king_from == SQ_NONE || rook_from == SQ_NONE) return true;

    bool is_kingside = (cr & KING_SIDE);
    Square king_to = is_kingside ? (us == WHITE ? SQ_G1 : SQ_G8) : (us == WHITE ? SQ_C1 : SQ_C8);
    Square rook_to = is_kingside ? (us == WHITE ? SQ_F1 : SQ_F8) : (us == WHITE ? SQ_D1 : SQ_D8);

    Bitboard king_path = BetweenBB[to_index(king_from)][to_index(king_to)] | square_bb(king_to);
    Bitboard rook_path = BetweenBB[to_index(rook_from)][to_index(rook_to)] | square_bb(rook_to);

    Bitboard obstacles = (king_path | rook_path) & ~square_bb(king_from) & ~square_bb(rook_from);
    return (occupied() & obstacles) != 0;
}

bool Position::attacked(Square sq, Color attacked_by) const {
    return attacked(sq, attacked_by, occupied());
}

bool Position::attacked(Square sq, Color attacked_by, Bitboard occ) const {
    if (pawn_attacks(~attacked_by, sq) & pieces(attacked_by, PAWN)) return true;
    if (knight_attacks(sq) & pieces(attacked_by, KNIGHT)) return true;
    if (king_attacks(sq) & pieces(attacked_by, KING)) return true;

    if (rook_attacks(sq, occ) & (pieces(attacked_by, ROOK) | pieces(attacked_by, QUEEN))) return true;
    if (bishop_attacks(sq, occ) & (pieces(attacked_by, BISHOP) | pieces(attacked_by, QUEEN))) return true;

    return false;
}

Bitboard Position::attackers_to(Square sq, Color c) const {
    return attackers_to(sq, c, occupied());
}

Bitboard Position::attackers_to(Square sq, Color c, Bitboard occ) const {
    return (pawn_attacks(~c, sq) & pieces(c, PAWN))
         | (knight_attacks(sq) & pieces(c, KNIGHT))
         | (king_attacks(sq) & pieces(c, KING))
         | (rook_attacks(sq, occ) & (pieces(c, ROOK) | pieces(c, QUEEN)))
         | (bishop_attacks(sq, occ) & (pieces(c, BISHOP) | pieces(c, QUEEN)));
}

Bitboard Position::checkers() const {
    return checkers(side_to_move_color);
}

Bitboard Position::checkers(Color c) const {
    Square ksq = king_square(c);
    if (ksq == SQ_NONE) return 0ULL;
    return attackers_to(ksq, ~c, occupied());
}

Bitboard Position::blockers_for_king(Color c) const {
    Square ksq = king_square(c);
    if (ksq == SQ_NONE) return 0ULL;
    Color them = ~c;

    Bitboard blockers = 0ULL;
    Bitboard snipers = ((rook_attacks(ksq, 0ULL) & (pieces(them, ROOK) | pieces(them, QUEEN)))
                     | (bishop_attacks(ksq, 0ULL) & (pieces(them, BISHOP) | pieces(them, QUEEN))));

    Bitboard occ = occupied() ^ snipers;
    while (snipers) {
        Square sniper_sq = lsb(snipers);
        snipers &= snipers - 1;
        Bitboard b = (BetweenBB[to_index(ksq)][to_index(sniper_sq)] ^ square_bb(sniper_sq)) & occ;
        if (b && !more_than_one(b)) {
            blockers |= b;
        }
    }
    return blockers;
}

bool Position::pseudo_legal(Move m) const {
    if (!m.is_ok()) return false;

    Square from = m.from_sq();
    Square to = m.to_sq();
    if (from >= SQUARE_NB || to >= SQUARE_NB) return false;

    Piece pc = board[to_index(from)];
    Color us = side_to_move_color;

    // 1. Piece must exist and belong to the side to move
    if (pc == NO_PIECE || color_of(pc) != us) return false;

    PieceType pt = type_of(pc);
    MoveType type = m.type_of();

    if (type == CASTLING) {
        if (pt != KING) return false;
        if (us == WHITE) {
            if (to == castling_rook_sq[to_index(WHITE_OO)]) return (st->castling_rights & WHITE_OO) != 0;
            if (to == castling_rook_sq[to_index(WHITE_OOO)]) return (st->castling_rights & WHITE_OOO) != 0;
        } else {
            if (to == castling_rook_sq[to_index(BLACK_OO)]) return (st->castling_rights & BLACK_OO) != 0;
            if (to == castling_rook_sq[to_index(BLACK_OOO)]) return (st->castling_rights & BLACK_OOO) != 0;
        }
        return false;
    }

    Piece target = board[to_index(to)];

    // 2. Cannot capture our own pieces or the enemy King
    if (target != NO_PIECE && (color_of(target) == us || type_of(target) == KING)) {
        return false;
    }

    // 3. Verify piece can actually attack/reach target square
    Bitboard occ = occupied();

    if (pt == PAWN) {
        Bitboard attacks = pawn_attacks(us, from);
        if (target != NO_PIECE) {
            return (attacks & square_bb(to)) != 0;
        } else {
            Square push1 = from + pawn_push(us);
            if (to == push1) return true;
            Square push2 = from + 2 * pawn_push(us);
            if (to == push2 && (relative_rank(us, from) == RANK_2) && board[to_index(push1)] == NO_PIECE) return true;
            return false;
        }
    }

    if (pt == KNIGHT) return (knight_attacks(from) & square_bb(to)) != 0;
    if (pt == BISHOP) return (bishop_attacks(from, occ) & square_bb(to)) != 0;
    if (pt == ROOK)   return (rook_attacks(from, occ) & square_bb(to)) != 0;
    if (pt == QUEEN)  return (queen_attacks(from, occ) & square_bb(to)) != 0;
    if (pt == KING)   return (king_attacks(from) & square_bb(to)) != 0;

    return false;
}

bool Position::legal(Move m) const {
    Color us = side_to_move_color;
    Square from = m.from_sq();
    Square to = m.to_sq();
    MoveType type = m.type_of();

    if (type == EN_PASSANT) {
        Square ksq = king_square(us);
        Square cap_sq = to - pawn_push(us);
        Bitboard occ = (occupied() ^ square_bb(from) ^ square_bb(cap_sq)) | square_bb(to);
        return !attacked(ksq, ~us, occ);
    }

    if (type == CASTLING) {
        if (in_check()) return false;
        Square r_to = to; // Rook's starting square
        CastlingRights cr = (us == WHITE) ? (r_to == castling_rook_sq[to_index(WHITE_OO)] ? WHITE_OO : WHITE_OOO)
                                          : (r_to == castling_rook_sq[to_index(BLACK_OO)] ? BLACK_OO : BLACK_OOO);
        bool is_kingside = (cr & KING_SIDE);
        Square king_to = is_kingside ? (us == WHITE ? SQ_G1 : SQ_G8) : (us == WHITE ? SQ_C1 : SQ_C8);
        
        Direction step = (king_to > from) ? EAST : WEST;
        Square sq = from;
        while (true) {
            if (attacked(sq, ~us)) return false;
            if (sq == king_to) break;
            sq = sq + step;
        }
        return true;
    }

    Square ksq = king_square(us);
    if (type_of(board[to_index(from)]) == KING) {
        return !attacked(to, ~us, (occupied() ^ square_bb(from)) & ~square_bb(to));
    }

    Bitboard chk = checkers(us);
    if (chk) {
        if (more_than_one(chk)) return false;
        Square checker_sq = lsb(chk);
        Bitboard target_mask = square_bb(checker_sq) | (BetweenBB[to_index(ksq)][to_index(checker_sq)] ^ square_bb(checker_sq));
        if (!(square_bb(to) & target_mask)) return false;
    }

    Bitboard pinned = blockers_for_king(us);
    return !(pinned & square_bb(from)) || aligned(from, to, ksq);
}

bool Position::in_check() const {
    return attacked(king_square(side_to_move_color), ~side_to_move_color);
}

bool Position::make_move(Move m, StateInfo& new_state) {
    if (!m.is_ok() || m.from_sq() >= SQUARE_NB || m.to_sq() >= SQUARE_NB || !legal(m)) {
        return false;
    }

    new_state = *st;
    new_state.previous = st;
    new_state.captured_piece = NO_PIECE;
    new_state.en_passant_square = SQ_NONE;
    new_state.rule50++;

    Square from = m.from_sq();
    Square to = m.to_sq();
    MoveType type = m.type_of();
    Piece pc = board[to_index(from)];
    PieceType pt = type_of(pc);
    Color us = side_to_move_color;

    if (NNUE::use_nnue && pt == KING && new_state.accumulator) {
        new_state.accumulator->computed[to_index(us)] = false;
    }

    new_state.key ^= CastlingKeys[to_index(st->castling_rights)];
    if (st->en_passant_square != SQ_NONE) {
        new_state.key ^= EnPassantKeys[to_index(st->en_passant_square)];
    }
    new_state.key ^= SideKey;

    if (pt == PAWN) {
        new_state.rule50 = 0;
    }

    Piece captured = (type == CASTLING) ? NO_PIECE : board[to_index(to)];
    if (type == EN_PASSANT) {
        Square cap_sq = to - pawn_push(us);
        captured = board[to_index(cap_sq)];
        new_state.captured_piece = captured;
        
        new_state.key ^= PieceKeys[to_index(captured)][to_index(cap_sq)];
        remove_piece(cap_sq);
        
        new_state.rule50 = 0;
    } else if (captured != NO_PIECE) {
        new_state.captured_piece = captured;
        
        new_state.key ^= PieceKeys[to_index(captured)][to_index(to)];
        remove_piece(to);
        
        new_state.rule50 = 0;
    }

    if (type == NORMAL) {
        move_piece_internal(from, to);
        new_state.key ^= PieceKeys[to_index(pc)][to_index(from)] ^ PieceKeys[to_index(pc)][to_index(to)];

        if (pt == PAWN && std::abs(std::to_underlying(to) - std::to_underlying(from)) == 16) {
            new_state.en_passant_square = static_cast<Square>((std::to_underlying(from) + std::to_underlying(to)) / 2);
            new_state.key ^= EnPassantKeys[to_index(new_state.en_passant_square)];
        }
    } else {
        new_state.key ^= PieceKeys[to_index(pc)][to_index(from)];

        if (type == PROMOTION) {
            remove_piece(from);
            PieceType promo_pt = m.promotion_type();
            Piece promo_pc = make_piece(us, promo_pt);
            add_piece(promo_pc, to);
            new_state.key ^= PieceKeys[to_index(promo_pc)][to_index(to)];
        } else if (type == EN_PASSANT) {
            remove_piece(from);
            add_piece(pc, to);
            new_state.key ^= PieceKeys[to_index(pc)][to_index(to)];
        } else if (type == CASTLING) {
            bool is_kingside = (to == castling_rook_sq[to_index(us == WHITE ? WHITE_OO : BLACK_OO)]);
            Square king_to = is_kingside ? (us == WHITE ? SQ_G1 : SQ_G8) : (us == WHITE ? SQ_C1 : SQ_C8);
            Square rook_to = is_kingside ? (us == WHITE ? SQ_F1 : SQ_F8) : (us == WHITE ? SQ_D1 : SQ_D8);

            Piece king = pc;
            Piece rook = board[to_index(to)];

            remove_piece(from);

            remove_piece(to);
            new_state.key ^= PieceKeys[to_index(rook)][to_index(to)];

            add_piece(king, king_to);
            new_state.key ^= PieceKeys[to_index(king)][to_index(king_to)];

            add_piece(rook, rook_to);
            new_state.key ^= PieceKeys[to_index(rook)][to_index(rook_to)];
        }
    }

    new_state.castling_rights = static_cast<CastlingRights>(
        new_state.castling_rights & castling_rights_mask[to_index(from)] & castling_rights_mask[to_index(to)]
    );
    new_state.key ^= CastlingKeys[to_index(new_state.castling_rights)];

    // Populate NNUE dirty piece modification descriptor
    DirtyPiece& dp = new_state.dirty_piece;
    dp.count = 0;

    if (type == CASTLING) {
        // The King moves, so the side-to-move's accumulator is fully refreshed.
        // We only log the Rook so the opponent's accumulator updates incrementally.
        bool is_kingside = (to == castling_rook_sq[to_index(us == WHITE ? WHITE_OO : BLACK_OO)]);
        Square rook_to = is_kingside ? (us == WHITE ? SQ_F1 : SQ_F8) : (us == WHITE ? SQ_D1 : SQ_D8);
        
        dp.piece[0] = make_piece(us, ROOK);
        dp.from[0]  = to; // Rook's original square
        dp.to[0]    = rook_to;
        dp.count = 1;
    } else {
        if (pt != KING) {
            if (type == PROMOTION) {
                Piece promo_pc = make_piece(us, m.promotion_type());
                dp.piece[0] = pc;
                dp.from[0]  = from;
                dp.to[0]    = SQ_NONE;
                dp.piece[1] = promo_pc;
                dp.from[1]  = SQ_NONE;
                dp.to[1]    = to;
                dp.count = 2;
            } else {
                dp.piece[0] = pc;
                dp.from[0]  = from;
                dp.to[0]    = to;
                dp.count = 1;
            }
        }
        
        // We MUST log the captured piece even if pt == KING, so the opponent's accumulator updates
        if (captured != NO_PIECE) {
            Square cap_sq = (type == EN_PASSANT) ? (to - pawn_push(us)) : to;
            dp.piece[dp.count] = captured;
            dp.from[dp.count]  = cap_sq;
            dp.to[dp.count]    = SQ_NONE;
            dp.count++;
        }
    }

    side_to_move_color = ~side_to_move_color;
    st = &new_state;

    return true;
}

void Position::unmake_move(Move m) {
    side_to_move_color = ~side_to_move_color;

    Square from = m.from_sq();
    Square to = m.to_sq();
    MoveType type = m.type_of();
    Color us = side_to_move_color;

    if (type == PROMOTION) {
        remove_piece(to);
        add_piece(make_piece(us, PAWN), from);
    } else if (type == CASTLING) {
        bool is_kingside = (to == castling_rook_sq[to_index(us == WHITE ? WHITE_OO : BLACK_OO)]);
        Square king_to = is_kingside ? (us == WHITE ? SQ_G1 : SQ_G8) : (us == WHITE ? SQ_C1 : SQ_C8);
        Square rook_to = is_kingside ? (us == WHITE ? SQ_F1 : SQ_F8) : (us == WHITE ? SQ_D1 : SQ_D8);

        Piece king = make_piece(us, KING);
        Piece rook = make_piece(us, ROOK);

        remove_piece(king_to);
        remove_piece(rook_to);

        add_piece(king, from);
        add_piece(rook, to);
    } else {
        move_piece_internal(to, from);
    }

    Piece captured = st->captured_piece;
    if (captured != NO_PIECE) {
        if (type == EN_PASSANT) {
            Square cap_sq = to - pawn_push(us);
            add_piece(captured, cap_sq);
        } else {
            add_piece(captured, to);
        }
    }

    st = st->previous;
}

void Position::make_null_move(StateInfo& new_state) {
    new_state.castling_rights = st->castling_rights;
    new_state.rule50 = st->rule50 + 1;
    new_state.en_passant_square = SQ_NONE;
    new_state.captured_piece = NO_PIECE;
    new_state.previous = st;

    new_state.key = st->key ^ SideKey;
    if (st->en_passant_square != SQ_NONE) {
        new_state.key ^= EnPassantKeys[to_index(st->en_passant_square)];
    }

    side_to_move_color = ~side_to_move_color;
    st = &new_state;
}

void Position::unmake_null_move() {
    side_to_move_color = ~side_to_move_color;
    st = st->previous;
}

void Position::print(bool Use_UTF8, bool Use_Color) const {
    std::string reset     = Use_Color ? "\033[0m" : "";
    std::string border    = Use_Color ? "\033[1;34m" : ""; // Bold Blue grid
    std::string label     = Use_Color ? "\033[1;32m" : ""; // Bold Green coordinates & headers
    std::string value     = Use_Color ? "\033[1;35m" : ""; // Bold Magenta values
    std::string white_pc  = Use_Color ? "\033[1;33m" : ""; // Bold Yellow for White
    std::string black_pc  = Use_Color ? "\033[1;36m" : ""; // Bold Cyan for Black
    
    std::cout << "\n";
    
    if (Use_UTF8) {
        std::cout << border << "   ┌───┬───┬───┬───┬───┬───┬───┬───┐" << reset << "\n";
    } else {
        std::cout << border << "   +---+---+---+---+---+---+---+---+" << reset << "\n";
    }

    for (int r = 7; r >= 0; --r) {
        if (Use_UTF8) {
            std::cout << label << " " << (r + 1) << " " << border << "│" << reset;
        } else {
            std::cout << label << " " << (r + 1) << " " << border << "|" << reset;
        }

        for (int f = 0; f < 8; ++f) {
            Square sq = make_square(static_cast<File>(f), static_cast<Rank>(r));
            Piece pc = board[to_index(sq)];
            const char* c = " ";
            std::string pc_color = "";
            
            if (pc != NO_PIECE) {
                pc_color = (color_of(pc) == WHITE) ? white_pc : black_pc;
            }
            
            switch (pc) {
                case W_PAWN:   c = Use_UTF8 ? "♙\uFE0E" : "P"; break;
                case W_KNIGHT: c = Use_UTF8 ? "♘\uFE0E" : "N"; break;
                case W_BISHOP: c = Use_UTF8 ? "♗\uFE0E" : "B"; break;
                case W_ROOK:   c = Use_UTF8 ? "♖\uFE0E" : "R"; break;
                case W_QUEEN:  c = Use_UTF8 ? "♕\uFE0E" : "Q"; break;
                case W_KING:   c = Use_UTF8 ? "♔\uFE0E" : "K"; break;
                case B_PAWN:   c = Use_UTF8 ? "♟\uFE0E" : "p"; break;
                case B_KNIGHT: c = Use_UTF8 ? "♞\uFE0E" : "n"; break;
                case B_BISHOP: c = Use_UTF8 ? "♝\uFE0E" : "b"; break;
                case B_ROOK:   c = Use_UTF8 ? "♜\uFE0E" : "r"; break;
                case B_QUEEN:  c = Use_UTF8 ? "♛\uFE0E" : "q"; break;
                case B_KING:   c = Use_UTF8 ? "♚\uFE0E" : "k"; break;
                default: break;
            }
            if (Use_UTF8) {
                std::cout << " " << pc_color << c << reset << border << " │" << reset;
            } else {
                std::cout << " " << pc_color << c << reset << border << " |" << reset;
            }
        }
        std::cout << "\n";

        if (r > 0) {
            if (Use_UTF8) {
                std::cout << border << "   ├───┼───┼───┼───┼───┼───┼───┼───┤" << reset << "\n";
            } else {
                std::cout << border << "   +---+---+---+---+---+---+---+---+" << reset << "\n";
            }
        }
    }

    if (Use_UTF8) {
        std::cout << border << "   └───┴───┴───┴───┴───┴───┴───┴───┘" << reset << "\n";
    } else {
        std::cout << border << "   +---+---+---+---+---+---+---+---+" << reset << "\n";
    }
    std::cout << label << "     a   b   c   d   e   f   g   h" << reset << "\n\n";
    std::cout << std::format("  {}Side to move : {}{}{}\n", label, value, (side_to_move_color == WHITE) ? "White" : "Black", reset);
    std::cout << std::format("  {}Zobrist Key  : {}{:#018x}{}\n", label, value, st->key, reset);
    std::cout << std::format("  {}FEN          : {}{}{}\n\n", label, value, get_fen(), reset);
}


Value Position::see(Move m) const {
    Square from = m.from_sq();
    Square to = m.to_sq();
    MoveType type = m.type_of();

    if (type == CASTLING) {
        return VALUE_ZERO;
    }

    Piece pc = board[to_index(from)];
    PieceType pt = type_of(pc);
    PieceType captured_pt = type_of(board[to_index(to)]);

    if (type == EN_PASSANT) {
        captured_pt = PAWN;
    }

    Value gain[32];
    gain[0] = get_piece_value(captured_pt);

    Bitboard occ = occupied();
    occ ^= from; // Remove the moving piece from occupancy

    // Get all initial attackers of the target square
    Bitboard pawns = (pawn_attacks(BLACK, to) & pieces(WHITE, PAWN)) |
                     (pawn_attacks(WHITE, to) & pieces(BLACK, PAWN));
    Bitboard knights = knight_attacks(to) & pieces(KNIGHT);
    Bitboard bishops = bishop_attacks(to, occ) & (pieces(BISHOP) | pieces(QUEEN));
    Bitboard rooks = rook_attacks(to, occ) & (pieces(ROOK) | pieces(QUEEN));
    Bitboard kings = king_attacks(to) & pieces(KING);
    Bitboard attackers = pawns | knights | bishops | rooks | kings;

    Color side = ~side_to_move_color;
    int depth = 1;
    Value current_piece_value = get_piece_value(pt);

    while (true) {
        Bitboard side_attackers = attackers & pieces(side) & occ;
        if (!side_attackers) break;

        // Find the least valuable attacker for the active side
        Square attacker_sq = SQ_NONE;
        for (PieceType t = PAWN; t <= KING; ++t) {
            Bitboard subset = side_attackers & pieces(t);
            if (subset) {
                attacker_sq = get_LSB(subset);
                break;
            }
        }

        if (attacker_sq == SQ_NONE) break;

        occ ^= attacker_sq; // Remove the capturing piece from occupancy

        // Reveal potential X-ray attackers
        PieceType atk_type = type_of(board[to_index(attacker_sq)]);
        if (atk_type == PAWN || atk_type == BISHOP || atk_type == QUEEN) {
            attackers |= bishop_attacks(to, occ) & (pieces(BISHOP) | pieces(QUEEN));
        }
        if (atk_type == ROOK || atk_type == QUEEN) {
            attackers |= rook_attacks(to, occ) & (pieces(ROOK) | pieces(QUEEN));
        }

        gain[depth] = current_piece_value - gain[depth - 1];
        current_piece_value = get_piece_value(atk_type);
        depth++;
        side = ~side;
    }

    // Minimax back propagation
    while (--depth > 0) {
        gain[depth - 1] = gain[depth - 1] - std::max(0, gain[depth]);
    }

    return gain[0];
}

} // namespace Bully
