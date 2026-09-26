#include "tablebase.h"

#include "board.h"
#include "move.h"
#include "types.h"

#include <algorithm>
#include <bit>
#include <string>

extern "C" {
#include "tbprobe.h"
}

namespace SHAYVERI {

namespace Tablebase {

namespace {

    WDL decode_wdl(unsigned result) {
        switch (result) {
            case TB_LOSS: return WDL::Loss;
            case TB_BLESSED_LOSS: return WDL::BlessedLoss;
            case TB_DRAW: return WDL::Draw;
            case TB_CURSED_WIN: return WDL::CursedWin;
            case TB_WIN: return WDL::Win;
            default: return WDL::Failed;
        }
    }

    PieceType decode_promotion(unsigned promotion) {
        switch (promotion) {
            case TB_PROMOTES_QUEEN: return QUEEN;
            case TB_PROMOTES_ROOK: return ROOK;
            case TB_PROMOTES_BISHOP: return BISHOP;
            case TB_PROMOTES_KNIGHT: return KNIGHT;
            default: return NONE_PTYPE;
        }
    }

} // namespace

bool initialize(const std::string &path) { return tb_init(path.c_str()); }

void shutdown() { tb_free(); }

unsigned max_pieces() { return TB_LARGEST; }

bool can_probe(const Board &board) {
    return TB_LARGEST > 0 && board.castling == 0
        && static_cast<unsigned>(std::popcount(board.occupied)) <= TB_LARGEST;
}

WDL probe_wdl(const Board &board) {
    if (!can_probe(board) || board.half_move != 0) return WDL::Failed;

    const unsigned result = tb_probe_wdl(
        board.occupancies[WHITE], board.occupancies[BLACK],
        board.bit_boards[WK] | board.bit_boards[BK],
        board.bit_boards[WQ] | board.bit_boards[BQ],
        board.bit_boards[WR] | board.bit_boards[BR],
        board.bit_boards[WB] | board.bit_boards[BB],
        board.bit_boards[WN] | board.bit_boards[BN],
        board.bit_boards[WP] | board.bit_boards[BP],
        static_cast<unsigned>(board.half_move), 
        static_cast<unsigned>(board.castling),
        board.en_passant == SQ_NONE ? 0u : static_cast<unsigned>(board.en_passant),
        board.side_to_move == WHITE);

    return decode_wdl(result);
}

RootResult probe_root(const Board &board) {
    if (!can_probe(board)) return {WDL::Failed, MOVE_NONE, 0};

    const unsigned result = tb_probe_root(
        board.occupancies[WHITE], board.occupancies[BLACK],
        board.bit_boards[WK] | board.bit_boards[BK],
        board.bit_boards[WQ] | board.bit_boards[BQ],
        board.bit_boards[WR] | board.bit_boards[BR],
        board.bit_boards[WB] | board.bit_boards[BB],
        board.bit_boards[WN] | board.bit_boards[BN],
        board.bit_boards[WP] | board.bit_boards[BP],
        static_cast<unsigned>(board.half_move),
        static_cast<unsigned>(board.castling),
        board.en_passant == SQ_NONE ? 0u : static_cast<unsigned>(board.en_passant),
        board.side_to_move == WHITE,
        nullptr);

    if (result != TB_RESULT_FAILED && result != TB_RESULT_CHECKMATE && result != TB_RESULT_STALEMATE) {
        const WDL wdl = decode_wdl(TB_GET_WDL(result));
        if (wdl != WDL::Failed) {
            const Square from = static_cast<Square>(TB_GET_FROM(result));
            const Square to = static_cast<Square>(TB_GET_TO(result));
            const PieceType promotion = decode_promotion(TB_GET_PROMOTES(result));
            const Move move = TB_GET_EP(result)
                ? create_ep_move(from, to)
                : create_move(from, to, promotion);

            const int distance = static_cast<int>(TB_GET_DTZ(result));
            const int dtz = wdl == WDL::Loss || wdl == WDL::BlessedLoss
                ? -distance
                : (wdl == WDL::Win || wdl == WDL::CursedWin ? distance : 0);

            return {wdl, move, dtz};
        }
    }

    TbRootMoves results{};
    const int success = tb_probe_root_wdl(
        board.occupancies[WHITE], board.occupancies[BLACK],
        board.bit_boards[WK] | board.bit_boards[BK],
        board.bit_boards[WQ] | board.bit_boards[BQ],
        board.bit_boards[WR] | board.bit_boards[BR],
        board.bit_boards[WB] | board.bit_boards[BB],
        board.bit_boards[WN] | board.bit_boards[BN],
        board.bit_boards[WP] | board.bit_boards[BP],
        static_cast<unsigned>(board.half_move),
        static_cast<unsigned>(board.castling),
        board.en_passant == SQ_NONE ? 0u : static_cast<unsigned>(board.en_passant),
        board.side_to_move == WHITE,
        true,
        &results);

    if (success == 0 || results.size == 0) return {WDL::Failed, MOVE_NONE, 0};

    const TbRootMove &best = *std::max_element(results.moves,
        results.moves + results.size,
        [](const TbRootMove &lhs, const TbRootMove &rhs) {
            return lhs.tbRank < rhs.tbRank;
        });

    WDL wdl = WDL::Draw;
    if (best.tbRank >= 1000)
        wdl = WDL::Win;
    else if (best.tbRank > 0)
        wdl = WDL::CursedWin;
    else if (best.tbRank <= -1000)
        wdl = WDL::Loss;
    else if (best.tbRank < 0)
        wdl = WDL::BlessedLoss;

    const Square from = static_cast<Square>(TB_MOVE_FROM(best.move));
    const Square to = static_cast<Square>(TB_MOVE_TO(best.move));

    const PieceType promotion = decode_promotion(TB_MOVE_PROMOTES(best.move));

    const Piece moving_piece = board.get_piece(from);
    const bool is_en_passant = get_type(moving_piece) == PAWN
        && to == board.en_passant
        && board.get_piece(to) == NONE_PIECE;
    const Move move = is_en_passant ? create_ep_move(from, to) : create_move(from, to, promotion);

    return {wdl, move, 0};
}

} // namespace Tablebase

} // namespace SHAYVERI
