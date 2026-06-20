#include <gtest/gtest.h>
#include "infoset.h"
#include <cmath>

using namespace cfr;

// ---------- InfoNode construction ----------

TEST(InfoNode, DefaultConstructsTwoActions) {
    InfoNode node;
    EXPECT_EQ(node.num_actions, 2);
    for (int a = 0; a < MAX_ACTIONS; a++) {
        EXPECT_DOUBLE_EQ(node.regret_sum[a], 0.0);
        EXPECT_DOUBLE_EQ(node.strategy_sum[a], 0.0);
    }
}

TEST(InfoNode, ConstructsWithNActions) {
    InfoNode node(4);
    EXPECT_EQ(node.num_actions, 4);
}

// ---------- get_strategy (regret matching) ----------

TEST(InfoNode, UniformStrategyWhenAllRegretZero) {
    InfoNode node(3);
    double strat[MAX_ACTIONS];
    node.get_strategy(strat);
    for (int a = 0; a < 3; a++)
        EXPECT_NEAR(strat[a], 1.0 / 3.0, 1e-12);
}

TEST(InfoNode, StrategyProportionalToPositiveRegrets) {
    InfoNode node(2);
    node.regret_sum[0] = 3.0;
    node.regret_sum[1] = 1.0;
    double strat[MAX_ACTIONS];
    node.get_strategy(strat);
    EXPECT_NEAR(strat[0], 0.75, 1e-12);
    EXPECT_NEAR(strat[1], 0.25, 1e-12);
}

TEST(InfoNode, NegativeRegretsClampedToZeroInStrategy) {
    InfoNode node(3);
    node.regret_sum[0] = -5.0;
    node.regret_sum[1] = 10.0;
    node.regret_sum[2] = 0.0;
    double strat[MAX_ACTIONS];
    node.get_strategy(strat);
    EXPECT_NEAR(strat[0], 0.0, 1e-12);
    EXPECT_NEAR(strat[1], 1.0, 1e-12);
    EXPECT_NEAR(strat[2], 0.0, 1e-12);
}

TEST(InfoNode, AllNegativeRegretsFallBackToUniform) {
    InfoNode node(2);
    node.regret_sum[0] = -10.0;
    node.regret_sum[1] = -3.0;
    double strat[MAX_ACTIONS];
    node.get_strategy(strat);
    EXPECT_NEAR(strat[0], 0.5, 1e-12);
    EXPECT_NEAR(strat[1], 0.5, 1e-12);
}

TEST(InfoNode, StrategySumsToOne) {
    InfoNode node(5);
    node.regret_sum[0] = 1.0;
    node.regret_sum[1] = 2.0;
    node.regret_sum[2] = 3.0;
    node.regret_sum[3] = 4.0;
    node.regret_sum[4] = 5.0;
    double strat[MAX_ACTIONS];
    node.get_strategy(strat);
    double sum = 0.0;
    for (int a = 0; a < 5; a++) sum += strat[a];
    EXPECT_NEAR(sum, 1.0, 1e-12);
}

// ---------- get_average_strategy ----------

TEST(InfoNode, AverageStrategyUniformWhenNoAccumulation) {
    InfoNode node(3);
    double avg[MAX_ACTIONS];
    node.get_average_strategy(avg);
    for (int a = 0; a < 3; a++)
        EXPECT_NEAR(avg[a], 1.0 / 3.0, 1e-12);
}

TEST(InfoNode, AverageStrategyNormalized) {
    InfoNode node(2);
    node.strategy_sum[0] = 100.0;
    node.strategy_sum[1] = 300.0;
    double avg[MAX_ACTIONS];
    node.get_average_strategy(avg);
    EXPECT_NEAR(avg[0], 0.25, 1e-12);
    EXPECT_NEAR(avg[1], 0.75, 1e-12);
}

TEST(InfoNode, AverageStrategySumsToOne) {
    InfoNode node(4);
    node.strategy_sum[0] = 10.0;
    node.strategy_sum[1] = 20.0;
    node.strategy_sum[2] = 30.0;
    node.strategy_sum[3] = 40.0;
    double avg[MAX_ACTIONS];
    node.get_average_strategy(avg);
    double sum = 0.0;
    for (int a = 0; a < 4; a++) sum += avg[a];
    EXPECT_NEAR(sum, 1.0, 1e-12);
}

// ---------- floor_regrets (CFR+) ----------

TEST(InfoNode, FloorRegretsZeroesNegatives) {
    InfoNode node(3);
    node.regret_sum[0] = -5.0;
    node.regret_sum[1] = 3.0;
    node.regret_sum[2] = -0.1;
    node.floor_regrets();
    EXPECT_DOUBLE_EQ(node.regret_sum[0], 0.0);
    EXPECT_DOUBLE_EQ(node.regret_sum[1], 3.0);
    EXPECT_DOUBLE_EQ(node.regret_sum[2], 0.0);
}

TEST(InfoNode, FloorRegretsPreservesPositives) {
    InfoNode node(2);
    node.regret_sum[0] = 7.5;
    node.regret_sum[1] = 0.0;
    node.floor_regrets();
    EXPECT_DOUBLE_EQ(node.regret_sum[0], 7.5);
    EXPECT_DOUBLE_EQ(node.regret_sum[1], 0.0);
}

// ---------- InfoMap ----------

TEST(InfoMap, InsertAndRetrieve) {
    InfoMap nodes;
    nodes.emplace("K:pb", InfoNode(2));
    EXPECT_EQ(nodes.count("K:pb"), 1u);
    EXPECT_EQ(nodes.at("K:pb").num_actions, 2);
}

TEST(InfoMap, MissingKeyNotFound) {
    InfoMap nodes;
    EXPECT_EQ(nodes.count("nonexistent"), 0u);
}
