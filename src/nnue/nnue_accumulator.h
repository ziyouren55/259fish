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

// Class for difference calculation of NNUE evaluation function

#ifndef NNUE_ACCUMULATOR_H_INCLUDED
#define NNUE_ACCUMULATOR_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../types.h"
#include "nnue_architecture.h"
#include "nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE {

template<IndexType Size>
struct alignas(CacheLineSize) Accumulator;

template<IndexType TransformedFeatureDimensions>
class FeatureTransformer;

// Class that holds the result of affine transformation of input features 类，用于存储输入特征的仿射变换结果
template<IndexType Size>
struct alignas(CacheLineSize) Accumulator {
    std::int16_t               accumulation[COLOR_NB][Size];//主要存放NNEU评估值，每个颜色一个数组，每个数组长度为Size
    std::int32_t               psqtAccumulation[COLOR_NB][PSQTBuckets];//主要存放位置价值表的累加值，每个颜色一个数组，每个数组长度为PSQTBuckets
    std::array<bool, COLOR_NB> computed;//主要存放每个颜色的计算状态，每个颜色一个bool值
};


// AccumulatorCaches struct provides per-thread accumulator caches, where each
// cache contains multiple entries for each of the possible king squares.
// When the accumulator needs to be refreshed, the cached entry is used to more
// efficiently update the accumulator, instead of rebuilding it from scratch.
// This idea, was first described by Luecx (author of Koivisto) and
// is commonly referred to as "Finny Tables".

// 累加器缓存，用于存储每个可能的国王方格的多个条目，当累加器需要刷新时，使用缓存的条目来更有效地更新累加器，而不是从头重建
// 这个想法最初由Luecx（Koivisto的作者）描述，通常被称为“Finny Tables”

struct AccumulatorCaches {

#if NNUE_COMPAT
    // clang-format off
    static constexpr uint8_t KingCacheMaps[SQUARE_NB] = {
        0,  0,  0,  1,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  1,  0,  0,  0,
        };
        // clang-format on
#else
    // clang-format off
    static constexpr uint8_t KingCacheMaps[SQUARE_NB] = {
      0,  0,  0,  6,  0,  3,  0,  0,  0,
      0,  0,  0,  7,  1,  4,  0,  0,  0,
      0,  0,  0,  8,  2,  5,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  8,  2,  5,  0,  0,  0,
      0,  0,  0,  7,  1,  4,  0,  0,  0,
      0,  0,  0,  6,  0,  3,  0,  0,  0,
    };
    // clang-format on
#endif
    //构造函数，用于初始化累加器缓存
    template<typename Networks>
    AccumulatorCaches(const Networks& networks) {
        clear(networks);
    }

    // 模板类，用于存储累加器缓存
    template<IndexType Size>
    struct alignas(CacheLineSize) Cache {

        // 结构体，用于存储累加器缓存条目
        struct alignas(CacheLineSize) Entry {
            BiasType       accumulation[Size]; //特征权重累加值
            PSQTWeightType psqtAccumulation[PSQTBuckets];//PSQT(棋子和位置)评估累加值
            Bitboard       byColorBB[COLOR_NB];//按颜色存储的棋子位棋盘
            Bitboard       byTypeBB[PIECE_TYPE_NB];//按棋子类型存储的位棋盘

            // To initialize a refresh entry, we set all its bitboards empty,
            // so we put the biases in the accumulation, without any weights on top
            // 为了初始化一个刷新条目，我们设置所有它的位图都是空的，所以我们把偏差放在累加器中，没有权重
            void clear(const BiasType* biases) {

                std::memcpy(accumulation, biases, sizeof(accumulation));//将偏差复制到累加器中
                std::memset((uint8_t*) this + offsetof(Entry, psqtAccumulation), 0,//将位置价值表的累加值设置为0
                            sizeof(Entry) - offsetof(Entry, psqtAccumulation));//将位置价值表的累加值设置为0
            }
        };

        template<typename Network>
        void clear(const Network& network) {
            for (auto& entries1D : entries)//遍历每个1D数组
                for (auto& entry : entries1D)//遍历每个1D数组中的每个条目
                    entry.clear(network.featureTransformer->biases);//将偏差复制到累加器中
        }

        std::array<Entry, COLOR_NB>& operator[](int index) { return entries[index]; }//返回第index个1D数组

        std::array<std::array<Entry, COLOR_NB>, (9 + 6) * 2 * 3> entries;//遍历所有可能的国王方格，每个方格一个数组，每个数组长度为COLOR_NB，每个条目长度为Entry
    };

    template<typename Networks>
    void clear(const Networks& networks) {
        big.clear(networks.big);
    }//清除累加器缓存，调用big的clear函数

    Cache<TransformedFeatureDimensionsBig> big;//大缓存，每个方格一个数组，每个数组长度为COLOR_NB，每个条目长度为Entry
};


struct AccumulatorState {
    Accumulator<TransformedFeatureDimensionsBig> accumulatorBig;//大累加器，每个方格一个数组，每个数组长度为COLOR_NB，每个条目长度为Entry
    DirtyPiece                                   dirtyPiece;//脏棋子，每个棋子一个bool值

    template<IndexType Size>
    auto& acc() noexcept {
        static_assert(Size == TransformedFeatureDimensionsBig, "Invalid size for accumulator");//断言，如果Size不等于TransformedFeatureDimensionsBig，则报错

        if constexpr (Size == TransformedFeatureDimensionsBig)//如果Size等于TransformedFeatureDimensionsBig，则返回accumulatorBig
            return accumulatorBig;
    }

    template<IndexType Size>
    const auto& acc() const noexcept {
        static_assert(Size == TransformedFeatureDimensionsBig, "Invalid size for accumulator");//断言，如果Size不等于TransformedFeatureDimensionsBig，则报错

        if constexpr (Size == TransformedFeatureDimensionsBig)//如果Size等于TransformedFeatureDimensionsBig，则返回accumulatorBig
            return accumulatorBig;
    }

    void reset(const DirtyPiece& dp) noexcept;//重置累加器状态
};


class AccumulatorStack {
   public:
    AccumulatorStack() ://构造函数，用于初始化累加器栈
        accumulators(MAX_PLY + 1),//主要存放累加器状态，每个状态一个AccumulatorState，必须要有一个初始界面，所有要加一
        size{1} {}//主要存放累加器栈的大小，初始化为1，则说明有效累加器数量为1（类似索引）

    [[nodiscard]] const AccumulatorState& latest() const noexcept;//返回最新的累加器状态（不可修改返回值，因为返回值是const类型）

    void reset() noexcept;//重置累加器栈
    void push(const DirtyPiece& dirtyPiece) noexcept;//将脏棋子推入累加器栈
    void pop() noexcept;//将累加器栈的最新状态弹出

    template<IndexType Dimensions>
    void evaluate(const Position&                       pos,
                  const FeatureTransformer<Dimensions>& featureTransformer,
                  AccumulatorCaches::Cache<Dimensions>& cache) noexcept;

   private:
    [[nodiscard]] AccumulatorState& mut_latest() noexcept;//返回最新的累加器状态（可修改返回值，因为返回值是AccumulatorState&类型）

    template<Color Perspective, IndexType Dimensions>
    void evaluate_side(const Position&                       pos,
                       const FeatureTransformer<Dimensions>& featureTransformer,
                       AccumulatorCaches::Cache<Dimensions>& cache) noexcept;

    template<Color Perspective, IndexType Dimensions>
    [[nodiscard]] std::size_t find_last_usable_accumulator() const noexcept;

    template<Color Perspective, IndexType Dimensions>
    void forward_update_incremental(const Position&                       pos,
                                    const FeatureTransformer<Dimensions>& featureTransformer,
                                    const std::size_t                     begin) noexcept;

    template<Color Perspective, IndexType Dimensions>
    void backward_update_incremental(const Position&                       pos,
                                     const FeatureTransformer<Dimensions>& featureTransformer,
                                     const std::size_t                     end) noexcept;

    std::vector<AccumulatorState> accumulators;
    std::size_t                   size;
};

}  // namespace Stockfish::Eval::NNUE

#endif  // NNUE_ACCUMULATOR_H_INCLUDED
