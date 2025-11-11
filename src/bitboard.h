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

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "types.h"

#ifdef USE_PEXT
    #define IF_NOT_PEXT(...)
#else
    #define IF_NOT_PEXT(...) __VA_ARGS__
#endif

namespace Stockfish {

namespace Bitboards {

void        init();
std::string pretty(Bitboard b);

}  // namespace Stockfish::Bitboards

// 斗兽棋 7×9 棋盘位索引（bit index）展示：
// 北(上)
//   r8: 56, 57, 58, 59, 60, 61, 62
//   r7: 49, 50, 51, 52, 53, 54, 55
//   r6: 42, 43, 44, 45, 46, 47, 48
//   r5: 35, 36, 37, 38, 39, 40, 41
//   r4: 28, 29, 30, 31, 32, 33, 34
//   r3: 21, 22, 23, 24, 25, 26, 27
//   r2: 14, 15, 16, 17, 18, 19, 20
//   r1:  7,  8,  9, 10, 11, 12, 13
//   r0:  0,  1,  2,  3,  4,  5,  6
//       fa, fb, fc, fd, fe, ff, fg
// 西(左)                                   东(右)

// modify:斗兽棋没有王宫概念
//modify:修改行列线表示
constexpr Bitboard FileABB = []() {
    Bitboard bb = 0;
    for (int r = 0; r < RANK_NB; ++r)
        bb |= (Bitboard(1) << (r * FILE_NB));  // file = 0
    return bb;
}();
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;

constexpr Bitboard Rank0BB = 0x7F;  // 7 bits for 7 files
constexpr Bitboard Rank1BB = Rank0BB << (FILE_NB * 1);
constexpr Bitboard Rank2BB = Rank0BB << (FILE_NB * 2);
constexpr Bitboard Rank3BB = Rank0BB << (FILE_NB * 3);
constexpr Bitboard Rank4BB = Rank0BB << (FILE_NB * 4);
constexpr Bitboard Rank5BB = Rank0BB << (FILE_NB * 5);
constexpr Bitboard Rank6BB = Rank0BB << (FILE_NB * 6);
constexpr Bitboard Rank7BB = Rank0BB << (FILE_NB * 7);
constexpr Bitboard Rank8BB = Rank0BB << (FILE_NB * 8);

//delete:删除兵线
// constexpr Bitboard HalfBB[2]  = {Rank0BB | Rank1BB | Rank2BB | Rank3BB | Rank4BB,
//                                  Rank5BB | Rank6BB | Rank7BB | Rank8BB | Rank9BB};


extern uint8_t PopCnt16[1 << 16];
extern uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

extern Bitboard SquareBB[SQUARE_NB];
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];
extern Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];

// ==== Jungle 几何与位板 ====
extern Bitboard JungleAreaBB;      // 7x9 的有效区（嵌入 9x10）
extern Bitboard RiverBB;           // 两块 2x3 的小河
extern Bitboard LandBB;            // JungleAreaBB \ RiverBB
extern Bitboard TrapBB[COLOR_NB];  // 双方陷阱（对方进入则降级为0）
extern Bitboard DenBB[COLOR_NB];   // 双方兽穴（不可进入己方兽穴）

// 基础四邻
extern Bitboard ORTH1[SQUARE_NB];        // 上下左右一格（已裁到 JungleAreaBB 内）
extern Bitboard LAND_ORTH1[SQUARE_NB];   // 陆地上的四邻
extern Bitboard RIVER_ORTH1[SQUARE_NB];  // 河里的四邻（基本只给鼠用）

// 狮/虎跳河预计算
enum Dir4 : int {
    DN = 0,
    DS = 1,
    DE = 2,
    DW = 3
};
extern Square   JumpTarget[SQUARE_NB][4];    // 无跳落点则 SQ_NONE
extern Bitboard JumpPathMask[SQUARE_NB][4];  // 仅包含“河中”的中间格

int popcount(Bitboard b);  // required for 128 bit pext

// Magic holds all magic bitboards relevant data for a single square
struct Magic {
    Bitboard  mask;
    Bitboard* attacks;
    unsigned  shift;
    IF_NOT_PEXT(Bitboard magic;)

    // Compute the attack's index using the 'magic bitboards' approach
    unsigned index(Bitboard occupied) const {

#ifdef USE_PEXT
        return unsigned(pext(occupied, mask, shift));
#else
        return unsigned(((occupied & mask) * magic) >> shift);
#endif
    }
};

//delete:删除棋子magic

constexpr Bitboard square_bb(Square s) {
    assert(is_ok(s));
    return SquareBB[s];
}


// Overloads of bitwise operators between a Bitboard and a Square for testing
// whether a given bit is set in a bitboard, and for setting and clearing bits.

constexpr Bitboard  operator&(Bitboard b, Square s) { return b & square_bb(s); }
constexpr Bitboard  operator|(Bitboard b, Square s) { return b | square_bb(s); }
constexpr Bitboard  operator^(Bitboard b, Square s) { return b ^ square_bb(s); }
constexpr Bitboard& operator|=(Bitboard& b, Square s) { return b |= square_bb(s); }
constexpr Bitboard& operator^=(Bitboard& b, Square s) { return b ^= square_bb(s); }

constexpr Bitboard operator&(Square s, Bitboard b) { return b & s; }
constexpr Bitboard operator|(Square s, Bitboard b) { return b | s; }
constexpr Bitboard operator^(Square s, Bitboard b) { return b ^ s; }

constexpr Bitboard operator|(Square s1, Square s2) { return square_bb(s1) | s2; }

constexpr bool more_than_one(Bitboard b) { return bool(b & (b - 1)); }


// rank_bb() and file_bb() return a bitboard representing all the squares on
// the given file or rank.

constexpr Bitboard rank_bb(Rank r) { return Rank0BB << (FILE_NB * r); }

constexpr Bitboard rank_bb(Square s) { return rank_bb(rank_of(s)); }

constexpr Bitboard file_bb(File f) { return FileABB << std::uint8_t(f); }

constexpr Bitboard file_bb(Square s) { return file_bb(file_of(s)); }


// Moves a bitboard one or two steps as specified by the direction D
template<Direction D>
constexpr Bitboard shift(Bitboard b) {
    return D == NORTH         ? (b & ~Rank8BB) << std::uint8_t(NORTH)
         : D == SOUTH         ? b >> std::uint8_t(NORTH)
         : D == NORTH + NORTH ? (b & ~Rank8BB & ~Rank7BB) << std::uint8_t(NORTH + NORTH)
         : D == SOUTH + SOUTH ? b >> std::uint8_t(NORTH + NORTH)
         : D == EAST          ? (b & ~FileGBB) << std::uint8_t(EAST)
         : D == WEST          ? (b & ~FileABB) >> std::uint8_t(EAST)
         : D == NORTH_EAST    ? (b & ~FileGBB) << std::uint8_t(NORTH_EAST)
         : D == NORTH_WEST    ? (b & ~FileABB) << std::uint8_t(NORTH_WEST)
         : D == SOUTH_EAST    ? (b & ~FileGBB) >> std::uint8_t(NORTH_WEST)
         : D == SOUTH_WEST    ? (b & ~FileABB) >> std::uint8_t(NORTH_EAST)
                              : Bitboard(0);
}

//todo:是否有用？
// Returns the squares attacked by pawns of the given color
// from the squares in the given bitboard.
template<Color C>
constexpr Bitboard pawn_attacks_bb(Square s) {
    Bitboard b      = square_bb(s);
    Bitboard attack = shift < C == WHITE ? NORTH : SOUTH > (b);
    if ((C == WHITE && rank_of(s) > RANK_4) || (C == BLACK && rank_of(s) < RANK_5))
        attack |= shift<WEST>(b) | shift<EAST>(b);
    return attack;
}

//todo:是否有用？
// Returns the squares that if there is a pawn
// of the given color in there, it can attack the square s
template<Color C>
constexpr Bitboard pawn_attacks_to_bb(Square s) {
    Bitboard b      = square_bb(s);
    Bitboard attack = shift < C == WHITE ? SOUTH : NORTH > (b);
    if ((C == WHITE && rank_of(s) > RANK_4) || (C == BLACK && rank_of(s) < RANK_5))
        attack |= shift<WEST>(b) | shift<EAST>(b);
    return attack;
}

//todo:是否有用？
// Returns a bitboard representing an entire line (from board edge
// to board edge) that intersects the two given squares. If the given squares
// are not on a same file/rank/diagonal, the function returns 0. For instance,
// line_bb(SQ_C4, SQ_F7) will return a bitboard with the A2-G8 diagonal.
inline Bitboard line_bb(Square s1, Square s2) {

    assert(is_ok(s1) && is_ok(s2));
    return LineBB[s1][s2];
}

//todo:是否有用？
// Returns a bitboard representing the squares in the semi-open
// segment between the squares s1 and s2 (excluding s1 but including s2). If the
// given squares are not on a same file/rank/diagonal, it returns s2. For instance,
// between_bb(SQ_C4, SQ_F7) will return a bitboard with squares D5, E6 and F7, but
// between_bb(SQ_E6, SQ_F8) will return a bitboard with the square F8. This trick
// allows to generate non-king evasion moves faster: the defending piece must either
// interpose itself to cover the check or capture the checking piece.
inline Bitboard between_bb(Square s1, Square s2) {

    assert(is_ok(s1) && is_ok(s2));
    return BetweenBB[s1][s2];
}

//todo:是否有用？
// Returns true if the squares s1, s2 and s3 are aligned either on a
// straight or on a diagonal line.
inline bool aligned(Square s1, Square s2, Square s3) { return bool(line_bb(s1, s2) & s3); }


// distance() functions return the distance between x and y, defined as the
// number of steps for a king in x to reach y.

template<typename T1 = Square>
inline int distance(Square x, Square y);

template<>
inline int distance<File>(Square x, Square y) {
    return std::abs(file_of(x) - file_of(y));
}

template<>
inline int distance<Rank>(Square x, Square y) {
    return std::abs(rank_of(x) - rank_of(y));
}

template<>
inline int distance<Square>(Square x, Square y) {
    return SquareDistance[x][y];
}

inline int edge_distance(File f) { return std::min(f, File(FILE_G - f)); }
inline int edge_distance(Rank r) { return std::min(r, Rank(RANK_8 - r)); }


// Returns the pseudo attacks of the given piece type
// assuming an empty board.


// Returns the attacks by the given piece
// assuming the board is occupied according to the passed Bitboard.
// Sliding piece attacks do not continue passed an occupied square.
// 翻译：返回给定棋子的伪攻击，假设棋盘为空。
// 滑动棋子攻击不会继续通过已占据的方格。
// template<PieceType Pt>
// inline Bitboard attacks_bb(Square s) {
//     static_assert(Pt >= ELEPHANT && Pt <= RAT, "attacks_bb(Square): bad PieceType");
//     if constexpr (Pt == RAT) {
//         // 鼠可上下水：四向一步（含陆地与河）
//         return ORTH1[s];
//     } else if constexpr (Pt == LION || Pt == TIGER) {
//         // 狮/虎：仅“一步陆地邻接”（不含跳河）
//         return LAND_ORTH1[s];
//     } else {
//         // 其它动物：只允许陆地一步邻接
//         return LAND_ORTH1[s];
//     }
// }

template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occAll) {
    static_assert(Pt >= ELEPHANT && Pt <= RAT, "attacks_bb: bad PieceType");

    if constexpr (Pt == RAT) {
        // 鼠：陆/水都能走一步。是否允许从水里吃岸上子等留给上层判。
        (void)occAll;
        return ORTH1[s];
    }
    else if constexpr (Pt == LION || Pt == TIGER) {
        // 狮/虎：一步陆地邻接 + 跳河（路径上“河格”无占用才可）
        Bitboard a = LAND_ORTH1[s];

        // 只老鼠能在河里：河中占用 = occAll & RiverBB
        Bitboard occRats = occAll & RiverBB;

        for (int d = 0; d < 4; ++d) {
            Square t = JumpTarget[s][d];
            if (t == SQ_NONE) continue;

            // 路径几何只包含“河中的中间格”
            if ((JumpPathMask[s][d] & occRats) == 0)
                a |= square_bb(t);
        }
        return a;
    }
    else {
        // 其它动物：一步陆地邻接（无“路径阻挡”的概念）
        (void)occAll;
        return LAND_ORTH1[s];
    }
}

// Returns the attacks by the given piece
// assuming the board is occupied according to the passed Bitboard.
// Sliding piece attacks do not continue passed an occupied square.
// 运行时分派版本
inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occAll) {
    switch (pt) {
        case ELEPHANT: return attacks_bb<ELEPHANT>(s, occAll);
        case LION:     return attacks_bb<LION>(s,     occAll);
        case TIGER:    return attacks_bb<TIGER>(s,    occAll);
        case PANTHER:  return attacks_bb<PANTHER>(s,  occAll);
        case WOLF:     return attacks_bb<WOLF>(s,     occAll);
        case DOG:      return attacks_bb<DOG>(s,      occAll);
        case CAT:      return attacks_bb<CAT>(s,      occAll);
        case RAT:      return attacks_bb<RAT>(s,      occAll);
        default:       return Bitboard(0);
    }
}

// Counts the number of non-zero bits in a bitboard
//modify:找gpt要的算法，由于从128降为64，简化了很多
// 1) popcount：优先用内建/POPCNT，否则用 Kernighan 算法兜底
inline int popcount(Bitboard b) {
#if defined(_MSC_VER)
    return int(__popcnt64(b));  // MSVC/VC 自带
#elif defined(__POPCNT__) || defined(__SSE4_2__) || defined(__AVX2__)
    return __builtin_popcountll(b);  // GCC/Clang
#else
    // 兜底：Brian Kernighan 技巧（对 63 位也很快）
    int c = 0;
    while (b)
    {
        b &= (b - 1);
        ++c;
    }
    return c;
#endif
}

// 2) lsb：返回最低 1 位的索引（0..62）
inline Square lsb(Bitboard b) {
    assert(b);
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return Square(idx);
#else
    return Square(__builtin_ctzll(b));
#endif
}

// 3) 仅保留最低位
inline Bitboard least_significant_square_bb(Bitboard b) {
    assert(b);
    return b & (~b + 1);  // 或 b & -b
}

// 4) 弹出最低位并返回其索引
inline Square pop_lsb(Bitboard& b) {
    assert(b);
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

}  // namespace Stockfish

#endif  // #ifndef BITBOARD_H_INCLUDED
