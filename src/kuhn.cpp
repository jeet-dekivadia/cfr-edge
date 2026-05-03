#include "kuhn.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <vector>
#include <unordered_map>

namespace cfr {
namespace kuhn {

// ---- game logic helpers ----

static bool is_terminal(const std::string& h) {
    int n = h.size();
    if (n < 2) return false;
    if (n == 2) return (h == "pp" || h == "bb" || h == "bp");
    if (n == 3) return (h == "pbb" || h == "pbp");
    return false;
}

static int acting_player(const std::string& h) {
    return h.size() % 2 == 0 ? 0 : 1;
}

// returns utility for player 0
static double payoff(int p0_card, int p1_card, const std::string& h) {
    bool p0_higher = p0_card > p1_card;
    if (h == "pp")  return p0_higher ?  1.0 : -1.0;
    if (h == "bp")  return  1.0;
    if (h == "bb")  return p0_higher ?  2.0 : -2.0;
    if (h == "pbp") return -1.0;
    if (h == "pbb") return p0_higher ?  2.0 : -2.0;
    assert(false);
    return 0.0;
}

// ---- CFR traversal ----

double cfr_traverse(InfoMap& nodes, int p0_card, int p1_card,
                    const std::string& history,
                    double p0_reach, double p1_reach,
                    Mode mode, int iteration) {
    if (is_terminal(history)) {
        return payoff(p0_card, p1_card, history);
    }

    int player = acting_player(history);
    int card = (player == 0) ? p0_card : p1_card;

    char buf[8];
    buf[0] = card_name(card);
    buf[1] = ':';
    int len = 2;
    for (char c : history) buf[len++] = c;
    buf[len] = '\0';
    std::string key(buf, len);

    auto it = nodes.find(key);
    if (it == nodes.end()) {
        it = nodes.emplace(key, InfoNode(NUM_ACTIONS)).first;
    }
    InfoNode& node = it->second;

    double strategy[NUM_ACTIONS];
    node.get_strategy(strategy);

    double util[NUM_ACTIONS];
    double node_util = 0.0;
    const char action_char[2] = {'p', 'b'};

    for (int a = 0; a < NUM_ACTIONS; a++) {
        std::string next = history + action_char[a];
        double np0 = p0_reach, np1 = p1_reach;
        if (player == 0) np0 *= strategy[a];
        else             np1 *= strategy[a];

        util[a] = cfr_traverse(nodes, p0_card, p1_card, next, np0, np1, mode, iteration);
        node_util += strategy[a] * util[a];
    }

    double cf_reach = (player == 0) ? p1_reach : p0_reach;
    double my_reach = (player == 0) ? p0_reach : p1_reach;
    double sign     = (player == 0) ? 1.0 : -1.0;

    for (int a = 0; a < NUM_ACTIONS; a++) {
        node.regret_sum[a] += cf_reach * sign * (util[a] - node_util);
    }

    // strategy-sum weighting: 1 (CFR), t (CFR+), t^2 (DCFR)
    double weight;
    if      (mode == Mode::CFR_PLUS) weight = (double)iteration;
    else if (mode == Mode::DCFR)     weight = (double)iteration * (double)iteration;
    else                             weight = 1.0;

    for (int a = 0; a < NUM_ACTIONS; a++) {
        node.strategy_sum[a] += weight * my_reach * strategy[a];
    }

    return node_util;
}

// ---- correct best-response (infoset-level) ----
//
// The old per-deal traversal let the BR player pick DIFFERENT actions at the
// same infoset for different deals, overestimating exploitability.
//
// The fix: process ALL deals simultaneously. At BR-player nodes, group by the
// player's private card (which defines the infoset for a given history), then
// pick a SINGLE best action for the whole group. This enforces strategy
// consistency across deals that share an infoset.

struct DealState {
    int  p0_card, p1_card;
    double opp_reach;   // opponent's reach probability to current node
};

static double br_traverse(
        const std::vector<DealState>& states,
        const std::string& history,
        int br_player,
        const InfoMap& nodes) {

    if (states.empty()) return 0.0;

    if (is_terminal(history)) {
        double total = 0.0;
        for (const auto& s : states) {
            double u = payoff(s.p0_card, s.p1_card, history);
            if (br_player == 1) u = -u;
            total += s.opp_reach * u;
        }
        return total;
    }

    int player = acting_player(history);
    const char ac[] = {'p', 'b'};

    if (player == br_player) {
        // Group by the BR player's private card - each group is one infoset.
        // Within a group, pick ONE best action (the max over the summed value).
        std::unordered_map<int, std::vector<DealState>> by_card;
        for (const auto& s : states) {
            int card = (br_player == 0) ? s.p0_card : s.p1_card;
            by_card[card].push_back(s);
        }

        double total = 0.0;
        for (auto& [card, group] : by_card) {
            double best = -1e18;
            for (int a = 0; a < NUM_ACTIONS; a++) {
                double v = br_traverse(group, history + ac[a], br_player, nodes);
                if (v > best) best = v;
            }
            total += best;
        }
        return total;

    } else {
        // Opponent follows average strategy.
        // Each deal may have a different infoset (different private card) and
        // therefore a different mixed strategy - handle per-state.
        std::vector<DealState> child[NUM_ACTIONS];
        for (const auto& s : states) {
            int card = (player == 0) ? s.p0_card : s.p1_card;
            char buf[8];
            buf[0] = card_name(card); buf[1] = ':';
            int len = 2;
            for (char c : history) buf[len++] = c;
            std::string key(buf, len);

            double strat[2] = {0.5, 0.5};
            auto it = nodes.find(key);
            if (it != nodes.end()) it->second.get_average_strategy(strat);

            for (int a = 0; a < NUM_ACTIONS; a++) {
                if (strat[a] > 1e-10)
                    child[a].push_back({s.p0_card, s.p1_card, s.opp_reach * strat[a]});
            }
        }

        double total = 0.0;
        for (int a = 0; a < NUM_ACTIONS; a++)
            total += br_traverse(child[a], history + ac[a], br_player, nodes);
        return total;
    }
}

double exploitability(const InfoMap& nodes) {
    constexpr int deals[][2] = {{0,1},{0,2},{1,0},{1,2},{2,0},{2,1}};

    std::vector<DealState> all;
    all.reserve(6);
    for (const auto& d : deals)
        all.push_back({d[0], d[1], 1.0});

    // br_traverse returns sum_{deals} opp_reach*value;
    // divide by 6 (equal deal probability) and by 2 (average over players)
    double br0 = br_traverse(all, "", 0, nodes);
    double br1 = br_traverse(all, "", 1, nodes);
    return (br0 + br1) / 12.0;
}

// ---- training loop ----

std::vector<std::pair<int,double>> train(InfoMap& nodes, int iterations,
                                          Mode mode, int eval_every) {
    constexpr int deals[][2] = {{0,1},{0,2},{1,0},{1,2},{2,0},{2,1}};
    std::vector<std::pair<int,double>> curve;

    for (int t = 1; t <= iterations; t++) {

        // DCFR: discount positive regrets before this iteration's traversal.
        // alpha=1.5 (aggressive discount), negative regrets floored to 0.
        if (mode == Mode::DCFR) {
            double alpha  = 1.5;
            double pt     = std::pow((double)t, alpha);
            double factor = pt / (pt + 1.0);
            for (auto& [key, node] : nodes) {
                for (int a = 0; a < node.num_actions; a++)
                    node.regret_sum[a] = std::max(node.regret_sum[a], 0.0) * factor;
            }
        }

        for (const auto& d : deals)
            cfr_traverse(nodes, d[0], d[1], "", 1.0, 1.0, mode, t);

        // CFR+ / DCFR: floor regrets AFTER all deals complete (not mid-iteration).
        if (mode == Mode::CFR_PLUS || mode == Mode::DCFR) {
            for (auto& [key, node] : nodes)
                node.floor_regrets();
        }

        if (t == 1 || t % eval_every == 0 || t == iterations)
            curve.push_back({t, exploitability(nodes)});
    }
    return curve;
}

// ---- print ----

void print_strategy(const InfoMap& nodes) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Kuhn Poker Average Strategy:\n";
    std::cout << "InfoSet     Pass    Bet\n";
    std::vector<std::string> keys;
    for (const auto& kv : nodes) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (const auto& k : keys) {
        double s[NUM_ACTIONS];
        nodes.at(k).get_average_strategy(s);
        std::cout << std::setw(10) << std::left << k
                  << std::setw(8) << s[PASS]
                  << std::setw(8) << s[BET] << "\n";
    }
}

} // namespace kuhn
} // namespace cfr
