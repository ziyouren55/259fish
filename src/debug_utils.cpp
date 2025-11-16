// debug_utils.cpp
#include "debug_utils.h"
#include <iostream>
#include <iomanip>

namespace Stockfish {

// 所有有效的棋子类型（排除 NO_PIECE）
namespace {
constexpr Piece AllPieces[] = {W_ELEPHANT, W_LION, W_TIGER, W_PANTHER, W_WOLF, W_DOG, W_CAT, W_RAT,
                               B_ELEPHANT, B_LION, B_TIGER, B_PANTHER, B_WOLF, B_DOG, B_CAT, B_RAT};
constexpr int   AllPiecesCount = sizeof(AllPieces) / sizeof(AllPieces[0]);

// 辅助函数：根据运行时的 PieceType 调用模板 count 函数
inline int count_by_piece_type(const Position& pos, PieceType pt, Color c) {
    switch (pt)
    {
    case ELEPHANT :
        return pos.count<ELEPHANT>(c);
    case LION :
        return pos.count<LION>(c);
    case TIGER :
        return pos.count<TIGER>(c);
    case PANTHER :
        return pos.count<PANTHER>(c);
    case WOLF :
        return pos.count<WOLF>(c);
    case DOG :
        return pos.count<DOG>(c);
    case CAT :
        return pos.count<CAT>(c);
    case RAT :
        return pos.count<RAT>(c);
    default :
        return 0;
    }
}
}

// PieceDebugInfo 实现
std::string PieceDebugInfo::to_string() const {
    std::ostringstream oss;
    const char*        pieceNames[] = {"NO_PIECE", "ELEPHANT", "LION", "TIGER", "PANTHER",
                                       "WOLF",     "DOG",      "CAT",  "RAT"};
    const char*        colorNames[] = {"WHITE", "BLACK"};

    oss << colorNames[color] << "_" << pieceNames[pieceType] << " (count=" << count
        << ", alive=" << (isAlive ? "YES" : "NO");
    if (isAlive && square != SQ_NONE)
    {
        File f = file_of(square);
        Rank r = rank_of(square);
        oss << ", square=" << char('A' + f) << int(r);
    }
    oss << ")";
    return oss.str();
}

// PositionDebugSnapshot 实现
std::string PositionDebugSnapshot::to_full_report() const {
    std::ostringstream oss;
    oss << "========== 局面调试信息 ==========\n";
    oss << "当前走棋方: " << (sideToMove == WHITE ? "WHITE" : "BLACK") << "\n";
    oss << "步数: " << gamePly << "\n";
    oss << "哈希键: 0x" << std::hex << positionKey << std::dec << "\n\n";

    oss << "--- 棋子统计 ---\n";
    oss << "白方: " << whitePieceCount << " 个棋子, 子力价值: " << whiteMaterial << "\n";
    oss << "黑方: " << blackPieceCount << " 个棋子, 子力价值: " << blackMaterial << "\n\n";

    oss << "--- 存活棋子 ---\n";
    for (int i = 0; i < AllPiecesCount; ++i)
    {
        Piece pc = AllPieces[i];
        if (pieces[pc].isAlive)
        {
            oss << "  " << pieces[pc].to_string() << "\n";
        }
    }

    oss << "\n--- 死亡棋子 ---\n";
    for (int i = 0; i < AllPiecesCount; ++i)
    {
        Piece pc = AllPieces[i];
        if (!pieces[pc].isAlive && pieces[pc].count == 0)
        {
            oss << "  " << pieces[pc].to_string() << "\n";
        }
    }

    oss << "\n--- 棋盘文本表示 ---\n";
    oss << boardText << "\n";

    oss << "\n--- 位棋盘信息 ---\n";
    oss << "所有棋子:\n" << allPiecesBB << "\n";
    oss << "白方棋子:\n" << whitePiecesBB << "\n";
    oss << "黑方棋子:\n" << blackPiecesBB << "\n";

    // 新增：按类型分类的位棋盘
    oss << "\n--- 按类型分类的位棋盘 ---\n";
    const char* typeNames[] = {"NO_PIECE", "ELEPHANT", "LION", "TIGER", "PANTHER",
                               "WOLF",     "DOG",      "CAT",  "RAT"};
    for (PieceType pt = NO_PIECE_TYPE; pt < PIECE_TYPE_NB; ++pt)
    {
        if (!byTypeBBString[pt].empty())
        {
            oss << typeNames[pt] << ":\n" << byTypeBBString[pt] << "\n";
        }
    }

    // 新增：按颜色分类的位棋盘
    oss << "\n--- 按颜色分类的位棋盘 ---\n";
    oss << "WHITE:\n" << byColorBBString[WHITE] << "\n";
    oss << "BLACK:\n" << byColorBBString[BLACK] << "\n";

    // 新增：只显示棋子的位置
    oss << "\n--- 棋子位置列表（省去空格子） ---\n";
    oss << boardPiecesOnly << "\n";

    return oss.str();
}

std::string PositionDebugSnapshot::to_compact_report() const {
    std::ostringstream oss;
    oss << "Ply=" << gamePly << " " << (sideToMove == WHITE ? "W" : "B") << " to move | "
        << "W:" << whitePieceCount << "/" << whiteMaterial << " B:" << blackPieceCount << "/"
        << blackMaterial;
    return oss.str();
}

// DebugUtils 实现
PositionDebugSnapshot DebugUtils::create_snapshot(const Position& pos) {
    PositionDebugSnapshot snapshot;

    // 基本信息
    snapshot.sideToMove  = pos.side_to_move();
    snapshot.gamePly     = pos.game_ply();
    snapshot.positionKey = pos.key();

    // 初始化所有棋子信息（包括 NO_PIECE）
    snapshot.pieces[NO_PIECE].piece     = NO_PIECE;
    snapshot.pieces[NO_PIECE].pieceType = NO_PIECE_TYPE;
    snapshot.pieces[NO_PIECE].color     = WHITE;  // NO_PIECE 没有颜色，这里用 WHITE 作为默认值
    snapshot.pieces[NO_PIECE].count     = 0;
    snapshot.pieces[NO_PIECE].isAlive   = false;
    snapshot.pieces[NO_PIECE].square    = SQ_NONE;

    // 初始化所有有效棋子信息
    for (int i = 0; i < AllPiecesCount; ++i)
    {
        Piece pc                      = AllPieces[i];
        snapshot.pieces[pc].piece     = pc;
        snapshot.pieces[pc].pieceType = type_of(pc);
        snapshot.pieces[pc].color     = color_of(pc);
        snapshot.pieces[pc].count     = count_by_piece_type(pos, type_of(pc), color_of(pc));
        snapshot.pieces[pc].isAlive   = (snapshot.pieces[pc].count > 0);
        snapshot.pieces[pc].square    = SQ_NONE;
    }

    // 查找每个存活棋子的位置
    for (Square s = SQ_A0; s < SQUARE_NB; ++s)
    {
        Piece pc = pos.piece_on(s);
        if (pc != NO_PIECE)
        {
            snapshot.pieces[pc].square = s;
        }
    }

    // 位棋盘文本表示
    snapshot.allPiecesBB   = bitboard_to_string(pos.pieces());
    snapshot.whitePiecesBB = bitboard_to_string(pos.pieces(WHITE));
    snapshot.blackPiecesBB = bitboard_to_string(pos.pieces(BLACK));

    for (PieceType pt = ELEPHANT; pt < PIECE_TYPE_NB; ++pt)
    {
        snapshot.byTypeBB[pt] = bitboard_to_string(pos.pieces(pt));
    }

    // 新增：按类型分类的位棋盘可视化字符串（从 Position 的 byTypeBB 数组获取）
    for (PieceType pt = NO_PIECE_TYPE; pt < PIECE_TYPE_NB; ++pt)
    {
        snapshot.byTypeBBString[pt] = bitboard_to_string(pos.pieces(pt));
    }

    // 新增：按颜色分类的位棋盘可视化字符串（从 Position 的 byColorBB 数组获取）
    for (Color c = WHITE; c <= BLACK; ++c)
    {
        snapshot.byColorBBString[c] = bitboard_to_string(pos.pieces(c));
    }

    // 棋盘文本表示
    snapshot.boardText = board_to_string(pos);

    // 新增：只显示棋子的位置（省去空格子）
    std::ostringstream piecesOss;
    const char*        pieceNames[] = {"NO_PIECE", "ELEPHANT", "LION", "TIGER", "PANTHER",
                                       "WOLF",     "DOG",      "CAT",  "RAT"};
    const char*        colorNames[] = {"WHITE", "BLACK"};
    bool               first        = true;

    for (Square s = SQ_A0; s < SQUARE_NB; ++s)
    {
        Piece pc = pos.piece_on(s);
        if (pc != NO_PIECE)
        {
            if (!first)
            {
                piecesOss << "\n";
            }
            File f = file_of(s);
            Rank r = rank_of(s);
            piecesOss << colorNames[color_of(pc)] << "_" << pieceNames[type_of(pc)] << "@"
                      << char('A' + f) << int(r);
            first = false;
        }
    }
    snapshot.boardPiecesOnly = piecesOss.str();

    // 统计信息
    snapshot.whitePieceCount = popcount(pos.pieces(WHITE));
    snapshot.blackPieceCount = popcount(pos.pieces(BLACK));
    snapshot.whiteMaterial   = pos.major_material(WHITE);
    snapshot.blackMaterial   = pos.major_material(BLACK);

    return snapshot;
}

std::string DebugUtils::bitboard_to_string(Bitboard bb) {
    std::ostringstream oss;
    oss << "  A B C D E F G\n";
    for (Rank r = RANK_8; r >= RANK_0; --r)
    {
        oss << int(r) << " ";  // 必须使用 int() 转换
        for (File f = FILE_A; f < FILE_NB; ++f)
        {
            Square s = make_square(f, r);
            if (bb & s)
            {
                oss << "1 ";
            }
            else
            {
                oss << ". ";
            }
        }
        oss << "\n";
    }
    return oss.str();
}

std::string DebugUtils::board_to_string(const Position& pos) {
    std::ostringstream oss;

    oss << "  A B C D E F G\n";
    for (Rank r = RANK_8; r >= RANK_0; --r)
    {
        oss << int(r) << " ";  // 必须使用 int() 转换
        for (File f = FILE_A; f < FILE_NB; ++f)
        {
            Square s  = make_square(f, r);
            Piece  pc = pos.piece_on(s);
            if (pc == NO_PIECE)
            {
                if(DenBB[WHITE] & s || s & DenBB[BLACK]) oss << "_ ";
                else if(s & TrapBB[WHITE] || s & TrapBB[BLACK]) oss << "* ";
                else if(s & RiverBB) oss << "~ ";
                else oss << ". ";
            }
            else
            {
                int idx = int(pc);
                if (idx < 25 && PieceToChar[idx] != ' ')
                {
                    oss << PieceToChar[idx] << " ";
                }
                else
                {
                    oss << "? ";
                }
            }
        }
        oss << "\n";
    }
    return oss.str();
}

void DebugUtils::print_snapshot(const PositionDebugSnapshot& snapshot) {
    std::cout << snapshot.to_full_report() << std::endl;
}

void DebugUtils::print_position(const Position& pos) {
    PositionDebugSnapshot snapshot = create_snapshot(pos);
    print_snapshot(snapshot);
}

bool DebugUtils::verify_position(const Position& pos, std::string& errorMsg) {
    // 首先使用内置的检查
    if (!pos.pos_is_ok())
    {
        errorMsg = "Position::pos_is_ok() 返回 false";
        return false;
    }

    // 额外的验证：检查pieceCount是否与位棋盘一致
    for (int i = 0; i < AllPiecesCount; ++i)
    {
        Piece    pc      = AllPieces[i];
        int      count   = count_by_piece_type(pos, type_of(pc), color_of(pc));
        Bitboard bb      = pos.pieces(color_of(pc), type_of(pc));
        int      bbCount = popcount(bb);

        if (count != bbCount)
        {
            std::ostringstream oss;
            oss << "棋子 " << int(pc) << " 的计数不一致: pieceCount=" << count
                << ", bitboardCount=" << bbCount;
            errorMsg = oss.str();
            return false;
        }
    }

    return true;
}

std::vector<PieceDebugInfo> DebugUtils::get_alive_pieces(const Position& pos, Color c) {
    std::vector<PieceDebugInfo> result;
    PositionDebugSnapshot       snapshot = create_snapshot(pos);

    for (int i = 0; i < AllPiecesCount; ++i)
    {
        Piece pc = AllPieces[i];
        if (color_of(pc) == c && snapshot.pieces[pc].isAlive)
        {
            result.push_back(snapshot.pieces[pc]);
        }
    }
    return result;
}

std::vector<PieceDebugInfo> DebugUtils::get_dead_pieces(const Position& pos, Color c) {
    std::vector<PieceDebugInfo> result;
    PositionDebugSnapshot       snapshot = create_snapshot(pos);

    for (int i = 0; i < AllPiecesCount; ++i)
    {
        Piece pc = AllPieces[i];
        if (color_of(pc) == c && !snapshot.pieces[pc].isAlive)
        {
            result.push_back(snapshot.pieces[pc]);
        }
    }
    return result;
}

std::string DebugUtils::compare_positions(const Position& pos1, const Position& pos2) {
    std::ostringstream    oss;
    PositionDebugSnapshot snap1 = create_snapshot(pos1);
    PositionDebugSnapshot snap2 = create_snapshot(pos2);

    oss << "========== 局面对比 ==========\n";

    // 比较基本信息
    if (snap1.sideToMove != snap2.sideToMove)
    {
        oss << "走棋方不同: " << snap1.gamePly << " vs " << snap2.gamePly << "\n";
    }

    // 比较棋子位置
    oss << "\n棋子位置差异:\n";
    for (int i = 0; i < AllPiecesCount; ++i)
    {
        Piece pc = AllPieces[i];
        if (snap1.pieces[pc].square != snap2.pieces[pc].square)
        {
            oss << "  " << snap1.pieces[pc].to_string() << " -> " << snap2.pieces[pc].to_string()
                << "\n";
        }
    }

    return oss.str();
}

// ========== Move 相关调试方法实现 ==========

std::string DebugUtils::square_to_string(Square s) {
    if (s == SQ_NONE)
    {
        return "NONE";
    }
    File               f = file_of(s);
    Rank               r = rank_of(s);
    std::ostringstream oss;
    oss << char('A' + f) << int(r);
    return oss.str();
}

MoveDebugInfo DebugUtils::create_move_info(Move m) {
    MoveDebugInfo info;
    info.move    = m;
    info.rawData = m.raw();

    if (m == Move::none())
    {
        info.isNone        = true;
        info.isNull        = false;
        info.isValid       = false;
        info.fromSquare    = SQ_NONE;
        info.toSquare      = SQ_NONE;
        info.fromString    = "NONE";
        info.toString      = "NONE";
        info.moveString    = "(none)";
        info.compactString = "(none)";
    }
    else if (m == Move::null())
    {
        info.isNone        = false;
        info.isNull        = true;
        info.isValid       = false;
        info.fromSquare    = SQ_NONE;
        info.toSquare      = SQ_NONE;
        info.fromString    = "NULL";
        info.toString      = "NULL";
        info.moveString    = "(null)";
        info.compactString = "0000";
    }
    else
    {
        info.isNone        = false;
        info.isNull        = false;
        info.isValid       = m.is_ok();
        info.fromSquare    = m.from_sq();
        info.toSquare      = m.to_sq();
        info.fromString    = square_to_string(info.fromSquare);
        info.toString      = square_to_string(info.toSquare);
        info.moveString    = info.fromString + "->" + info.toString;
        info.compactString = info.fromString + info.toString;
    }

    return info;
}

std::string DebugUtils::move_to_string(Move m) {
    MoveDebugInfo info = create_move_info(m);
    return info.moveString;
}

std::string DebugUtils::move_to_compact_string(Move m) {
    MoveDebugInfo info = create_move_info(m);
    return info.compactString;
}

std::string DebugUtils::move_to_string_with_piece(Move m, const Position& pos) {
    if (m == Move::none() || m == Move::null())
    {
        return move_to_string(m);
    }

    Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece  pc   = pos.piece_on(from);

    std::ostringstream oss;

    // 棋子名称
    const char* pieceNames[] = {"NO_PIECE", "ELEPHANT", "LION", "TIGER", "PANTHER",
                                "WOLF",     "DOG",      "CAT",  "RAT"};
    const char* colorNames[] = {"WHITE", "BLACK"};

    if (pc != NO_PIECE)
    {
        oss << colorNames[color_of(pc)] << "_" << pieceNames[type_of(pc)] << " ";
    }

    // 移动表示
    oss << square_to_string(from) << "->" << square_to_string(to);

    // 如果是吃子
    Piece captured = pos.piece_on(to);
    if (captured != NO_PIECE)
    {
        oss << " (captures " << colorNames[color_of(captured)] << "_"
            << pieceNames[type_of(captured)] << ")";
    }

    return oss.str();
}

void DebugUtils::print_move(Move m) {
    MoveDebugInfo info = create_move_info(m);
    print_move_info(info);
}

void DebugUtils::print_move_info(const MoveDebugInfo& info) {
    std::cout << "========== Move 调试信息 ==========\n";
    std::cout << "原始数据: 0x" << std::hex << info.rawData << std::dec << " (" << info.rawData
              << ")\n";
    std::cout << "起始格子: " << info.fromString << " (Square=" << int(info.fromSquare) << ")\n";
    std::cout << "目标格子: " << info.toString << " (Square=" << int(info.toSquare) << ")\n";
    std::cout << "移动表示: " << info.moveString << "\n";
    std::cout << "紧凑格式: " << info.compactString << "\n";
    std::cout << "是否有效: " << (info.isValid ? "YES" : "NO") << "\n";
    std::cout << "是否Null: " << (info.isNull ? "YES" : "NO") << "\n";
    std::cout << "是否None: " << (info.isNone ? "YES" : "NO") << "\n";
    std::cout << std::endl;
}

// MoveDebugInfo 成员方法实现
std::string MoveDebugInfo::to_string() const { return moveString; }

std::string MoveDebugInfo::to_compact_string() const { return compactString; }

}  // namespace Stockfish