#include "abstraction.h"
#include "leduc.h"
#include "leduc_utils.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace cfr {
namespace abstraction {

// ---- Bucketing ----

int bucket_id(int priv_rank, int board_rank, BucketScheme scheme) {
    switch (scheme) {
    case BucketScheme::NONE:
    case BucketScheme::RANK_ONLY:
        if (board_rank < 0) return priv_rank;
        return priv_rank * 3 + board_rank;

    case BucketScheme::STRENGTH:
        if (board_rank < 0) return priv_rank;
        if (priv_rank == board_rank) return 0; // pair
        if (priv_rank > board_rank)  return 1; // high card
        return 2;                               // low card
    }
    return 0;
}

int num_buckets_preflop(BucketScheme scheme) {
    (void)scheme; return 3;
}

int num_buckets_postflop(BucketScheme scheme) {
    switch (scheme) {
    case BucketScheme::NONE:
    case BucketScheme::RANK_ONLY: return 9;
    case BucketScheme::STRENGTH:  return 3;
    }
    return 9;
}

std::string abstract_info_key(int private_card, int board_card,
                               int round, const std::string& history,
                               BucketScheme scheme) {
    int priv_rank  = leduc::card_rank(private_card);
    // board_rank is -1 pre-flop (round 0) so bucket_id uses preflop path
    int board_rank = (round >= 1) ? leduc::card_rank(board_card) : -1;
    int bid = bucket_id(priv_rank, board_rank, scheme);
    return "B" + std::to_string(bid) + ":" + history;
}

// ---- Action abstraction ----

void get_abstracted_actions(int num_bets, const std::string& round_hist,
                            ActionScheme scheme,
                            int* actions, int& n_actions) {
    // Determine max raises based on abstraction:
    //   FULL     -> respect the full game's MAX_RAISES cap
    //   NO_RAISE -> cap at 1 (only the first bet; no raise allowed)
    const int max_r = (scheme == ActionScheme::NO_RAISE) ? 1 : leduc::MAX_RAISES;

    // Reuse the shared action-building logic from leduc_utils.h.
    // We can't call the template directly with a runtime max_r, so inline it.
    n_actions = 0;
    bool facing_bet = false;
    if (!round_hist.empty()) {
        int bets = 0, calls = 0;
        for (char c : round_hist) {
            if (c == 'b') { bets++; }
            if (c == 'c' && bets > calls) { calls++; }
        }
        facing_bet = (bets > calls);
    }

    if (!facing_bet) {
        actions[n_actions++] = leduc::CHECK_CALL;
        if (num_bets < max_r)
            actions[n_actions++] = leduc::BET_RAISE;
    } else {
        actions[n_actions++] = leduc::FOLD;
        actions[n_actions++] = leduc::CHECK_CALL;
        if (num_bets < max_r)
            actions[n_actions++] = leduc::BET_RAISE;
    }
}

// ---- Shared game-tree helpers (from leduc_utils.h) ----

static inline int acting_player_round(const std::string& round_hist) {
    return leduc::acting_player_in_round(round_hist);
}

static inline bool round_over(const std::string& rh, bool& folded) {
    return leduc::round_over(rh, folded);
}

static inline int showdown_winner(const int cards[3]) {
    return leduc::showdown_winner(cards);
}

// ---- Abstracted CFR traversal ----

static double abstract_cfr(InfoMap& nodes,
                            const int cards[3],
                            int round,
                            const std::string& history,
                            int num_bets, int chips[2],
                            double r0, double r1,
                            const AbstractConfig& cfg,
                            int iteration) {
    auto slash = history.rfind('/');
    std::string rh = (slash == std::string::npos) ? history : history.substr(slash + 1);

    bool folded = false;
    if (round_over(rh, folded)) {
        if (folded) {
            int fp = acting_player_round(std::string(rh.begin(), rh.end()-1));
            return (fp == 0) ? -(double)chips[0] : (double)chips[1];
        }
        if (round == 0) {
            int nc[2] = {chips[0], chips[1]};
            return abstract_cfr(nodes, cards, 1, history + "/", 0, nc, r0, r1, cfg, iteration);
        }
        int w = showdown_winner(cards);
        if (w > 0) return  (double)chips[1];
        if (w < 0) return -(double)chips[0];
        return 0.0;
    }

    int player = acting_player_round(rh);
    std::string key = abstract_info_key(cards[player], cards[2], round, history, cfg.buckets);

    int avail[3]; int na;
    get_abstracted_actions(num_bets, rh, cfg.actions, avail, na);

    auto it = nodes.find(key);
    if (it == nodes.end())
        it = nodes.emplace(key, InfoNode(na)).first;
    InfoNode& node = it->second;

    double strat[4];
    node.get_strategy(strat);

    double util[4], nutil = 0.0;
    for (int i = 0; i < na; i++) {
        int a = avail[i];
        char ac;
        int nb = num_bets;
        int nc[2] = {chips[0], chips[1]};

        if (a == leduc::FOLD)       ac = 'f';
        else if (a == leduc::CHECK_CALL) {
            ac = 'c';
            if (num_bets > 0) nc[player] = nc[1-player];
        } else {
            ac = 'b'; nb++;
            nc[player] = nc[1-player] + leduc::BET_SIZE[round];
        }

        double nr0 = r0, nr1 = r1;
        if (player == 0) nr0 *= strat[i]; else nr1 *= strat[i];
        util[i] = abstract_cfr(nodes, cards, round, history + ac, nb, nc, nr0, nr1, cfg, iteration);
        nutil += strat[i] * util[i];
    }

    double cf   = (player == 0) ? r1 : r0;
    double my   = (player == 0) ? r0 : r1;
    double sign = (player == 0) ? 1.0 : -1.0;

    for (int i = 0; i < na; i++)
        node.regret_sum[i] += cf * sign * (util[i] - nutil);

    double weight;
    if      (cfg.cfr_mode == Mode::CFR_PLUS) weight = (double)iteration;
    else if (cfg.cfr_mode == Mode::DCFR)     weight = (double)iteration * (double)iteration;
    else                                      weight = 1.0;

    for (int i = 0; i < na; i++)
        node.strategy_sum[i] += weight * my * strat[i];

    return nutil;
}

// ---- Correct infoset-level BR for the abstract game ----
//
// The BR player groups deals by their ABSTRACT infoset (bucket + history).
// This correctly enforces that the BR player can only distinguish deals up to
// the same resolution as the abstract strategy.

struct AbsDealState {
    int p0_card, p1_card, board;
    double opp_reach;
};

static void apply_abs_action(int player, int num_bets, int round,
                             int a_idx, const int avail[],
                             const int chips_in[2],
                             char& act_char, int& nb, int nc[2]) {
    nb = num_bets;
    nc[0] = chips_in[0]; nc[1] = chips_in[1];
    int a = avail[a_idx];
    if (a == leduc::FOLD)            { act_char = 'f'; }
    else if (a == leduc::CHECK_CALL) {
        act_char = 'c';
        if (num_bets > 0) nc[player] = nc[1-player];
    } else {
        act_char = 'b'; nb++;
        nc[player] = nc[1-player] + leduc::BET_SIZE[round];
    }
}

static double abstract_br_traverse(
        const std::vector<AbsDealState>& states,
        int round,
        const std::string& history,
        int num_bets,
        int chips_in[2],
        int br_player,
        const InfoMap& abstract_nodes,
        const AbstractConfig& cfg) {

    if (states.empty()) return 0.0;

    auto slash = history.rfind('/');
    std::string rh = (slash == std::string::npos) ? history : history.substr(slash + 1);

    bool folded = false;
    if (round_over(rh, folded)) {
        if (folded) {
            int fp = acting_player_round(std::string(rh.begin(), rh.end()-1));
            double u_raw = (fp == 0) ? -(double)chips_in[0] : (double)chips_in[1];
            double total = 0.0;
            for (const auto& s : states)
                total += s.opp_reach * ((br_player == 0) ? u_raw : -u_raw);
            return total;
        }
        if (round == 0) {
            int nc[2] = {chips_in[0], chips_in[1]};
            return abstract_br_traverse(states, 1, history + "/", 0, nc,
                                         br_player, abstract_nodes, cfg);
        }
        double total = 0.0;
        for (const auto& s : states) {
            const int cards[3] = {s.p0_card, s.p1_card, s.board};
            int w = showdown_winner(cards);
            double u;
            if (w > 0)      u =  (double)chips_in[1];
            else if (w < 0) u = -(double)chips_in[0];
            else            u = 0.0;
            if (br_player == 1) u = -u;
            total += s.opp_reach * u;
        }
        return total;
    }

    int player = acting_player_round(rh);
    int avail[3]; int na;
    get_abstracted_actions(num_bets, rh, cfg.actions, avail, na);

    if (player == br_player) {
        // Group by ABSTRACT infoset key so the BR player can only condition
        // on the same information as the abstract strategy.
        std::unordered_map<std::string, std::vector<AbsDealState>> by_bucket;
        for (const auto& s : states) {
            int pcard = (player == 0) ? s.p0_card : s.p1_card;
            by_bucket[abstract_info_key(pcard, s.board, round, history, cfg.buckets)].push_back(s);
        }

        double total = 0.0;
        for (auto& [key, group] : by_bucket) {
            double best = -1e18;
            for (int i = 0; i < na; i++) {
                char ac; int nb; int nc[2];
                apply_abs_action(player, num_bets, round, i, avail,
                                  chips_in, ac, nb, nc);
                double v = abstract_br_traverse(group, round, history + ac,
                                                 nb, nc, br_player, abstract_nodes, cfg);
                if (v > best) best = v;
            }
            total += best;
        }
        return total;

    } else {
        // Opponent follows abstract average strategy.
        std::vector<AbsDealState> child_states[3];

        for (const auto& s : states) {
            int pcard = (player == 0) ? s.p0_card : s.p1_card;
            std::string key = abstract_info_key(pcard, s.board, round, history, cfg.buckets);

            double strat[4];
            auto it = abstract_nodes.find(key);
            if (it != abstract_nodes.end()) it->second.get_average_strategy(strat);
            else for (int i = 0; i < na; i++) strat[i] = 1.0 / na;

            for (int i = 0; i < na; i++)
                if (strat[i] > 1e-10)
                    child_states[i].push_back({s.p0_card, s.p1_card, s.board,
                                               s.opp_reach * strat[i]});
        }

        double total = 0.0;
        for (int i = 0; i < na; i++) {
            if (child_states[i].empty()) continue;
            char ac; int nb; int nc[2];
            apply_abs_action(player, num_bets, round, i, avail,
                              chips_in, ac, nb, nc);
            total += abstract_br_traverse(child_states[i], round, history + ac,
                                           nb, nc, br_player, abstract_nodes, cfg);
        }
        return total;
    }
}

// Exploitability within the abstract game (uses abstract infoset keys for
// both training and BR evaluation — correctly measures convergence).
static double abstract_exploitability(const InfoMap& abstract_nodes,
                                       const AbstractConfig& cfg) {
    auto deals = leduc::all_deals();
    std::vector<AbsDealState> all;
    all.reserve(deals.size());
    for (const auto& d : deals)
        all.push_back({d.cards[0], d.cards[1], d.cards[2], d.prob});

    int chips[2] = {1, 1};
    double br0 = abstract_br_traverse(all, 0, "", 0, chips, 0, abstract_nodes, cfg);
    double br1 = abstract_br_traverse(all, 0, "", 0, chips, 1, abstract_nodes, cfg);
    return (br0 + br1) / 2.0;
}

// ---- experiment runner ----

AbstractResult run_abstraction_experiment(const AbstractConfig& config) {
    InfoMap abstract_nodes;
    auto deals = leduc::all_deals();
    AbstractResult result;

    for (int t = 1; t <= config.iterations; t++) {
        // DCFR discount
        if (config.cfr_mode == Mode::DCFR) {
            double alpha  = 1.5;
            double pt     = std::pow((double)t, alpha);
            double factor = pt / (pt + 1.0);
            for (auto& [key, node] : abstract_nodes)
                for (int a = 0; a < node.num_actions; a++)
                    node.regret_sum[a] = std::max(node.regret_sum[a], 0.0) * factor;
        }

        for (const auto& deal : deals) {
            int chips[2] = {1, 1};
            abstract_cfr(abstract_nodes, deal.cards, 0, "", 0, chips,
                         deal.prob, deal.prob, config, t);
        }

        // CFR+ / DCFR: floor regrets after all deals
        if (config.cfr_mode == Mode::CFR_PLUS || config.cfr_mode == Mode::DCFR) {
            for (auto& [key, node] : abstract_nodes)
                node.floor_regrets();
        }

        if (t == 1 || t % config.eval_every == 0 || t == config.iterations) {
            // Evaluate exploitability within the abstract game (correct measurement).
            double expl = abstract_exploitability(abstract_nodes, config);
            result.exploit_curve.push_back({t, expl});
        }
    }

    result.num_abstract_infosets = abstract_nodes.size();
    result.final_exploit = result.exploit_curve.empty()
                         ? 0.0 : result.exploit_curve.back().second;

    // Count full-game infosets for comparison
    InfoMap full_nodes;
    leduc::train(full_nodes, 1, Mode::CFR, 1);  // 1 iter just to populate node keys
    result.num_original_infosets = full_nodes.size();

    return result;
}

} // namespace abstraction
} // namespace cfr
