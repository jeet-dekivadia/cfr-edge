#include "abstraction.h"
#include "leduc.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <iomanip>

namespace cfr {
namespace abstraction {

// ---- Bucketing ----

int bucket_id(int priv_rank, int board_rank, BucketScheme scheme) {
    switch (scheme) {
    case BucketScheme::NONE:
    case BucketScheme::RANK_ONLY:
        // preflop: just the private rank (0,1,2)
        if (board_rank < 0) return priv_rank;
        // postflop: (private_rank * 3 + board_rank)
        return priv_rank * 3 + board_rank;

    case BucketScheme::STRENGTH:
        if (board_rank < 0) {
            // preflop: all cards are "equal" in strength bucket
            // or we can rank them: 0=J(low), 1=Q(mid), 2=K(high)
            return priv_rank; // 3 preflop buckets
        }
        // postflop: pair=0, high(priv>board)=1, low(priv<=board)=2
        if (priv_rank == board_rank) return 0; // pair
        if (priv_rank > board_rank) return 1;  // high card
        return 2;                               // low card
    }
    return 0;
}

int num_buckets_preflop(BucketScheme scheme) {
    switch (scheme) {
    case BucketScheme::NONE:
    case BucketScheme::RANK_ONLY: return 3;
    case BucketScheme::STRENGTH:  return 3;
    }
    return 3;
}

int num_buckets_postflop(BucketScheme scheme) {
    switch (scheme) {
    case BucketScheme::NONE:
    case BucketScheme::RANK_ONLY: return 9; // 3*3
    case BucketScheme::STRENGTH:  return 3; // pair/high/low
    }
    return 9;
}

std::string abstract_info_key(int private_card, int board_card,
                               int round, const std::string& history,
                               BucketScheme scheme) {
    int priv_rank = leduc::card_rank(private_card);
    int board_rank = (round >= 1) ? leduc::card_rank(board_card) : -1;
    int bid = bucket_id(priv_rank, board_rank, scheme);

    std::string key = "B" + std::to_string(bid) + ":" + history;
    return key;
}

// ---- Action abstraction ----

void get_abstracted_actions(int num_bets, const std::string& round_hist,
                            ActionScheme scheme,
                            int* actions, int& n_actions) {
    n_actions = 0;

    bool facing_bet = false;
    if (!round_hist.empty()) {
        // check if there's an unmatched bet
        int bets = 0, calls = 0;
        for (char c : round_hist) {
            if (c == 'b') bets++;
            if (c == 'c' && bets > calls) calls++;
        }
        facing_bet = (bets > calls);
    }

    if (!facing_bet) {
        // no bet: check or bet
        actions[n_actions++] = leduc::CHECK_CALL;
        if (scheme == ActionScheme::FULL || num_bets < 1)
            actions[n_actions++] = leduc::BET_RAISE;
    } else {
        // facing bet: fold, call, or raise
        actions[n_actions++] = leduc::FOLD;
        actions[n_actions++] = leduc::CHECK_CALL;
        if (scheme == ActionScheme::FULL && num_bets < leduc::MAX_RAISES)
            actions[n_actions++] = leduc::BET_RAISE;
        // NO_RAISE: no raise option
    }
}

// ---- abstracted CFR traversal for Leduc ----

static int acting_player_round(const std::string& round_hist) {
    return round_hist.size() % 2 == 0 ? 0 : 1;
}

static bool round_over(const std::string& rh, bool& folded) {
    folded = false;
    int n = rh.size();
    if (n < 2) return false;
    if (rh.back() == 'f') { folded = true; return true; }
    if (n >= 2 && rh[n-1] == 'c' && (rh[n-2] == 'c' || rh[n-2] == 'b'))
        return true;
    return false;
}

static int showdown_winner(const int cards[3]) {
    int r0 = leduc::card_rank(cards[0]);
    int r1 = leduc::card_rank(cards[1]);
    int board = leduc::card_rank(cards[2]);
    bool pair0 = (r0 == board), pair1 = (r1 == board);
    if (pair0 && !pair1) return 1;
    if (!pair0 && pair1) return -1;
    if (r0 > r1) return 1;
    if (r0 < r1) return -1;
    return 0;
}

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
            return abstract_cfr(nodes, cards, 1, history + "/", 0, nc,
                                r0, r1, cfg, iteration);
        }
        int w = showdown_winner(cards);
        if (w > 0) return (double)chips[1];
        if (w < 0) return -(double)chips[0];
        return 0.0;
    }

    int player = acting_player_round(rh);
    std::string key = abstract_info_key(cards[player], cards[2], round, history,
                                         cfg.buckets);

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

        if (a == leduc::FOLD) ac = 'f';
        else if (a == leduc::CHECK_CALL) {
            ac = 'c';
            if (num_bets > 0) nc[player] = nc[1-player];
        } else {
            ac = 'b'; nb++;
            nc[player] = nc[1-player] + leduc::BET_SIZE[round];
        }

        double nr0 = r0, nr1 = r1;
        if (player == 0) nr0 *= strat[i]; else nr1 *= strat[i];

        util[i] = abstract_cfr(nodes, cards, round, history + ac,
                                nb, nc, nr0, nr1, cfg, iteration);
        nutil += strat[i] * util[i];
    }

    double cf = (player == 0) ? r1 : r0;
    double my = (player == 0) ? r0 : r1;
    double sign = (player == 0) ? 1.0 : -1.0;

    for (int i = 0; i < na; i++)
        node.regret_sum[i] += cf * sign * (util[i] - nutil);

    if (cfg.cfr_mode == Mode::CFR_PLUS) node.floor_regrets();

    double w = (cfg.cfr_mode == Mode::CFR_PLUS) ? (double)iteration : 1.0;
    for (int i = 0; i < na; i++)
        node.strategy_sum[i] += w * my * strat[i];

    return nutil;
}

// ---- experiment runner ----

AbstractResult run_abstraction_experiment(const AbstractConfig& config) {
    InfoMap abstract_nodes;
    auto deals = leduc::all_deals();
    AbstractResult result;

    for (int t = 1; t <= config.iterations; t++) {
        for (auto& deal : deals) {
            int chips[2] = {1, 1};
            abstract_cfr(abstract_nodes, deal.cards, 0, "", 0, chips,
                         deal.prob, deal.prob, config, t);
        }

        if (t == 1 || t % config.eval_every == 0 || t == config.iterations) {
            // measure exploitability of the abstracted strategy
            // mapped back to the full game via the bucketing
            // (for simplicity, we measure within the abstract game)
            double expl = leduc::exploitability(abstract_nodes);
            result.exploit_curve.push_back({t, expl});
        }
    }

    result.num_abstract_infosets = abstract_nodes.size();
    result.final_exploit = result.exploit_curve.empty() ? 0.0 : result.exploit_curve.back().second;

    // also train full game for comparison baseline
    InfoMap full_nodes;
    leduc::train(full_nodes, config.iterations, config.cfr_mode, config.eval_every);
    result.num_original_infosets = full_nodes.size();

    return result;
}

} // namespace abstraction
} // namespace cfr
