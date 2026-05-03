#include "holdem.h"
#include "hand_eval.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <sstream>

namespace cfr {
namespace holdem {

// ---- Pre-flop bucket (169 canonical classes) ----

int preflop_bucket(int c0, int c1) {
    int r0 = handeval::card_rank(c0);
    int r1 = handeval::card_rank(c1);
    int s0 = handeval::card_suit(c0);
    int s1 = handeval::card_suit(c1);

    // Normalise: r0 >= r1
    if (r0 < r1) { std::swap(r0, r1); std::swap(s0, s1); }

    bool suited = (s0 == s1);
    bool pair   = (r0 == r1);

    if (pair) {
        // 13 pair buckets: AA=0, KK=1, ..., 22=12
        return 12 - r0;
    }
    if (suited) {
        // 78 suited hand buckets: AKs=13, AQs=14, ..., 32s=90
        // Ordered by (high_rank, low_rank) desc
        int high = r0, low = r1;  // high > low always
        // Index in the 78 suited hands (ordered A-high first):
        // high goes 12..1, for each high, low goes (high-1)..0
        int idx = 0;
        for (int h = 12; h > high; h--) idx += h;  // count all hands with higher 'high'
        idx += (high - 1 - low);
        return 13 + idx;
    } else {
        // 78 offsuit hand buckets: AKo=91, ..., 32o=168
        int high = r0, low = r1;
        int idx = 0;
        for (int h = 12; h > high; h--) idx += h;
        idx += (high - 1 - low);
        return 91 + idx;
    }
}

std::string preflop_bucket_str(int bucket) {
    static constexpr const char* RANK_CHAR = "23456789TJQKA";
    if (bucket < 13) {
        // Pair
        char r = RANK_CHAR[12 - bucket];
        return std::string({r, r});
    }
    if (bucket < 91) {
        // Suited: reconstruct (high, low)
        int idx = bucket - 13;
        int high = 12;
        while (high > 0 && idx >= high) { idx -= high; high--; }
        int low = high - 1 - idx;
        return std::string({RANK_CHAR[high], RANK_CHAR[low], 's'});
    }
    // Offsuit
    int idx = bucket - 91;
    int high = 12;
    while (high > 0 && idx >= high) { idx -= high; high--; }
    int low = high - 1 - idx;
    return std::string({RANK_CHAR[high], RANK_CHAR[low], 'o'});
}

// ---- Post-flop hand-strength bucket (O(1), no MC sampling) ----
// Uses the raw 5-card (or 6/7-card) hand evaluator rank as a proxy for
// hand strength. Lower eval rank = stronger hand.
// We normalise the rank to [0, NUM_POSTFLOP_BUCKETS) by dividing the
// [1..7461] rank range into equal-width bins.
// Bucket 0 = strongest (near royal flush), Bucket 7 = weakest (high card).

int postflop_bucket(int c0, int c1, const int board[], int board_count) {
    const auto& ev = handeval::get_evaluator();
    int rank = 7461;  // default: worst possible

    if (board_count >= 5) {
        int cards7[7] = {c0, c1, board[0], board[1], board[2], board[3], board[4]};
        rank = ev.eval7(cards7);
    } else if (board_count >= 3) {
        // Use best 5-of-5 (2 hole + 3 board)
        int cards5[5] = {c0, c1, board[0], board[1], board[2]};
        rank = ev.eval5(cards5);
    } else {
        // Pre-flop or 1-2 board cards: bucket by hole card ranks only
        int r0 = handeval::card_rank(c0), r1 = handeval::card_rank(c1);
        // Higher ranks → lower bucket (stronger)
        return std::max(0, NUM_POSTFLOP_BUCKETS - 1 - (r0 + r1) * NUM_POSTFLOP_BUCKETS / 25);
    }

    // rank in [1..7461]: bucket 0 = best, 7 = worst
    int bucket = (rank - 1) * NUM_POSTFLOP_BUCKETS / 7461;
    return std::min(bucket, NUM_POSTFLOP_BUCKETS - 1);
}

// ---- Info key ----

std::string make_holdem_key(int bucket, int street, const std::string& history) {
    std::string key;
    key.reserve(32);
    if (street == 0) {
        key = "PF" + std::to_string(bucket) + ":";
    } else {
        key = "B" + std::to_string(bucket) + ":" + STREET_NAME[street] + ":";
    }
    key += history;
    return key;
}

// ---- Deal sampling ----

HoldemDeal sample_deal(std::mt19937& rng) {
    // Shuffle a 52-card deck and deal 2+2+5 cards
    std::array<int, 52> deck;
    std::iota(deck.begin(), deck.end(), 0);
    std::shuffle(deck.begin(), deck.end(), rng);
    HoldemDeal d;
    d.hole[0][0] = deck[0]; d.hole[0][1] = deck[1];
    d.hole[1][0] = deck[2]; d.hole[1][1] = deck[3];
    for (int i = 0; i < 5; i++) d.board[i] = deck[4 + i];
    return d;
}

// ---- Game tree helpers ----

// Returns the number of board cards visible at the start of a given street.
static inline int board_cards_at_start(int street) {
    static const int n[] = {0, 3, 4, 5};  // preflop, flop, turn, river
    return (street >= 0 && street < 4) ? n[street] : 5;
}

// Apply a bet action: returns the new pot and stack sizes.
// Returns false if action is impossible (e.g., fold when not facing a bet).
static void apply_action(int action, int player, double pot, double stack[2],
                          double to_call,
                          double& new_pot, double new_stack[2], double& new_to_call) {
    new_stack[0] = stack[0]; new_stack[1] = stack[1];
    new_to_call  = 0.0;

    switch (action) {
    case ACT_FOLD:
        // Folding: no chip movement, just signal terminal
        new_pot = pot;
        break;
    case ACT_CHECK_CALL:
        if (to_call > 0.0) {
            // Call
            double call_amt = std::min(to_call, stack[player]);
            new_stack[player] -= call_amt;
            new_pot = pot + call_amt;
            new_to_call = 0.0;
        } else {
            // Check
            new_pot = pot;
            new_to_call = 0.0;
        }
        break;
    default: {
        // Bet / raise
        static const double BET_FRACS[] = {0.0, 0.0, 0.33, 0.75, 1.0, 1e9};
        double frac = BET_FRACS[action];
        double bet_size = (frac >= 1e8) ? stack[player] :  // all-in
                          std::min(frac * pot + to_call, stack[player]);
        new_stack[player] -= bet_size;
        new_pot = pot + bet_size;
        new_to_call = bet_size - to_call;  // new amount to call above prior bet
        break;
    }
    }
}

// Determine available actions given current state.
static int get_holdem_actions(double to_call, int num_bets, double stack_acting,
                               int* actions) {
    int n = 0;
    if (to_call > 0.0) {
        actions[n++] = ACT_FOLD;
        if (stack_acting > 0.0) actions[n++] = ACT_CHECK_CALL;
    } else {
        actions[n++] = ACT_CHECK_CALL;  // check
    }
    // Bet/raise actions (only if not already max bets and have chips)
    if (num_bets < MAX_BETS_PER_STREET && stack_acting > 0.0) {
        actions[n++] = ACT_BET_33;
        actions[n++] = ACT_BET_75;
        actions[n++] = ACT_BET_100;
        if (stack_acting > 0.0) actions[n++] = ACT_ALLIN;
    }
    return n;
}

// ---- MCCFR External Sampling traversal ----

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
                       std::mt19937& rng) {

    // ---- Terminal: fold ----
    if (!history.empty() && history.back() == 'f') {
        // The player who just folded was the acting player before the fold.
        // Working backwards: length of history before 'f' gives us the acting player.
        // The fold was the PREVIOUS action; we're now at the terminal node.
        // Return utility from traverser's perspective.
        // The folding player is NOT the traverser in the recursive call context -
        // we handle fold returns inline in the action loop below.
        // This branch is only reached if history ends in 'f' AND it's a terminal.
        // Payoff: the player who folded loses what they put in; other wins the pot.
        // We track pot already, so just return pot as gain for the non-folder.
        // The caller has already accounted for who folded, so we return pot here.
        return pot;
    }

    // ---- Street transition ----
    // A street ends when both players have acted and no bet is pending (to_call==0)
    // and at least one action has been taken (checked or called).
    bool street_over = false;
    if (!history.empty()) {
        char last = history.back();
        // After a call (with no remaining to_call) or after check-check
        if (last == 'c' && to_call <= 1e-9) street_over = true;
        if (last == 'x') street_over = true;  // check (we encode check as 'x')
        // Actually let's re-examine: we encode actions as chars differently below.
    }
    // We'll handle street transitions in the action application logic.

    // ---- Terminal: river showdown ----
    if (street == 4) {
        // River over: showdown
        int all7_0[7] = {deal.hole[0][0], deal.hole[0][1],
                         deal.board[0], deal.board[1], deal.board[2],
                         deal.board[3], deal.board[4]};
        int all7_1[7] = {deal.hole[1][0], deal.hole[1][1],
                         deal.board[0], deal.board[1], deal.board[2],
                         deal.board[3], deal.board[4]};
        const auto& ev = handeval::get_evaluator();
        int r0 = ev.eval7(all7_0), r1 = ev.eval7(all7_1);
        double half_pot = pot * 0.5;
        if (r0 < r1) return (traverser == 0) ?  half_pot : -half_pot;
        if (r0 > r1) return (traverser == 0) ? -half_pot :  half_pot;
        return 0.0;  // tie
    }

    // ---- Determine acting player ----
    // Pre-flop: player 0 (SB/button) acts first.
    // Post-flop: player 1 (BB) acts first.
    // Within a street, alternate each action.
    int num_actions_taken = static_cast<int>(history.size());  // rough
    // Simpler: just use history length within the current street section.
    // We pass history as the full history, with '/' separating streets.
    // Extract the current street's sub-history.
    std::string street_hist;
    auto last_slash = history.rfind('/');
    if (last_slash == std::string::npos) street_hist = history;
    else street_hist = history.substr(last_slash + 1);

    int player = (street == 0)
        ? (street_hist.size() % 2 == 0 ? 0 : 1)   // preflop: 0 first
        : (street_hist.size() % 2 == 0 ? 1 : 0);  // postflop: 1 first (OOP acts first)

    // ---- Look up or create info node ----
    int board_cnt = board_cards_at_start(street);
    int bucket;
    if (street == 0) {
        bucket = preflop_bucket(deal.hole[player][0], deal.hole[player][1]);
    } else {
        bucket = postflop_bucket(deal.hole[player][0], deal.hole[player][1],
                                  deal.board, board_cnt);
    }
    std::string key = make_holdem_key(bucket, street, street_hist);

    int avail[6];
    int na = get_holdem_actions(to_call, num_bets, stack[player], avail);

    auto it = nodes.find(key);
    if (it == nodes.end())
        it = nodes.emplace(key, InfoNode(na)).first;
    InfoNode& node = it->second;

    // Get current strategy via regret matching
    double strat[6];
    node.get_strategy(strat);

    // ---- Strategy sum update (for average strategy tracking) ----
    double weight = 1.0;
    if      (mode == Mode::CFR_PLUS) weight = static_cast<double>(iteration);
    else if (mode == Mode::DCFR)     weight = static_cast<double>(iteration) *
                                               static_cast<double>(iteration);

    // ---- MCCFR External Sampling ----
    if (player == traverser) {
        // Traverser: iterate over ALL actions, recurse, accumulate regrets.
        double util[6] = {};
        double node_util = 0.0;

        for (int i = 0; i < na; i++) {
            int action = avail[i];

            // Compute next state
            double new_pot; double new_stack[2]; double new_to_call;
            apply_action(action, player, pot, stack, to_call,
                         new_pot, new_stack, new_to_call);

            std::string next_hist;
            double child_util;

            if (action == ACT_FOLD) {
                // Traverser folds → loses what they put in
                child_util = -pot / 2.0;  // simplified: traverser loses their half
            } else {
                // Build next history char
                char c;
                if (action == ACT_CHECK_CALL && to_call <= 1e-9) c = 'x';  // check
                else if (action == ACT_CHECK_CALL) c = 'c';                  // call
                else c = '0' + (action - ACT_BET_33 + 1);                   // bet codes: 1,2,3,4

                next_hist = street_hist + c;

                // Detect street-over after a call or check-check
                bool now_street_over = false;
                if (action == ACT_CHECK_CALL && to_call > 1e-9) {
                    // Called: street over
                    now_street_over = true;
                }
                if (action == ACT_CHECK_CALL && to_call <= 1e-9 && !next_hist.empty()
                    && next_hist.size() >= 2 && next_hist.back() == 'x'
                    && next_hist[next_hist.size()-2] == 'x') {
                    // Check-check: street over
                    now_street_over = true;
                }

                if (now_street_over && street < 3) {
                    // Move to next street
                    std::string next_full = history + (history.empty() ? "" : "/") + next_hist + "/";
                    child_util = mccfr_traverse(nodes, deal, street + 1,
                                                 next_full, 0,
                                                 new_pot, new_stack, 0.0,
                                                 traverser, mode, iteration, rng);
                } else if (now_street_over && street == 3) {
                    // River over → showdown
                    int all7_0[7] = {deal.hole[0][0], deal.hole[0][1],
                                     deal.board[0], deal.board[1], deal.board[2],
                                     deal.board[3], deal.board[4]};
                    int all7_1[7] = {deal.hole[1][0], deal.hole[1][1],
                                     deal.board[0], deal.board[1], deal.board[2],
                                     deal.board[3], deal.board[4]};
                    const auto& ev = handeval::get_evaluator();
                    int r0 = ev.eval7(all7_0), r1 = ev.eval7(all7_1);
                    double hp = new_pot * 0.5;
                    if (r0 < r1) child_util = (traverser == 0) ?  hp : -hp;
                    else if (r0 > r1) child_util = (traverser == 0) ? -hp : hp;
                    else child_util = 0.0;
                } else {
                    std::string next_full = (history.empty() ? "" : history) +
                                            (last_slash == std::string::npos ? "" : "") +
                                            next_hist;
                    // Reconstruct full history properly
                    std::string base = (last_slash == std::string::npos) ? "" :
                                       history.substr(0, last_slash + 1);
                    child_util = mccfr_traverse(nodes, deal, street,
                                                 base + next_hist,
                                                 num_bets + (action >= ACT_BET_33 ? 1 : 0),
                                                 new_pot, new_stack, new_to_call,
                                                 traverser, mode, iteration, rng);
                }
            }

            util[i] = child_util;
            node_util += strat[i] * child_util;
        }

        // Regret accumulation (counterfactual regret for traverser)
        for (int i = 0; i < na; i++)
            node.regret_sum[i] += util[i] - node_util;

        // Strategy sum (for average strategy)
        for (int i = 0; i < na; i++)
            node.strategy_sum[i] += weight * strat[i];

        return node_util;

    } else {
        // Opponent: sample ONE action proportional to strategy (external sampling).
        // Choose action index via weighted sample.
        double r = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
        double cum = 0.0;
        int chosen = 0;
        for (int i = 0; i < na; i++) {
            cum += strat[i];
            if (r <= cum || i == na - 1) { chosen = i; break; }
        }

        // Update opponent's strategy sum (they are not the traverser but still
        // need their strategy tracked for the average-strategy computation)
        for (int i = 0; i < na; i++)
            node.strategy_sum[i] += weight * strat[i];

        int action = avail[chosen];
        double new_pot; double new_stack[2]; double new_to_call;
        apply_action(action, player, pot, stack, to_call,
                     new_pot, new_stack, new_to_call);

        if (action == ACT_FOLD) {
            // Opponent folds → traverser wins pot
            return new_pot * 0.5;
        }

        char c;
        if (action == ACT_CHECK_CALL && to_call <= 1e-9) c = 'x';
        else if (action == ACT_CHECK_CALL) c = 'c';
        else c = '0' + (action - ACT_BET_33 + 1);

        std::string next_street_hist = street_hist + c;

        bool now_street_over = false;
        if (action == ACT_CHECK_CALL && to_call > 1e-9) now_street_over = true;
        if (action == ACT_CHECK_CALL && to_call <= 1e-9
            && next_street_hist.size() >= 2
            && next_street_hist.back() == 'x'
            && next_street_hist[next_street_hist.size()-2] == 'x')
            now_street_over = true;

        std::string base_hist = (last_slash == std::string::npos) ? "" :
                                 history.substr(0, last_slash + 1);

        if (now_street_over && street < 3) {
            return mccfr_traverse(nodes, deal, street + 1,
                                   base_hist + next_street_hist + "/",
                                   0, new_pot, new_stack, 0.0,
                                   traverser, mode, iteration, rng);
        } else if (now_street_over && street == 3) {
            int all7_0[7] = {deal.hole[0][0], deal.hole[0][1],
                             deal.board[0], deal.board[1], deal.board[2],
                             deal.board[3], deal.board[4]};
            int all7_1[7] = {deal.hole[1][0], deal.hole[1][1],
                             deal.board[0], deal.board[1], deal.board[2],
                             deal.board[3], deal.board[4]};
            const auto& ev = handeval::get_evaluator();
            int r0 = ev.eval7(all7_0), r1 = ev.eval7(all7_1);
            double hp = new_pot * 0.5;
            if (r0 < r1) return (traverser == 0) ?  hp : -hp;
            if (r0 > r1) return (traverser == 0) ? -hp :  hp;
            return 0.0;
        }

        return mccfr_traverse(nodes, deal, street,
                               base_hist + next_street_hist,
                               num_bets + (action >= ACT_BET_33 ? 1 : 0),
                               new_pot, new_stack, new_to_call,
                               traverser, mode, iteration, rng);
    }
}

// ---- Training loop ----

std::vector<std::pair<int,double>> train_holdem(InfoMap& nodes,
                                                  int iterations,
                                                  Mode mode,
                                                  int eval_every) {
    std::mt19937 rng(12345);
    std::vector<std::pair<int,double>> curve;

    for (int t = 1; t <= iterations; t++) {
        // DCFR: discount positive regrets before this iteration
        if (mode == Mode::DCFR) {
            double alpha  = 1.5;
            double pt     = std::pow(static_cast<double>(t), alpha);
            double factor = pt / (pt + 1.0);
            for (auto& [key, nd] : nodes)
                for (int a = 0; a < nd.num_actions; a++)
                    nd.regret_sum[a] = std::max(nd.regret_sum[a], 0.0) * factor;
        }

        // Sample a deal and run MCCFR for both players
        HoldemDeal deal = sample_deal(rng);
        double stacks[2] = {STARTING_STACK - SMALL_BLIND, STARTING_STACK - BIG_BLIND};
        double pot = SMALL_BLIND + BIG_BLIND;
        double to_call = BIG_BLIND - SMALL_BLIND;  // SB needs to call the extra 0.5

        // Traverse for player 0, then player 1
        for (int traverser = 0; traverser < 2; traverser++) {
            mccfr_traverse(nodes, deal, 0, "", 0, pot, stacks, to_call,
                            traverser, mode, t, rng);
        }

        // CFR+ / DCFR: floor regrets after traversal
        if (mode == Mode::CFR_PLUS || mode == Mode::DCFR) {
            for (auto& [key, nd] : nodes)
                nd.floor_regrets();
        }

        if (t == 1 || t % eval_every == 0 || t == iterations) {
            double expl = holdem_exploitability_estimate(nodes, 500);
            curve.push_back({t, expl});
            if (t % (eval_every * 5) == 0)
                std::cout << "  [HoldEm MCCFR] iter=" << t
                          << " expl≈" << std::scientific << std::setprecision(3)
                          << expl << " infosets=" << nodes.size() << "\n";
        }
    }
    return curve;
}

// ---- Monte Carlo exploitability estimate ----

double holdem_exploitability_estimate(const InfoMap& nodes, int num_samples) {
    // Fast exploitability proxy for HUNL:
    // Sample deals and measure the average regret magnitude at preflop nodes.
    // A Nash strategy has zero regret; high regret = exploitable.
    // This is O(num_samples) with no tree traversal.
    std::mt19937 rng(99999);
    double total_regret = 0.0;
    int count = 0;

    for (int s = 0; s < num_samples; s++) {
        HoldemDeal deal = sample_deal(rng);
        for (int player = 0; player < 2; player++) {
            int bucket = preflop_bucket(deal.hole[player][0], deal.hole[player][1]);
            std::string key = make_holdem_key(bucket, 0, "");
            auto it = nodes.find(key);
            if (it != nodes.end()) {
                const auto& nd = it->second;
                // Sum of positive regrets / num_actions as proxy
                double pos_regret = 0.0;
                for (int a = 0; a < nd.num_actions; a++)
                    pos_regret += std::max(nd.regret_sum[a], 0.0);
                total_regret += pos_regret / nd.num_actions;
            }
        }
        count += 2;
    }
    return (count > 0) ? total_regret / count : 0.0;
}

// ---- Print strategy ----

void print_holdem_strategy(const InfoMap& nodes) {
    std::cout << "\nTexas Hold'em (MCCFR) - Pre-flop Strategy Sample:\n";
    std::cout << std::fixed << std::setprecision(3);
    std::vector<std::string> keys;
    keys.reserve(nodes.size());
    for (const auto& [k, _] : nodes) {
        if (k.substr(0, 2) == "PF") keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());
    // Print first 30 preflop infosets as a sample
    int shown = 0;
    for (const auto& k : keys) {
        if (shown++ >= 30) break;
        const auto& nd = nodes.at(k);
        double s[6];
        nd.get_average_strategy(s);
        std::cout << std::setw(24) << std::left << k << "  ";
        static const char* ACT_NAME[] = {"fold","chk/call","bet33","bet75","bet100","allin"};
        for (int a = 0; a < nd.num_actions; a++)
            if (s[a] > 0.01) std::cout << ACT_NAME[a] << "=" << s[a] << " ";
        std::cout << "\n";
    }
}

} // namespace holdem
} // namespace cfr
