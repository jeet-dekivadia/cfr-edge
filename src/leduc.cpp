#include "leduc.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace cfr {
namespace leduc {

// ---- deal enumeration ----

std::vector<Deal> all_deals() {
    std::vector<Deal> deals;
    // pick 3 distinct cards from 6-card deck
    // prob = 1 / (6*5*4) ... but we normalize at the end
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

static int acting_player(const std::string& round_hist) {
    return round_hist.size() % 2 == 0 ? 0 : 1;
}

// who wins at showdown? returns +1 if P0 wins, -1 if P1 wins
static int showdown_winner(const int cards[3]) {
    int r0 = card_rank(cards[0]);
    int r1 = card_rank(cards[1]);
    int board = card_rank(cards[2]);
    bool pair0 = (r0 == board);
    bool pair1 = (r1 == board);
    if (pair0 && !pair1) return +1;
    if (!pair0 && pair1) return -1;
    // both pair or neither: higher card wins
    if (r0 > r1) return +1;
    if (r0 < r1) return -1;
    return 0; // tie
}

// build info set key
// format: "Rank:Board:history" where Board is '?' in round 0
static std::string make_info_key(int player_card, int board_card, int round,
                                  const std::string& full_hist) {
    std::string key;
    key += rank_name(card_rank(player_card));
    key += ':';
    if (round >= 1) {
        key += rank_name(card_rank(board_card));
    } else {
        key += '?';
    }
    key += ':';
    key += full_hist;
    return key;
}

// available actions at this point in a round
static void get_actions(int num_bets, const std::string& round_hist,
                        int* actions, int& n_actions) {
    n_actions = 0;
    if (round_hist.empty() || (round_hist.size() == 1 && round_hist[0] == 'c')) {
        // no bet yet: can check or bet
        actions[n_actions++] = CHECK_CALL; // check
        actions[n_actions++] = BET_RAISE;  // bet
    } else {
        // facing a bet: fold, call, or raise (if raises left)
        actions[n_actions++] = FOLD;
        actions[n_actions++] = CHECK_CALL; // call
        if (num_bets < MAX_RAISES) {
            actions[n_actions++] = BET_RAISE; // raise
        }
    }
}

// check if current round is over (returns true, and also whether someone folded)
static bool round_over(const std::string& rh, bool& folded) {
    folded = false;
    int n = rh.size();
    if (n < 2) return false;
    char last = rh.back();
    if (last == 'f') { folded = true; return true; }
    // two consecutive checks
    if (n >= 2 && rh[n-1] == 'c' && rh[n-2] == 'c') return true;
    // bet then call  or raise then call
    if (n >= 2 && rh[n-1] == 'c' && rh[n-2] == 'b') return true;
    return false;
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

    // split history to get current round's portion
    auto slash = history.rfind('/');
    std::string round_hist = (slash == std::string::npos) ? history : history.substr(slash + 1);

    // check if round is done
    bool folded = false;
    if (round_over(round_hist, folded)) {
        if (folded) {
            // whoever folded loses. the last actor folded.
            int folder = acting_player(round_hist.substr(0, round_hist.size()-1));
            // wait, the fold is the last action, so the player who folded is
            // the one whose turn it was
            int fold_player = acting_player(std::string(round_hist.begin(), round_hist.end() - 1));
            return (fold_player == 0) ? -(double)chips_in[0] : (double)chips_in[1];
        }

        if (round == 0) {
            // move to round 1 (board card revealed)
            int new_chips[2] = {chips_in[0], chips_in[1]};
            return cfr_traverse(nodes, cards, 1, history + "/", 0, new_chips,
                                p0_reach, p1_reach, mode, iteration);
        } else {
            // showdown in round 1
            int w = showdown_winner(cards);
            if (w > 0) return (double)chips_in[1];   // P0 wins P1's chips
            if (w < 0) return -(double)chips_in[0];  // P0 loses their chips
            return 0.0; // tie
        }
    }

    int player = acting_player(round_hist);
    int card = cards[player];
    std::string key = make_info_key(card, cards[2], round, history);

    int avail[3];
    int n_actions;
    get_actions(num_bets, round_hist, avail, n_actions);

    auto it = nodes.find(key);
    if (it == nodes.end()) {
        it = nodes.emplace(key, InfoNode(n_actions)).first;
    }
    InfoNode& node = it->second;

    double strategy[4];
    node.get_strategy(strategy);

    double util[4];
    double node_util = 0.0;

    for (int i = 0; i < n_actions; i++) {
        int a = avail[i];
        char act_char;
        int new_bets = num_bets;
        int new_chips[2] = {chips_in[0], chips_in[1]};

        if (a == FOLD) {
            act_char = 'f';
        } else if (a == CHECK_CALL) {
            act_char = 'c';
            if (num_bets > 0) {
                // calling: match opponent's chips
                int opp = 1 - player;
                new_chips[player] = new_chips[opp];
            }
        } else { // BET_RAISE
            act_char = 'b';
            new_bets++;
            int opp = 1 - player;
            // put in opponent's chips + bet_size
            new_chips[player] = new_chips[opp] + BET_SIZE[round];
        }

        std::string next_hist = history + act_char;
        double np0 = p0_reach, np1 = p1_reach;
        if (player == 0) np0 *= strategy[i];
        else             np1 *= strategy[i];

        util[i] = cfr_traverse(nodes, cards, round, next_hist, new_bets,
                                new_chips, np0, np1, mode, iteration);
        node_util += strategy[i] * util[i];
    }

    double cf_reach = (player == 0) ? p1_reach : p0_reach;
    double my_reach = (player == 0) ? p0_reach : p1_reach;
    double sign     = (player == 0) ? 1.0 : -1.0;

    for (int i = 0; i < n_actions; i++) {
        double regret = sign * (util[i] - node_util);
        node.regret_sum[i] += cf_reach * regret;
    }

    if (mode == Mode::CFR_PLUS)
        node.floor_regrets();

    double weight = (mode == Mode::CFR_PLUS) ? (double)iteration : 1.0;
    for (int i = 0; i < n_actions; i++) {
        node.strategy_sum[i] += weight * my_reach * strategy[i];
    }

    return node_util;
}

// ---- best response ----

static double best_response_val(const InfoMap& nodes,
                                const int cards[3],
                                int round,
                                const std::string& history,
                                int num_bets,
                                int chips_in[2],
                                int br_player) {

    auto slash = history.rfind('/');
    std::string round_hist = (slash == std::string::npos) ? history : history.substr(slash + 1);

    bool folded = false;
    if (round_over(round_hist, folded)) {
        if (folded) {
            int fold_player = acting_player(std::string(round_hist.begin(), round_hist.end() - 1));
            double u = (fold_player == 0) ? -(double)chips_in[0] : (double)chips_in[1];
            return (br_player == 0) ? u : -u;
        }
        if (round == 0) {
            int new_chips[2] = {chips_in[0], chips_in[1]};
            return best_response_val(nodes, cards, 1, history + "/", 0, new_chips, br_player);
        } else {
            int w = showdown_winner(cards);
            double u;
            if (w > 0) u = (double)chips_in[1];
            else if (w < 0) u = -(double)chips_in[0];
            else u = 0.0;
            return (br_player == 0) ? u : -u;
        }
    }

    int player = acting_player(round_hist);
    int card = cards[player];
    std::string key = make_info_key(card, cards[2], round, history);

    int avail[3];
    int n_actions;
    get_actions(num_bets, round_hist, avail, n_actions);

    if (player == br_player) {
        double best = -1e18;
        for (int i = 0; i < n_actions; i++) {
            int a = avail[i];
            char act_char;
            int new_bets = num_bets;
            int new_chips[2] = {chips_in[0], chips_in[1]};

            if (a == FOLD) act_char = 'f';
            else if (a == CHECK_CALL) {
                act_char = 'c';
                if (num_bets > 0) new_chips[player] = new_chips[1-player];
            } else {
                act_char = 'b';
                new_bets++;
                new_chips[player] = new_chips[1-player] + BET_SIZE[round];
            }
            double v = best_response_val(nodes, cards, round, history + act_char,
                                          new_bets, new_chips, br_player);
            if (v > best) best = v;
        }
        return best;
    } else {
        auto it = nodes.find(key);
        double strat[4];
        if (it != nodes.end()) {
            it->second.get_average_strategy(strat);
        } else {
            for (int i = 0; i < n_actions; i++) strat[i] = 1.0 / n_actions;
        }
        double ev = 0.0;
        for (int i = 0; i < n_actions; i++) {
            int a = avail[i];
            char act_char;
            int new_bets = num_bets;
            int new_chips[2] = {chips_in[0], chips_in[1]};

            if (a == FOLD) act_char = 'f';
            else if (a == CHECK_CALL) {
                act_char = 'c';
                if (num_bets > 0) new_chips[player] = new_chips[1-player];
            } else {
                act_char = 'b';
                new_bets++;
                new_chips[player] = new_chips[1-player] + BET_SIZE[round];
            }
            ev += strat[i] * best_response_val(nodes, cards, round, history + act_char,
                                                new_bets, new_chips, br_player);
        }
        return ev;
    }
}

double exploitability(const InfoMap& nodes) {
    auto deals = all_deals();
    double br0 = 0.0, br1 = 0.0;
    for (auto& deal : deals) {
        int chips[2] = {1, 1}; // ante
        br0 += deal.prob * best_response_val(nodes, deal.cards, 0, "", 0, chips, 0);
        br1 += deal.prob * best_response_val(nodes, deal.cards, 0, "", 0, chips, 1);
    }
    return (br0 + br1) / 2.0;
}

// ---- training ----

std::vector<std::pair<int,double>> train(InfoMap& nodes, int iterations,
                                          Mode mode, int eval_every) {
    auto deals = all_deals();
    std::vector<std::pair<int,double>> curve;

    for (int t = 1; t <= iterations; t++) {
        for (auto& deal : deals) {
            int chips[2] = {1, 1};
            cfr_traverse(nodes, deal.cards, 0, "", 0, chips,
                         deal.prob, deal.prob, mode, t);
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
    std::cout << "Leduc Hold'em Average Strategy:\n";
    std::vector<std::string> keys;
    for (auto& kv : nodes) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (auto& k : keys) {
        auto& nd = nodes.at(k);
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
