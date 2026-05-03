#pragma once

// Heads-Up No-Limit Texas Hold'em (simplified for CFR training).
//
// Game parameters:
//   Starting stacks: 100 big blinds each
//   Blinds: SB=0.5bb, BB=1bb (player 0 = SB/button pre-flop)
//   Streets: preflop, flop, turn, river
//   Bet sizes (as fraction of pot): {0.33, 0.75, 1.0, all-in}
//   Max bets per street: 4 (open/bet, 3 raises)
//
// Card abstraction:
//   Pre-flop:  169 canonical hand classes (suit isomorphism)
//   Post-flop: EHS² bucketed into NUM_POSTFLOP_BUCKETS bins
//
// MCCFR variant: External Sampling - only sample one deal per iteration,
// traverse the acting player's subtree completely, sample opponent actions.

#include "infoset.h"
#include "hand_eval.h"
#include <string>
#include <vector>
#include <utility>
#include <random>
#include <array>

namespace cfr {
namespace holdem {

// ---- Game constants ----
static constexpr double STARTING_STACK = 100.0;  // big blinds
static constexpr double SMALL_BLIND    = 0.5;
static constexpr double BIG_BLIND      = 1.0;
static constexpr int    MAX_BETS_PER_STREET = 4;
static constexpr int    NUM_STREETS    = 4;  // pre-flop, flop, turn, river
static constexpr int    NUM_POSTFLOP_BUCKETS = 8;

// Action encoding
// 0 = fold, 1 = check/call, 2..5 = bet sizes (33%, 75%, 100%, all-in)
static constexpr int ACT_FOLD     = 0;
static constexpr int ACT_CHECK_CALL = 1;
static constexpr int ACT_BET_33   = 2;
static constexpr int ACT_BET_75   = 3;
static constexpr int ACT_BET_100  = 4;
static constexpr int ACT_ALLIN    = 5;
static constexpr int NUM_BET_ACTIONS = 4;  // 33, 75, 100, allin

// Street names
static constexpr const char* STREET_NAME[] = {"preflop","flop","turn","river"};

// ---- Card abstraction ----

// Map 2 hole cards to a pre-flop bucket (0..168).
// Pocket pairs: 13 buckets (AA=0, KK=1, ..., 22=12)
// Suited hands: 78 buckets (AKs=13, AQs=14, ..., 32s=90)
// Offsuit hands: 78 buckets (AKo=91, ..., 32o=168)
int preflop_bucket(int c0, int c1);

// Map hole cards + board to a post-flop EHS² bucket (0..NUM_POSTFLOP_BUCKETS-1).
// EHS = Expected Hand Strength (equity vs uniform random opponent range).
// EHS² = (EHS)² - captures both average and variance of equity.
int postflop_bucket(int c0, int c1, const int board[], int board_count);

// String representation of a preflop bucket (e.g., "AKs", "QQ", "72o")
std::string preflop_bucket_str(int bucket);

// ---- Information set key ----
// Format: "PF{bucket}:STREET:history"   (pre-flop)
//          "B{bucket}:STREET:history"   (post-flop, bucket = EHS² bin)
std::string make_holdem_key(int bucket, int street, const std::string& history);

// ---- Deal structure ----
struct HoldemDeal {
    int hole[2][2];  // hole[player][card_index], cards in [0..51]
    int board[5];    // board[0..4]
};

HoldemDeal sample_deal(std::mt19937& rng);

// ---- MCCFR External Sampling ----
//
// Per-iteration: sample one deal. For the traversing player, visit ALL
// actions (external sampling). For the opponent, sample ONE action
// proportional to the current mixed strategy.
//
// Returns the expected utility for the traverser at this node.
double mccfr_traverse(InfoMap& nodes,
                       const HoldemDeal& deal,
                       int street,
                       const std::string& history,
                       int num_bets,
                       double pot,
                       double stack[2],
                       double to_call,
                       int traverser,
                       Mode mode,
                       int iteration,
                       std::mt19937& rng);

// ---- Training loop ----
// Returns (iteration, exploitability) pairs.
// Exploitability for HUNL is estimated via Monte Carlo BR (expensive);
// we sample it every eval_every iterations using a reduced sample.
std::vector<std::pair<int,double>> train_holdem(InfoMap& nodes,
                                                  int iterations,
                                                  Mode mode,
                                                  int eval_every = 500);

// Monte Carlo exploitability estimate (sample-based best response).
double holdem_exploitability_estimate(const InfoMap& nodes,
                                       int num_samples = 10000);

void print_holdem_strategy(const InfoMap& nodes);

} // namespace holdem
} // namespace cfr
