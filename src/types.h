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

#ifndef TYPES_H_INCLUDED
    #define TYPES_H_INCLUDED

// When compiling with provided Makefile (e.g. for Linux and OSX), configuration
// is done automatically. To get started type 'make help'.
//
// When Makefile is not used (e.g. with Microsoft Visual Studio) some switches
// need to be set manually:
//
// -DNDEBUG      | Disable debugging mode. Always use this for release.
//
// -DNO_PREFETCH | Disable use of prefetch asm-instruction. You may need this to
//               | run on some very old machines.
//
// -DUSE_POPCNT  | Add runtime support for use of popcnt asm-instruction. Works
//               | only in 64-bit mode and requires hardware with popcnt support.
//
// -DUSE_PEXT    | Add runtime support for use of pext asm-instruction. Works
//               | only in 64-bit mode and requires hardware with pext support.

    #include <cassert>
    #include <cstddef>
    #include <cstdint>
    #include <type_traits>
    #include <string_view>

    #if defined(_MSC_VER)
        // Disable some silly and noisy warnings from MSVC compiler
        #pragma warning(disable: 4127)  // Conditional expression is constant
        #pragma warning(disable: 4146)  // Unary minus operator applied to unsigned type
        #pragma warning(disable: 4800)  // Forcing value to bool 'true' or 'false'
    #endif

// Predefined macros hell:
//
// __GNUC__                Compiler is GCC, Clang or ICX
// __clang__               Compiler is Clang or ICX
// __INTEL_LLVM_COMPILER   Compiler is ICX
// _MSC_VER                Compiler is MSVC
// _WIN32                  Building on Windows (any)
// _WIN64                  Building on Windows 64 bit

// Enforce minimum GCC version
    #if defined(__GNUC__) && !defined(__clang__) \
      && (__GNUC__ < 9 || (__GNUC__ == 9 && __GNUC_MINOR__ < 3))
        #error "Pikafish requires GCC 9.3 or later for correct compilation"
    #endif

    // Enforce minimum Clang version
    #if defined(__clang__) && (__clang_major__ < 10)
        #error "Pikafish requires Clang 10.0 or later for correct compilation"
    #endif

    #define ASSERT_ALIGNED(ptr, alignment) assert(reinterpret_cast<uintptr_t>(ptr) % alignment == 0)

    #if defined(_MSC_VER) && !defined(__clang__)
        #include <__msvc_int128.hpp>  // Microsoft header for std::_Unsigned128
using __uint128_t = std::_Unsigned128;
    #endif

    #if defined(_WIN64) && defined(_MSC_VER)  // No Makefile used
        #include <intrin.h>                   // Microsoft header for _BitScanForward64()
        #define IS_64BIT
    #endif

    #if defined(USE_POPCNT) && defined(_MSC_VER)
        #include <nmmintrin.h>  // Microsoft header for _mm_popcnt_u64()
    #endif

    #if !defined(NO_PREFETCH) && defined(_MSC_VER)
        #include <xmmintrin.h>  // Microsoft header for _mm_prefetch()
    #endif

    #if defined(USE_PEXT)
        #include <immintrin.h>  // Header for _pext_u64() intrinsic
        #if defined(_MSC_VER) && !defined(__clang__)
            #define pext(b, m, s) \
                ((_pext_u64(b._Word[1], m._Word[1]) << s) | _pext_u64(b._Word[0], m._Word[0]))
        #else
            #define pext(b, m, s) ((_pext_u64(b >> 64, m >> 64) << s) | _pext_u64(b, m))
        #endif
    #else
        #define pext(b, m, s) 0
    #endif

namespace Stockfish {

    #ifdef USE_POPCNT
constexpr bool HasPopCnt = true;
    #else
constexpr bool HasPopCnt = false;
    #endif

    #ifdef USE_PEXT
constexpr bool HasPext = true;
    #else
constexpr bool HasPext = false;
    #endif

    #ifdef IS_64BIT
constexpr bool Is64Bit = true;
    #else
constexpr bool Is64Bit = false;
    #endif

    #ifndef ENABLE_NNUE
        #define ENABLE_NNUE 0
    #endif

    #ifndef CHANGE_FOR_COMPAT
        #define CHANGE_FOR_COMPAT 1  //仅仅表示为兼容而修改，并非实意
    #endif

    #ifndef DEBUG_NUMS
        #define DEBUG_NUMS 100
    #endif

    #define RANK_ONE_BASED

    
using Key      = uint64_t;
using Bitboard = uint64_t;

constexpr int MAX_MOVES = 128;
constexpr int MAX_PLY   = 246;

enum Color : int8_t {
    WHITE,
    BLACK,
    COLOR_NB = 2
};

enum Bound : int8_t {
    BOUND_NONE,
    BOUND_UPPER,
    BOUND_LOWER,
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};  // 是当前节点是否被截断的标志

// Value is used as an alias for int, this is done to differentiate between a search
// value and any other integer value. The values used in search are always supposed
// to be in the range (-VALUE_NONE, VALUE_NONE] and should not exceed this range.
using Value = int;

constexpr Value VALUE_ZERO     = 0;
constexpr Value VALUE_DRAW     = 0;
constexpr Value VALUE_NONE     = 32002;
constexpr Value VALUE_INFINITE = 32001;

constexpr Value VALUE_MATE             = 32000;
constexpr Value VALUE_MATE_IN_MAX_PLY  = VALUE_MATE - MAX_PLY;
constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY;

constexpr bool is_valid(Value value) { return value != VALUE_NONE; }

constexpr bool is_win(Value value) {
    assert(is_valid(value));
    return value >= VALUE_MATE_IN_MAX_PLY;
}

constexpr bool is_loss(Value value) {
    assert(is_valid(value));
    return value <= VALUE_MATED_IN_MAX_PLY;
}

constexpr bool is_decisive(Value value) { return is_win(value) || is_loss(value); }

// 斗兽棋棋子价值 (按捕食能力排序)todo:值是瞎写的
constexpr Value ElephantValue = 1000;  // 象 - 最高价值
constexpr Value LionValue     = 800;   // 狮 - 第二高
constexpr Value TigerValue    = 600;   // 虎 - 第三高
constexpr Value PantherValue  = 400;   // 豹 - 第四高
constexpr Value WolfValue     = 300;   // 狼 - 第五高
constexpr Value DogValue      = 200;   // 狗 - 第六高
constexpr Value CatValue      = 100;   // 猫 - 第七高
constexpr Value RatValue      = 50;    // 鼠 - 最低价值

// clang-format off
//todo :少了to结构
enum PieceType : std::int8_t {
    NO_PIECE_TYPE,
    ELEPHANT, LION, TIGER, PANTHER, WOLF, DOG, CAT, RAT, 
    ALL_PIECES = 0,
    PIECE_TYPE_NB = 9
};


enum Piece : std::int8_t {
    NO_PIECE,
    W_ELEPHANT               , W_LION, W_TIGER, W_PANTHER, W_WOLF, W_DOG, W_CAT, W_RAT,
    B_ELEPHANT = ELEPHANT + 16, B_LION, B_TIGER, B_PANTHER, B_WOLF, B_DOG, B_CAT, B_RAT,
    PIECE_NB
};

//删减了一个0值，为了对齐好用位运算
constexpr Value PieceValue[PIECE_NB] = {
   VALUE_ZERO, 
   ElephantValue, LionValue, TigerValue, PantherValue, WolfValue, DogValue, CatValue, RatValue,
   VALUE_ZERO, VALUE_ZERO, VALUE_ZERO, VALUE_ZERO, VALUE_ZERO, VALUE_ZERO, VALUE_ZERO, VALUE_ZERO, 
   ElephantValue, LionValue, TigerValue, PantherValue, WolfValue, DogValue, CatValue, RatValue};
// clang-format on

constexpr std::string_view PieceToChar(" ELTPWDCR        eltpwdcr");

//add:添加棋子rank信息
static constexpr int JungleRankPT[PIECE_TYPE_NB] = {
  /* NO_PIECE_TYPE */ 0,
  /* ELEPHANT      */ 8,
  /* LION          */ 7,
  /* TIGER         */ 6,
  /* PANTHER       */ 5,
  /* WOLF          */ 4,
  /* DOG           */ 3,
  /* CAT           */ 2,
  /* RAT           */ 1};

using Depth = int;

// The following DEPTH_ constants are used for transposition table entries
// and quiescence search move generation stages. In regular search, the
// depth stored in the transposition table is literal: the search depth
// (effort) used to make the corresponding transposition table value. In
// quiescence search, however, the transposition table entries only store
// the current quiescence move generation stage (which should thus compare
// lower than any regular search depth).
constexpr Depth DEPTH_QS = 0;
// For transposition table entries where no searching at all was done
// (whether regular or qsearch) we use DEPTH_UNSEARCHED, which should thus
// compare lower than any quiescence or regular depth. DEPTH_ENTRY_OFFSET
// is used only for the transposition table entry occupancy check (see tt.cpp),
// and should thus be lower than DEPTH_UNSEARCHED.
constexpr Depth DEPTH_UNSEARCHED   = -2;
constexpr Depth DEPTH_ENTRY_OFFSET = -3;

// 斗兽棋棋盘: 7x9 = 63个格子
// clang-format off
enum Square : int8_t {
    SQ_A0, SQ_B0, SQ_C0, SQ_D0, SQ_E0, SQ_F0, SQ_G0,
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8,
    SQ_NONE,

    SQUARE_ZERO = 0,
    SQUARE_NB   = 63
};
// clang-format on

enum Direction : int8_t {
    NORTH = 7,  // 斗兽棋每行7个格子
    EAST  = 1,
    SOUTH = -NORTH,
    WEST  = -EAST,

    NORTH_EAST = NORTH + EAST,
    SOUTH_EAST = SOUTH + EAST,
    SOUTH_WEST = SOUTH + WEST,
    NORTH_WEST = NORTH + WEST
};

enum File : int8_t {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_NB = 7
};

enum Rank : int8_t {
    RANK_0,
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,

    RANK_NB = 9
};

// For fast repetition checks
struct BloomFilter {
    constexpr static uint64_t FILTER_SIZE = 1 << 14;
    uint8_t                   operator[](Key key) const { return table[key & (FILTER_SIZE - 1)]; }
    uint8_t&                  operator[](Key key) { return table[key & (FILTER_SIZE - 1)]; }

   private:
    uint8_t table[1 << 14];
};

// Keep track of what a move changes on the board (used by NNUE)
struct DirtyPiece {
    Piece  pc;  // this is never allowed to be NO_PIECE
    Square from, to;

    // if remove_sq is SQ_NONE, remove_pc is allowed to be uninitialized
    Square remove_sq;
    Piece  remove_pc;

    bool requires_refresh[2];
};

    #define ENABLE_INCR_OPERATORS_ON(T) \
        constexpr T& operator++(T& d) { return d = T(int(d) + 1); } \
        constexpr T& operator--(T& d) { return d = T(int(d) - 1); }

ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)
ENABLE_INCR_OPERATORS_ON(Color)

    #undef ENABLE_INCR_OPERATORS_ON

constexpr Direction operator-(Direction d) { return Direction(-int(d)); }
constexpr Direction operator+(Direction d1, Direction d2) { return Direction(int(d1) + int(d2)); }
constexpr Direction operator*(int i, Direction d) { return Direction(i * int(d)); }

// Additional operators to add a Direction to a Square
constexpr Square  operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square  operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
constexpr Square& operator+=(Square& s, Direction d) { return s = s + d; }
constexpr Square& operator-=(Square& s, Direction d) { return s = s - d; }

// Toggle color
constexpr Color operator~(Color c) { return Color(c ^ BLACK); }

// Swap color of piece B_KNIGHT <-> W_KNIGHT
constexpr Piece operator~(Piece pc) { return Piece(pc ^ 16); }

constexpr Value mate_in(int ply) { return VALUE_MATE - ply; }

constexpr Value mated_in(int ply) { return -VALUE_MATE + ply; }

constexpr Square make_square(File f, Rank r) { return Square(r * FILE_NB + f); }

constexpr Piece make_piece(Color c, PieceType pt) { return Piece((c << 4) + pt); }

//modify:同上operator类似
constexpr PieceType type_of(Piece pc) { return PieceType(pc & 15); }
//modify:同上operator类似
constexpr Color color_of(Piece pc) {
    assert(pc != NO_PIECE);
    return Color((pc >> 4));
}

constexpr bool is_ok(Square s) { return s >= SQ_A0 && s <= SQ_G8; }

constexpr File file_of(Square s) { return File(s % FILE_NB); }

constexpr Rank rank_of(Square s) { return Rank(s / FILE_NB); }

// Swap A0 <-> A8
constexpr Square flip_rank(Square s) { return make_square(file_of(s), Rank(RANK_8 - rank_of(s))); }

// Swap A0 <-> G0
constexpr Square flip_file(Square s) { return make_square(File(FILE_G - file_of(s)), rank_of(s)); }

// Based on a congruential pseudo-random number generator
//todo:不知道什么东西
constexpr Key make_key(uint64_t seed) {
    return seed * 6364136223846793005ULL + 1442695040888963407ULL;
}

// A move needs 16 bits to be stored
//
// bit  0- 5: destination square (from 0 to 62) - 6 bits for 63 squares
// bit  6-11: origin square (from 0 to 62) - 6 bits for 63 squares
// bit 12-15: reserved for special move types
//
// Special cases are Move::none() and Move::null(). We can sneak these in because
// in any normal move the destination square and origin square are always different,
// but Move::none() and Move::null() have the same origin and destination square.

//modify:由于棋盘大小从90转为63，故一个格子占位数从7变为6
class Move {
   public:
    Move() = default;
    constexpr explicit Move(std::uint16_t d) :
        data(d) {}

    constexpr Move(Square from, Square to) :
        data((from << 6) + to) {}

    static constexpr Move make(Square from, Square to) { return Move((from << 6) + to); }

    constexpr Square from_sq() const {
        assert(is_ok());
        return Square((data >> 6) & 0x3F);
    }

    constexpr Square to_sq() const {
        assert(is_ok());
        return Square(data & 0x3F);
    }

    constexpr int from_to() const { return data & 0xFFF; }

    constexpr bool is_ok() const { return none().data != data && null().data != data; }

    static constexpr Move null() { return Move(65); }
    static constexpr Move none() { return Move(0); }

    constexpr bool operator==(const Move& m) const { return data == m.data; }
    constexpr bool operator!=(const Move& m) const { return data != m.data; }

    constexpr explicit operator bool() const { return data != 0; }

    constexpr std::uint16_t raw() const { return data; }

    struct MoveHash {
        std::size_t operator()(const Move& m) const { return make_key(m.data); }
    };

   protected:
    std::uint16_t data;
};

template<typename T, typename... Ts>
struct is_all_same {
    static constexpr bool value = (std::is_same_v<T, Ts> && ...);
};

template<typename... Ts>
constexpr auto is_all_same_v = is_all_same<Ts...>::value;

}  // namespace Stockfish

#endif  // #ifndef TYPES_H_INCLUDED

#include "tune.h"  // Global visibility to tuning setup
