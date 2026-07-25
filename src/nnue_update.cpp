#include "nnue_update.h"

#include "attacks.h"
#include "make.h"

#include <cstdlib>
#include <immintrin.h>

namespace SHAYVERI {

namespace NNUE {

namespace {

int piece_type_index(PieceType pt) {
    return static_cast<int>(pt) - 1;
}

inline void add_row(I16 *destination, const I16 *source, int count) {
    int i = 0;
#ifdef __AVX2__
    for (; i + 16 <= count; i += 16) {
        const __m256i dst = _mm256_loadu_si256(
            reinterpret_cast<const __m256i *>(destination + i));
        const __m256i add = _mm256_loadu_si256(
            reinterpret_cast<const __m256i *>(source + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(destination + i),
                            _mm256_add_epi16(dst, add));
    }
#endif
    for (; i < count; ++i) destination[i] += source[i];
}

inline void sub_row(I16 *destination, const I16 *source, int count) {
    int i = 0;
#ifdef __AVX2__
    for (; i + 16 <= count; i += 16) {
        const __m256i dst = _mm256_loadu_si256(
            reinterpret_cast<const __m256i *>(destination + i));
        const __m256i sub = _mm256_loadu_si256(
            reinterpret_cast<const __m256i *>(source + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(destination + i),
                            _mm256_sub_epi16(dst, sub));
    }
#endif
    for (; i < count; ++i) destination[i] -= source[i];
}

inline void copy_add_sub(I16 *destination, const I16 *source,
                         const I16 *add, const I16 *sub,
                         const I16 *second_sub, int count) {
    int i = 0;
#ifdef __AVX2__
    for (; i + 16 <= count; i += 16) {
        __m256i value = _mm256_loadu_si256(
            reinterpret_cast<const __m256i *>(source + i));
        value = _mm256_add_epi16(
            value, _mm256_loadu_si256(
                reinterpret_cast<const __m256i *>(add + i)));
        value = _mm256_sub_epi16(
            value, _mm256_loadu_si256(
                reinterpret_cast<const __m256i *>(sub + i)));
        if (second_sub) {
            value = _mm256_sub_epi16(
                value, _mm256_loadu_si256(
                    reinterpret_cast<const __m256i *>(second_sub + i)));
        }
        _mm256_storeu_si256(
            reinterpret_cast<__m256i *>(destination + i), value);
    }
#endif
    for (; i < count; ++i) {
        int value = static_cast<int>(source[i]) + add[i] - sub[i];
        if (second_sub) value -= second_sub[i];
        destination[i] = static_cast<I16>(value);
    }
}

void acc_add(Accumulator &acc, Piece p, Square sq, Square white_king_sq, Square black_king_sq) {
    int wi = 0;
    int bi = 0;
    feature_indices(piece_type_index(get_type(p)), static_cast<int>(get_colour(p)),
                    sq, white_king_sq, black_king_sq, wi, bi);
    const int hidden = active_hidden_size();
    add_row(acc.vals[0], feature_weights[wi], hidden);
    add_row(acc.vals[1], feature_weights[bi], hidden);
}

void acc_sub(Accumulator &acc, Piece p, Square sq, Square white_king_sq, Square black_king_sq) {
    int wi = 0;
    int bi = 0;
    feature_indices(piece_type_index(get_type(p)), static_cast<int>(get_colour(p)),
                    sq, white_king_sq, black_king_sq, wi, bi);
    const int hidden = active_hidden_size();
    sub_row(acc.vals[0], feature_weights[wi], hidden);
    sub_row(acc.vals[1], feature_weights[bi], hidden);
}

} // namespace

void update_accumulator(Accumulator &child, const Accumulator &parent,
                        const Board &post, Move m, const Undo &u) {
    Square from = move_from(m);
    Square to = move_to(m);
    PieceType promo = move_promo(m);
    Colour stm = flip(post.side_to_move);
    Piece moved = promo != NONE_PTYPE
                    ? (stm == WHITE ? WP : BP)
                    : post.get_piece(to);
    Piece captured = u.captured;
    Square white_king_sq = king_square(post, WHITE);
    Square black_king_sq = king_square(post, BLACK);

    if (get_type(moved) == KING && has_king_buckets()) {
        const bool is_castling = std::abs(get_file(from) - get_file(to)) == 2;
        if (captured == NONE_PIECE && !is_castling) {
            const int refreshed_perspective = static_cast<int>(stm);
            const int stable_perspective = refreshed_perspective ^ 1;
            const Square stable_king_sq = stable_perspective == WHITE
                ? white_king_sq : black_king_sq;
            const int piece_type = piece_type_index(KING);
            const int piece_colour = static_cast<int>(stm);
            const int add_index = feature_index(
                piece_type, piece_colour, to,
                stable_perspective, stable_king_sq);
            const int sub_index = feature_index(
                piece_type, piece_colour, from,
                stable_perspective, stable_king_sq);

            copy_add_sub(child.vals[stable_perspective],
                         parent.vals[stable_perspective],
                         feature_weights[add_index],
                         feature_weights[sub_index], nullptr,
                         active_hidden_size());
            child.refresh_perspective(post, refreshed_perspective);
            return;
        }
        child.refresh(post);
        return;
    }

    if (get_type(moved) == KING) {
        if (stm == WHITE) white_king_sq = from;
        else              black_king_sq = from;
    }

    if (captured == NONE_PIECE && promo == NONE_PTYPE &&
        get_type(moved) != KING && !is_ep_move(m)) {
        int add_white = 0;
        int add_black = 0;
        int sub_white = 0;
        int sub_black = 0;
        const int piece_type = piece_type_index(get_type(moved));
        const int piece_colour = static_cast<int>(get_colour(moved));

        feature_indices(piece_type, piece_colour, to,
                        white_king_sq, black_king_sq,
                        add_white, add_black);
        feature_indices(piece_type, piece_colour, from,
                        white_king_sq, black_king_sq,
                        sub_white, sub_black);
        const int hidden = active_hidden_size();
        copy_add_sub(child.vals[0], parent.vals[0],
                     feature_weights[add_white], feature_weights[sub_white],
                     nullptr, hidden);
        copy_add_sub(child.vals[1], parent.vals[1],
                     feature_weights[add_black], feature_weights[sub_black],
                     nullptr, hidden);
        return;
    }

    if (captured != NONE_PIECE && promo == NONE_PTYPE &&
        get_type(moved) != KING && !is_ep_move(m)) {
        int add_white = 0;
        int add_black = 0;
        int move_sub_white = 0;
        int move_sub_black = 0;
        int capture_sub_white = 0;
        int capture_sub_black = 0;

        feature_indices(piece_type_index(get_type(moved)),
                        static_cast<int>(get_colour(moved)), to,
                        white_king_sq, black_king_sq,
                        add_white, add_black);
        feature_indices(piece_type_index(get_type(moved)),
                        static_cast<int>(get_colour(moved)), from,
                        white_king_sq, black_king_sq,
                        move_sub_white, move_sub_black);
        feature_indices(piece_type_index(get_type(captured)),
                        static_cast<int>(get_colour(captured)), to,
                        white_king_sq, black_king_sq,
                        capture_sub_white, capture_sub_black);

        const int hidden = active_hidden_size();
        copy_add_sub(child.vals[0], parent.vals[0],
                     feature_weights[add_white],
                     feature_weights[move_sub_white],
                     feature_weights[capture_sub_white], hidden);
        copy_add_sub(child.vals[1], parent.vals[1],
                     feature_weights[add_black],
                     feature_weights[move_sub_black],
                     feature_weights[capture_sub_black], hidden);
        return;
    }

    child = parent;

    if (is_ep_move(m)) {
        Square cap_sq = (stm == WHITE) ? to - 8 : to + 8;
        acc_sub(child, captured, cap_sq, white_king_sq, black_king_sq);
        acc_sub(child, moved, from, white_king_sq, black_king_sq);
        acc_add(child, moved, to, white_king_sq, black_king_sq);
        return;
    }

    if (get_type(moved) == KING && std::abs(get_file(from) - get_file(to)) == 2) {
        const CastleInfo castle = castle_info(stm, get_file(to) == FILE_G);

        acc_sub(child, moved, from, white_king_sq, black_king_sq);
        acc_add(child, moved, to, white_king_sq, black_king_sq);
        acc_sub(child, castle.rook, castle.rook_from, white_king_sq, black_king_sq);
        acc_add(child, castle.rook, castle.rook_to, white_king_sq, black_king_sq);
        return;
    }

    if (captured != NONE_PIECE && !u.was_ep)
        acc_sub(child, captured, to, white_king_sq, black_king_sq);

    acc_sub(child, moved, from, white_king_sq, black_king_sq);
    if (promo != NONE_PTYPE) acc_add(child, promotion_piece(stm, promo), to, white_king_sq, black_king_sq);
    else                     acc_add(child, moved, to, white_king_sq, black_king_sq);
}

} // namespace NNUE

} // namespace SHAYVERI
