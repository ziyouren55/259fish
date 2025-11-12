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

#include "position.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

#include "bitboard.h"
#include "misc.h"
#include "movegen.h"
#include "tt.h"
#include "uci.h"

using std::string;

namespace Stockfish {

namespace Zobrist {

Key psq[PIECE_NB][SQUARE_NB];
Key side, noPawns;
} //保存所有哈希随机数，包括各格子-各棋子的 psq、行棋方 side、以及“无兵”位 noPawns。

namespace {

    constexpr std::string_view PieceToChar(" ELTPWDCR eltpwdcr");

    static constexpr Piece Pieces[] = {W_ELEPHANT, W_LION, W_TIGER,    W_PANTHER, W_WOLF,  W_DOG,
                                       W_CAT,      W_RAT,  B_ELEPHANT, B_LION,    B_TIGER, B_PANTHER,
                                       B_WOLF,     B_DOG,  B_CAT,      B_RAT};
    }  // namespace

// Returns an ASCII representation of the position 把棋盘打印成ASCII码
std::ostream& operator<<(std::ostream& os, const Position& pos) {

    os << "\n +---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8; r >= RANK_0; --r)
    {
        for (File f = FILE_A; f <= FILE_G; ++f)
            os << " | " << PieceToChar[pos.piece_on(make_square(f, r))];

        os << " | " << int(r) << "\n +---+---+---+---+---+---+---+\n";
    }

    os << "   a   b   c   d   e   f   g\n"
       << "\nFen: " << pos.fen() << "\nKey: " << std::hex << std::uppercase << std::setfill('0')
       << std::setw(16) << pos.key() << std::setfill(' ') << std::dec << "\n";

    return os;
}


// Initializes at startup the various arrays used to compute hash keys 在启动时初始化用于计算哈希键的各个数组
void Position::init() {

    PRNG rng(1070372);

    for (Piece pc : Pieces)
        for (Square s = SQ_A0; s <= SQ_G8; ++s)
            Zobrist::psq[pc][s] = rng.rand<Key>();

    Zobrist::side    = rng.rand<Key>();
    Zobrist::noPawns = rng.rand<Key>();
}


// Initializes the position object with the given FEN string.
// This function is not very robust - make sure that input FENs are correct,
// this is assumed to be the responsibility of the GUI.
// 用给定的FEN字符串初始化位置对象。
// 这个函数不是很健壮——确保输入的FEN是正确的，
// 假设这是GUI的责任。
Position& Position::set(const string& fenStr, StateInfo* si) {
    /*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 9 and ending with rank 0. Within each rank, the contents of each
      square are described from file A through file I. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("RACPNBK") whilst Black uses lowercase ("racpnbk"). Blank squares are
      noted using digits 1 through 9 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   4) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/

/* 
    整个 FEN 只用 ASCII 字符，字段之间用空格分隔，一共 6 个字段。
    第 1 个字段：棋子布局
                先写 9 路 10 行中的第 9 行（黑方底线），依次到第 0 行（红方底线）。
                每行从 A 文件到 I 文件（也就是“左”到“右”）描述格子内容。
                棋子用字母表示：
                    红（白）方大写 - “R A C P N B K”
                    黑方小写 - “r a c p n b k”
                (R=车、A=仕、C=炮、P=兵、N=马、B=相、K=将)
                    连续空格用数字 1-9 表示有多少个空格。
                    不同行之间用 “/” 分隔。
    第 2 个字段：轮到谁走
                “w” 表示轮到红（白）方走；“b” 表示黑方走。
    第 3 个字段：50 步规则用的半回合计数（象棋用到是 Rule-60，但思路类似）。
                自上一次“兵走或吃子”以来已经过了多少步，用来判断是否能提出和棋。
    第 4 个字段：全回合计数
                从 1 开始计，每下完黑方再 +1。 
*/
    unsigned char      token;
    size_t             idx;
    Square             sq = SQ_A8;
    std::istringstream ss(fenStr);

    std::memset(this, 0, sizeof(Position));

    #if ENABLE_NNUE
    midEncoding[WHITE] = midEncoding[BLACK] = Eval::NNUE::Features::HalfKAv2_hm::BalanceEncoding;
    #else
    midEncoding[WHITE] = midEncoding[BLACK] = 0;
    #endif
    st = si;

    ss >> std::noskipws;

    // 1. Piece placement
    while ((ss >> token) && !isspace(token))
    {
        if (isdigit(token))
            sq += (token - '0') * EAST;  // Advance the given number of files

        else if (token == '/')
            sq += 2 * SOUTH;

        else if ((idx = PieceToChar.find(token)) != string::npos)
        {
            put_piece(Piece(idx), sq);
            ++sq;
        }
    }

    // 2. Active color

    ss >> token;
    sideToMove = (token == 'w' ? WHITE : BLACK);
    ss >> token;

    //跳过中间没用到的字段
    /*  
    棋子布局
    轮到谁走
    王车易位信息
    过路兵信息
    半回合计数
    全回合计数 
    */
    while ((ss >> token) && !isspace(token))
        ;

    while ((ss >> token) && !isspace(token))
        ;

    // 3-4. Halfmove clock and fullmove number
    ss >> std::skipws >> st->rule60 >> gamePly;

    // Convert from fullmove starting from 1 to gamePly starting from 0,
    // handle also common incorrect FEN with fullmove = 0.
    gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);

    set_state();

    assert(pos_is_ok());

    return *this;
}


// Sets king attacks to detect if a move gives check
// 设置国王攻击以检测是否移动给出将军
void Position::set_check_info() const {

    update_blockers<WHITE>();
    update_blockers<BLACK>();

    Square ksq = king_square(~sideToMove);
    // todo
    // We have to take special cares about the cannon and checks
    // st->needSlowCheck =
    //   checkers() || (attacks_bb<ROOK>(king_square(sideToMove)) & pieces(~sideToMove, CANNON));

    // st->checkSquares[PAWN]   = attacks_bb<PAWN_TO>(ksq, sideToMove);
    // st->checkSquares[KNIGHT] = attacks_bb<KNIGHT_TO>(ksq, pieces());
    // st->checkSquares[CANNON] = attacks_bb<CANNON>(ksq, pieces());
    // st->checkSquares[ROOK]   = attacks_bb<ROOK>(ksq, pieces());
    // st->checkSquares[KING] = st->checkSquares[ADVISOR] = st->checkSquares[BISHOP] = 0;

    // Bitboard hollowCannons = st->checkSquares[ROOK] & pieces(sideToMove, CANNON);
    // if (hollowCannons)
    // {
    //     Bitboard hollowCannonDiscover = Bitboard(0);
    //     while (hollowCannons)
    //         hollowCannonDiscover |= between_bb(ksq, pop_lsb(hollowCannons));
    //     for (PieceType pt = ROOK; pt < KING; ++pt)
    //         st->checkSquares[pt] |= hollowCannonDiscover;
    // }

    // 是否需要慢速检查，先保守开启
    st->needSlowCheck = true;

    // 巢穴特殊：若王在巢穴，则从巢穴四向邻格落子即可将军
    if (square_bb(ksq) & DenBB[~sideToMove]) {
        Bitboard nbr = ORTH1[ksq];
        st->checkSquares[ELEPHANT] = nbr;
        st->checkSquares[LION]     = nbr;
        st->checkSquares[TIGER]    = nbr;
        st->checkSquares[PANTHER]  = nbr;
        st->checkSquares[WOLF]     = nbr;
        st->checkSquares[DOG]      = nbr;
        st->checkSquares[CAT]      = nbr;
        st->checkSquares[RAT]      = nbr;
        return;
    }

    Bitboard occ = pieces();

    // 反向攻击预计算：从对方王格出发，哪些格子若被该类型子占据，会直接将军
    st->checkSquares[RAT]      = attacks_bb<RAT>(ksq, occ);
    st->checkSquares[CAT]      = attacks_bb<CAT>(ksq, occ);
    st->checkSquares[DOG]      = attacks_bb<DOG>(ksq, occ);
    st->checkSquares[WOLF]     = attacks_bb<WOLF>(ksq, occ);
    st->checkSquares[PANTHER]  = attacks_bb<PANTHER>(ksq, occ);
    st->checkSquares[ELEPHANT] = attacks_bb<ELEPHANT>(ksq, occ);
    st->checkSquares[LION]     = attacks_bb<LION>(ksq, occ);
    st->checkSquares[TIGER]    = attacks_bb<TIGER>(ksq, occ);
}


// Computes the hash keys of the position, and other
// data that once computed is updated incrementally as moves are made.
// The function is only used when a new position is set up
// 计算位置的哈希键和其他数据，这些数据一旦计算出来，在移动时会增量更新。
// 该函数仅在设置新位置时使用
void Position::set_state() const {

    st->key               = 0;
    st->minorPieceKey     = 0;
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = 0;
    st->pawnKey                                   = Zobrist::noPawns;
    st->majorMaterial[WHITE] = st->majorMaterial[BLACK] = VALUE_ZERO;
    st->checkersBB = checkers_to(~sideToMove, king_square(sideToMove));
    st->move       = Move::none();

    set_check_info();

    for (Bitboard b = pieces(); b;)
    {
        Square    s  = pop_lsb(b);
        Piece     pc = piece_on(s);
        PieceType pt = type_of(pc);
        st->key ^= Zobrist::psq[pc][s];

        //todo

        // if (pt == PAWN)
        //     st->pawnKey ^= Zobrist::psq[pc][s];

        // else
        // {
        //     st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];

        //     if (pt != KING && (pt & 1))
        //     {
        //         st->majorMaterial[color_of(pc)] += PieceValue[pc];
        //         if (pt != ROOK)
        //             st->minorPieceKey ^= Zobrist::psq[pc][s];
        //     }
        // }
        if(pt == RAT)
            st->pawnKey ^= Zobrist::psq[pc][s];

        st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];
        st->majorMaterial[color_of(pc)] += PieceValue[pc];
    }

    if (sideToMove == BLACK)
        st->key ^= Zobrist::side;
}


// Returns a FEN representation of the position.
// 返回位置的FEN表示
string Position::fen() const {

    int                emptyCnt;
    std::ostringstream ss;

    for (Rank r = RANK_8; r >= RANK_0; --r)
    {
        for (File f = FILE_A; f <= FILE_G; ++f)
        {
            for (emptyCnt = 0; f <= FILE_G && empty(make_square(f, r)); ++f)
                ++emptyCnt;

            if (emptyCnt)
                ss << emptyCnt;

            if (f <= FILE_G)
                ss << PieceToChar[piece_on(make_square(f, r))];
        }

        if (r > RANK_0)
            ss << '/';
    }

    ss << (sideToMove == WHITE ? " w " : " b ");

    ss << '-';

    ss << " - " << st->rule60 << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;

    return ss.str();
}



// Calculates st->blockersForKing[c] and st->pinners[~c],
// which store respectively the pieces preventing king of color c from being in check
// and the slider pieces of color ~c pinning pieces of color c to the king.
// 计算st->blockersForKing[c]和st->pinners[~c]，
// 分别存储阻止颜色为c的国王被将军的棋子和颜色为~c的滑动棋子将颜色为c的棋子固定在国王位置的棋子。
template<Color c>
void Position::update_blockers() const {

    Square ksq             = king_square(c);
    st->blockersForKing[c] = 0;
    st->pinners[~c]        = 0;

    //todo

    // Snipers are pieces that attack 's' when a piece and other pieces are removed
    // Bitboard snipers = ((attacks_bb<ROOK>(ksq) & (pieces(ROOK) | pieces(CANNON) | pieces(KING)))
    //                     | (attacks_bb<KNIGHT>(ksq) & pieces(KNIGHT)))
    //                  & pieces(~c);
    // Bitboard occupancy = pieces() ^ (snipers & ~pieces(CANNON));

    // while (snipers)
    // {
    //     Square   sniperSq = pop_lsb(snipers);
    //     bool     isCannon = type_of(piece_on(sniperSq)) == CANNON;
    //     Bitboard b = between_bb(ksq, sniperSq) & (isCannon ? pieces() ^ sniperSq : occupancy);

    //     if (b && ((!isCannon && !more_than_one(b)) || (isCannon && popcount(b) == 2)))
    //     {
    //         st->blockersForKing[c] |= b;
    //         if (b & pieces(c))
    //             st->pinners[~c] |= sniperSq;
    //     }
    // }
}


// Computes a bitboard of all pieces which attack a given square.
// Slider attacks use the occupied bitboard to indicate occupancy.
// 计算给定方格的所有攻击棋子位棋盘。
// 滑动攻击使用位棋盘表示占用。
Bitboard Position::attackers_to(Square s, Bitboard occupied) const {

    return (attacks_bb<RAT>(s, occupied) & pieces(RAT))
         | (attacks_bb<LION>(s, occupied) & pieces(LION))
         | (attacks_bb<TIGER>(s, occupied) & pieces(TIGER))
         | (attacks_bb<PANTHER>(s, occupied) & pieces(PANTHER))
         | (attacks_bb<WOLF>(s, occupied) & pieces(WOLF))
         | (attacks_bb<DOG>(s, occupied) & pieces(DOG))
         | (attacks_bb<CAT>(s, occupied) & pieces(CAT))
         | (attacks_bb<ELEPHANT>(s, occupied) & pieces(ELEPHANT));
}


// Computes a bitboard of all pieces of a given color
// which gives check to a given square. Slider attacks use the occupied bitboard
// to indicate occupancy.
// 计算给定颜色的所有棋子位棋盘，这些棋子可以将军给定方格。
// 滑动攻击使用位棋盘表示占用。
Bitboard Position::checkers_to(Color c, Square s, Bitboard occupied) const {

    if (square_bb(s) & (DenBB[~c])) {
        Bitboard nbr = ORTH1[s];     // 巢穴的四向邻格（边界会自然裁掉）
        // 若你的棋盘定义里巢穴邻格一定是陆地，下面这句就足够：
        return nbr & pieces(c);
    }

    return ((attacks_bb<RAT>(s, occupied)        & pieces(RAT))
        | (attacks_bb<CAT>(s, occupied)        & pieces(CAT))
        | (attacks_bb<DOG>(s, occupied)        & pieces(DOG))
        | (attacks_bb<WOLF>(s, occupied)       & pieces(WOLF))
        | (attacks_bb<PANTHER>(s, occupied)    & pieces(PANTHER))
        | (attacks_bb<ELEPHANT>(s, occupied)   & pieces(ELEPHANT))
        | (attacks_bb<LION>(s, occupied) & pieces(LION))
        | (attacks_bb<TIGER>(s, occupied) & pieces(TIGER)))
         & pieces(c);
}


// Tests whether a pseudo-legal move is legal
// 测试伪合法移动是否合法
bool Position::legal(Move m) const {

    assert(m.is_ok());

    Color    us       = sideToMove;
    Square   from     = m.from_sq();
    Square   to       = m.to_sq();
    Bitboard occupied = (pieces() ^ from) | to;

    assert(color_of(moved_piece(m)) == us);
    //todo
    // assert(piece_on(king_square(us)) == make_piece(us, KING));

    // If the moving piece is a king, check whether the destination square is
    // attacked by the opponent.
    //todo
    // if (type_of(piece_on(from)) == KING)
    //     return !(checkers_to(~us, to, occupied));

    // If we don't need slow check. A non-king move is always legal when either:
    // 1. Not moving a pinned piece.
    // 2. Moving a pinned non-cannon piece and aligned with king.
    // 3. Moving a pinned cannon and aligned with king but it's not a capture move.
    // if (!st->needSlowCheck
    //     && (!(blockers_for_king(us) & from)
    //         || (((type_of(piece_on(from)) != CANNON) || !capture(m))
    //             && aligned(from, to, king_square(us)))))
    //     return true;

    if (!st->needSlowCheck
        && (!(blockers_for_king(us) & from)
            || (( !capture(m))
                && aligned(from, to, king_square(us)))))
        return true;

    // A non-king move is legal if the king is not under attack after the move.
    return !(checkers_to(~us, king_square(us), occupied) & ~square_bb(to));
}


// Takes a random move and tests whether the move is
// pseudo-legal. It is used to validate moves from TT that can be corrupted
// due to SMP concurrent access or hash position key aliasing.
// 随机移动并测试移动是否伪合法。
// 它用于验证来自TT的移动，由于SMP并发访问或哈希位置键别名而可能被破坏。
bool Position::pseudo_legal(const Move m) const {

    Color  us   = sideToMove;
    Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece  pc   = moved_piece(m);

    // If the 'from' square is not occupied by a piece belonging to the side to
    // move, the move is obviously not legal.
    if (pc == NO_PIECE || color_of(pc) != us)
        return false;

    // The destination square cannot be occupied by a friendly piece
    if (pieces(us) & to)
        return false;

    // Handle the special cases
    //todo
    // if (type_of(pc) == PAWN)
    //     return bool(attacks_bb<PAWN>(from, us) & to);
    // else if (type_of(pc) == CANNON && !capture(m))
    //     return bool(attacks_bb<ROOK>(from, pieces()) & to);
    // else
        return bool(attacks_bb(type_of(pc), from, pieces()) & to);
}


// Tests whether a pseudo-legal move gives a check
// 测试伪合法移动是否给出将军
bool Position::gives_check(Move m) const {

    assert(m.is_ok());
    assert(color_of(moved_piece(m)) == sideToMove);

    Square from = m.from_sq();
    Square to   = m.to_sq();
    Square ksq  = king_square(~sideToMove);

    PieceType pt = type_of(moved_piece(m));

    // Is there a direct check?
    // if (pt == CANNON && aligned(from, to, ksq))
    // {
    //     if (attacks_bb<CANNON>(to, (pieces() ^ from) | to) & ksq)
    //         return true;
    // }
    // else if (check_squares(pt) & to)
    //     return true;
    if(check_squares(pt) & to)
        return true;

    // Is there a discovered check?
    // if (attacks_bb<ROOK>(ksq) & pieces(sideToMove, CANNON))
    //     return bool(checkers_to(sideToMove, ksq, (pieces() ^ from) | to) & ~square_bb(from));
    // else if ((blockers_for_king(~sideToMove) & from) && !aligned(from, to, ksq))
    //     return true;

    return false;
}


// Makes a move, and saves all information necessary
// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
// moves should be filtered out before this function is called.
// If a pointer to the TT table is passed, the entry for the new position
// will be prefetched
// 执行移动，并保存所有必要信息到StateInfo对象中。
// 假设移动是合法的。伪合法的移动应该在调用此函数之前过滤掉。
// 如果传递了TT表的指针，新的位置的条目将被预取。
DirtyPiece Position::do_move(Move                      m,
                             StateInfo&                newSt,
                             bool                      givesCheck,
                             const TranspositionTable* tt = nullptr) {

    assert(m.is_ok());
    assert(&newSt != st);

    // Update the bloom filter
    ++filter[st->key];

    Key k = st->key ^ Zobrist::side;

    // Copy some fields of the old state to our new StateInfo object except the
    // ones which are going to be recalculated from scratch anyway and then switch
    // our state pointer to point to the new (ready to be updated) state.
    std::memcpy(&newSt, st, offsetof(StateInfo, key));
    newSt.previous = st;
    st             = &newSt;
    st->move       = m;

    // Increment ply counters. Clamp to 10 checks for each side in rule 60
    // In particular, rule60 will be reset to zero later on in case of a capture.
    ++gamePly;
    if (!givesCheck || ++st->check10[sideToMove] <= 10)
    {
        if (st->check10[~sideToMove] > 10 && st->previous->checkersBB)
            ++st->check10[~sideToMove];
        else
            ++st->rule60;
    }
    ++st->pliesFromNull;

    Color  us       = sideToMove;
    Color  them     = ~us;
    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  pc       = piece_on(from);
    Piece  captured = piece_on(to);

    DirtyPiece dp;
    dp.pc   = pc;
    dp.from = from;
    dp.to   = to;

    assert(color_of(pc) == us);
    assert(captured == NO_PIECE || color_of(captured) == them);
    // assert(type_of(captured) != KING);

    //todo
    // if (pc == make_piece(us, KING))
    // {
    //     dp.requires_refresh[us] = true;
    //     bool mirror_before = Eval::NNUE::FeatureSet::KingBuckets[king_square(them)][from][0].second;
    //     bool mirror_after  = Eval::NNUE::FeatureSet::KingBuckets[king_square(them)][to][0].second;
    //     dp.requires_refresh[them] = (mirror_before != mirror_after);
    // }
    // else
    dp.requires_refresh[us] = dp.requires_refresh[them] = false;

    #if ENABLE_NNUE
    bool mid_mirror_before[2] = {Eval::NNUE::FeatureSet::requires_mid_mirror(*this, us),
                                 Eval::NNUE::FeatureSet::requires_mid_mirror(*this, them)};
    #endif

    if (captured)
    {
        Square capsq = to;

        //todo
        // If the captured piece is a pawn, update pawn hash key, otherwise
        // update major material.
        // if (type_of(captured) == PAWN)
        //     st->pawnKey ^= Zobrist::psq[captured][capsq];

        // else
        // {
        //     st->nonPawnKey[them] ^= Zobrist::psq[captured][capsq];

        //     if (type_of(captured) & 1)
        //     {
        //         st->majorMaterial[them] -= PieceValue[captured];
        //         if (type_of(captured) != ROOK)
        //             st->minorPieceKey ^= Zobrist::psq[captured][capsq];
        //     }
        //     

        // }
        if(type_of(captured) == RAT)
            st->pawnKey ^= Zobrist::psq[pc][capsq];

        st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][capsq];
        st->majorMaterial[color_of(pc)] += PieceValue[pc];

        dp.remove_pc = captured;
        dp.remove_sq = capsq;

    #if ENABLE_NNUE
        auto attack_bucket_before = Eval::NNUE::FeatureSet::make_attack_bucket(*this, them);
    #endif
        
        // Update board and piece lists
        remove_piece(capsq);
        
    #if ENABLE_NNUE
        auto attack_bucket_after = Eval::NNUE::FeatureSet::make_attack_bucket(*this, them);
        
        if (attack_bucket_before != attack_bucket_after)
            dp.requires_refresh[them] = true;
    #endif

        // Update hash key
        k ^= Zobrist::psq[captured][capsq];

        // Reset rule 60 counter
        st->check10[WHITE] = st->check10[BLACK] = st->rule60 = 0;
    }
    else
        dp.remove_sq = SQ_NONE;

    // Update hash key
    k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    // If the moving piece is a pawn, update pawn hash key.
    // if (type_of(pc) == PAWN)
    //     st->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    // else
    // {
    //     st->nonPawnKey[us] ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

    //     if (type_of(pc) == KNIGHT && type_of(pc) == CANNON)
    //         st->minorPieceKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    // }
    if (type_of(pc) == RAT)
        st->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    else
    {
        st->nonPawnKey[us] ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    }
    

    // Move the piece.
    move_piece(from, to);

    #if ENABLE_NNUE
    dp.requires_refresh[us] |=
      (mid_mirror_before[0] != Eval::NNUE::FeatureSet::requires_mid_mirror(*this, us));
    dp.requires_refresh[them] |=
      (mid_mirror_before[1] != Eval::NNUE::FeatureSet::requires_mid_mirror(*this, them));
    #endif

    // Update the key with the final value
    st->key = k;
    if (tt)
        prefetch(tt->first_entry(key()));

    // Set capture piece
    st->capturedPiece = captured;

    // Calculate checkers bitboard (if move gives check)
    st->checkersBB = givesCheck ? checkers_to(us, king_square(them)) : Bitboard(0);
    assert(givesCheck == bool(st->checkersBB));

    sideToMove = ~sideToMove;

    // Update king attacks used for fast check detection
    set_check_info();

    assert(pos_is_ok());

    assert(dp.pc != NO_PIECE);
    assert(!bool(captured) ^ (dp.remove_sq != SQ_NONE));
    assert(dp.from != SQ_NONE && dp.to != SQ_NONE);
    return dp;
}


// Unmakes a move. When it returns, the position should
// be restored to exactly the same state as before the move was made.
// 撤销移动。当返回时，位置应恢复到移动前的完全相同状态。
void Position::undo_move(Move m) {

    assert(m.is_ok());

    sideToMove = ~sideToMove;

    Square from = m.from_sq();
    Square to   = m.to_sq();

    assert(empty(from));
    //todo
    // assert(type_of(st->capturedPiece) != KING);

    move_piece(to, from);  // Put the piece back at the source square

    if (st->capturedPiece)
    {
        Square capsq = to;

        put_piece(st->capturedPiece, capsq);  // Restore the captured piece
    }

    // Finally point our state pointer back to the previous state
    st = st->previous;
    --gamePly;

    // Update the bloom filter
    --filter[st->key];

    assert(pos_is_ok());
}


// Used to do a "null move": it flips
// the side to move without executing any move on the board.
// 执行“空移动”：它翻转行棋方，而不在棋盘上执行任何移动。
void Position::do_null_move(StateInfo& newSt, const TranspositionTable& tt) {

    assert(!checkers());
    assert(&newSt != st);

    // Update the bloom filter
    ++filter[st->key];

    std::memcpy(&newSt, st, sizeof(StateInfo));

    newSt.previous = st;
    st             = &newSt;

    st->key ^= Zobrist::side;
    prefetch(tt.first_entry(key()));

    st->pliesFromNull = 0;

    sideToMove = ~sideToMove;

    set_check_info();

    assert(pos_is_ok());
}


// Must be used to undo a "null move"
// 必须用于撤销“空移动”
void Position::undo_null_move() {

    assert(!checkers());

    st         = st->previous;
    sideToMove = ~sideToMove;

    // Update the bloom filter
    --filter[st->key];
}


// Tests if the SEE (Static Exchange Evaluation)
// value of move is greater or equal to the given threshold. We'll use an
// algorithm similar to alpha-beta pruning with a null window.
// 测试移动的SEE（静态交换评估）值是否大于或等于给定的阈值。我们将使用类似于alpha-beta剪枝的算法，窗口为空。
bool Position::see_ge(Move m, int threshold) const {

    // assert(m.is_ok());

    // Square from = m.from_sq(), to = m.to_sq();

    // int swap = PieceValue[piece_on(to)] - threshold;
    // if (swap < 0)
    //     return false;

    // swap = PieceValue[piece_on(from)] - swap;
    // if (swap <= 0)
    //     return true;

    // assert(color_of(piece_on(from)) == sideToMove);
    // Bitboard occupied  = pieces() ^ from ^ to;  // xoring to is important for pinned piece logic
    // Color    stm       = sideToMove;
    // Bitboard attackers = attackers_to(to, occupied);

    // Flying general
    // if (attackers & pieces(stm, KING))
    //     attackers |= attacks_bb<ROOK>(to, occupied & ~pieces(ROOK)) & pieces(~stm, KING);
    // if (attackers & pieces(~stm, KING))
    //     attackers |= attacks_bb<ROOK>(to, occupied & ~pieces(ROOK)) & pieces(stm, KING);

    // Bitboard nonCannons = attackers & ~pieces(CANNON);
    // Bitboard cannons    = attackers & pieces(CANNON);
    // Bitboard stmAttackers, bb;
    // int      res = 1;

    // while (true)
    // {
    //     stm = ~stm;
    //     attackers &= occupied;

    //     // If stm has no more attackers then give up: stm loses
    //     if (!(stmAttackers = attackers & pieces(stm)))
    //         break;

    //     // Don't allow pinned pieces to attack as long as there are
    //     // pinners on their original square.
    //     if (pinners(~stm) & occupied)
    //     {
    //         stmAttackers &= ~blockers_for_king(stm);

    //         if (!stmAttackers)
    //             break;
    //     }

    //     res ^= 1;

    //     // Locate and remove the next least valuable attacker, and add to the
    //     // bitboard 'attackers' any protential attackers when it is removed.
    //     if ((bb = stmAttackers & pieces(PAWN)))
    //     {
    //         if ((swap = PawnValue - swap) < res)
    //             break;
    //         occupied ^= least_significant_square_bb(bb);

    //         nonCannons |= attacks_bb<ROOK>(to, occupied) & pieces(ROOK);
    //         cannons   = attacks_bb<CANNON>(to, occupied) & pieces(CANNON);
    //         attackers = nonCannons | cannons;
    //     }

    //     else if ((bb = stmAttackers & pieces(BISHOP)))
    //     {
    //         if ((swap = BishopValue - swap) < res)
    //             break;
    //         occupied ^= least_significant_square_bb(bb);
    //     }

    //     else if ((bb = stmAttackers & pieces(ADVISOR)))
    //     {
    //         if ((swap = AdvisorValue - swap) < res)
    //             break;
    //         occupied ^= least_significant_square_bb(bb);

    //         nonCannons |= attacks_bb<KNIGHT_TO>(to, occupied) & pieces(KNIGHT);
    //         attackers = nonCannons | cannons;
    //     }

    //     else if ((bb = stmAttackers & pieces(CANNON)))
    //     {
    //         if ((swap = CannonValue - swap) < res)
    //             break;
    //         occupied ^= least_significant_square_bb(bb);

    //         cannons   = attacks_bb<CANNON>(to, occupied) & pieces(CANNON);
    //         attackers = nonCannons | cannons;
    //     }

    //     else if ((bb = stmAttackers & pieces(KNIGHT)))
    //     {
    //         if ((swap = KnightValue - swap) < res)
    //             break;
    //         occupied ^= least_significant_square_bb(bb);
    //     }

    //     else if ((bb = stmAttackers & pieces(ROOK)))
    //     {
    //         swap = RookValue - swap;
    //         occupied ^= least_significant_square_bb(bb);

    //         nonCannons |= attacks_bb<ROOK>(to, occupied) & pieces(ROOK);
    //         cannons   = attacks_bb<CANNON>(to, occupied) & pieces(CANNON);
    //         attackers = nonCannons | cannons;
    //     }

    //     else  // KING
    //           // If we "capture" with the king but the opponent still has attackers,
    //           // reverse the result.
    //         return (attackers & ~pieces(stm)) ? res ^ 1 : res;
    // }

    // return bool(res);

    assert(m.is_ok());

    Square from = m.from_sq(), to = m.to_sq();

    int swap = PieceValue[piece_on(to)] - threshold;
    if (swap < 0)
        return false;

    swap = PieceValue[piece_on(from)] - swap;
    if (swap <= 0)
        return true;

    assert(color_of(piece_on(from)) == sideToMove);
    Bitboard occupied  = pieces() ^ from ^ to;  // xoring to is important for pinned piece logic
    Color    stm       = sideToMove;
    Bitboard attackers = attackers_to(to, occupied);

    // 当前 to 上所站的“占据者”的类型（第一回合吃完后是 from 的类型）
    PieceType curDefPT = type_of(piece_on(from));

    // 选择“按价值从低到高”的下一个可用攻击子（需满足 can_capture）
    auto pick_least_valuable_attacker = [&](Bitboard stmAttackers, PieceType& outPT, Square& outSq) -> bool {
        static constexpr PieceType order[] = { RAT, CAT, DOG, WOLF, PANTHER, TIGER, LION, ELEPHANT };
        for (PieceType pt : order) {
            Bitboard bb = stmAttackers & pieces(pt);
            while (bb) {
                Square s = pop_lsb(bb);
                if (can_capture(stm, pt, curDefPT, s, to)) {
                    outPT = pt; outSq = s;
                    return true;
                }
            }
        }
        return false;
    };

    int res = 1;

    while (true)
    {
        stm = ~stm;
        attackers &= occupied;

        Bitboard stmAttackers = attackers & pieces(stm);
        if (!stmAttackers)
            break;

        // 被钉住的棋子禁用
        if (pinners(~stm) & occupied) {
            stmAttackers &= ~blockers_for_king(stm);
            if (!stmAttackers)
                break;
        }

        res ^= 1;

        PieceType attPT = NO_PIECE_TYPE;
        Square    attSq = SQ_NONE;
        if (!pick_least_valuable_attacker(stmAttackers, attPT, attSq))
            break;

        if ((swap = PieceValue[make_piece(stm, attPT)] - swap) < res)
            break;

        // 移除该攻击子后，它成为新的“to 上占据者”
        occupied ^= square_bb(attSq);
        curDefPT = attPT;

        // 重新计算新的攻击者集合（可能打开狮/虎跳河线）
        attackers = attackers_to(to, occupied);
    }

    return bool(res);
    
}


// Like do_move(), but a little lighter
// 类似于 do_move()，但稍微轻量一些
std::pair<Piece, int> Position::light_do_move(Move m) {

    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  captured = piece_on(to);
    int    id       = idBoard[to];

    // Update id board
    idBoard[to]   = idBoard[from];
    idBoard[from] = 0;

    if (captured)
        // Update board and piece lists
        remove_piece(to);

    move_piece(from, to);

    sideToMove = ~sideToMove;

    return {captured, id};
}


// Like undo_move(), but a little lighter
void Position::light_undo_move(Move m, Piece captured, int id) {

    sideToMove = ~sideToMove;

    Square from = m.from_sq();
    Square to   = m.to_sq();

    // Put back id board
    idBoard[from] = idBoard[to];
    idBoard[to]   = id;

    move_piece(to, from);  // Put the piece back at the source square

    if (captured)
    {
        Square capsq = to;

        put_piece(captured, capsq);  // Restore the captured piece
    }
}


// Tests whether a pseudo-legal move is chase legal
// 测试伪合法移动是否是牵制合法的
bool Position::chase_legal(Move m) const {

    assert(m.is_ok());

    Color    us       = sideToMove;
    Square   from     = m.from_sq();
    Square   to       = m.to_sq();
    Bitboard occupied = (pieces() ^ from) | to;

    assert(color_of(moved_piece(m)) == us);
    // assert(piece_on(king_square(us)) == make_piece(us, KING));

    // If the moving piece is a king, check whether the destination
    // square is not under new attack after the move.
    //todo
    // if (type_of(piece_on(from)) == KING)
    //     return !(checkers_to(~us, to, occupied));

    // A non-king move is chase legal if the king is not under new attack after the move.
    return !(checkers_to(~us, king_square(us), occupied) & ~square_bb(to));
}


// Calculates the chase information for a given color.
// 计算给定颜色的牵制信息。
uint16_t Position::chased(Color c) {

    uint16_t chase = 0;

    std::swap(c, sideToMove);

    // King and pawn can legally perpetual chase
    // Bitboard attackers = pieces(sideToMove) ^ pieces(sideToMove, KING, PAWN);
    Bitboard attackers = pieces(sideToMove);
    while (attackers)
    {
        Square    from         = pop_lsb(attackers);
        PieceType attackerType = type_of(piece_on(from));
        Bitboard  attacks      = attacks_bb(attackerType, from, pieces());

        // Restrict to pinners if pinned, otherwise exclude attacks on unpromoted pawns and checks
        // if (blockers_for_king(sideToMove) & from)
        //     attacks &= pinners(~sideToMove) & ~pieces(KING);
        // else
        //     attacks &= (pieces(~sideToMove) ^ pieces(~sideToMove, KING, PAWN))
        //              | (pieces(~sideToMove, PAWN) & HalfBB[sideToMove]);
        // 只考虑攻击到对方占据的格子
        attacks &= pieces(~sideToMove);

        while (attacks)
        {
            Square to = pop_lsb(attacks);
            Move   m  = Move(from, to);

            PieceType defenderType = type_of(piece_on(to));
            // 斗兽棋吃法判定
            if (!can_capture(sideToMove, attackerType, defenderType, from, to))
                continue;
            if (!chase_legal(m))
                continue;

            bool trueChase = false;

            // 规则1：攻击严格更强的子，视为追逐
            if (JungleRankPT[attackerType] < JungleRankPT[defenderType])
            {
                trueChase = true;
            }
            else
            {
                // 规则2：否则若对方无法合法反吃，也视为追逐
                const auto& [captured, id] = light_do_move(m);

                // 现在轮到对方走
                Bitboard recaptures = attackers_to(to) & pieces(sideToMove);

                bool canRecapture = false;
                while (recaptures)
                {
                    Square    s     = pop_lsb(recaptures);
                    PieceType recPT = type_of(piece_on(s));
                    if (can_capture(sideToMove, recPT, attackerType, s, to)
                        && chase_legal(Move(s, to)))
                    {
                        canRecapture = true;
                        break;
                    }
                }

                light_undo_move(m, captured, id);

                trueChase = !canRecapture;
            }

            if (trueChase)
                chase |= (1 << idBoard[to]);
            // if (chase_legal(m))
            // {
            //     // Attacks against stronger pieces
            //     if ((attackerType == KNIGHT || attackerType == CANNON)
            //         && type_of(piece_on(to)) == ROOK)
            //         chase |= (1 << idBoard[to]);
            //     if ((attackerType == ADVISOR || attackerType == BISHOP)
            //         && type_of(piece_on(to)) & 1)
            //         chase |= (1 << idBoard[to]);
            //     // Attacks against potentially unprotected pieces
            //     else
            //     {
            //         bool trueChase             = true;
            //         const auto& [captured, id] = light_do_move(m);
            //         Bitboard recaptures        = attackers_to(to) & pieces(sideToMove);
            //         while (recaptures)
            //         {
            //             Square s = pop_lsb(recaptures);
            //             if (chase_legal(Move(s, to)))
            //             {
            //                 trueChase = false;
            //                 break;
            //             }
            //         }
            //         light_undo_move(m, captured, id);

            //         if (trueChase)
            //         {
            //             // Exclude mutual/symmetric attacks except pins
            //             if (attackerType == type_of(piece_on(to)))
            //             {
            //                 sideToMove = ~sideToMove;
            //                 if ((attackerType == KNIGHT && ((between_bb(from, to) ^ to) & pieces()))
            //                     || !chase_legal(Move(to, from)))
            //                     chase |= (1 << idBoard[to]);
            //                 sideToMove = ~sideToMove;
            //             }
            //             else
            //                 chase |= (1 << idBoard[to]);
            //         }
            //     }
            // }
        }
    }

    std::swap(c, sideToMove);

    return chase;
}


// Detects chases from state st - d to state st
// 从状态 st - d 检测牵制到状态 st
Value Position::detect_chases(int d, int ply) {

    // Grant each piece on board a unique id for each side
    int whiteId = 0;
    int blackId = 0;
    for (Square s = SQ_A0; s <= SQ_G8; ++s)
        if (board[s] != NO_PIECE)
            idBoard[s] = color_of(board[s]) == WHITE ? whiteId++ : blackId++;

    Color us = sideToMove, them = ~us;

    // Rollback until we reached st - d
    uint16_t chase[COLOR_NB] = {0xFFFF, 0xFFFF};
    for (int i = 0; i < d; ++i)
    {
        if (st->checkersBB)
            return VALUE_DRAW;
        else if (!chase[~sideToMove])
        {
            if (!chase[sideToMove])
                break;
            light_undo_move(st->move, st->capturedPiece);
            st = st->previous;
        }
        else
        {
            uint16_t after = chased(~sideToMove);
            light_undo_move(st->move, st->capturedPiece);
            st = st->previous;
            // Take the exact diff to detect the chase
            chase[sideToMove] &= after & ~chased(sideToMove);
        }
    }

    return bool(chase[us]) ^ bool(chase[them]) ? chase[us] ? mated_in(ply) : mate_in(ply)
                                               : VALUE_DRAW;
}


// Tests whether the position may end the game by rule 60, insufficient material, draw repetition,
// perpetual check repetition or perpetual chase repetition that allows a player to claim a game result.
// 测试是否可以通过规则60、棋子不足、和棋重复、长将重复或长牵制重复使玩家获得游戏结果。
bool Position::rule_judge(Value& result, int ply) {

    // Restore rule 60 by adding back the checks
    int end = std::min(st->rule60 + std::max(0, st->check10[WHITE] - 10)
                         + std::max(0, st->check10[BLACK] - 10),
                       st->pliesFromNull);

    if (end >= 4 && filter[st->key] >= 1)
    {
        int        cnt       = 0;
        StateInfo* stp       = st->previous->previous;
        bool       checkThem = st->checkersBB && stp->checkersBB;
        bool       checkUs   = st->previous->checkersBB && stp->previous->checkersBB;

        for (int i = 4; i <= end; i += 2)
        {
            stp = stp->previous->previous;
            checkThem &= bool(stp->checkersBB);

            // Return a score if a position repeats once earlier but strictly
            // after the root, or repeats twice before or at the root.
            if (stp->key == st->key && (++cnt == 2 || ply > i))
            {
                if (!checkThem && !checkUs)
                {
                    // Copy the current position to a rollback struct, so we don't need to do those moves again
                    Position rollback;
                    memcpy((void*) &rollback, (const void*) this, offsetof(Position, filter));

                    // Chasing detection
                    result = rollback.detect_chases(i, ply);
                }
                else
                    // Checking detection
                    result = !checkUs ? mate_in(ply) : !checkThem ? mated_in(ply) : VALUE_DRAW;

                // 3 folds and 2 fold draws can be judged immediately
                if (result == VALUE_DRAW || cnt == 2)
                    return true;

                // 2 fold mates need further investigations
                if (filter[st->key] <= 1)
                {
                    // Not exceeding rule 60 and have the same previous step
                    if (st->rule60 < 120 && st->previous->key == stp->previous->key)
                    {
                        // Even if we entering this loop again, it will not lead to a 3 fold repetition
                        StateInfo* prev = st->previous;
                        while ((prev = prev->previous) != stp)
                            if (filter[prev->key] > 1)
                                break;
                        if (prev == stp)
                            return true;
                    }
                    // We know there can't be another fold
                    break;
                }
            }

            if (i + 1 <= end)
                checkUs &= bool(stp->previous->checkersBB);
        }
    }

    // 60 move rule
    if (st->rule60 >= 120)
    {
        result = MoveList<LEGAL>(*this).size() ? VALUE_DRAW : mated_in(ply);
        return true;
    }

    //todo
    // Draw by insufficient material
    // if (count<PAWN>() == 0)
    // {
    //     enum DrawLevel : int {
    //         NO_DRAW,      // There is no drawing situation exists
    //         DIRECT_DRAW,  // A draw can be directly yielded without any checks
    //         MATE_DRAW     // We need to check for mate before yielding a draw
    //     };

    //     int level = [&]() {
    //         // No cannons left on the board
    //         if (!major_material())
    //             return DIRECT_DRAW;

    //         // One cannon left on the board
    //         if (major_material() == CannonValue)
    //         {
    //             // See which side is holding this cannon, and this side must not possess any advisors
    //             Color cannonSide = major_material(WHITE) == CannonValue ? WHITE : BLACK;
    //             if (count<ADVISOR>(cannonSide) == 0)
    //             {
    //                 // No advisors left on the board
    //                 if (count<ADVISOR>(~cannonSide) == 0)
    //                     return DIRECT_DRAW;

    //                 // One advisor left on the board
    //                 if (count<ADVISOR>(~cannonSide) == 1)
    //                     return count<BISHOP>(cannonSide) == 0 ? DIRECT_DRAW : MATE_DRAW;

    //                 // Two advisors left on the board
    //                 if (count<BISHOP>(cannonSide) == 0)
    //                     return MATE_DRAW;
    //             }
    //         }

    //         // Two cannons left on the board, one for each side, and no advisors left on the board
    //         if (major_material(WHITE) == CannonValue && major_material(BLACK) == CannonValue
    //             && count<ADVISOR>() == 0)
    //             return count<BISHOP>() == 0 ? DIRECT_DRAW : MATE_DRAW;

    //         return NO_DRAW;
    //     }();

    //     if (level != NO_DRAW)
    //     {
    //         if (level == MATE_DRAW)
    //         {
    //             MoveList<LEGAL> moves(*this);
    //             if (moves.size() == 0)
    //             {
    //                 result = mated_in(ply);
    //                 return true;
    //             }
    //             for (const auto& move : moves)
    //             {
    //                 StateInfo tempSt;
    //                 do_move(move, tempSt);
    //                 bool mate = MoveList<LEGAL>(*this).size() == 0;
    //                 undo_move(move);
    //                 if (mate)
    //                     return false;
    //             }
    //         }
    //         result = VALUE_DRAW;
    //         return true;
    //     }
    // }

    return false;
}


// Flips position with the white and black sides reversed. This
// is only useful for debugging e.g. for finding evaluation symmetry bugs.
// 翻转位置，使白方和黑方互换。这在调试时非常有用，例如用于查找评估对称性错误。
void Position::flip() {

    string            f, token;
    std::stringstream ss(fen());

    for (Rank r = RANK_8; r >= RANK_0; --r)  // Piece placement
    {
        std::getline(ss, token, r > RANK_0 ? '/' : ' ');
        f.insert(0, token + (f.empty() ? " " : "/"));
    }

    ss >> token;                        // Active color
    f += (token == "w" ? "B " : "W ");  // Will be lowercased later

    ss >> token;
    f += token + " ";

    std::transform(f.begin(), f.end(), f.begin(),
                   [](char c) { return char(islower(c) ? toupper(c) : tolower(c)); });

    ss >> token;
    f += token;

    std::getline(ss, token);  // Half and full moves
    f += token;

    set(f, st);

    assert(pos_is_ok());
}


// Performs some consistency checks for the position object
// and raise an assert if something wrong is detected.
// This is meant to be helpful when debugging.
// 对位置对象执行一些一致性检查，如果检测到任何问题，则引发断言。这在调试时非常有用。
bool Position::pos_is_ok() const {

    constexpr bool Fast = true;  // Quick (default) or full check?

    if ((sideToMove != WHITE && sideToMove != BLACK))
        assert(0 && "pos_is_ok: Default");

    if (Fast)
        return true;

    //delete:删除king检查

    //delete:删除pawn检查

    if ((pieces(WHITE) & pieces(BLACK)) || (pieces(WHITE) | pieces(BLACK)) != pieces()
        || popcount(pieces(WHITE)) > 8 || popcount(pieces(BLACK)) > 8)
        assert(0 && "pos_is_ok: Bitboards");

    for (PieceType p1 = ELEPHANT; p1 <= RAT; ++p1)
        for (PieceType p2 = ELEPHANT; p2 <= RAT; ++p2)
            if (p1 != p2 && (pieces(p1) & pieces(p2)))
                assert(0 && "pos_is_ok: Bitboards");

    for (Piece pc : Pieces)
        if (pieceCount[pc] != popcount(pieces(color_of(pc), type_of(pc)))
            || pieceCount[pc] != std::count(board, board + SQUARE_NB, pc))
            assert(0 && "pos_is_ok: Pieces");

    return true;
}

//add:新增 —— 吃子判定辅助，关于棋子等级
bool Position::can_capture(Color us, PieceType atk, PieceType dfd, Square from, Square to) const {
    const Bitboard fromBB = square_bb(from);
    const Bitboard toBB   = square_bb(to);

    // 1) 己方兽穴禁入（与吃无关，但这里兜底更安全）
    if (DenBB[us] & toBB)
        return false;

    const bool fromRiver = bool(RiverBB & fromBB);
    const bool toRiver   = bool(RiverBB & toBB);

    // 2) 陷阱：对方踩在我方陷阱 -> 防守子视为 0 级
    const bool trapped   = bool(TrapBB[us] & toBB);
    int dfdRank = trapped ? 0 : JungleRankPT[dfd];
    int atkRank = JungleRankPT[atk];

    // 3) 水域交互（路径可达留给 attackers_to，这里只做交战域限制）
    if (atk != RAT) {
        // 非鼠不能参与任何“涉水交战”（from 或 to 在水都不行）
        if (fromRiver || toRiver)
            return false;
    } else {
        // 鼠在水中只能与水中鼠互吃
        if (fromRiver && toRiver && dfd != RAT)
            return false;

        // 常见赛规：水中鼠不能吃“任何”岸上动物（不止象）
        if (fromRiver && !toRiver)
            return false;

        // 常见赛规：岸上鼠不能吃水中鼠
        if (!fromRiver && toRiver)
            return false;
    }

    // 4) 陷阱优先：被困则任何子都可吃（在水域限制已过的前提下）
    if (trapped)
        return true;

    // 5) 鼠-象特例（仅在非陷阱时生效）
    if (atk == RAT && dfd == ELEPHANT)
        return true;
    if (atk == ELEPHANT && dfd == RAT)
        return false;

    // 6) 一般强弱：大吃小或同级
    return atkRank >= dfdRank;
}

}  // namespace Stockfish
