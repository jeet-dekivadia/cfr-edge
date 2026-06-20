#include <gtest/gtest.h>
#include "holdem.h"
#include "hand_eval.h"
#include <set>
#include <random>

using namespace cfr;
using namespace cfr::holdem;
using cfr::handeval::make_card;

// ---------- preflop_bucket ----------

TEST(Holdem, PreflopBucketPocketPairs) {
    // AA: rank=12, rank=12 -> pair bucket 0
    EXPECT_EQ(preflop_bucket(make_card(12,0), make_card(12,1)), 0);
    // KK -> bucket 1
    EXPECT_EQ(preflop_bucket(make_card(11,0), make_card(11,1)), 1);
    // 22 -> bucket 12
    EXPECT_EQ(preflop_bucket(make_card(0,0), make_card(0,1)), 12);
}

TEST(Holdem, PreflopBucketSuited) {
    // AKs: bucket 13
    EXPECT_EQ(preflop_bucket(make_card(12,0), make_card(11,0)), 13);
}

TEST(Holdem, PreflopBucketOffsuit) {
    // AKo: bucket 91
    EXPECT_EQ(preflop_bucket(make_card(12,0), make_card(11,1)), 91);
}

TEST(Holdem, PreflopBucketRange) {
    // All buckets should be in [0, 168]
    for (int c0 = 0; c0 < 52; c0++) {
        for (int c1 = c0 + 1; c1 < 52; c1++) {
            int b = preflop_bucket(c0, c1);
            EXPECT_GE(b, 0);
            EXPECT_LE(b, 168);
        }
    }
}

TEST(Holdem, PreflopBucket169Classes) {
    // There should be exactly 169 unique preflop classes
    std::set<int> seen;
    for (int c0 = 0; c0 < 52; c0++)
        for (int c1 = c0 + 1; c1 < 52; c1++)
            seen.insert(preflop_bucket(c0, c1));
    EXPECT_EQ(seen.size(), 169u);
}

TEST(Holdem, PreflopBucketSymmetric) {
    // bucket(c0,c1) == bucket(c1,c0)
    EXPECT_EQ(preflop_bucket(make_card(12,0), make_card(11,1)),
              preflop_bucket(make_card(11,1), make_card(12,0)));
}

// ---------- preflop_bucket_str ----------

TEST(Holdem, PreflopBucketStrPairs) {
    EXPECT_EQ(preflop_bucket_str(0), "AA");
    EXPECT_EQ(preflop_bucket_str(1), "KK");
    EXPECT_EQ(preflop_bucket_str(12), "22");
}

TEST(Holdem, PreflopBucketStrSuited) {
    EXPECT_EQ(preflop_bucket_str(13), "AKs");
}

TEST(Holdem, PreflopBucketStrOffsuit) {
    EXPECT_EQ(preflop_bucket_str(91), "AKo");
}

// ---------- postflop_bucket ----------

TEST(Holdem, PostflopBucketRange) {
    const int board[3] = {make_card(10,0), make_card(9,1), make_card(8,2)};
    for (int c0 = 0; c0 < 52; c0++) {
        bool skip = false;
        for (int i = 0; i < 3; i++) if (c0 == board[i]) { skip = true; break; }
        if (skip) continue;
        for (int c1 = c0 + 1; c1 < 52; c1++) {
            skip = false;
            for (int i = 0; i < 3; i++) if (c1 == board[i]) { skip = true; break; }
            if (skip) continue;
            int b = postflop_bucket(c0, c1, board, 3);
            EXPECT_GE(b, 0);
            EXPECT_LT(b, NUM_POSTFLOP_BUCKETS);
        }
    }
}

// ---------- make_holdem_key ----------

TEST(Holdem, MakeHoldemKeyPreflop) {
    std::string key = make_holdem_key(13, 0, "x1");
    EXPECT_EQ(key, "PF13:x1");
}

TEST(Holdem, MakeHoldemKeyPostflop) {
    std::string key = make_holdem_key(5, 1, "xc");
    EXPECT_EQ(key, "B5:flop:xc");
}

TEST(Holdem, MakeHoldemKeyTurn) {
    std::string key = make_holdem_key(3, 2, "");
    EXPECT_EQ(key, "B3:turn:");
}

TEST(Holdem, MakeHoldemKeyRiver) {
    std::string key = make_holdem_key(0, 3, "x");
    EXPECT_EQ(key, "B0:river:x");
}

// ---------- sample_deal ----------

TEST(Holdem, SampleDealDistinctCards) {
    std::mt19937 rng(42);
    for (int trial = 0; trial < 100; trial++) {
        auto deal = sample_deal(rng);
        std::set<int> cards;
        cards.insert(deal.hole[0][0]);
        cards.insert(deal.hole[0][1]);
        cards.insert(deal.hole[1][0]);
        cards.insert(deal.hole[1][1]);
        for (int i = 0; i < 5; i++) cards.insert(deal.board[i]);
        EXPECT_EQ(cards.size(), 9u);  // 2+2+5 = 9 distinct cards
    }
}

TEST(Holdem, SampleDealCardRanges) {
    std::mt19937 rng(42);
    auto deal = sample_deal(rng);
    for (int p = 0; p < 2; p++)
        for (int c = 0; c < 2; c++) {
            EXPECT_GE(deal.hole[p][c], 0);
            EXPECT_LT(deal.hole[p][c], 52);
        }
    for (int i = 0; i < 5; i++) {
        EXPECT_GE(deal.board[i], 0);
        EXPECT_LT(deal.board[i], 52);
    }
}

// ---------- Training (short) ----------

TEST(Holdem, ShortTrainingProducesInfosets) {
    InfoMap nodes;
    auto curve = train_holdem(nodes, 50, Mode::DCFR, 50);
    EXPECT_FALSE(curve.empty());
    EXPECT_GT(nodes.size(), 0u);
}
