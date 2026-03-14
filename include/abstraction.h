#pragma once

// Card bucketing and action abstraction for Leduc Hold'em.
//
// Card bucketing: maps (private_card, board_card) -> bucket_id
//   - Fine: each (rank, board_rank) pair is its own bucket
//   - Coarse: group by {pair, high_card, low_card}
//   - Custom: user-defined mapping
//
// Action abstraction: reduce the action space
//   - Full: all actions available
//   - Reduced: merge bet/raise into a single aggressive action (no raise distinction)

#include "infoset.h"
#include <string>
#include <vector>
#include <utility>
#include <functional>

namespace cfr {
namespace abstraction {

// ---- Card bucketing ----

enum class BucketScheme {
    NONE,      // no bucketing (identity)
    RANK_ONLY, // use rank only (ignore suit), same as fine for Leduc
    STRENGTH,  // group by hand strength: pair / high / low
};

// Returns bucket id for a Leduc hand
// private_rank in {0=J, 1=Q, 2=K}, board_rank same (-1 if preflop)
int bucket_id(int private_rank, int board_rank, BucketScheme scheme);

// Number of buckets for a scheme (preflop, postflop)
int num_buckets_preflop(BucketScheme scheme);
int num_buckets_postflop(BucketScheme scheme);

// Build abstracted info set key
std::string abstract_info_key(int private_card, int board_card,
                               int round, const std::string& history,
                               BucketScheme scheme);

// ---- Action abstraction ----

enum class ActionScheme {
    FULL,       // fold/check-call/bet-raise (standard)
    NO_RAISE,   // fold/check-call/bet only (no raise allowed, max 1 bet)
};

// Get available actions under abstraction
void get_abstracted_actions(int num_bets, const std::string& round_hist,
                            ActionScheme scheme,
                            int* actions, int& n_actions);

// ---- Abstracted Leduc trainer ----

struct AbstractConfig {
    BucketScheme buckets  = BucketScheme::NONE;
    ActionScheme actions  = ActionScheme::FULL;
    Mode cfr_mode         = Mode::CFR;
    int iterations        = 5000;
    int eval_every        = 200;
};

struct AbstractResult {
    std::vector<std::pair<int,double>> exploit_curve;
    double final_exploit;
    size_t num_abstract_infosets;
    size_t num_original_infosets;
};

// Run abstracted Leduc training and evaluate against the full game
AbstractResult run_abstraction_experiment(const AbstractConfig& config);

} // namespace abstraction
} // namespace cfr
