#include <gtest/gtest.h>
#include "simd_utils.h"
#include <vector>
#include <cmath>

using namespace cfr::simd;

// ---------- batch_regret_match_2actions ----------

TEST(SimdUtils, BatchRegretMatch2Uniform) {
    const size_t N = 8;
    std::vector<double> r0(N, 0.0), r1(N, 0.0);
    std::vector<double> s0(N), s1(N);
    batch_regret_match_2actions(r0.data(), r1.data(), s0.data(), s1.data(), N);
    for (size_t i = 0; i < N; i++) {
        EXPECT_NEAR(s0[i], 0.5, 1e-10);
        EXPECT_NEAR(s1[i], 0.5, 1e-10);
    }
}

TEST(SimdUtils, BatchRegretMatch2Proportional) {
    const size_t N = 5;
    std::vector<double> r0 = {3.0, 0.0, 10.0, -1.0, 5.0};
    std::vector<double> r1 = {1.0, 0.0,  0.0, -1.0, 5.0};
    std::vector<double> s0(N), s1(N);
    batch_regret_match_2actions(r0.data(), r1.data(), s0.data(), s1.data(), N);
    // Node 0: 3/(3+1) = 0.75
    EXPECT_NEAR(s0[0], 0.75, 1e-10);
    EXPECT_NEAR(s1[0], 0.25, 1e-10);
    // Node 1: both zero -> uniform
    EXPECT_NEAR(s0[1], 0.5, 1e-10);
    EXPECT_NEAR(s1[1], 0.5, 1e-10);
    // Node 2: 10/(10+0) = 1.0
    EXPECT_NEAR(s0[2], 1.0, 1e-10);
    EXPECT_NEAR(s1[2], 0.0, 1e-10);
    // Node 3: both negative -> uniform
    EXPECT_NEAR(s0[3], 0.5, 1e-10);
    EXPECT_NEAR(s1[3], 0.5, 1e-10);
    // Node 4: 5/(5+5) = 0.5
    EXPECT_NEAR(s0[4], 0.5, 1e-10);
    EXPECT_NEAR(s1[4], 0.5, 1e-10);
}

TEST(SimdUtils, BatchRegretMatch2LargeN) {
    const size_t N = 1000;
    std::vector<double> r0(N), r1(N);
    std::vector<double> s0(N), s1(N);
    for (size_t i = 0; i < N; i++) {
        r0[i] = static_cast<double>(i);
        r1[i] = static_cast<double>(N - i);
    }
    batch_regret_match_2actions(r0.data(), r1.data(), s0.data(), s1.data(), N);
    for (size_t i = 0; i < N; i++) {
        EXPECT_NEAR(s0[i] + s1[i], 1.0, 1e-10);
        EXPECT_GE(s0[i], 0.0);
        EXPECT_GE(s1[i], 0.0);
    }
}

// ---------- batch_regret_match_3actions ----------

TEST(SimdUtils, BatchRegretMatch3Uniform) {
    const size_t N = 7;
    std::vector<double> r0(N, 0.0), r1(N, 0.0), r2(N, 0.0);
    std::vector<double> s0(N), s1(N), s2(N);
    batch_regret_match_3actions(r0.data(), r1.data(), r2.data(),
                                 s0.data(), s1.data(), s2.data(), N);
    for (size_t i = 0; i < N; i++) {
        EXPECT_NEAR(s0[i], 1.0/3.0, 1e-10);
        EXPECT_NEAR(s1[i], 1.0/3.0, 1e-10);
        EXPECT_NEAR(s2[i], 1.0/3.0, 1e-10);
    }
}

TEST(SimdUtils, BatchRegretMatch3SumsToOne) {
    const size_t N = 10;
    std::vector<double> r0(N), r1(N), r2(N);
    std::vector<double> s0(N), s1(N), s2(N);
    for (size_t i = 0; i < N; i++) {
        r0[i] = static_cast<double>(i);
        r1[i] = static_cast<double>(i * 2);
        r2[i] = -static_cast<double>(i);
    }
    batch_regret_match_3actions(r0.data(), r1.data(), r2.data(),
                                 s0.data(), s1.data(), s2.data(), N);
    for (size_t i = 0; i < N; i++) {
        EXPECT_NEAR(s0[i] + s1[i] + s2[i], 1.0, 1e-10);
    }
}

// ---------- batch_floor_zero ----------

TEST(SimdUtils, BatchFloorZero) {
    std::vector<double> data = {-5.0, 3.0, -0.1, 0.0, 7.5, -100.0, 1e-15, -1e-15};
    batch_floor_zero(data.data(), data.size());
    EXPECT_DOUBLE_EQ(data[0], 0.0);
    EXPECT_DOUBLE_EQ(data[1], 3.0);
    EXPECT_DOUBLE_EQ(data[2], 0.0);
    EXPECT_DOUBLE_EQ(data[3], 0.0);
    EXPECT_DOUBLE_EQ(data[4], 7.5);
    EXPECT_DOUBLE_EQ(data[5], 0.0);
    EXPECT_DOUBLE_EQ(data[6], 1e-15);
    EXPECT_DOUBLE_EQ(data[7], 0.0);
}

TEST(SimdUtils, BatchFloorZeroLargeN) {
    const size_t N = 1024;
    std::vector<double> data(N);
    for (size_t i = 0; i < N; i++)
        data[i] = (i % 2 == 0) ? -static_cast<double>(i) : static_cast<double>(i);
    batch_floor_zero(data.data(), N);
    for (size_t i = 0; i < N; i++) {
        EXPECT_GE(data[i], 0.0);
        if (i % 2 == 1) EXPECT_DOUBLE_EQ(data[i], static_cast<double>(i));
    }
}

// ---------- batch_discount_regrets ----------

TEST(SimdUtils, BatchDiscountRegrets) {
    std::vector<double> data = {10.0, -5.0, 0.0, 20.0, -1.0};
    batch_discount_regrets(data.data(), 0.8, data.size());
    EXPECT_NEAR(data[0], 8.0, 1e-10);    // 10 * 0.8
    EXPECT_DOUBLE_EQ(data[1], 0.0);       // negative -> 0
    EXPECT_DOUBLE_EQ(data[2], 0.0);       // zero -> 0
    EXPECT_NEAR(data[3], 16.0, 1e-10);   // 20 * 0.8
    EXPECT_DOUBLE_EQ(data[4], 0.0);       // negative -> 0
}

// ---------- batch_axpy ----------

TEST(SimdUtils, BatchAxpy) {
    std::vector<double> dst = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> src = {10.0, 20.0, 30.0, 40.0, 50.0};
    batch_axpy(dst.data(), src.data(), 0.5, dst.size());
    EXPECT_NEAR(dst[0], 6.0, 1e-10);   // 1 + 0.5*10
    EXPECT_NEAR(dst[1], 12.0, 1e-10);  // 2 + 0.5*20
    EXPECT_NEAR(dst[2], 18.0, 1e-10);  // 3 + 0.5*30
    EXPECT_NEAR(dst[3], 24.0, 1e-10);  // 4 + 0.5*40
    EXPECT_NEAR(dst[4], 30.0, 1e-10);  // 5 + 0.5*50
}

TEST(SimdUtils, BatchAxpyLargeN) {
    const size_t N = 1000;
    std::vector<double> dst(N, 1.0), src(N, 2.0);
    batch_axpy(dst.data(), src.data(), 3.0, N);
    for (size_t i = 0; i < N; i++)
        EXPECT_NEAR(dst[i], 7.0, 1e-10);  // 1 + 3*2
}
