#pragma once

// SIMD-accelerated regret matching and strategy normalization
// Uses SSE2/AVX2 when available, scalar fallback otherwise

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <cmath>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
    #define HAS_SSE2 1
    #include <emmintrin.h>
#endif

#if defined(__AVX2__) || (defined(_M_X64) && defined(__AVX2__))
    #define HAS_AVX2 1
    #include <immintrin.h>
#endif

namespace cfr {
namespace simd {

// Batch regret matching: given regret arrays for N nodes with K actions,
// compute strategy arrays. SoA layout assumed.
//
// regrets[action][node] -> strategy[action][node]
//
// Each node independently: strategy[a] = max(regret[a],0) / sum_a max(regret[a],0)
// If sum == 0, uniform 1/K.

inline void batch_regret_match_2actions(const double* __restrict regret0,
                                         const double* __restrict regret1,
                                         double* __restrict strat0,
                                         double* __restrict strat1,
                                         size_t n) {
#if HAS_SSE2
    const __m128d zero = _mm_setzero_pd();
    const __m128d half = _mm_set1_pd(0.5);
    size_t i = 0;
    // process 2 nodes at a time (128-bit = 2 doubles)
    for (; i + 1 < n; i += 2) {
        __m128d r0 = _mm_loadu_pd(regret0 + i);
        __m128d r1 = _mm_loadu_pd(regret1 + i);
        __m128d p0 = _mm_max_pd(r0, zero);
        __m128d p1 = _mm_max_pd(r1, zero);
        __m128d sum = _mm_add_pd(p0, p1);

        // check if sum > 0
        __m128d mask = _mm_cmpgt_pd(sum, zero);
        // safe division: where sum>0 divide, else use 0.5
        __m128d inv = _mm_div_pd(_mm_set1_pd(1.0), _mm_max_pd(sum, _mm_set1_pd(1e-300)));
        __m128d s0 = _mm_mul_pd(p0, inv);
        __m128d s1 = _mm_mul_pd(p1, inv);
        // blend: if sum was 0, use 0.5
        s0 = _mm_or_pd(_mm_and_pd(mask, s0), _mm_andnot_pd(mask, half));
        s1 = _mm_or_pd(_mm_and_pd(mask, s1), _mm_andnot_pd(mask, half));

        _mm_storeu_pd(strat0 + i, s0);
        _mm_storeu_pd(strat1 + i, s1);
    }
    // scalar tail
    for (; i < n; i++) {
        double p0 = std::max(regret0[i], 0.0);
        double p1 = std::max(regret1[i], 0.0);
        double s = p0 + p1;
        if (s > 0) { strat0[i] = p0/s; strat1[i] = p1/s; }
        else       { strat0[i] = 0.5;  strat1[i] = 0.5;  }
    }
#else
    for (size_t i = 0; i < n; i++) {
        double p0 = std::max(regret0[i], 0.0);
        double p1 = std::max(regret1[i], 0.0);
        double s = p0 + p1;
        if (s > 0) { strat0[i] = p0/s; strat1[i] = p1/s; }
        else       { strat0[i] = 0.5;  strat1[i] = 0.5;  }
    }
#endif
}

inline void batch_regret_match_3actions(const double* __restrict r0,
                                         const double* __restrict r1,
                                         const double* __restrict r2,
                                         double* __restrict s0,
                                         double* __restrict s1,
                                         double* __restrict s2,
                                         size_t n) {
#if HAS_SSE2
    const __m128d zero = _mm_setzero_pd();
    const __m128d third = _mm_set1_pd(1.0 / 3.0);
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m128d p0 = _mm_max_pd(_mm_loadu_pd(r0 + i), zero);
        __m128d p1 = _mm_max_pd(_mm_loadu_pd(r1 + i), zero);
        __m128d p2 = _mm_max_pd(_mm_loadu_pd(r2 + i), zero);
        __m128d sum = _mm_add_pd(_mm_add_pd(p0, p1), p2);

        __m128d mask = _mm_cmpgt_pd(sum, zero);
        __m128d inv = _mm_div_pd(_mm_set1_pd(1.0), _mm_max_pd(sum, _mm_set1_pd(1e-300)));

        __m128d out0 = _mm_mul_pd(p0, inv);
        __m128d out1 = _mm_mul_pd(p1, inv);
        __m128d out2 = _mm_mul_pd(p2, inv);

        out0 = _mm_or_pd(_mm_and_pd(mask, out0), _mm_andnot_pd(mask, third));
        out1 = _mm_or_pd(_mm_and_pd(mask, out1), _mm_andnot_pd(mask, third));
        out2 = _mm_or_pd(_mm_and_pd(mask, out2), _mm_andnot_pd(mask, third));

        _mm_storeu_pd(s0 + i, out0);
        _mm_storeu_pd(s1 + i, out1);
        _mm_storeu_pd(s2 + i, out2);
    }
    for (; i < n; i++) {
        double pp0 = std::max(r0[i], 0.0);
        double pp1 = std::max(r1[i], 0.0);
        double pp2 = std::max(r2[i], 0.0);
        double sum = pp0 + pp1 + pp2;
        if (sum > 0) { s0[i]=pp0/sum; s1[i]=pp1/sum; s2[i]=pp2/sum; }
        else         { s0[i]=s1[i]=s2[i]=1.0/3.0; }
    }
#else
    for (size_t i = 0; i < n; i++) {
        double pp0 = std::max(r0[i], 0.0);
        double pp1 = std::max(r1[i], 0.0);
        double pp2 = std::max(r2[i], 0.0);
        double sum = pp0 + pp1 + pp2;
        if (sum > 0) { s0[i]=pp0/sum; s1[i]=pp1/sum; s2[i]=pp2/sum; }
        else         { s0[i]=s1[i]=s2[i]=1.0/3.0; }
    }
#endif
}

// Batch floor at zero (for CFR+)
inline void batch_floor_zero(double* data, size_t n) {
#if HAS_SSE2
    const __m128d zero = _mm_setzero_pd();
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m128d v = _mm_loadu_pd(data + i);
        _mm_storeu_pd(data + i, _mm_max_pd(v, zero));
    }
    for (; i < n; i++)
        data[i] = std::max(data[i], 0.0);
#else
    for (size_t i = 0; i < n; i++)
        data[i] = std::max(data[i], 0.0);
#endif
}

// Batch add: dst[i] += scale * src[i]
inline void batch_axpy(double* __restrict dst, const double* __restrict src,
                       double scale, size_t n) {
#if HAS_SSE2
    const __m128d vs = _mm_set1_pd(scale);
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m128d d = _mm_loadu_pd(dst + i);
        __m128d s = _mm_loadu_pd(src + i);
        _mm_storeu_pd(dst + i, _mm_add_pd(d, _mm_mul_pd(vs, s)));
    }
    for (; i < n; i++)
        dst[i] += scale * src[i];
#else
    for (size_t i = 0; i < n; i++)
        dst[i] += scale * src[i];
#endif
}

} // namespace simd
} // namespace cfr
