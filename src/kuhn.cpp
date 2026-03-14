#include "kuhn.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

namespace cfr {
namespace kuhn {

// ---- game logic helpers ----

static bool is_terminal(const std::string& h) {
    // terminal histories: pp, bb, bp, pbb, pbp
    int n = h.size();
    if (n < 2) return false;
    if (n == 2) return (h == "pp" || h == "bb" || h == "bp");
    if (n == 3) return (h == "pbb" || h == "pbp");
    return false;
}

static int acting_player(const std::string& h) {
    // P0 at history lengths 0,2  P1 at length 1
    return h.size() % 2 == 0 ? 0 : 1;
}

// returns utility for player 0
static double payoff(int p0_card, int p1_card, const std::string& h) {
    bool p0_higher = p0_card > p1_card;
    if (h == "pp")  return p0_higher ?  1.0 : -1.0;
    if (h == "bp")  return  1.0;                     // P0 bet, P1 fold
    if (h == "bb")  return p0_higher ?  2.0 : -2.0;  // P0 bet, P1 call
    if (h == "pbp") return -1.0;                      // P0 check, P1 bet, P0 fold
    if (h == "pbb") return p0_higher ?  2.0 : -2.0;  // P0 check, P1 bet, P0 call
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

    // build info set key  e.g. "K:pb"
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
    char action_char[2] = {'p', 'b'};

    for (int a = 0; a < NUM_ACTIONS; a++) {
        std::string next = history + action_char[a];
        double np0 = p0_reach, np1 = p1_reach;
        if (player == 0) np0 *= strategy[a];
        else             np1 *= strategy[a];

        util[a] = cfr_traverse(nodes, p0_card, p1_card, next, np0, np1, mode, iteration);
        node_util += strategy[a] * util[a];
    }

    // update regrets and strategy sums
    double cf_reach  = (player == 0) ? p1_reach : p0_reach;
    double my_reach  = (player == 0) ? p0_reach : p1_reach;
    double sign      = (player == 0) ? 1.0 : -1.0;

    for (int a = 0; a < NUM_ACTIONS; a++) {
        double regret = sign * (util[a] - node_util);
        node.regret_sum[a] += cf_reach * regret;
    }

    // CFR+: floor regrets
    if (mode == Mode::CFR_PLUS) {
        node.floor_regrets();
    }

    // strategy sum (linear weighting for CFR+)
    double weight = (mode == Mode::CFR_PLUS) ? (double)iteration : 1.0;
    for (int a = 0; a < NUM_ACTIONS; a++) {
        node.strategy_sum[a] += weight * my_reach * strategy[a];
    }

    return node_util;
}

// ---- best response ----

// returns expected utility from br_player's perspective
// br_player plays optimally, opponent follows avg strategy
static double best_response(const InfoMap& nodes, int p0_card, int p1_card,
                            const std::string& history, int br_player) {
    if (is_terminal(history)) {
        double u = payoff(p0_card, p1_card, history);
        return (br_player == 0) ? u : -u;
    }

    int player = acting_player(history);
    int card = (player == 0) ? p0_card : p1_card;

    char buf[8];
    buf[0] = card_name(card); buf[1] = ':';
    int len = 2;
    for (char c : history) buf[len++] = c;
    std::string key(buf, len);

    char ac[2] = {'p', 'b'};

    if (player == br_player) {
        // pick best action
        double best = -1e18;
        for (int a = 0; a < NUM_ACTIONS; a++) {
            double v = best_response(nodes, p0_card, p1_card, history + ac[a], br_player);
            if (v > best) best = v;
        }
        return best;
    } else {
        // follow average strategy
        double strat[NUM_ACTIONS];
        auto it = nodes.find(key);
        if (it != nodes.end()) {
            it->second.get_average_strategy(strat);
        } else {
            for (int a = 0; a < NUM_ACTIONS; a++) strat[a] = 1.0 / NUM_ACTIONS;
        }
        double ev = 0.0;
        for (int a = 0; a < NUM_ACTIONS; a++) {
            ev += strat[a] * best_response(nodes, p0_card, p1_card, history + ac[a], br_player);
        }
        return ev;
    }
}

double exploitability(const InfoMap& nodes) {
    constexpr int deals[][2] = {{0,1},{0,2},{1,0},{1,2},{2,0},{2,1}};
    double br0 = 0.0, br1 = 0.0;
    for (auto& d : deals) {
        br0 += best_response(nodes, d[0], d[1], "", 0);
        br1 += best_response(nodes, d[0], d[1], "", 1);
    }
    return (br0 + br1) / 12.0;  // /6 for deals, /2 for avg over players
}

// ---- training loop ----

std::vector<std::pair<int,double>> train(InfoMap& nodes, int iterations,
                                          Mode mode, int eval_every) {
    constexpr int deals[][2] = {{0,1},{0,2},{1,0},{1,2},{2,0},{2,1}};
    std::vector<std::pair<int,double>> curve;

    for (int t = 1; t <= iterations; t++) {
        for (auto& d : deals) {
            cfr_traverse(nodes, d[0], d[1], "", 1.0, 1.0, mode, t);
        }
        if (t == 1 || t % eval_every == 0 || t == iterations) {
            double expl = exploitability(nodes);
            curve.push_back({t, expl});
        }
    }
    return curve;
}

// ---- print ----

void print_strategy(const InfoMap& nodes) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Kuhn Poker Average Strategy:\n";
    std::cout << "InfoSet     Pass    Bet\n";
    // collect and sort keys
    std::vector<std::string> keys;
    for (auto& kv : nodes) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (auto& k : keys) {
        double s[NUM_ACTIONS];
        nodes.at(k).get_average_strategy(s);
        std::cout << std::setw(10) << std::left << k
                  << std::setw(8) << s[PASS]
                  << std::setw(8) << s[BET] << "\n";
    }
}

} // namespace kuhn
} // namespace cfr
