#pragma once

// SIMD-accelerated regret matching and strategy normalization.
// Uses AVX2 (256-bit, 4 doubles/cycle) when available,
// falls back to SSE2 (128-bit, 2 doubles/cycle), then scalar.
//
// All functions operate on SoA (Structure-of-Arrays) layouts:
//   regrets[action][node] → strategy[action][node]

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <cmath>

// ---- Feature detection ----
// SSE2 is guaranteed on all x86-64 targets.
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
    #define HAS_SSE2 1
    #include <emmintrin.h>
#endif

// AVX2: available when compiler explicitly enables it (-mavx2 / /arch:AVX2).
// Note: do NOT assume AVX2 from _M_X64 - that only guarantees SSE2.
#if defined(__AVX2__)
    #define HAS_AVX2 1
    #include <immintrin.h>
#endif

namespace cfr {
namespace simd {

// ---- 2-action batch regret matching ----
// For each node i in [0, n): strategy[a][i] = max(regret[a][i], 0) / sum_pos
// If sum_pos == 0: uniform (0.5, 0.5).
inline void batch_regret_match_2actions(const double* __restrict regret0,
                                         const double* __restrict regret1,
                                         double* __restrict strat0,
                                         double* __restrict strat1,
                                         size_t n) {
#if HAS_AVX2
    const __m256d zero  = _mm256_setzero_pd();
    const __m256d half  = _mm256_set1_pd(0.5);
    const __m256d tiny  = _mm256_set1_pd(1e-300);
    const __m256d one   = _mm256_set1_pd(1.0);
    size_t i = 0;
    // Process 4 nodes at a time (256-bit = 4 doubles)
    for (; i + 3 < n; i += 4) {
        __m256d r0  = _mm256_loadu_pd(regret0 + i);
        __m256d r1  = _mm256_loadu_pd(regret1 + i);
        __m256d p0  = _mm256_max_pd(r0, zero);
        __m256d p1  = _mm256_max_pd(r1, zero);
        __m256d sum = _mm256_add_pd(p0, p1);
        // mask: sum > 0
        __m256d mask = _mm256_cmp_pd(sum, zero, _CMP_GT_OQ);
        __m256d inv  = _mm256_div_pd(one, _mm256_max_pd(sum, tiny));
        __m256d s0   = _mm256_mul_pd(p0, inv);
        __m256d s1   = _mm256_mul_pd(p1, inv);
        // blend: where sum==0 use 0.5
        s0 = _mm256_blendv_pd(half, s0, mask);
        s1 = _mm256_blendv_pd(half, s1, mask);
        _mm256_storeu_pd(strat0 + i, s0);
        _mm256_storeu_pd(strat1 + i, s1);
    }
    // scalar tail
    for (; i < n; i++) {
        double p0 = std::max(regret0[i], 0.0);
        double p1 = std::max(regret1[i], 0.0);
        double s  = p0 + p1;
        strat0[i] = (s > 0.0) ? p0 / s : 0.5;
        strat1[i] = (s > 0.0) ? p1 / s : 0.5;
    }
#elif HAS_SSE2
    const __m128d zero = _mm_setzero_pd();
    const __m128d half = _mm_set1_pd(0.5);
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m128d r0  = _mm_loadu_pd(regret0 + i);
        __m128d r1  = _mm_loadu_pd(regret1 + i);
        __m128d p0  = _mm_max_pd(r0, zero);
        __m128d p1  = _mm_max_pd(r1, zero);
        __m128d sum = _mm_add_pd(p0, p1);
        __m128d mask = _mm_cmpgt_pd(sum, zero);
        __m128d inv  = _mm_div_pd(_mm_set1_pd(1.0),
                                   _mm_max_pd(sum, _mm_set1_pd(1e-300)));
        __m128d s0   = _mm_mul_pd(p0, inv);
        __m128d s1   = _mm_mul_pd(p1, inv);
        s0 = _mm_or_pd(_mm_and_pd(mask, s0), _mm_andnot_pd(mask, half));
        s1 = _mm_or_pd(_mm_and_pd(mask, s1), _mm_andnot_pd(mask, half));
        _mm_storeu_pd(strat0 + i, s0);
        _mm_storeu_pd(strat1 + i, s1);
    }
    for (; i < n; i++) {
        double p0 = std::max(regret0[i], 0.0);
        double p1 = std::max(regret1[i], 0.0);
        double s  = p0 + p1;
        strat0[i] = (s > 0.0) ? p0 / s : 0.5;
        strat1[i] = (s > 0.0) ? p1 / s : 0.5;
    }
#else
    for (size_t i = 0; i < n; i++) {
        double p0 = std::max(regret0[i], 0.0);
        double p1 = std::max(regret1[i], 0.0);
        double s  = p0 + p1;
        strat0[i] = (s > 0.0) ? p0 / s : 0.5;
        strat1[i] = (s > 0.0) ? p1 / s : 0.5;
    }
#endif
}

// ---- 3-action batch regret matching ----
inline void batch_regret_match_3actions(const double* __restrict r0,
                                         const double* __restrict r1,
                                         const double* __restrict r2,
                                         double* __restrict s0,
                                         double* __restrict s1,
                                         double* __restrict s2,
                                         size_t n) {
#if HAS_AVX2
    const __m256d zero  = _mm256_setzero_pd();
    const __m256d third = _mm256_set1_pd(1.0 / 3.0);
    const __m256d tiny  = _mm256_set1_pd(1e-300);
    const __m256d one   = _mm256_set1_pd(1.0);
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        __m256d p0  = _mm256_max_pd(_mm256_loadu_pd(r0 + i), zero);
        __m256d p1  = _mm256_max_pd(_mm256_loadu_pd(r1 + i), zero);
        __m256d p2  = _mm256_max_pd(_mm256_loadu_pd(r2 + i), zero);
        __m256d sum = _mm256_add_pd(_mm256_add_pd(p0, p1), p2);
        __m256d mask = _mm256_cmp_pd(sum, zero, _CMP_GT_OQ);
        __m256d inv  = _mm256_div_pd(one, _mm256_max_pd(sum, tiny));
        __m256d o0   = _mm256_mul_pd(p0, inv);
        __m256d o1   = _mm256_mul_pd(p1, inv);
        __m256d o2   = _mm256_mul_pd(p2, inv);
        _mm256_storeu_pd(s0 + i, _mm256_blendv_pd(third, o0, mask));
        _mm256_storeu_pd(s1 + i, _mm256_blendv_pd(third, o1, mask));
        _mm256_storeu_pd(s2 + i, _mm256_blendv_pd(third, o2, mask));
    }
    for (; i < n; i++) {
        double pp0 = std::max(r0[i], 0.0);
        double pp1 = std::max(r1[i], 0.0);
        double pp2 = std::max(r2[i], 0.0);
        double sum = pp0 + pp1 + pp2;
        if (sum > 0.0) { s0[i]=pp0/sum; s1[i]=pp1/sum; s2[i]=pp2/sum; }
        else           { s0[i]=s1[i]=s2[i]=1.0/3.0; }
    }
#elif HAS_SSE2
    const __m128d zero  = _mm_setzero_pd();
    const __m128d third = _mm_set1_pd(1.0 / 3.0);
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m128d p0  = _mm_max_pd(_mm_loadu_pd(r0 + i), zero);
        __m128d p1  = _mm_max_pd(_mm_loadu_pd(r1 + i), zero);
        __m128d p2  = _mm_max_pd(_mm_loadu_pd(r2 + i), zero);
        __m128d sum = _mm_add_pd(_mm_add_pd(p0, p1), p2);
        __m128d mask = _mm_cmpgt_pd(sum, zero);
        __m128d inv  = _mm_div_pd(_mm_set1_pd(1.0),
                                   _mm_max_pd(sum, _mm_set1_pd(1e-300)));
        __m128d o0 = _mm_mul_pd(p0, inv);
        __m128d o1 = _mm_mul_pd(p1, inv);
        __m128d o2 = _mm_mul_pd(p2, inv);
        _mm_storeu_pd(s0 + i, _mm_or_pd(_mm_and_pd(mask,o0), _mm_andnot_pd(mask,third)));
        _mm_storeu_pd(s1 + i, _mm_or_pd(_mm_and_pd(mask,o1), _mm_andnot_pd(mask,third)));
        _mm_storeu_pd(s2 + i, _mm_or_pd(_mm_and_pd(mask,o2), _mm_andnot_pd(mask,third)));
    }
    for (; i < n; i++) {
        double pp0 = std::max(r0[i], 0.0); double pp1 = std::max(r1[i], 0.0);
        double pp2 = std::max(r2[i], 0.0); double sum = pp0+pp1+pp2;
        if (sum>0.0) { s0[i]=pp0/sum; s1[i]=pp1/sum; s2[i]=pp2/sum; }
        else         { s0[i]=s1[i]=s2[i]=1.0/3.0; }
    }
#else
    for (size_t i = 0; i < n; i++) {
        double pp0=std::max(r0[i],0.0), pp1=std::max(r1[i],0.0), pp2=std::max(r2[i],0.0);
        double sum=pp0+pp1+pp2;
        if (sum>0.0) { s0[i]=pp0/sum; s1[i]=pp1/sum; s2[i]=pp2/sum; }
        else         { s0[i]=s1[i]=s2[i]=1.0/3.0; }
    }
#endif
}

// ---- Floor at zero (CFR+) ----
inline void batch_floor_zero(double* data, size_t n) {
#if HAS_AVX2
    const __m256d zero = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 3 < n; i += 4)
        _mm256_storeu_pd(data + i, _mm256_max_pd(_mm256_loadu_pd(data + i), zero));
    for (; i < n; i++) data[i] = std::max(data[i], 0.0);
#elif HAS_SSE2
    const __m128d zero = _mm_setzero_pd();
    size_t i = 0;
    for (; i + 1 < n; i += 2)
        _mm_storeu_pd(data + i, _mm_max_pd(_mm_loadu_pd(data + i), zero));
    for (; i < n; i++) data[i] = std::max(data[i], 0.0);
#else
    for (size_t i = 0; i < n; i++) data[i] = std::max(data[i], 0.0);
#endif
}

// ---- DCFR discount: positive *= factor, negative → 0 ----
// factor = t^alpha / (t^alpha + 1), alpha = 1.5
inline void batch_discount_regrets(double* data, double factor, size_t n) {
#if HAS_AVX2
    const __m256d zero = _mm256_setzero_pd();
    const __m256d vf   = _mm256_set1_pd(factor);
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        __m256d v    = _mm256_loadu_pd(data + i);
        __m256d pos  = _mm256_max_pd(v, zero);      // floor negatives to 0
        __m256d disc = _mm256_mul_pd(pos, vf);       // scale positives
        _mm256_storeu_pd(data + i, disc);
    }
    for (; i < n; i++) data[i] = (data[i] > 0.0) ? data[i] * factor : 0.0;
#elif HAS_SSE2
    const __m128d zero = _mm_setzero_pd();
    const __m128d vf   = _mm_set1_pd(factor);
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m128d v    = _mm_loadu_pd(data + i);
        __m128d pos  = _mm_max_pd(v, zero);
        __m128d disc = _mm_mul_pd(pos, vf);
        _mm_storeu_pd(data + i, disc);
    }
    for (; i < n; i++) data[i] = (data[i] > 0.0) ? data[i] * factor : 0.0;
#else
    for (size_t i = 0; i < n; i++)
        data[i] = (data[i] > 0.0) ? data[i] * factor : 0.0;
#endif
}

// ---- Batch add: dst[i] += scale * src[i] ----
inline void batch_axpy(double* __restrict dst, const double* __restrict src,
                        double scale, size_t n) {
#if HAS_AVX2
    const __m256d vs = _mm256_set1_pd(scale);
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        __m256d d = _mm256_loadu_pd(dst + i);
        __m256d s = _mm256_loadu_pd(src + i);
        _mm256_storeu_pd(dst + i, _mm256_fmadd_pd(vs, s, d));
    }
    for (; i < n; i++) dst[i] += scale * src[i];
#elif HAS_SSE2
    const __m128d vs = _mm_set1_pd(scale);
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m128d d = _mm_loadu_pd(dst + i);
        __m128d s = _mm_loadu_pd(src + i);
        _mm_storeu_pd(dst + i, _mm_add_pd(d, _mm_mul_pd(vs, s)));
    }
    for (; i < n; i++) dst[i] += scale * src[i];
#else
    for (size_t i = 0; i < n; i++) dst[i] += scale * src[i];
#endif
}

} // namespace simd
} // namespace cfr
