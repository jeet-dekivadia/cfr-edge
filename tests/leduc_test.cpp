#include <gtest/gtest.h>
#include "leduc.h"
#include "leduc_utils.h"
#include <algorithm>
#include <set>

using namespace cfr;
using namespace cfr::leduc;

// ---------- Constants ----------

TEST(Leduc, Constants) {
    EXPECT_EQ(JACK, 0);
    EXPECT_EQ(QUEEN, 1);
    EXPECT_EQ(KING, 2);
    EXPECT_EQ(DECK_SIZE, 6);
    EXPECT_EQ(CARDS_PER_RANK, 2);
}

// ---------- rank_name ----------

TEST(Leduc, RankNames) {
    EXPECT_EQ(rank_name(JACK), 'J');
    EXPECT_EQ(rank_name(QUEEN), 'Q');
    EXPECT_EQ(rank_name(KING), 'K');
}

// ---------- card_rank ----------

TEST(Leduc, CardRank) {
    // Cards 0,1 are Jacks; 2,3 are Queens; 4,5 are Kings
    EXPECT_EQ(card_rank(0), 0);
    EXPECT_EQ(card_rank(1), 0);
    EXPECT_EQ(card_rank(2), 1);
    EXPECT_EQ(card_rank(3), 1);
    EXPECT_EQ(card_rank(4), 2);
    EXPECT_EQ(card_rank(5), 2);
}

// ---------- all_deals ----------

TEST(Leduc, AllDealsCount) {
    auto deals = all_deals();
    // 6 * 5 * 4 = 120 deals (3 distinct cards from 6)
    EXPECT_EQ(deals.size(), 120u);
}

TEST(Leduc, AllDealsNoDuplicateCards) {
    auto deals = all_deals();
    for (const auto& d : deals) {
        EXPECT_NE(d.cards[0], d.cards[1]);
        EXPECT_NE(d.cards[0], d.cards[2]);
        EXPECT_NE(d.cards[1], d.cards[2]);
    }
}

TEST(Leduc, AllDealsProbabilitySumToOne) {
    auto deals = all_deals();
    double sum = 0.0;
    for (const auto& d : deals) sum += d.prob;
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

// ---------- round_over ----------

TEST(Leduc, RoundOverCheckCheck) {
    bool folded = false;
    EXPECT_TRUE(round_over("cc", folded));
    EXPECT_FALSE(folded);
}

TEST(Leduc, RoundOverBetCall) {
    bool folded = false;
    EXPECT_TRUE(round_over("bc", folded));
    EXPECT_FALSE(folded);
}

TEST(Leduc, RoundOverFold) {
    bool folded = false;
    EXPECT_TRUE(round_over("bf", folded));
    EXPECT_TRUE(folded);
}

TEST(Leduc, RoundOverCheckBetCall) {
    bool folded = false;
    EXPECT_TRUE(round_over("cbc", folded));
    EXPECT_FALSE(folded);
}

TEST(Leduc, RoundNotOverSingleAction) {
    bool folded = false;
    EXPECT_FALSE(round_over("c", folded));
    EXPECT_FALSE(round_over("b", folded));
}

TEST(Leduc, RoundNotOverEmpty) {
    bool folded = false;
    EXPECT_FALSE(round_over("", folded));
}

TEST(Leduc, RoundOverBetBetCall) {
    // b (bet), b (raise), c (call raise)
    bool folded = false;
    // "bbc" - last char 'c', second-to-last 'b' -> round over
    EXPECT_TRUE(round_over("bbc", folded));
    EXPECT_FALSE(folded);
}

// ---------- showdown_winner ----------

TEST(Leduc, ShowdownPairBeatsHighCard) {
    // P0 has J (rank 0), P1 has Q (rank 1), board is J (rank 0)
    // P0 pairs with board -> P0 wins
    int cards1[3] = {0, 2, 1};  // J0, Q0, J1
    EXPECT_EQ(showdown_winner(cards1), +1);
}

TEST(Leduc, ShowdownHigherRankWins) {
    // P0 has Q, P1 has J, board is K -> no pairs, Q > J
    int cards[3] = {2, 0, 4};  // Q0, J0, K0
    EXPECT_EQ(showdown_winner(cards), +1);
}

TEST(Leduc, ShowdownLowerRankLoses) {
    // P0 has J, P1 has K, board is Q -> no pairs, J < K
    int cards[3] = {0, 4, 2};  // J0, K0, Q0
    EXPECT_EQ(showdown_winner(cards), -1);
}

TEST(Leduc, ShowdownSameRankTies) {
    // P0 has J0, P1 has J1, board is Q -> same rank, tie
    int cards[3] = {0, 1, 2};  // J0, J1, Q0
    EXPECT_EQ(showdown_winner(cards), 0);
}

TEST(Leduc, ShowdownP1PairWins) {
    // P0 has J, P1 has K, board is K -> P1 pairs
    int cards[3] = {0, 4, 5};  // J0, K0, K1
    EXPECT_EQ(showdown_winner(cards), -1);
}

// ---------- acting_player_in_round ----------

TEST(Leduc, ActingPlayerAlternates) {
    EXPECT_EQ(acting_player_in_round(""), 0);
    EXPECT_EQ(acting_player_in_round("c"), 1);
    EXPECT_EQ(acting_player_in_round("cb"), 0);
    EXPECT_EQ(acting_player_in_round("cbc"), 1);
}

// ---------- Training ----------

TEST(Leduc, TrainingReducesExploitability) {
    InfoMap nodes;
    auto curve = train(nodes, 200, Mode::CFR, 50);
    EXPECT_FALSE(curve.empty());
    EXPECT_LT(curve.back().second, curve.front().second);
}

TEST(Leduc, ExploitabilityNonNegative) {
    InfoMap nodes;
    auto curve = train(nodes, 100, Mode::CFR, 50);
    for (const auto& [iter, expl] : curve)
        EXPECT_GE(expl, 0.0);
}

TEST(Leduc, CorrectInfosetCount) {
    InfoMap nodes;
    train(nodes, 10, Mode::CFR, 10);
    // Leduc has 288 infosets
    EXPECT_EQ(nodes.size(), 288u);
}

TEST(Leduc, DCFRConverges) {
    InfoMap nodes;
    auto curve = train(nodes, 500, Mode::DCFR, 100);
    EXPECT_LT(curve.back().second, 0.1);
}
