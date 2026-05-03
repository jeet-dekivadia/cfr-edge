#include "leduc.h"
#include "leduc_utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
#include <unordered_map>

namespace cfr {
namespace leduc {

// ---- deal enumeration ----

std::vector<Deal> all_deals() {
    std::vector<Deal> deals;
    for (int a = 0; a < DECK_SIZE; a++)
        for (int b = 0; b < DECK_SIZE; b++)
            for (int c = 0; c < DECK_SIZE; c++) {
                if (a == b || a == c || b == c) continue;
                deals.push_back({{a, b, c}, 1.0});
            }
    double norm = 1.0 / deals.size();
    for (auto& d : deals) d.prob = norm;
    return deals;
}

// ---- game helpers ----
// round_over() and showdown_winner() are provided by leduc_utils.h (same namespace).
// acting_player() is a thin alias for acting_player_in_round().

static inline int acting_player(const std::string& round_hist) {
    return acting_player_in_round(round_hist);
}

static std::string make_info_key(int player_card, int board_card, int round,
                                   const std::string& full_hist) {
    std::string key;
    key += rank_name(card_rank(player_card));
    key += ':';
    if (round >= 1) key += rank_name(card_rank(board_card));
    else            key += '?';
    key += ':';
    key += full_hist;
    return key;
}

static void get_actions(int num_bets, const std::string& round_hist,
                        int* actions, int& n_actions) {
    get_actions_tpl<FOLD, CHECK_CALL, BET_RAISE, MAX_RAISES>(
        num_bets, round_hist, actions, n_actions);
}

// ---- CFR traversal ----

double cfr_traverse(InfoMap& nodes,
                    const int cards[3],
                    int round,
                    const std::string& history,
                    int num_bets,
                    int chips_in[2],
                    double p0_reach, double p1_reach,
                    Mode mode, int iteration) {

    auto slash = history.rfind('/');
    std::string round_hist = (slash == std::string::npos) ? history : history.substr(slash + 1);

    bool folded = false;
    if (round_over(round_hist, folded)) {
        if (folded) {
            int fold_player = acting_player(std::string(round_hist.begin(), round_hist.end() - 1));
            return (fold_player == 0) ? -(double)chips_in[0] : (double)chips_in[1];
        }
        if (round == 0) {
            int new_chips[2] = {chips_in[0], chips_in[1]};
            return cfr_traverse(nodes, cards, 1, history + "/", 0, new_chips,
                                p0_reach, p1_reach, mode, iteration);
        } else {
            int w = cfr::leduc::showdown_winner(cards);
            if (w > 0) return  (double)chips_in[1];
            if (w < 0) return -(double)chips_in[0];
            return 0.0;
        }
    }

    int player = acting_player(round_hist);
    int card = cards[player];
    std::string key = make_info_key(card, cards[2], round, history);

    int avail[3]; int n_actions;
    get_actions(num_bets, round_hist, avail, n_actions);

    auto it = nodes.find(key);
    if (it == nodes.end())
        it = nodes.emplace(key, InfoNode(n_actions)).first;
    InfoNode& node = it->second;

    double strategy[4];
    node.get_strategy(strategy);

    double util[4], node_util = 0.0;

    for (int i = 0; i < n_actions; i++) {
        int a = avail[i];
        char act_char;
        int nb = num_bets;
        int nc[2] = {chips_in[0], chips_in[1]};

        if (a == FOLD) { act_char = 'f'; }
        else if (a == CHECK_CALL) {
            act_char = 'c';
            if (num_bets > 0) nc[player] = nc[1-player];
        } else {
            act_char = 'b'; nb++;
            nc[player] = nc[1-player] + BET_SIZE[round];
        }

        double np0 = p0_reach, np1 = p1_reach;
        if (player == 0) np0 *= strategy[i]; else np1 *= strategy[i];
        util[i] = cfr_traverse(nodes, cards, round, history + act_char,
                                nb, nc, np0, np1, mode, iteration);
        node_util += strategy[i] * util[i];
    }

    double cf_reach = (player == 0) ? p1_reach : p0_reach;
    double my_reach = (player == 0) ? p0_reach : p1_reach;
    double sign     = (player == 0) ? 1.0 : -1.0;

    for (int i = 0; i < n_actions; i++)
        node.regret_sum[i] += cf_reach * sign * (util[i] - node_util);

    // strategy-sum weighting: 1 (CFR), t (CFR+), t^2 (DCFR)
    double weight;
    if      (mode == Mode::CFR_PLUS) weight = (double)iteration;
    else if (mode == Mode::DCFR)     weight = (double)iteration * (double)iteration;
    else                             weight = 1.0;

    for (int i = 0; i < n_actions; i++)
        node.strategy_sum[i] += weight * my_reach * strategy[i];

    return node_util;
}

// ---- correct best-response (infoset-level) ----
//
// Process all deals simultaneously so that infoset-consistent BR strategies
// are enforced. Deals sharing the same infoset (same private card + board)
// must use the SAME action - enforced by grouping before picking max.

struct LeDealState {
    int p0_card, p1_card, board;
    double opp_reach;
};

static double leduc_br_traverse(
        const std::vector<LeDealState>& states,
        int round,
        const std::string& history,
        int num_bets,
        int chips_in[2],
        int br_player,
        const InfoMap& nodes) {

    if (states.empty()) return 0.0;

    auto slash = history.rfind('/');
    std::string round_hist = (slash == std::string::npos) ? history : history.substr(slash + 1);

    bool folded = false;
    if (round_over(round_hist, folded)) {
        if (folded) {
            int fold_player = acting_player(std::string(round_hist.begin(), round_hist.end() - 1));
            double u_raw = (fold_player == 0) ? -(double)chips_in[0] : (double)chips_in[1];
            double total = 0.0;
            for (const auto& s : states)
                total += s.opp_reach * ((br_player == 0) ? u_raw : -u_raw);
            return total;
        }
        if (round == 0) {
            int nc[2] = {chips_in[0], chips_in[1]};
            return leduc_br_traverse(states, 1, history + "/", 0, nc, br_player, nodes);
        }
        // showdown
        double total = 0.0;
        for (const auto& s : states) {
            const int cards[3] = {s.p0_card, s.p1_card, s.board};
            int w = cfr::leduc::showdown_winner(cards);
            double u;
            if (w > 0)       u =  (double)chips_in[1];
            else if (w < 0)  u = -(double)chips_in[0];
            else             u = 0.0;
            if (br_player == 1) u = -u;
            total += s.opp_reach * u;
        }
        return total;
    }

    int player = acting_player(round_hist);
    int avail[3]; int n_actions;
    get_actions(num_bets, round_hist, avail, n_actions);

    // Helper lambda to build next-state chips/bets for action i
    auto apply_action = [&](int i, int& nb, int nc[2], char& act_char) {
        nb = num_bets;
        nc[0] = chips_in[0]; nc[1] = chips_in[1];
        int a = avail[i];
        if (a == FOLD)            { act_char = 'f'; }
        else if (a == CHECK_CALL) {
            act_char = 'c';
            if (num_bets > 0) nc[player] = nc[1-player];
        } else {
            act_char = 'b'; nb++;
            nc[player] = nc[1-player] + BET_SIZE[round];
        }
    };

    if (player == br_player) {
        // Group by infoset key: (private_card, board_card, round, history)
        // Round 0: board unknown → key uses '?'; round 1: board known.
        std::unordered_map<std::string, std::vector<LeDealState>> by_infoset;
        for (const auto& s : states) {
            int pcard = (player == 0) ? s.p0_card : s.p1_card;
            by_infoset[make_info_key(pcard, s.board, round, history)].push_back(s);
        }

        double total = 0.0;
        for (auto& [key, group] : by_infoset) {
            double best = -1e18;
            for (int i = 0; i < n_actions; i++) {
                int nb; int nc[2]; char act_char;
                apply_action(i, nb, nc, act_char);
                double v = leduc_br_traverse(group, round, history + act_char,
                                              nb, nc, br_player, nodes);
                if (v > best) best = v;
            }
            total += best;
        }
        return total;

    } else {
        // Opponent follows average strategy (per-deal lookup)
        std::vector<LeDealState> child_states[3];

        for (const auto& s : states) {
            int pcard = (player == 0) ? s.p0_card : s.p1_card;
            std::string key = make_info_key(pcard, s.board, round, history);

            double strat[4];
            auto it = nodes.find(key);
            if (it != nodes.end()) it->second.get_average_strategy(strat);
            else for (int i = 0; i < n_actions; i++) strat[i] = 1.0 / n_actions;

            for (int i = 0; i < n_actions; i++)
                if (strat[i] > 1e-10)
                    child_states[i].push_back({s.p0_card, s.p1_card, s.board,
                                               s.opp_reach * strat[i]});
        }

        double total = 0.0;
        for (int i = 0; i < n_actions; i++) {
            if (child_states[i].empty()) continue;
            int nb; int nc[2]; char act_char;
            apply_action(i, nb, nc, act_char);
            total += leduc_br_traverse(child_states[i], round, history + act_char,
                                        nb, nc, br_player, nodes);
        }
        return total;
    }
}

double exploitability(const InfoMap& nodes) {
    auto deals = all_deals();

    std::vector<LeDealState> all_states;
    all_states.reserve(deals.size());
    for (const auto& d : deals)
        all_states.push_back({d.cards[0], d.cards[1], d.cards[2], d.prob});

    int chips[2] = {1, 1};
    // deal.prob is already normalised (1/120), so br0 = E[u_0 | BR_0, avg_1]
    double br0 = leduc_br_traverse(all_states, 0, "", 0, chips, 0, nodes);
    double br1 = leduc_br_traverse(all_states, 0, "", 0, chips, 1, nodes);
    return (br0 + br1) / 2.0;
}

// ---- training loop ----

std::vector<std::pair<int,double>> train(InfoMap& nodes, int iterations,
                                          Mode mode, int eval_every) {
    auto deals = all_deals();
    std::vector<std::pair<int,double>> curve;

    for (int t = 1; t <= iterations; t++) {

        // DCFR: discount positive regrets, floor negatives to 0.
        if (mode == Mode::DCFR) {
            double alpha  = 1.5;
            double pt     = std::pow((double)t, alpha);
            double factor = pt / (pt + 1.0);
            for (auto& [key, node] : nodes)
                for (int a = 0; a < node.num_actions; a++)
                    node.regret_sum[a] = std::max(node.regret_sum[a], 0.0) * factor;
        }

        for (const auto& deal : deals) {
            int chips[2] = {1, 1};
            cfr_traverse(nodes, deal.cards, 0, "", 0, chips,
                         deal.prob, deal.prob, mode, t);
        }

        // CFR+ / DCFR: floor regrets after ALL deals complete (not mid-iteration).
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
    std::cout << "Leduc Hold'em Average Strategy:\n";
    std::vector<std::string> keys;
    for (const auto& kv : nodes) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (const auto& k : keys) {
        const auto& nd = nodes.at(k);
        double s[4];
        nd.get_average_strategy(s);
        std::cout << std::setw(20) << std::left << k << "  ";
        for (int a = 0; a < nd.num_actions; a++)
            std::cout << std::setw(8) << s[a];
        std::cout << "\n";
    }
}

} // namespace leduc
} // namespace cfr
