#include <gtest/gtest.h>
#include "hand_eval.h"
#include <vector>
#include <algorithm>
#include <set>

using namespace cfr::handeval;

// ---------- Card encoding ----------

TEST(HandEval, MakeCardEncoding) {
    // rank=0 (2), suit=0 (clubs) -> card 0
    EXPECT_EQ(make_card(0, 0), 0);
    // Ace of spades: rank=12, suit=3
    EXPECT_EQ(make_card(12, 3), 51);
}

TEST(HandEval, CardRankExtraction) {
    EXPECT_EQ(card_rank(0), 0);    // 2c
    EXPECT_EQ(card_rank(51), 12);  // As
    EXPECT_EQ(card_rank(4), 1);    // 3c
}

TEST(HandEval, CardSuitExtraction) {
    EXPECT_EQ(card_suit(0), 0);   // clubs
    EXPECT_EQ(card_suit(1), 1);   // diamonds
    EXPECT_EQ(card_suit(2), 2);   // hearts
    EXPECT_EQ(card_suit(3), 3);   // spades
}

TEST(HandEval, CardStr) {
    EXPECT_EQ(card_str(0), "2c");
    EXPECT_EQ(card_str(51), "As");
    EXPECT_EQ(card_str(make_card(12, 0)), "Ac");
    EXPECT_EQ(card_str(make_card(8, 2)), "Th");
}

// ---------- Hand category ----------

TEST(HandEval, CategoryNames) {
    EXPECT_EQ(HandEvaluator::category(1), "Straight Flush");
    EXPECT_EQ(HandEvaluator::category(9), "Straight Flush");
    EXPECT_EQ(HandEvaluator::category(11), "Four of a Kind");
    EXPECT_EQ(HandEvaluator::category(165), "Four of a Kind");
    EXPECT_EQ(HandEvaluator::category(167), "Full House");
    EXPECT_EQ(HandEvaluator::category(321), "Full House");
    EXPECT_EQ(HandEvaluator::category(323), "Flush");
    EXPECT_EQ(HandEvaluator::category(1598), "Flush");
    EXPECT_EQ(HandEvaluator::category(1600), "Straight");
    EXPECT_EQ(HandEvaluator::category(1608), "Straight");
    EXPECT_EQ(HandEvaluator::category(1610), "Three of a Kind");
    EXPECT_EQ(HandEvaluator::category(2466), "Three of a Kind");
    EXPECT_EQ(HandEvaluator::category(2468), "Two Pair");
    EXPECT_EQ(HandEvaluator::category(3324), "Two Pair");
    EXPECT_EQ(HandEvaluator::category(3326), "One Pair");
    EXPECT_EQ(HandEvaluator::category(6184), "One Pair");
    EXPECT_EQ(HandEvaluator::category(6186), "High Card");
    EXPECT_EQ(HandEvaluator::category(7461), "High Card");
}

// ---------- 5-card evaluation ----------

TEST(HandEval, Eval5RoyalFlushBeatsStraightFlush) {
    const auto& ev = get_evaluator();
    // Royal flush: Ac Kc Qc Jc Tc (all clubs)
    int royal[5] = {make_card(12,0), make_card(11,0), make_card(10,0),
                    make_card(9,0), make_card(8,0)};
    // 9-high straight flush: 9c 8c 7c 6c 5c
    int sf9[5] = {make_card(7,0), make_card(6,0), make_card(5,0),
                  make_card(4,0), make_card(3,0)};
    int rank_royal = ev.eval5(royal);
    int rank_sf9 = ev.eval5(sf9);
    EXPECT_LT(rank_royal, rank_sf9);  // lower rank = stronger
}

TEST(HandEval, Eval5FourOfAKindBeatsFullHouse) {
    const auto& ev = get_evaluator();
    // Four aces + king kicker
    int quads[5] = {make_card(12,0), make_card(12,1), make_card(12,2),
                    make_card(12,3), make_card(11,0)};
    // Kings full of aces
    int boat[5] = {make_card(11,0), make_card(11,1), make_card(11,2),
                   make_card(12,0), make_card(12,1)};
    EXPECT_LT(ev.eval5(quads), ev.eval5(boat));
}

TEST(HandEval, Eval5FlushBeatsStraight) {
    const auto& ev = get_evaluator();
    // A-high flush (non-straight): Ac Qc Tc 8c 6c
    int flush[5] = {make_card(12,0), make_card(10,0), make_card(8,0),
                    make_card(6,0), make_card(4,0)};
    // A-high straight (non-flush): As Kd Qh Jc Ts
    int straight[5] = {make_card(12,3), make_card(11,1), make_card(10,2),
                       make_card(9,0), make_card(8,3)};
    EXPECT_LT(ev.eval5(flush), ev.eval5(straight));
}

TEST(HandEval, Eval5StraightBeatsTrips) {
    const auto& ev = get_evaluator();
    // Straight: 9-8-7-6-5 mixed suits
    int straight[5] = {make_card(7,0), make_card(6,1), make_card(5,2),
                       make_card(4,3), make_card(3,0)};
    // Three of a kind: AAA + K + Q
    int trips[5] = {make_card(12,0), make_card(12,1), make_card(12,2),
                    make_card(11,3), make_card(10,0)};
    EXPECT_LT(ev.eval5(straight), ev.eval5(trips));
}

TEST(HandEval, Eval5TripsBeatsTwoPair) {
    const auto& ev = get_evaluator();
    // Trips: 222 + AK
    int trips[5] = {make_card(0,0), make_card(0,1), make_card(0,2),
                    make_card(12,0), make_card(11,0)};
    // Two pair: AAKK + Q
    int twopair[5] = {make_card(12,0), make_card(12,1), make_card(11,0),
                      make_card(11,1), make_card(10,0)};
    EXPECT_LT(ev.eval5(trips), ev.eval5(twopair));
}

TEST(HandEval, Eval5HighPairBeatsHighCard) {
    const auto& ev = get_evaluator();
    // Pair of aces + KQJ
    int pair[5] = {make_card(12,0), make_card(12,1), make_card(11,0),
                   make_card(10,0), make_card(9,0)};
    // High card: AKQJ9 mixed suits
    int highcard[5] = {make_card(12,0), make_card(11,1), make_card(10,2),
                       make_card(9,3), make_card(7,0)};
    EXPECT_LT(ev.eval5(pair), ev.eval5(highcard));
}

TEST(HandEval, Eval5WheelStraightFlush) {
    const auto& ev = get_evaluator();
    // A-2-3-4-5 all clubs (wheel straight flush)
    int wheel_sf[5] = {make_card(12,0), make_card(0,0), make_card(1,0),
                       make_card(2,0), make_card(3,0)};
    int rank = ev.eval5(wheel_sf);
    EXPECT_LE(rank, 10);  // straight flush range 1..10
    EXPECT_GE(rank, 1);
}

// ---------- 7-card evaluation ----------

TEST(HandEval, Eval7SelectsBestFive) {
    const auto& ev = get_evaluator();
    // 7 cards containing a royal flush + 2 garbage
    int cards[7] = {make_card(12,0), make_card(11,0), make_card(10,0),
                    make_card(9,0), make_card(8,0),
                    make_card(0,1), make_card(1,2)};
    int rank = ev.eval7(cards);
    EXPECT_LE(rank, 10);  // should find the straight flush
}

TEST(HandEval, Eval7HigherPairBetter) {
    const auto& ev = get_evaluator();
    // Hand 1: pair of aces with random kickers
    int h1[7] = {make_card(12,0), make_card(12,1), make_card(10,2),
                 make_card(8,3), make_card(6,0), make_card(4,1), make_card(2,2)};
    // Hand 2: pair of kings with random kickers
    int h2[7] = {make_card(11,0), make_card(11,1), make_card(10,2),
                 make_card(8,3), make_card(6,0), make_card(4,1), make_card(2,2)};
    EXPECT_LT(ev.eval7(h1), ev.eval7(h2));
}

// ---------- Equity ----------

TEST(HandEval, EquityOnCompleteBoardIsZeroOrOne) {
    const auto& ev = get_evaluator();
    // AA vs KK on a board of Q-J-T-9-2 (no help for either)
    int h1[2] = {make_card(12,0), make_card(12,1)};  // AA
    int h2[2] = {make_card(11,0), make_card(11,1)};  // KK
    int board[5] = {make_card(10,2), make_card(9,3), make_card(8,0),
                    make_card(7,1), make_card(0,2)};
    float eq = ev.equity(h1, h2, board, 5);
    // With a complete board, equity should be 0, 0.5, or 1
    EXPECT_TRUE(eq == 0.0f || eq == 0.5f || eq == 1.0f);
}

TEST(HandEval, EquityRange) {
    const auto& ev = get_evaluator();
    int h1[2] = {make_card(12,0), make_card(12,1)};  // AA
    int h2[2] = {make_card(0,0), make_card(1,1)};    // 23o
    int board[3] = {make_card(10,2), make_card(9,3), make_card(8,0)};
    float eq = ev.equity(h1, h2, board, 3);
    EXPECT_GE(eq, 0.0f);
    EXPECT_LE(eq, 1.0f);
}

// ---------- Singleton ----------

TEST(HandEval, SingletonReturnsConsistentResults) {
    const auto& e1 = get_evaluator();
    const auto& e2 = get_evaluator();
    EXPECT_EQ(&e1, &e2);
}
