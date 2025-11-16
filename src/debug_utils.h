// debug_utils.h
#ifndef DEBUG_UTILS_H_INCLUDED
#define DEBUG_UTILS_H_INCLUDED

#include "position.h"
#include "bitboard.h"
#include <string>
#include <vector>
#include <sstream>

namespace Stockfish {

// 单个棋子的调试信息
struct PieceDebugInfo {
    Piece     piece;      // 棋子类型（如 W_ELEPHANT, B_LION）
    PieceType pieceType;  // 棋子类型（如 ELEPHANT, LION）
    Color     color;      // 颜色（WHITE/BLACK）
    Square    square;     // 当前位置（如果存活），否则为 SQ_NONE
    bool      isAlive;    // 是否存活
    int       count;      // 该类型棋子的数量（通常为0或1）

    // 转换为可读字符串
    std::string to_string() const;
};

// 整个局面的调试快照
struct PositionDebugSnapshot {
    // 基本信息
    Color sideToMove;   // 当前走棋方
    int   gamePly;      // 当前步数
    Key   positionKey;  // 局面哈希键

    // 所有棋子的信息（按棋子类型组织）
    PieceDebugInfo pieces[PIECE_NB];  // 每个可能的棋子类型的信息

    // 位棋盘信息（文本表示）
    std::string allPiecesBB;              // 所有棋子的位棋盘
    std::string whitePiecesBB;            // 白方棋子位棋盘
    std::string blackPiecesBB;            // 黑方棋子位棋盘
    std::string byTypeBB[PIECE_TYPE_NB];  // 按类型分类的位棋盘

    // 新增：按类型和按颜色的位棋盘可视化字符串
    std::string byTypeBBString[PIECE_TYPE_NB];  // 按类型分类的位棋盘可视化字符串
    std::string byColorBBString[COLOR_NB];      // 按颜色分类的位棋盘可视化字符串

    // 棋盘状态（7x9的文本表示）
    std::string boardText;  // 可读的棋盘文本表示

    // 新增：只显示棋子的位置（省去空格子）
    std::string boardPiecesOnly;  // 只显示有棋子的格子，格式：棋子@位置

    // 统计信息
    int   whitePieceCount;  // 白方棋子总数
    int   blackPieceCount;  // 黑方棋子总数
    Value whiteMaterial;    // 白方子力价值
    Value blackMaterial;    // 黑方子力价值

    // 转换为完整的调试报告
    std::string to_full_report() const;
    std::string to_compact_report() const;
};

// Move 的调试信息
struct MoveDebugInfo {
    Move          move;           // Move 对象
    std::uint16_t rawData;        // 原始数据
    Square        fromSquare;     // 起始格子
    Square        toSquare;       // 目标格子
    std::string   fromString;     // 起始格子的字符串表示（如 "A0"）
    std::string   toString;       // 目标格子的字符串表示（如 "B1"）
    std::string   moveString;     // 移动的字符串表示（如 "A0->B1"）
    std::string   compactString;  // 紧凑格式（如 "A0B1"）
    bool          isValid;        // 是否为有效移动
    bool          isNull;         // 是否为 null move
    bool          isNone;         // 是否为 none move

    // 转换为可读字符串
    std::string to_string() const;
    std::string to_compact_string() const;
};

// 调试工具类
class DebugUtils {
   public:
    // 从Position对象创建调试快照
    static PositionDebugSnapshot create_snapshot(const Position& pos);

    // 打印调试信息到控制台
    static void print_snapshot(const PositionDebugSnapshot& snapshot);
    static void print_position(const Position& pos);

    // 将位棋盘转换为可读字符串（7x9格式）
    static std::string bitboard_to_string(Bitboard bb);

    // 将整个棋盘转换为文本表示
    static std::string board_to_string(const Position& pos);

    // 检查局面一致性（增强版）
    static bool verify_position(const Position& pos, std::string& errorMsg);

    // 获取所有存活棋子的列表
    static std::vector<PieceDebugInfo> get_alive_pieces(const Position& pos, Color c);

    // 获取所有死亡棋子的列表
    static std::vector<PieceDebugInfo> get_dead_pieces(const Position& pos, Color c);

    // 比较两个局面的差异
    static std::string compare_positions(const Position& pos1, const Position& pos2);

    // ========== Move 相关调试方法 ==========

    // 将 Square 转换为字符串表示（如 "A0", "B1"）
    static std::string square_to_string(Square s);

    // 从 Move 创建调试信息
    static MoveDebugInfo create_move_info(Move m);

    // 将 Move 转换为字符串表示（如 "A0->B1"）
    static std::string move_to_string(Move m);

    // 将 Move 转换为紧凑字符串表示（如 "A0B1"）
    static std::string move_to_compact_string(Move m);

    // 将 Move 转换为带棋子信息的字符串（需要 Position 上下文）
    static std::string move_to_string_with_piece(Move m, const Position& pos);

    // 打印 Move 调试信息
    static void print_move(Move m);
    static void print_move_info(const MoveDebugInfo& info);
};

}  // namespace Stockfish

#endif  // DEBUG_UTILS_H_INCLUDED