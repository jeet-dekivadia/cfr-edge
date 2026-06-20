#include <gtest/gtest.h>
#include "soa_store.h"
#include <cmath>
#include <string>

using namespace cfr;

// ---------- get_or_add ----------

TEST(SoAStore, GetOrAddNewNode) {
    SoAStore store;
    auto [g, idx] = store.get_or_add("K:pb", 2);
    EXPECT_EQ(g, 1);  // group = num_actions - 1 = 1
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(store.total_nodes(), 1u);
}

TEST(SoAStore, GetOrAddDuplicateReturnsSame) {
    SoAStore store;
    auto [g1, i1] = store.get_or_add("key1", 2);
    auto [g2, i2] = store.get_or_add("key1", 2);
    EXPECT_EQ(g1, g2);
    EXPECT_EQ(i1, i2);
    EXPECT_EQ(store.total_nodes(), 1u);
}

TEST(SoAStore, MultipleNodesMultipleGroups) {
    SoAStore store;
    store.get_or_add("a", 2);
    store.get_or_add("b", 3);
    store.get_or_add("c", 2);
    EXPECT_EQ(store.total_nodes(), 3u);
}

// ---------- get_strategy / batch_compute_strategies ----------

TEST(SoAStore, UniformStrategyOnFreshNode) {
    SoAStore store;
    auto [g, idx] = store.get_or_add("node1", 2);
    store.batch_compute_strategies();
    double s0 = store.get_strategy(g, idx, 0);
    double s1 = store.get_strategy(g, idx, 1);
    EXPECT_NEAR(s0, 0.5, 1e-10);
    EXPECT_NEAR(s1, 0.5, 1e-10);
}

TEST(SoAStore, RegretMatchingInBatch) {
    SoAStore store;
    auto [g, idx] = store.get_or_add("node1", 2);
    store.add_regret(g, idx, 0, 3.0);
    store.add_regret(g, idx, 1, 1.0);
    store.batch_compute_strategies();
    EXPECT_NEAR(store.get_strategy(g, idx, 0), 0.75, 1e-10);
    EXPECT_NEAR(store.get_strategy(g, idx, 1), 0.25, 1e-10);
}

TEST(SoAStore, ThreeActionRegretMatching) {
    SoAStore store;
    auto [g, idx] = store.get_or_add("n3", 3);
    store.add_regret(g, idx, 0, 2.0);
    store.add_regret(g, idx, 1, 2.0);
    store.add_regret(g, idx, 2, 6.0);
    store.batch_compute_strategies();
    EXPECT_NEAR(store.get_strategy(g, idx, 0), 0.2, 1e-10);
    EXPECT_NEAR(store.get_strategy(g, idx, 1), 0.2, 1e-10);
    EXPECT_NEAR(store.get_strategy(g, idx, 2), 0.6, 1e-10);
}

// ---------- get_average_strategy ----------

TEST(SoAStore, AverageStrategyUniformWhenEmpty) {
    SoAStore store;
    auto [g, idx] = store.get_or_add("node1", 2);
    double avg[2];
    store.get_average_strategy(g, idx, avg);
    EXPECT_NEAR(avg[0], 0.5, 1e-10);
    EXPECT_NEAR(avg[1], 0.5, 1e-10);
}

TEST(SoAStore, AverageStrategyAfterAccumulation) {
    SoAStore store;
    auto [g, idx] = store.get_or_add("node1", 2);
    store.add_strategy_sum(g, idx, 0, 10.0);
    store.add_strategy_sum(g, idx, 1, 30.0);
    double avg[2];
    store.get_average_strategy(g, idx, avg);
    EXPECT_NEAR(avg[0], 0.25, 1e-10);
    EXPECT_NEAR(avg[1], 0.75, 1e-10);
}

// ---------- batch_floor_regrets ----------

TEST(SoAStore, FloorRegretsZeroesNegatives) {
    SoAStore store;
    auto [g, idx] = store.get_or_add("node1", 2);
    store.add_regret(g, idx, 0, -5.0);
    store.add_regret(g, idx, 1, 3.0);
    store.batch_floor_regrets();
    // After flooring, compute strategies again
    store.batch_compute_strategies();
    // Only action 1 has positive regret -> strategy should be (0, 1)
    EXPECT_NEAR(store.get_strategy(g, idx, 0), 0.0, 1e-10);
    EXPECT_NEAR(store.get_strategy(g, idx, 1), 1.0, 1e-10);
}

// ---------- batch_discount_regrets ----------

TEST(SoAStore, DiscountRegrets) {
    SoAStore store;
    auto [g, idx] = store.get_or_add("node1", 2);
    store.add_regret(g, idx, 0, 10.0);
    store.add_regret(g, idx, 1, -5.0);
    store.batch_discount_regrets(0.8);
    store.batch_compute_strategies();
    // Positive regret discounted to 10*0.8=8.0, negative to 0
    // Strategy: (8/(8+0), 0/(8+0)) = (1, 0)
    EXPECT_NEAR(store.get_strategy(g, idx, 0), 1.0, 1e-10);
    EXPECT_NEAR(store.get_strategy(g, idx, 1), 0.0, 1e-10);
}

// ---------- find ----------

TEST(SoAStore, FindExistingKey) {
    SoAStore store;
    store.get_or_add("mykey", 2);
    int g, idx;
    EXPECT_TRUE(store.find("mykey", g, idx));
    EXPECT_EQ(g, 1);
    EXPECT_EQ(idx, 0);
}

TEST(SoAStore, FindMissingKey) {
    SoAStore store;
    int g, idx;
    EXPECT_FALSE(store.find("nonexistent", g, idx));
}

// ---------- all_keys ----------

TEST(SoAStore, AllKeysReturnsInserted) {
    SoAStore store;
    store.get_or_add("a", 2);
    store.get_or_add("b", 3);
    auto& keys = store.all_keys();
    EXPECT_EQ(keys.size(), 2u);
    EXPECT_NE(keys.find("a"), keys.end());
    EXPECT_NE(keys.find("b"), keys.end());
}

// ---------- num_actions ----------

TEST(SoAStore, NumActionsForGroup) {
    SoAStore store;
    EXPECT_EQ(store.num_actions(0), 1);
    EXPECT_EQ(store.num_actions(1), 2);
    EXPECT_EQ(store.num_actions(2), 3);
}
