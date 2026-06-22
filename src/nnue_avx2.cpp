#include "nnue.h"

#ifdef __AVX2__

#include <immintrin.h>

namespace SHAYVERI {
namespace NNUE {

namespace {

I64 hsum256_epi64(__m256i v) {
    __m128i lo = _mm256_castsi256_si128(v);
    __m128i hi = _mm256_extracti128_si256(v, 1);
    __m128i s2 = _mm_add_epi64(lo, hi);

    I64 a = _mm_extract_epi64(s2, 0);
    I64 b = _mm_extract_epi64(s2, 1);
    return a + b;
}

__m256i mullo_epi32_to_epi64(__m256i a, __m256i b) {
    __m256i even = _mm256_mul_epi32(a, b);
    __m256i odd = _mm256_mul_epi32(_mm256_srli_epi64(a, 32),
                                   _mm256_srli_epi64(b, 32));
    return _mm256_add_epi64(even, odd);
}

__m256i div255_epi32(__m256i x) {
    // Exact floor(x / 255) for x in [0, 255 * 255].
    const __m256i one = _mm256_set1_epi32(1);
    __m256i t = _mm256_add_epi32(x, one);
    t = _mm256_add_epi32(t, _mm256_srli_epi32(x, 8));
    return _mm256_srli_epi32(t, 8);
}

__m256i crelu16_to_i32(__m128i x) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i max = _mm_set1_epi16(static_cast<I16>(L1_SCALE));
    x = _mm_min_epi16(_mm_max_epi16(x, zero), max);
    return _mm256_cvtepi16_epi32(x);
}

__m256i screlu16_to_i32(__m128i x) {
    __m256i y = crelu16_to_i32(x);
    y = _mm256_mullo_epi32(y, y);
    return div255_epi32(y);
}

} // namespace

int evaluate_avx2(int side_to_move, const Accumulator &acc) {
    const I16 *stm_acc = acc.vals[side_to_move];
    const I16 *nstm_acc = acc.vals[side_to_move ^ 1];

    __m256i sum_stm = _mm256_setzero_si256();
    __m256i sum_nstm = _mm256_setzero_si256();
    const bool use_screlu = uses_screlu();

    for (int i = 0; i < HIDDEN_SIZE; i += 8) {
        __m128i stm = _mm_loadu_si128(reinterpret_cast<const __m128i *>(stm_acc + i));
        __m128i stm_w = _mm_loadu_si128(reinterpret_cast<const __m128i *>(output_weights + i));

        __m256i stm32 = use_screlu ? screlu16_to_i32(stm) : crelu16_to_i32(stm);
        __m256i stm_w32 = _mm256_cvtepi16_epi32(stm_w);
        sum_stm = _mm256_add_epi64(sum_stm, mullo_epi32_to_epi64(stm32, stm_w32));

        __m128i nstm = _mm_loadu_si128(reinterpret_cast<const __m128i *>(nstm_acc + i));
        __m128i nstm_w = _mm_loadu_si128(
            reinterpret_cast<const __m128i *>(output_weights + HIDDEN_SIZE + i));

        __m256i nstm32 = use_screlu ? screlu16_to_i32(nstm) : crelu16_to_i32(nstm);
        __m256i nstm_w32 = _mm256_cvtepi16_epi32(nstm_w);
        sum_nstm = _mm256_add_epi64(sum_nstm, mullo_epi32_to_epi64(nstm32, nstm_w32));
    }

    I64 total = hsum256_epi64(sum_stm) + hsum256_epi64(sum_nstm);
    total += output_bias;
    int score = static_cast<int>(
        total * OUTPUT_SCALE / (static_cast<I64>(L1_SCALE) * L1_SCALE));
    return score;
}

} // namespace NNUE

} // namespace SHAYVERI

#endif // __AVX2__
