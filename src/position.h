/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED



#include <stdint.h>
#include <cassert>
#include <cstring>
#include <deque>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>

#include "bitboard.h"
#if ENABLE_NNUE
#include "nnue/features/half_ka_v2_hm.h"
#endif
#include "types.h"

namespace Stockfish {

class TranspositionTable;

// StateInfo struct stores information needed to restore a Position object to
// its previous state when we retract a move. Whenever a move is made on the
// board (by calling Position::do_move), a StateInfo object must be passed.

//可用于悔棋
struct StateInfo {

    // Copied when making a move
    Key     pawnKey;                //兵的哈希键值
    Key     minorPieceKey;          //小子的哈希键值 士象马炮
    Key     nonPawnKey[COLOR_NB];   //非兵的哈希键值
    Value   majorMaterial[COLOR_NB];//棋子价值
    int16_t check10[COLOR_NB];      //检查次数
    int     rule60;                 //规则60
    int     pliesFromNull;          //从空棋局开始走的步数

    // Not copied when making a move (will be recomputed anyhow)
    Key        key;                         //哈希键值
    Bitboard   checkersBB;                  //检查器
    StateInfo* previous;                    //前一个状态
    Bitboard   blockersForKing[COLOR_NB];   //国王的阻挡
    Bitboard   pinners[COLOR_NB];           //被牵制的棋子
    Bitboard   checkSquares[PIECE_TYPE_NB]; //检查的方格
    bool       needSlowCheck;               //需要慢速检查
    Piece      capturedPiece;               //被捕获的棋子
    Move       move;                        //移动
};//用于保存棋局信息


// A list to keep track of the position states along the setup moves (from the
// start position to the position just before the search starts). Needed by
// 'draw by repetition' detection. Use a std::deque because pointers to
// elements are not invalidated upon list resizing.
// 一个列表，用来记录从起始局面到搜索真正开始之前所有设定步骤对应的局面状态。它用于检测‘循环和（重复局面和棋）’
using StateListPtr = std::unique_ptr<std::deque<StateInfo>>;

// Position class stores information regarding the board representation as
// pieces, side to move, hash keys, etc. Important methods are
// do_move() and undo_move(), used by the search to update node info when
// traversing the search tree.
class Position {
   public:
    static void init();

    Position()                           = default;
    Position(const Position&)            = delete;
    Position& operator=(const Position&) = delete;

    // FEN string input/output
    Position&   set(const std::string& fenStr, StateInfo* si);
    Position&   set(const Position& pos, StateInfo* si);
    std::string fen() const;

    // Position representation
    Bitboard pieces() const;  // All pieces 整盘或按颜色返回位棋盘
    template<typename... PieceTypes>
    Bitboard pieces(PieceTypes... pts) const; //按类型返回位棋盘
    Bitboard pieces(Color c) const; //按颜色返回位棋盘
    template<typename... PieceTypes>
    Bitboard pieces(Color c, PieceTypes... pts) const; //按颜色和类型返回位棋盘
    Piece    piece_on(Square s) const; //返回指定方格的棋子
    bool     empty(Square s) const; //返回指定方格是否为空
    template<PieceType Pt>
    int count(Color c) const; //按颜色返回指定类型的棋子数量
    template<PieceType Pt>
    int      count() const; //返回所有棋子数量
    Square   king_square(Color c) const; //返回指定颜色的国王所在的方格
    uint64_t mid_encoding(Color c) const; //返回指定颜色的中盘编码

    // Checking
    Bitboard checkers() const; //返回检查器
    Bitboard blockers_for_king(Color c) const; //返回指定颜色的国王的阻挡
    Bitboard check_squares(PieceType pt) const; //返回指定类型的检查方格
    Bitboard pinners(Color c) const; //返回指定颜色的被牵制的棋子

    // Attacks to/from a given square
    Bitboard attackers_to(Square s) const; //返回指定方格的攻击者
    Bitboard attackers_to(Square s, Bitboard occupied) const; //返回指定方格的攻击者，并指定棋盘
    template<Color c>
    void     update_blockers() const; //更新阻挡
    Bitboard checkers_to(Color c, Square s) const; //返回指定颜色的检查器
    Bitboard checkers_to(Color c, Square s, Bitboard occupied) const; //返回指定颜色的检查器，并指定棋盘
    template<PieceType Pt>
    Bitboard attacks_by(Color c) const; //返回指定颜色的攻击者

    // Properties of moves
    bool  legal(Move m) const; //返回指定移动是否合法
    bool  pseudo_legal(const Move m) const; //返回指定移动是否伪合法
    bool  capture(Move m) const; //返回指定移动是否吃子
    bool  gives_check(Move m) const; //返回指定移动是否将军
    Piece moved_piece(Move m) const; //返回指定移动的移动棋子
    Piece captured_piece() const; //返回指定移动的被捕获棋子

    // Doing and undoing moves
    void       do_move(Move m, StateInfo& newSt, const TranspositionTable* tt); //执行移动
    DirtyPiece do_move(Move m, StateInfo& newSt, bool givesCheck, const TranspositionTable* tt); //执行移动，并返回移动的棋子
    void       undo_move(Move m); //撤销移动
    void       do_null_move(StateInfo& newSt, const TranspositionTable& tt); //执行空移动
    void       undo_null_move(); //撤销空移动

    // Static Exchange Evaluation
    bool see_ge(Move m, int threshold = 0) const; //返回指定移动是否吃子

    // Accessing hash keys
    Key key() const; //返回哈希键值
    Key pawn_key() const; //返回兵的哈希键值
    Key minor_piece_key() const; //返回小子的哈希键值
    Key defender_piece_key() const; //返回防守棋子的哈希键值
    Key non_pawn_key(Color c) const; //返回非兵的哈希键值

    // Other properties of the position
    Color    side_to_move() const; //返回当前走棋方
    int      game_ply() const; //返回当前走棋步数
    bool     rule_judge(Value& result, int ply = 0); //返回指定移动是否吃子
    int      rule60_count() const; //返回规则60的计数
    uint16_t chased(Color c); //返回指定颜色的被牵制的棋子
    Value    major_material(Color c) const; //返回指定颜色的棋子价值
    Value    major_material() const; //返回所有棋子价值

    // Position consistency check, for debugging
    bool pos_is_ok() const; //检查棋局是否合法
    bool can_capture(Color us, PieceType atk, PieceType dfd, Square from, Square to) const;
    void flip();  //翻转棋盘
    StateInfo* state() const; //返回当前状态

    void put_piece(Piece pc, Square s); //放置棋子
    void remove_piece(Square s); //移除棋子

   private:
    // Initialization helpers (used while setting up a position)
    void set_state() const; //设置状态
    void set_check_info() const; //设置检查信息

    // Other helpers
    void                  move_piece(Square from, Square to); //移动棋子
    std::pair<Piece, int> light_do_move(Move m); //执行移动，并返回移动的棋子
    void                  light_undo_move(Move m, Piece captured, int id = 0); //撤销移动
    Value                 detect_chases(int d, int ply = 0); //检测牵制
    bool                  chase_legal(Move m) const; //检查牵制是否合法
    template<bool AfterMove>
    Key adjust_key60(Key k) const; //调整哈希键值

    // Data members
    Piece      board[SQUARE_NB]; //棋盘
    Bitboard   byTypeBB[PIECE_TYPE_NB]; //按类型返回位棋盘
    Bitboard   byColorBB[COLOR_NB]; //按颜色返回位棋盘
    Square     kingSquare[COLOR_NB]; //国王所在的方格
    int        pieceCount[PIECE_NB]; //（分双方）棋子数量
    uint64_t   midEncoding[COLOR_NB]; //中盘编码
    StateInfo* st; //状态
    int        gamePly; //当前走棋步数
    Color      sideToMove; //当前走棋方

    // Bloom filter for fast repetition filtering
    BloomFilter filter; //用于快速重复过滤的布隆过滤器

    // Board for chasing detection
    int idBoard[SQUARE_NB]; //用于牵制检测的棋盘
};

std::ostream& operator<<(std::ostream& os, const Position& pos);

inline Color Position::side_to_move() const { return sideToMove; }

inline Piece Position::piece_on(Square s) const {
    assert(is_ok(s));
    return board[s];
}

inline bool Position::empty(Square s) const { return piece_on(s) == NO_PIECE; }

inline Piece Position::moved_piece(Move m) const { return piece_on(m.from_sq()); }

inline Bitboard Position::pieces() const { return byTypeBB[ALL_PIECES]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(PieceTypes... pts) const {
    return (byTypeBB[pts] | ...);
}

inline Bitboard Position::pieces(Color c) const { return byColorBB[c]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(Color c, PieceTypes... pts) const {
    return pieces(c) & pieces(pts...);
}

template<PieceType Pt>
inline int Position::count(Color c) const {
    return pieceCount[make_piece(c, Pt)];
}

template<PieceType Pt>
inline int Position::count() const {
    return count<Pt>(WHITE) + count<Pt>(BLACK);
}

//todo
inline Square Position::king_square(Color c) const {
    return c == WHITE ? lsb(DenBB[WHITE]) : lsb(DenBB[BLACK]);
}

inline uint64_t Position::mid_encoding(Color c) const { return midEncoding[c]; }

inline Bitboard Position::attackers_to(Square s) const { return attackers_to(s, pieces()); }

inline Bitboard Position::checkers_to(Color c, Square s) const {
    return checkers_to(c, s, pieces());
}

template<PieceType Pt>
inline Bitboard Position::attacks_by(Color c) const {

    Bitboard threats   = 0;
    Bitboard attackers = pieces(c, Pt);
    while (attackers)
    //todo
        // if (Pt == PAWN)
        //     threats |= attacks_bb<PAWN>(pop_lsb(attackers), c);
        // else
            threats |= attacks_bb<Pt>(pop_lsb(attackers), pieces());
    return threats;
}

inline Bitboard Position::checkers() const { return st->checkersBB; }

inline Bitboard Position::blockers_for_king(Color c) const { return st->blockersForKing[c]; }

inline Bitboard Position::pinners(Color c) const { return st->pinners[c]; }

inline Bitboard Position::check_squares(PieceType pt) const { return st->checkSquares[pt]; }

inline Key Position::key() const { return adjust_key60<false>(st->key); }

template<bool AfterMove>
inline Key Position::adjust_key60(Key k) const {
    return (st->rule60 < 14 - AfterMove ? k : k ^ make_key((st->rule60 - (14 - AfterMove)) / 8))
         ^ (filter[st->key] ? make_key(14) : 0);
}

inline Key Position::pawn_key() const { return st->pawnKey; }

inline Key Position::minor_piece_key() const { return st->minorPieceKey; }

inline Key Position::non_pawn_key(Color c) const { return st->nonPawnKey[c]; }

inline Value Position::major_material(Color c) const { return st->majorMaterial[c]; }

inline Value Position::major_material() const {
    return major_material(WHITE) + major_material(BLACK);
}

inline int Position::game_ply() const { return gamePly; }

inline int Position::rule60_count() const { return st->rule60; }

inline bool Position::capture(Move m) const {
    assert(m.is_ok());
    return !empty(m.to_sq());
}

inline Piece Position::captured_piece() const { return st->capturedPiece; }

inline void Position::put_piece(Piece pc, Square s) {

    board[s] = pc;
    byTypeBB[ALL_PIECES] |= byTypeBB[type_of(pc)] |= s;
    byColorBB[color_of(pc)] |= s;
    pieceCount[pc]++;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]++;
    //todo
#if ENABLE_NNUE
    midEncoding[color_of(pc)] += Eval::NNUE::Features::HalfKAv2_hm::MidMirrorEncoding[pc][s];
#endif
} //放置棋子

inline void Position::remove_piece(Square s) {

    Piece pc = board[s];
    byTypeBB[ALL_PIECES] ^= s;
    byTypeBB[type_of(pc)] ^= s;
    byColorBB[color_of(pc)] ^= s;
    board[s] = NO_PIECE;
    pieceCount[pc]--;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]--;
    //todo
#if ENABLE_NNUE
    midEncoding[color_of(pc)] -= Eval::NNUE::Features::HalfKAv2_hm::MidMirrorEncoding[pc][s];
#endif
} //移除棋子

inline void Position::move_piece(Square from, Square to) {

    Piece    pc     = board[from];
    Bitboard fromTo = from | to;
    byTypeBB[ALL_PIECES] ^= fromTo;
    byTypeBB[type_of(pc)] ^= fromTo;
    byColorBB[color_of(pc)] ^= fromTo;
    board[from] = NO_PIECE;
    board[to]   = pc;
    //todo
    // if (type_of(pc) == KING)
    //     kingSquare[color_of(pc)] = to;
    //todo
#if ENABLE_NNUE
    midEncoding[color_of(pc)] -= Eval::NNUE::Features::HalfKAv2_hm::MidMirrorEncoding[pc][from];
    midEncoding[color_of(pc)] += Eval::NNUE::Features::HalfKAv2_hm::MidMirrorEncoding[pc][to];
#endif
} //移动棋子

inline void Position::do_move(Move m, StateInfo& newSt, const TranspositionTable* tt = nullptr) {
    do_move(m, newSt, gives_check(m), tt);
} //执行移动

inline StateInfo* Position::state() const { return st; } //返回当前状态

inline Position& Position::set(const Position& pos, StateInfo* si) {

    set(pos.fen(), si);

    // Special cares for bloom filter
    std::memcpy(&filter, &pos.filter, sizeof(BloomFilter));

    return *this;
} //设置棋局

}  // namespace Stockfish

#endif  // #ifndef POSITION_H_INCLUDED
