#include <gtest/gtest.h>
#include "cfr.h"
#include <vector>
#include <utility>
#include <cmath>

using namespace cfr;

// ---------- estimate_convergence_rate ----------

TEST(BestResponse, ConvergenceRateNeedsFourPoints) {
    // Too few data points
    std::vector<std::pair<int,double>> curve = {{100, 0.1}, {200, 0.05}};
    double rate = estimate_convergence_rate(curve);
    EXPECT_DOUBLE_EQ(rate, 0.0);
}

TEST(BestResponse, ConvergenceRatePositiveForDecreasingCurve) {
    // ε ~ C * T^(-0.5), so rate should be ~0.5
    std::vector<std::pair<int,double>> curve;
    for (int t = 100; t <= 10000; t += 100)
        curve.push_back({t, 10.0 / std::sqrt(static_cast<double>(t))});
    double rate = estimate_convergence_rate(curve);
    EXPECT_NEAR(rate, 0.5, 0.05);
}

TEST(BestResponse, ConvergenceRateForLinear) {
    // ε ~ C * T^(-1.0), so rate should be ~1.0
    std::vector<std::pair<int,double>> curve;
    for (int t = 100; t <= 10000; t += 100)
        curve.push_back({t, 100.0 / static_cast<double>(t)});
    double rate = estimate_convergence_rate(curve);
    EXPECT_NEAR(rate, 1.0, 0.05);
}

TEST(BestResponse, ConvergenceRateZeroForConstant) {
    // Constant exploitability -> rate should be ~0
    std::vector<std::pair<int,double>> curve;
    for (int t = 100; t <= 10000; t += 100)
        curve.push_back({t, 1.0});
    double rate = estimate_convergence_rate(curve);
    EXPECT_NEAR(rate, 0.0, 0.05);
}

// ---------- TrainConfig defaults ----------

TEST(CfrConfig, DefaultValues) {
    TrainConfig cfg;
    EXPECT_EQ(cfg.game, Game::KUHN);
    EXPECT_EQ(cfg.mode, Mode::CFR);
    EXPECT_EQ(cfg.iterations, 10000);
    EXPECT_EQ(cfg.eval_every, 100);
    EXPECT_FALSE(cfg.use_soa);
}

// ---------- run_training integration ----------

TEST(CfrConfig, RunTrainingKuhn) {
    TrainConfig cfg;
    cfg.game = Game::KUHN;
    cfg.mode = Mode::DCFR;
    cfg.iterations = 500;
    cfg.eval_every = 100;
    auto result = run_training(cfg);
    EXPECT_GT(result.num_infosets, 0u);
    EXPECT_GT(result.elapsed_seconds, 0.0);
    EXPECT_FALSE(result.exploit_curve.empty());
    EXPECT_LT(result.final_exploitability, 0.05);
}

TEST(CfrConfig, RunTrainingLeduc) {
    TrainConfig cfg;
    cfg.game = Game::LEDUC;
    cfg.mode = Mode::CFR;
    cfg.iterations = 200;
    cfg.eval_every = 100;
    auto result = run_training(cfg);
    EXPECT_EQ(result.num_infosets, 288u);
    EXPECT_FALSE(result.exploit_curve.empty());
}
