#include <gtest/gtest.h>
#include "abstraction.h"
#include "leduc.h"

using namespace cfr;
using namespace cfr::abstraction;

// ---------- bucket_id ----------

TEST(Abstraction, BucketIdNonePreflop) {
    EXPECT_EQ(bucket_id(0, -1, BucketScheme::NONE), 0);  // J
    EXPECT_EQ(bucket_id(1, -1, BucketScheme::NONE), 1);  // Q
    EXPECT_EQ(bucket_id(2, -1, BucketScheme::NONE), 2);  // K
}

TEST(Abstraction, BucketIdNonePostflop) {
    // priv_rank * 3 + board_rank
    EXPECT_EQ(bucket_id(0, 0, BucketScheme::NONE), 0);  // J, board J
    EXPECT_EQ(bucket_id(0, 1, BucketScheme::NONE), 1);  // J, board Q
    EXPECT_EQ(bucket_id(2, 2, BucketScheme::NONE), 8);  // K, board K
}

TEST(Abstraction, BucketIdStrengthPair) {
    // pair: priv_rank == board_rank -> bucket 0
    EXPECT_EQ(bucket_id(0, 0, BucketScheme::STRENGTH), 0);
    EXPECT_EQ(bucket_id(1, 1, BucketScheme::STRENGTH), 0);
    EXPECT_EQ(bucket_id(2, 2, BucketScheme::STRENGTH), 0);
}

TEST(Abstraction, BucketIdStrengthHighCard) {
    // priv > board -> bucket 1 (high card)
    EXPECT_EQ(bucket_id(2, 0, BucketScheme::STRENGTH), 1);  // K with board J
    EXPECT_EQ(bucket_id(1, 0, BucketScheme::STRENGTH), 1);  // Q with board J
}

TEST(Abstraction, BucketIdStrengthLowCard) {
    // priv < board -> bucket 2 (low card)
    EXPECT_EQ(bucket_id(0, 1, BucketScheme::STRENGTH), 2);  // J with board Q
    EXPECT_EQ(bucket_id(0, 2, BucketScheme::STRENGTH), 2);  // J with board K
}

// ---------- num_buckets ----------

TEST(Abstraction, NumBucketsPreflop) {
    EXPECT_EQ(num_buckets_preflop(BucketScheme::NONE), 3);
    EXPECT_EQ(num_buckets_preflop(BucketScheme::STRENGTH), 3);
}

TEST(Abstraction, NumBucketsPostflop) {
    EXPECT_EQ(num_buckets_postflop(BucketScheme::NONE), 9);
    EXPECT_EQ(num_buckets_postflop(BucketScheme::RANK_ONLY), 9);
    EXPECT_EQ(num_buckets_postflop(BucketScheme::STRENGTH), 3);
}

// ---------- abstract_info_key ----------

TEST(Abstraction, AbstractInfoKeyPreflop) {
    // Round 0 (preflop), private card 0 (Jack, rank 0), board card irrelevant
    std::string key = abstract_info_key(0, -1, 0, "cb", BucketScheme::NONE);
    EXPECT_EQ(key, "B0:cb");
}

TEST(Abstraction, AbstractInfoKeyPostflop) {
    // Round 1, private card 4 (King, rank 2), board card 2 (Queen, rank 1)
    std::string key = abstract_info_key(4, 2, 1, "cc/", BucketScheme::NONE);
    // bucket = 2 * 3 + 1 = 7
    EXPECT_EQ(key, "B7:cc/");
}

TEST(Abstraction, AbstractInfoKeyStrengthBucket) {
    // Pair: private rank == board rank -> bucket 0
    std::string key = abstract_info_key(0, 1, 1, "cc/", BucketScheme::STRENGTH);
    // card 0 rank = 0, card 1 rank = 0 -> pair -> bucket 0
    EXPECT_EQ(key, "B0:cc/");
}

// ---------- get_abstracted_actions ----------

TEST(Abstraction, FullActionsOpeningNoFold) {
    int actions[3]; int na;
    get_abstracted_actions(0, "", ActionScheme::FULL, actions, na);
    EXPECT_EQ(na, 2);
    EXPECT_EQ(actions[0], leduc::CHECK_CALL);
    EXPECT_EQ(actions[1], leduc::BET_RAISE);
}

TEST(Abstraction, FullActionsFacingBet) {
    int actions[3]; int na;
    get_abstracted_actions(1, "b", ActionScheme::FULL, actions, na);
    EXPECT_EQ(na, 3);
    EXPECT_EQ(actions[0], leduc::FOLD);
    EXPECT_EQ(actions[1], leduc::CHECK_CALL);
    EXPECT_EQ(actions[2], leduc::BET_RAISE);
}

TEST(Abstraction, FullActionsMaxRaisesReached) {
    int actions[3]; int na;
    // MAX_RAISES=2, num_bets=2 -> no more betting
    get_abstracted_actions(2, "bb", ActionScheme::FULL, actions, na);
    EXPECT_EQ(na, 2);
    EXPECT_EQ(actions[0], leduc::FOLD);
    EXPECT_EQ(actions[1], leduc::CHECK_CALL);
}

TEST(Abstraction, NoRaiseSchemeNoBet) {
    int actions[3]; int na;
    get_abstracted_actions(1, "b", ActionScheme::NO_RAISE, actions, na);
    // NO_RAISE caps at 1 bet, so with 1 bet no more bet/raise
    EXPECT_EQ(na, 2);
    EXPECT_EQ(actions[0], leduc::FOLD);
    EXPECT_EQ(actions[1], leduc::CHECK_CALL);
}

// ---------- run_abstraction_experiment ----------

TEST(Abstraction, ExperimentProducesResults) {
    AbstractConfig cfg;
    cfg.buckets = BucketScheme::STRENGTH;
    cfg.actions = ActionScheme::FULL;
    cfg.cfr_mode = Mode::CFR;
    cfg.iterations = 100;
    cfg.eval_every = 50;

    auto result = run_abstraction_experiment(cfg);
    EXPECT_FALSE(result.exploit_curve.empty());
    EXPECT_GT(result.num_abstract_infosets, 0u);
    EXPECT_GT(result.num_original_infosets, 0u);
    // Abstracted game should have fewer infosets than original
    EXPECT_LE(result.num_abstract_infosets, result.num_original_infosets);
}
