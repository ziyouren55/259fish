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

//Definition of input features HalfKAv2_hm of NNUE evaluation function

#ifndef NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED
#define NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED

#include <array>
#include <cstdint>
#include <utility>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish {
class Position;
}

/* 
    特征向量与特征索引
    特征向量是完整的位向量[0, 0, 1, 0, ..., 1（第100位）, ..., 1（第9999位）, 0, 0]
    特征索引是被激活的特征的索引[3, 100, 9999]
    在代码中使用索引，能够减轻计算机负担
*/

namespace Stockfish::Eval::NNUE::Features {

// Feature HalfKAv2_hm: Combination of the position of own king and the
// position of pieces. Position mirrored such that king is always on d..e files.
// 特征 HalfKAv2_hm：结合自身国王的位置和棋子的位置。位置镜像化，使得国王总是位于 d..e 列。
class HalfKAv2_hm {

    // Unique number for each piece type on each square
    enum {
        // clang-format off
        PS_NONE     = 0,
        PS_W_ROOK   = 0,
        PS_B_ROOK   = 1 * SQUARE_NB,
        PS_W_CANNON = 2 * SQUARE_NB,
        PS_B_CANNON = 3 * SQUARE_NB,
        PS_W_KNIGHT = 4 * SQUARE_NB,
        PS_B_KNIGHT = 5 * SQUARE_NB,
        PS_AB_W_KP  = 6 * SQUARE_NB,  // 白王和兵合并到同一个平面，也用于仕和相
        PS_B_KP     = 7 * SQUARE_NB,  // 黑王和兵合并到同一个平面
        PS_NB       = 8 * SQUARE_NB
        // clang-format on
    };

    static constexpr IndexType PieceSquareIndex[COLOR_NB][PIECE_NB] = {
      // 约定：W - 我方，B - 对方
      // 从对方视角看，W 和 B 是反转的
      // clang-format off
      { PS_NONE, PS_W_ROOK, PS_AB_W_KP, PS_W_CANNON, PS_AB_W_KP, PS_W_KNIGHT, PS_AB_W_KP, PS_AB_W_KP,
        PS_NONE, PS_B_ROOK, PS_AB_W_KP, PS_B_CANNON, PS_B_KP   , PS_B_KNIGHT, PS_AB_W_KP, PS_B_KP   , },
      { PS_NONE, PS_B_ROOK, PS_AB_W_KP, PS_B_CANNON, PS_B_KP   , PS_B_KNIGHT, PS_AB_W_KP, PS_B_KP   ,
        PS_NONE, PS_W_ROOK, PS_AB_W_KP, PS_W_CANNON, PS_AB_W_KP, PS_W_KNIGHT, PS_AB_W_KP, PS_AB_W_KP, }
      // clang-format on
    };// 也是一个查找表，根据参数快速获取偏移基址

   public:
    // 特征名称
    static constexpr const char* Name = "HalfKAv2_hm";

    // Hash value embedded in the evaluation file 嵌入在评估文件中的哈希值
    static constexpr std::uint32_t HashValue = 0xd17b100;

    // todo 特征维度数量 6为折叠后将的6个可能位置（桶），2为是否有车，3为马炮3种情况，最后720为特征空间大小
    static constexpr IndexType Dimensions = 6 * 2 * 3 * static_cast<IndexType>(PS_NB);

    // 桶本质上就是一种分类方法，把多个相似的局面归为一类，所有落在同一个桶的特征，在神经网络输入时可以共享权重或用同一组参数处理。
    // 用桶能够减低特征维度，加快计算速度，提升泛化能力，减少内存占用。

    // 获取国王索引和镜像信息
    static constexpr auto KingBuckets = []() {
#define M(s) ((1 << 3) | s) //合成为一个4位bit 低3位为国王位置 最高位为1则为镜像
        // 存储为 (mirror << 3 | bucket)
        constexpr uint8_t KingBuckets[SQUARE_NB] = {
          // clang-format off
          0,  0,  0,  0,  1, M(0),  0,  0,  0, //M(0) = (1 << 3) | 0 =  1000 = 8
          0,  0,  0,  2,  3, M(2),  0,  0,  0, //M(2) = (1 << 3) | 2 =  1010 = 10
          0,  0,  0,  4,  5, M(4),  0,  0,  0, //M(4) = (1 << 3) | 4 =  1100 = 12
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  4,  5, M(4),  0,  0,  0,
          0,  0,  0,  2,  3, M(2),  0,  0,  0,
          0,  0,  0,  0,  1, M(0),  0,  0,  0,
          // clang-format on
        };//存的是国王合理位置的棋盘，m指mirror翻转
#undef M
        std::array<std::array<std::array<std::pair<int, bool>, 2>, SQUARE_NB>, SQUARE_NB> v{};
        for (uint8_t ksq = SQ_A0; ksq <= SQ_I9; ++ksq)// ksq指国王位置
            for (uint8_t oksq = SQ_A0; oksq <= SQ_I9; ++oksq)// oksq指对方国王位置
                for (uint8_t midm = 0; midm <= 1; ++midm)// midm指是否水平翻转
                {
                    uint8_t king_bucket_ = KingBuckets[ksq];
                    int     king_bucket  = king_bucket_ & 0x7; //己方国王位置
                    int     oking_bucket = KingBuckets[oksq] & 0x7; //对方国王位置
                    bool    mirror =
                      (king_bucket_ >> 3) //看国王是否在镜像位置(是否为M(s))
                      || ((king_bucket & 1) //看国王是否在中线，如果不在，就说明在左侧，不镜像；反之则看对方国王位置
                          && ((KingBuckets[oksq] >> 3) || (bool(oking_bucket & 1) && midm))); //看对方国王是否在镜像位置
                    v[ksq][oksq][midm].first  = king_bucket;
                    v[ksq][oksq][midm].second = mirror;
                }
        return v;//v是加速算法用的，把所有国王位置情况都列出来，1次查询获得桶号和镜像信息，用空间换时间
    }();

    // Get attack bucket based on attack feature 根据攻击特征获取攻击桶
    static constexpr auto AttackBucket = []() {
        std::array<std::array<std::array<int, 3>, 3>, 3> v{};
        for (uint8_t rook = 0; rook <= 2; ++rook)
            for (uint8_t knight = 0; knight <= 2; ++knight)
                for (uint8_t cannon = 0; cannon <= 2; ++cannon)
                    v[rook][knight][cannon] = [&] {
                        if (rook != 0)
                            if (knight > 0 && cannon > 0)
                                return 0;
                            else if (rook == 2 || knight + cannon > 1)
                                return 1;
                            else
                                return 2;
                        else if (knight > 0 && cannon > 0)
                            return 3;
                        else if (knight + cannon > 1)
                            return 4;
                        else
                            return 5;
                    }();
        return v;//和上面类似，也是根据三个参数来快速查找到该用哪个攻击桶
    }();

    // Square index mapping based on condition (Mirror, Rotate, ABMap) 根据条件（镜像、旋转、ABMap）获取方格索引映射
    static constexpr auto IndexMap = []() {
        // Map advisor and bishop location into White King plane 将仕和相的位置映射到白王平面
        constexpr uint8_t ABMap[SQUARE_NB] = {
          // clang-format off
           0,  0,  0,  1,  0,  2,  5,  0,  0,//0-8
           0,  0,  0,  0,  6,  0,  0,  0,  0,//9-16
           7,  0,  0,  8,  9, 10,  0,  0, 11,//17-24
           0,  0,  0,  0,  0,  0,  0,  0,  0,//25-32
           0,  0, 14,  0,  0,  0, 15,  0,  0,//33-40
           0,  0, 16,  0,  0,  0, 17,  0,  0,//41-48
           0,  0,  0,  0,  0,  0,  0,  0,  0,//49-56
          18,  0,  0, 19, 20, 23,  0,  0, 24,//57-64
           0,  0,  0,  0, 25,  0,  0,  0,  0,//65-72
           0,  0, 26, 28,  0, 30, 32,  0,  0,//73-80
          // clang-format on
        }; //为什么不是连续的？缺少了3,4...12，13 21 22
        std::array<std::array<std::array<std::array<std::uint8_t, SQUARE_NB>, 2>, 2>, 2> v{};
        for (uint8_t m = 0; m < 2; ++m) // 是否水平翻转
            for (uint8_t r = 0; r < 2; ++r) // 是否垂直翻转
                for (uint8_t ab = 0; ab < 2; ++ab)// 是否应用abMap 这个开关能给仕和相的位置提取出来，更紧凑
                    for (uint8_t s = 0; s < SQUARE_NB; ++s) // 遍历所有棋盘格子
                    {
                        uint8_t ss     = s;
                        ss             = m ? uint8_t(flip_file(Square(ss))) : ss;
                        ss             = r ? uint8_t(flip_rank(Square(ss))) : ss;
                        ss             = ab ? ABMap[ss] : ss;
                        v[m][r][ab][s] = ss;
                    } // 存的都是映射后的方格序号
        return v;
    }();

    // LayerStack buckets 层堆叠桶 
    static constexpr auto LayerStackBuckets = [] {
        std::array<std::array<std::array<std::array<uint8_t, 5>, 5>, 3>, 3> v{};
        for (uint8_t us_rook = 0; us_rook <= 2; ++us_rook) // 己方车数量0-2
            for (uint8_t opp_rook = 0; opp_rook <= 2; ++opp_rook) // 对方车数量0-2
                for (uint8_t us_knight_cannon = 0; us_knight_cannon <= 4; ++us_knight_cannon) // 己方马炮数量0-4
                    for (uint8_t opp_knight_cannon = 0; opp_knight_cannon <= 4; ++opp_knight_cannon) // 对方马炮数量0-4
                        v[us_rook][opp_rook][us_knight_cannon][opp_knight_cannon] = [&] {
                            if (us_rook == opp_rook) // 己方车数量和对方车数量相同
                                return us_rook * 4
                                     + int(us_knight_cannon + opp_knight_cannon >= 4) * 2
                                     + int(us_knight_cannon == opp_knight_cannon);
                            else if (us_rook == 2 && opp_rook == 1) // 己方车数量2，对方车数量1
                                return 12;
                            else if (us_rook == 1 && opp_rook == 2) // 己方车数量1，对方车数量2
                                return 13;
                            else if (us_rook > 0 && opp_rook == 0) // 己方车数量大于0，对方车数量0
                                return 14;
                            else  // us_rook == 0 && opp_rook > 0
                                return 15;
                        }();
        return v;
    }();

    /* 64 bit encoding related to mid mirror, which is devided into two parts, pieces counts and squares except for king
    *
    *  Encoding representations:
    *    Middle king   : | 1 bit | -> Active when king in FILE_E
    *    Piece types   : |advisor|bishop| pawn |knight|cannon| rook |
    *    Piece counts  : | 3 bits|3 bits|4 bits|3 bits|3 bits|3 bits| -> 19 bits
    *    Guarding bit  : | 1 bits| -> Set to one to prevent overflow from piece squares part into piece counts part
    *    Piece squares : | 7 bits|7 bits|8 bits|7 bits|7 bits|7 bits| -> 43 bits
    *
    *  Piece counts for the left flank (i.e. FILE_A to FILE_D) of the board are 1, for the right flank (i.e. FILE_F to
    *  FILE_I) are -1, mirroring that of the left flank but with a negative sign, and for the center is 0.
    *
    *  Piece squares for the left flank of the board are positive, counting from 0 to 39, for the right flank are
    *  negative, counting from -39 to 0, mirroring that of the left flank but with a negative sign, and for the center
    *  is 0. For black pieces, the encoding is vertically flipped.
    *
    *  Encoding for piece at the left flank of the board is positive, for example, encoding for a single piece 'rook'
    *  at square 'A3' (assuming A3 is 33) is:
    *    Middle king   : | 0|
    *    Piece counts  : | 0| 0| 0| 0| 0| 1|
    *    Guarding bits : | 0|
    *    Piece squares : | 0| 0| 0| 0| 0|33|
    *
    *  Encoding for piece at the right flank of the board is getting negated, which means a negative sign is added to
    *  the final encoding of that piece, for example, the encoding of the same piece on square 'I3' (assuming I3 is -33)
    *  is:
    *    -(Middle king   : | 0|
    *      Piece counts  : | 0| 0| 0| 0| 0| 1|
    *      Guarding bits : | 0|
    *      Piece squares : | 0| 0| 0| 0| 0|33|)
    *
    *  Encoding for piece at FILE_E of the board is all zero, for example, the encoding of the some piece on square 'E3'
    *  is:
    *    Middle king   : | 0| (| 1| if the piece is king)
    *    Piece counts  : | 0| 0| 0| 0| 0| 0|
    *    Guarding bits : | 0|
    *    Piece squares : | 0| 0| 0| 0| 0| 0|
    *
    *  The overall encoding of a balance position is (with the concept of complement numbers being used):
    *    Middle king   : | 1|
    *    Piece counts  : | 2| 2| 5| 2| 2| 2|
    *    Guarding bits : | 1|
    *    Piece squares : |39|39|76|39|39|39|
    *  Where 39 can account for subtraction of 0 - 39 for non-pawn pieces and 76 can account for subtraction of two pawns
    *  (0 + 1) - (38 + 39) at most in a balanced position.
    *
    *  Each piece placed will add its encoding to the overall representation, and removing a piece will do a subtraction.
    *
    *  Now we just need to test if the encoding of the board is smaller than balance position to see if we need mirroring:
    *    If the piece count is imbalance for both flanks, the result will be fully decided by the first part of the
    *    encoding, regardless of the second part and overflows.
    *    If the piece count is balance, the first part of the encoding must be the same, and the result will be decided
    *    by the second part, and all overflows of the second part will be automatically resolved at this time.
    */
    /* 与“中线镜像”(mid mirror) 相关的 64 位编码，分为两大部分：  
    *   ① 各类棋子“数量”   (piece counts)  
    *   ② 各类棋子“格子序号”(piece squares，国王除外)
    *
    *  编码格式：
    *    中线国王标志 : | 1 bit |  当己方王位于 E 列时为 1
    *    棋子类型顺序 : |  士  |  象  |  兵  |  马  |  炮  |  车  |
    *    棋子数量位数 : | 3位 | 3位 | 4位 | 3位 | 3位 | 3位 |  共 19 位
    *    护栏位(guard) : | 1 bit |  防止后半部分进位影响前半部分
    *    棋子格子位数 : | 7位 | 7位 | 8位 | 7位 | 7位 | 7位 |  共 43 位
    *
    *  棋子数量 (piece counts) 的编码规则
    *    ─ 左翼(FILE_A ~ FILE_D)：记为 +1
    *    ─ 右翼(FILE_F ~ FILE_I)：记为 -1（即左翼数量取负）
    *    ─ 中线(FILE_E)         ：记为  0
    *
    *  棋子格子 (piece squares) 的编码规则
    *    ─ 左翼格子：取 0 ~ 39 的正数
    *    ─ 右翼格子：取 -39 ~ 0 的负数（为左翼的镜像再取相反数）
    *    ─ 中线格子：取 0
    *    ─ 对黑方棋子，先做垂直翻转（rank 互换）后再按上面规则编码
    *
    *  例：单个“车”位于左翼 A3（假设 A3 的序号为 33）时的编码：
    *    中线国王   : | 0 |
    *    棋子数量   : | 0|0|0|0|0|1 |
    *    护栏位     : | 0 |
    *    棋子格子   : | 0|0|0|0|0|33 |
    *
    *  同一“车”若在右翼 I3（序号视作 -33）：
    *    = 取上述编码并整体加上负号，即所有字段取相反数
    *
    *  若棋子在中线 FILE_E，例如 E3：
    *    中线国王   : | 0 | （如果是王则为 1）
    *    棋子数量   : 全 0
    *    护栏位     : | 0 |
    *    棋子格子   : 全 0
    *
    *  “平衡局面”(balance position) 的整局编码（补码思想）预设为：
    *    中线国王   : | 1 |
    *    棋子数量   : | 2|2|5|2|2|2 |
    *    护栏位     : | 1 |
    *    棋子格子   : |39|39|76|39|39|39 |
    *    解释：39 可以覆盖非兵子从 0 减到 39 的差值；76 则能覆盖最多两兵的差值 ((0+1) − (38+39))
    *
    *  放置一枚棋子 → 将该棋子的编码加到整体表示；  
    *  移除一枚棋子 → 将该棋子的编码从整体表示中减去。
    *
    *  判断是否需要“中线镜像”只需比较当前编码与“平衡局面”：
    *    · 若左右两翼的棋子数量本就不平衡，只看前半部分（数量域）即可，无需考虑后半部分或进位溢出；  
    *    · 若数量平衡，则前半部分相同，此时比较后半部分即可，多余进位会被自动抵消。
    */
    static constexpr auto MidMirrorEncoding = [] {
        std::array<std::array<uint64_t, static_cast<size_t>(SQUARE_NB)>,
                   static_cast<size_t>(PIECE_NB)>
                          encodings{};
        constexpr uint8_t shifts[8][2]{{0, 0},   {44, 0},  {60, 36}, {47, 7},
                                       {53, 21}, {50, 14}, {57, 29}, {0, 0}}; // 8种棋子，2个参数，分别是s1数量偏移，s2格子偏移
        for (const auto& c : {WHITE, BLACK})
            for (uint8_t pt = ROOK; pt <= KING; ++pt)
                for (uint8_t r = RANK_0; r < RANK_NB; ++r)
                    for (uint8_t f = FILE_A; f < FILE_NB; ++f)
                    {
                        uint64_t encoding = 0;
                        if (f != FILE_E && pt != KING)
                        {
                            uint8_t r_           = c == WHITE ? r : RANK_9 - r;
                            uint8_t f_           = f < FILE_E ? f : FILE_I - f;
                            const auto& [s1, s2] = shifts[pt];
                            encoding             = (1ULL << s1)
                                     | ((uint64_t(File::FILE_D - f_) * 10 + uint64_t(r_)) << s2);//0-39的编码方式
                            encoding = f < FILE_E ? encoding : uint64_t(-int64_t(encoding));
                        }
                        else if (f != FILE_E && pt == KING)
                            encoding = 1ULL << 63;
                        uint8_t p        = static_cast<uint8_t>(make_piece(c, PieceType(pt)));
                        uint8_t sq       = static_cast<uint8_t>(make_square(File(f), Rank(r)));
                        encodings[p][sq] = encoding;
                    }// 按照上面注释的方式编码，并存起来（似乎没有累加）
        return encodings;
    }();

    static constexpr uint64_t BalanceEncoding{0xa4a92a74e989d3a7};
    // 1 010 010 0101 010 010 010 1 0100111 0100111 01001100 0100111 0100111 0100111


    // Maximum number of simultaneously active features. 最大同时激活的特征数量
    static constexpr IndexType MaxActiveDimensions = 32;
    using IndexList                                = ValueList<IndexType, MaxActiveDimensions>;

    // Returns whether the middle mirror is required. 是否需要中间镜像
    static bool requires_mid_mirror(const Position& pos, Color c);

    // Get attack bucket 获取攻击桶
    static IndexType make_attack_bucket(const Position& pos, Color c);

    // Get layer stack bucket 获取层堆叠桶
    static IndexType make_layer_stack_bucket(const Position& pos);

    // Index of a feature for a given king position and another piece on some square 给定国王位置和某个棋子在某个方格的特征索引
    template<Color Perspective>
    static IndexType make_index(Square s, Piece pc, int bucket, bool mirror);

    // Get a list of indices for recently changed features 获取最近更改的特征列表
    template<Color Perspective>
    static void append_changed_indices(
      int bucket, bool mirror, const DirtyPiece& dp, IndexList& removed, IndexList& added);

    // Returns whether the change stored in this DirtyPiece means
    // that a full accumulator refresh is required. 返回是否需要刷新累加器
    static bool requires_refresh(const DirtyPiece& dirtyPiece, Color perspective);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED
