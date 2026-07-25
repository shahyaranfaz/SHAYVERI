#include "move_io.h"

#include "make.h"
#include "move_gen.h"

#include <cctype>

namespace SHAYVERI {

std::string move_to_uci(Move move) {
    if (move == MOVE_NONE) return "0000";

    auto square_text = [](Square square) {
        std::string text;
        text += static_cast<char>('a' + get_file(square));
        text += static_cast<char>('1' + get_rank(square));
        return text;
    };

    std::string text =
        square_text(move_from(move)) + square_text(move_to(move));
    switch (move_promo(move)) {
        case KNIGHT: text += 'n'; break;
        case BISHOP: text += 'b'; break;
        case ROOK:   text += 'r'; break;
        case QUEEN:  text += 'q'; break;
        default: break;
    }
    return text;
}

static bool is_uci_square_text(const std::string &uci, int offset) {
    return offset >= 0
        && uci.size() > static_cast<std::size_t>(offset) + 1
        && uci[offset] >= 'a' && uci[offset] <= 'h'
        && uci[offset + 1] >= '1' && uci[offset + 1] <= '8';
}

static bool is_uci_move_text(const std::string &uci) {
    if (uci.size() != 4 && uci.size() != 5) return false;
    if (!is_uci_square_text(uci, 0) || !is_uci_square_text(uci, 2))
        return false;
    if (uci.size() == 5) {
        const char promotion = static_cast<char>(
            std::tolower(static_cast<unsigned char>(uci[4])));
        return promotion == 'n' || promotion == 'b'
            || promotion == 'r' || promotion == 'q';
    }
    return true;
}

Move uci_to_move(Board &board, const std::string &uci) {
    if (!is_uci_move_text(uci)) return MOVE_NONE;

    const Square from = make_square(
        File(uci[0] - 'a'), Rank(uci[1] - '1'));
    const Square to = make_square(
        File(uci[2] - 'a'), Rank(uci[3] - '1'));

    PieceType promotion = NONE_PTYPE;
    if (uci.size() == 5) {
        switch (std::tolower(static_cast<unsigned char>(uci[4]))) {
            case 'n': promotion = KNIGHT; break;
            case 'b': promotion = BISHOP; break;
            case 'r': promotion = ROOK; break;
            case 'q': promotion = QUEEN; break;
            default: break;
        }
    }

    const MoveList pseudo = generate_pseudo_legal_moves(board);
    for (int i = 0; i < pseudo.count; ++i) {
        const Move move = pseudo.moves[i];
        if (move_from(move) != from || move_to(move) != to
            || move_promo(move) != promotion)
            continue;

        Undo undo;
        if (!make_generated_move(board, move, undo)) continue;
        unmake_move(board, move, undo);
        return move;
    }
    return MOVE_NONE;
}

} // namespace SHAYVERI
