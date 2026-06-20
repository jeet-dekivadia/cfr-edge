#include <gtest/gtest.h>
#include "kuhn.h"
#include <cmath>

using namespace cfr;
using namespace cfr::kuhn;

// ---------- card_name ----------

TEST(Kuhn, CardNames) {
    EXPECT_EQ(card_name(JACK), 'J');
    EXPECT_EQ(card_name(QUEEN), 'Q');
    EXPECT_EQ(card_name(KING), 'K');
}

// ---------- CFR traversal produces valid nodes ----------

TEST(Kuhn, TraverseCreatesInfosets) {
    InfoMap nodes;
    cfr_traverse(nodes, JACK, QUEEN, "", 1.0, 1.0, Mode::CFR, 1);
    // At least 2 infosets should be created (J acting, Q acting)
    EXPECT_GE(nodes.size(), 2u);
}

// ---------- Training produces decreasing exploitability ----------

TEST(Kuhn, TrainingReducesExploitability) {
    InfoMap nodes;
    auto curve = train(nodes, 500, Mode::CFR, 100);
    EXPECT_FALSE(curve.empty());
    // Exploitability at the end should be less than at the beginning
    EXPECT_LT(curve.back().second, curve.front().second);
}

TEST(Kuhn, ExploitabilityNonNegative) {
    InfoMap nodes;
    auto curve = train(nodes, 200, Mode::CFR, 50);
    for (const auto& [iter, expl] : curve)
        EXPECT_GE(expl, 0.0);
}

// ---------- CFR+ training ----------

TEST(Kuhn, CFRPlusConverges) {
    InfoMap nodes;
    auto curve = train(nodes, 1000, Mode::CFR_PLUS, 200);
    EXPECT_LT(curve.back().second, 0.05);
}

// ---------- DCFR training ----------

TEST(Kuhn, DCFRConvergesFast) {
    InfoMap nodes;
    auto curve = train(nodes, 1000, Mode::DCFR, 200);
    EXPECT_LT(curve.back().second, 0.01);
}

// ---------- Exploitability of empty nodes ----------

TEST(Kuhn, EmptyNodesExploitability) {
    InfoMap nodes;
    double expl = exploitability(nodes);
    // With uniform strategy, exploitability should be finite and non-negative
    EXPECT_GE(expl, 0.0);
    EXPECT_LT(expl, 10.0);
}

// ---------- Number of infosets in Kuhn ----------

TEST(Kuhn, CorrectInfosetCount) {
    InfoMap nodes;
    train(nodes, 10, Mode::CFR, 10);
    // Kuhn poker has exactly 12 infosets (3 cards x 4 histories each player can see)
    EXPECT_EQ(nodes.size(), 12u);
}

// ---------- Strategy is valid probability distribution ----------

TEST(Kuhn, StrategyIsProbabilistic) {
    InfoMap nodes;
    train(nodes, 200, Mode::CFR, 200);
    for (const auto& [key, node] : nodes) {
        double avg[MAX_ACTIONS];
        node.get_average_strategy(avg);
        double sum = 0.0;
        for (int a = 0; a < node.num_actions; a++) {
            EXPECT_GE(avg[a], 0.0);
            EXPECT_LE(avg[a], 1.0);
            sum += avg[a];
        }
        EXPECT_NEAR(sum, 1.0, 1e-10);
    }
}
