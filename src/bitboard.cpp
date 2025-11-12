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

#include "bitboard.h"

#include <algorithm>
#include <bitset>
#include <initializer_list>

#include <set>
#include <utility>
// #ifndef USE_PEXT
//     #include "magics.h"
// #endif

namespace Stockfish {

uint8_t PopCnt16[1 << 16];
uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

Bitboard SquareBB[SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
//modify:修改伪攻击表大小
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];

//delete:删除magic，添加新结构
Bitboard JungleAreaBB, RiverBB, LandBB;//斗兽棋棋盘、河位棋盘表示、陆地棋盘表示
Bitboard TrapBB[COLOR_NB], DenBB[COLOR_NB];//陷阱位棋盘表示、兽穴位棋盘表示

Bitboard ORTH1[SQUARE_NB], LAND_ORTH1[SQUARE_NB], RIVER_ORTH1[SQUARE_NB];//纯几何四邻接表、限定为陆地的四邻接表、限定为河道的四邻接表

Square   JumpTarget[SQUARE_NB][4];//跳跃目标位棋盘表示
Bitboard JumpPathMask[SQUARE_NB][4];//跳跃路径位棋盘表示

namespace {

// Bitboard RookTable[0x108000];    // To store rook attacks
// Bitboard CannonTable[0x108000];  // To store cannon attacks
// Bitboard BishopTable[0x228];     // To store bishop attacks
// Bitboard KnightTable[0x380];     // To store knight attacks
// Bitboard KnightToTable[0x3E0];   // To store by knight attacks

// const std::set<Direction> KnightDirections{2 * SOUTH + WEST, 2 * SOUTH + EAST, SOUTH + 2 * WEST,
//                                            SOUTH + 2 * EAST, NORTH + 2 * WEST, NORTH + 2 * EAST,
//                                            2 * NORTH + WEST, 2 * NORTH + EAST};
// const std::set<Direction> BishopDirections{2 * NORTH_EAST, 2 * SOUTH_EAST, 2 * SOUTH_WEST,
//                                            2 * NORTH_WEST};

const std::set<Direction> BasicDirections{NORTH, SOUTH, EAST, WEST};

//modify: 狮、虎可以跳跃（类似国际象棋的马）
const std::set<Direction> JumpDirections{3 * SOUTH, 3 * NORTH, 2 * WEST, 2 * EAST};


template<PieceType pt>
void init_magics(Bitboard table[], Magic magics[] IF_NOT_PEXT(, const Bitboard magicsInit[]));

template<PieceType pt>
Bitboard lame_leaper_path(Direction d, Square s);

// Returns the bitboard of target square for the given step
// from the given square. If the step is off the board, returns empty bitboard.
Bitboard safe_destination(Square s, int step) {
    Square to = Square(s + step);
    return is_ok(to) && distance(s, to) <= 2 ? square_bb(to) : Bitboard(0);
}

}

// Returns an ASCII representation of a bitboard suitable
// to be printed to standard output. Useful for debugging.
//modify:适应斗兽棋棋盘
std::string Bitboards::pretty(Bitboard b) {

    std::string s = "+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8; r >= RANK_0; --r)
    {
        for (File f = FILE_A; f <= FILE_G; ++f)
            s += b & make_square(f, r) ? "| X " : "|   ";

        s += "| " + std::to_string(r) + "\n+---+---+---+---+---+---+---+\n";
    }
    s += "  a   b   c   d   e   f   g\n";

    return s;
}

// Initializes various bitboard tables. It is called at
// startup and relies on global objects to be already zero-initialized.
void Bitboards::init() {

    // --- 0) 基础位板 ---
    for (Square s = SQ_A0; s <= SQ_G8; ++s)
        SquareBB[s] = (Bitboard(1ULL) << std::uint8_t(s));

    for (Square s1 = SQ_A0; s1 <= SQ_G8; ++s1)
        for (Square s2 = SQ_A0; s2 <= SQ_G8; ++s2)
            SquareDistance[s1][s2] = std::max(distance<File>(s1, s2), distance<Rank>(s1, s2));

    // --- 1) 7×9 有效区 / 河 / 陆 ---
    JungleAreaBB = 0;
    for (Rank r = RANK_0; r <= RANK_8; ++r)
        JungleAreaBB |= rank_bb(r);

    const Bitboard rowsRiver = rank_bb(RANK_3) | rank_bb(RANK_4) | rank_bb(RANK_5);
    // 经典斗兽棋：左河 (b,c)×(r3..r5)，右河 (e,f)×(r3..r5)
    RiverBB = ((file_bb(FILE_B) | file_bb(FILE_C)) & rowsRiver)
            | ((file_bb(FILE_E) | file_bb(FILE_F)) & rowsRiver);

    LandBB = JungleAreaBB & ~RiverBB;

    // --- 2) 陷阱/兽穴（白方在底、黑方在顶）---
    // dens: d1 / d9 ； traps: c1/e1/d2 和 c9/e9/d8
    DenBB[WHITE]  = square_bb(make_square(FILE_D, RANK_0));
    TrapBB[WHITE] = square_bb(make_square(FILE_C, RANK_0)) | square_bb(make_square(FILE_E, RANK_0))
                  | square_bb(make_square(FILE_D, RANK_1));

    DenBB[BLACK]  = square_bb(make_square(FILE_D, RANK_8));
    TrapBB[BLACK] = square_bb(make_square(FILE_C, RANK_8)) | square_bb(make_square(FILE_E, RANK_8))
                  | square_bb(make_square(FILE_D, RANK_7));

    // --- 3) 四邻接表（纯几何，不看占用与规则）---
    for (Square s = SQ_A0; s <= SQ_G8; ++s)
    {
        Bitboard adj = 0;
        for (int step : {NORTH, SOUTH, EAST, WEST})
            adj |= safe_destination(s, step);  // 越界返回 0

        adj &= JungleAreaBB;  // 裁到 7×9
        ORTH1[s]       = adj;
        LAND_ORTH1[s]  = adj & LandBB;
        RIVER_ORTH1[s] = adj & RiverBB;
    }

    // --- 4) 狮/虎“跳河”预计算（从 s 朝四向，若紧邻是河则跨到第一块陆地）---
    auto inRiver = [&](Square x) { return bool(RiverBB & square_bb(x)); };
    auto inLand  = [&](Square x) { return bool(LandBB & square_bb(x)); };

    auto fill_jump = [&](Square s, int dIdx, Direction step) {
        Square cur = Square(s + step);
        if (!is_ok(cur) || !inRiver(cur))
            return;  // 紧邻不是河 → 无跳

        Bitboard path = 0;  // 仅收集“河中”经过格
        while (is_ok(cur) && inRiver(cur))
        {
            path |= square_bb(cur);
            cur = Square(cur + step);
        }
        if (!is_ok(cur) || !inLand(cur))
            return;  // 必须落在第一块陆地

        JumpTarget[s][dIdx]   = cur;
        JumpPathMask[s][dIdx] = path;
    };

    for (Square s = SQ_A0; s <= SQ_G8; ++s)
    {
        JumpTarget[s][0] = JumpTarget[s][1] = JumpTarget[s][2] = JumpTarget[s][3] = SQ_NONE;
        JumpPathMask[s][0] = JumpPathMask[s][1] = JumpPathMask[s][2] = JumpPathMask[s][3] = 0;

        fill_jump(s, 0, NORTH);
        fill_jump(s, 1, SOUTH);
        fill_jump(s, 2, EAST);
        fill_jump(s, 3, WEST);
    }

    // --- 5) 伪攻击表（纯几何模板；规则过滤放到走法层）---
    // 鼠：可上下水 → ORTH1；其它动物：仅陆地一步 → LAND_ORTH1
    for (Square s = SQ_A0; s <= SQ_G8; ++s)
    {
        PseudoAttacks[RAT][s]       = attacks_bb<RAT>(s,       Bitboard(0));
        PseudoAttacks[CAT][s]       = attacks_bb<CAT>(s,       Bitboard(0));
        PseudoAttacks[DOG][s]       = attacks_bb<DOG>(s,       Bitboard(0));
        PseudoAttacks[WOLF][s]      = attacks_bb<WOLF>(s,      Bitboard(0));
        PseudoAttacks[PANTHER][s]   = attacks_bb<PANTHER>(s,   Bitboard(0));
        PseudoAttacks[TIGER][s]     = attacks_bb<TIGER>(s,     Bitboard(0));  // 包含跳河落点
        PseudoAttacks[LION][s]      = attacks_bb<LION>(s,      Bitboard(0));  // 包含跳河落点
        PseudoAttacks[ELEPHANT][s]  = attacks_bb<ELEPHANT>(s,  Bitboard(0));
    }

    // --- 6) LineBB/BetweenBB 的最简初始化（斗兽棋基本用不到直线/夹线）---
    for (Square s1 = SQ_A0; s1 <= SQ_G8; ++s1)
        for (Square s2 = SQ_A0; s2 <= SQ_G8; ++s2)
        {
            LineBB[s1][s2]    = 0;
            BetweenBB[s1][s2] = square_bb(s2);  // 保持“半开区间含 s2”的语义
        }
}

//delete:删除magic相关逻辑


//namespace {
//
//template<PieceType pt>
//Bitboard sliding_attack(Square sq, Bitboard occupied) {
//    assert(pt == ROOK || pt == CANNON);
//    Bitboard attack = 0;
//
//    for (auto const& d : {NORTH, SOUTH, EAST, WEST})
//    {
//        bool hurdle = false;
//        for (Square s = sq + d; is_ok(s) && distance(s - d, s) == 1; s += d)
//        {
//            if (pt == ROOK || hurdle)
//                attack |= s;
//
//            if (occupied & s)
//            {
//                if (pt == CANNON && !hurdle)
//                    hurdle = true;
//                else
//                    break;
//            }
//        }
//    }
//
//    return attack;
//}
//
//template<PieceType pt>
//Bitboard lame_leaper_path(Direction d, Square s) {
//    Bitboard b  = 0;
//    Square   to = s + d;
//    if (!is_ok(to) || distance(s, to) >= 4)
//        return b;
//
//    // If piece type is by knight attacks, swap the source and destination square
//    if (pt == KNIGHT_TO)
//    {
//        std::swap(s, to);
//        d = -d;
//    }
//
//    Direction dr = d > 0 ? NORTH : SOUTH;
//    Direction df = (std::abs(d % NORTH) < NORTH / 2 ? d % NORTH : -(d % NORTH)) < 0 ? WEST : EAST;
//
//    int diff = std::abs(file_of(to) - file_of(s)) - std::abs(rank_of(to) - rank_of(s));
//    if (diff > 0)
//        s += df;
//    else if (diff < 0)
//        s += dr;
//    else
//        s += df + dr;
//
//    b |= s;
//    return b;
//}
//
//template<PieceType pt>
//Bitboard lame_leaper_path(Square s) {
//    Bitboard b = 0;
//    for (const auto& d : pt == BISHOP ? BishopDirections : KnightDirections)
//        b |= lame_leaper_path<pt>(d, s);
//    if (pt == BISHOP)
//        b &= HalfBB[rank_of(s) > RANK_4];
//    return b;
//}
//
//template<PieceType pt>
//Bitboard lame_leaper_attack(Square s, Bitboard occupied) {
//    Bitboard b = 0;
//    for (const auto& d : pt == BISHOP ? BishopDirections : KnightDirections)
//    {
//        Square to = s + d;
//        if (is_ok(to) && distance(s, to) < 4 && !(lame_leaper_path<pt>(d, s) & occupied))
//            b |= to;
//    }
//    if (pt == BISHOP)
//        b &= HalfBB[rank_of(s) > RANK_4];
//    return b;
//}
//
//
//// Computes all rook and bishop attacks at startup. Magic
//// bitboards are used to look up attacks of sliding pieces. As a reference see
//// https://www.chessprogramming.org/Magic_Bitboards. In particular, here we use
//// the so called "fancy" approach.
//template<PieceType pt>
//void init_magics(Bitboard table[], Magic magics[] IF_NOT_PEXT(, const Bitboard magicsInit[])) {
//
//    Bitboard edges, b;
//    uint64_t size = 0;
//
//    for (Square s = SQ_A0; s <= SQ_I9; ++s)
//    {
//        // Board edges are not considered in the relevant occupancies
//        edges = ((Rank0BB | Rank9BB) & ~rank_bb(s)) | ((FileABB | FileIBB) & ~file_bb(s));
//
//        // Given a square 's', the mask is the bitboard of sliding attacks from
//        // 's' computed on an empty board. The index must be big enough to contain
//        // all the attacks for each possible subset of the mask and so is 2 power
//        // the number of 1s of the mask.
//        Magic& m = magics[s];
//        m.mask   = pt == ROOK   ? sliding_attack<pt>(s, 0)
//                 : pt == CANNON ? RookMagics[s].mask
//                                : lame_leaper_path<pt>(s);
//        if (pt != KNIGHT_TO)
//            m.mask &= ~edges;
//
//#ifdef USE_PEXT
//        m.shift = popcount(uint64_t(m.mask));
//#else
//        m.magic = magicsInit[s];
//        m.shift = 128 - popcount(m.mask);
//#endif
//
//        // Set the offset for the attacks table of the square. We have individual
//        // table sizes for each square with "Fancy Magic Bitboards".
//        m.attacks = s == SQ_A0 ? table : magics[s - 1].attacks + size;
//
//        // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
//        // store the corresponding attack bitboard in m.attacks.
//        b = size = 0;
//        do
//        {
//            m.attacks[m.index(b)] =
//              pt == ROOK || pt == CANNON ? sliding_attack<pt>(s, b) : lame_leaper_attack<pt>(s, b);
//
//            size++;
//            b = (b - m.mask) & m.mask;
//        } while (b);
//    }
//}
//}

}  // namespace Stockfish
