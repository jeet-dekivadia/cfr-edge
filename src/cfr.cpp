#include "cfr.h"
#include "kuhn.h"
#include "leduc.h"
#include <chrono>
#include <iostream>

#ifdef HAS_OPENMP
#include <omp.h>
#endif

namespace cfr {

// ---- SoA-backed Kuhn traversal ----

namespace kuhn_soa {

static constexpr int NUM_ACTIONS = 2;
static constexpr char card_name[] = {'J', 'Q', 'K'};

static bool is_terminal(const std::string& h) {
    int n = h.size();
    if (n < 2) return false;
    if (n == 2) return (h == "pp" || h == "bb" || h == "bp");
    if (n == 3) return (h == "pbb" || h == "pbp");
    return false;
}

static double payoff(int c0, int c1, const std::string& h) {
    bool w = c0 > c1;
    if (h == "pp")  return w ?  1.0 : -1.0;
    if (h == "bp")  return  1.0;
    if (h == "bb")  return w ?  2.0 : -2.0;
    if (h == "pbp") return -1.0;
    if (h == "pbb") return w ?  2.0 : -2.0;
    return 0.0;
}

static double traverse(SoAStore& store, int c0, int c1,
                       const std::string& hist,
                       double r0, double r1,
                       Mode mode, int iter) {
    if (is_terminal(hist))
        return payoff(c0, c1, hist);

    int player = hist.size() % 2 == 0 ? 0 : 1;
    // handle "pb" case: length 2 -> player 0
    // already correct since 2%2==0

    int card = (player == 0) ? c0 : c1;
    char buf[8];
    buf[0] = card_name[card]; buf[1] = ':';
    int len = 2;
    for (char c : hist) buf[len++] = c;
    std::string key(buf, len);

    auto [g, idx] = store.get_or_add(key, NUM_ACTIONS);

    double strat[2];
    strat[0] = store.get_strategy(g, idx, 0);
    strat[1] = store.get_strategy(g, idx, 1);

    double util[2], node_util = 0.0;
    char ac[] = {'p', 'b'};

    for (int a = 0; a < 2; a++) {
        double nr0 = r0, nr1 = r1;
        if (player == 0) nr0 *= strat[a]; else nr1 *= strat[a];
        util[a] = traverse(store, c0, c1, hist + ac[a], nr0, nr1, mode, iter);
        node_util += strat[a] * util[a];
    }

    double cf = (player == 0) ? r1 : r0;
    double my = (player == 0) ? r0 : r1;
    double sign = (player == 0) ? 1.0 : -1.0;

    for (int a = 0; a < 2; a++)
        store.add_regret(g, idx, a, cf * sign * (util[a] - node_util));

    double w = (mode == Mode::CFR_PLUS) ? (double)iter : 1.0;
    for (int a = 0; a < 2; a++)
        store.add_strategy_sum(g, idx, a, w * my * strat[a]);

    return node_util;
}

} // namespace kuhn_soa

// ---- main training dispatch ----

TrainResult run_training(const TrainConfig& config) {
    TrainResult result;
    auto t0 = std::chrono::high_resolution_clock::now();

    if (!config.use_soa) {
        // standard AoS path
        InfoMap nodes;
        std::vector<std::pair<int,double>> curve;

        if (config.game == Game::KUHN) {
            curve = kuhn::train(nodes, config.iterations, config.mode, config.eval_every);
            result.num_infosets = nodes.size();
        } else {
            curve = leduc::train(nodes, config.iterations, config.mode, config.eval_every);
            result.num_infosets = nodes.size();
        }
        result.exploit_curve = curve;
        result.final_exploitability = curve.empty() ? 0.0 : curve.back().second;
    } else {
        // SoA path (Kuhn only for now, Leduc more complex)
        if (config.game == Game::KUHN) {
            SoAStore store;
            constexpr int deals[][2] = {{0,1},{0,2},{1,0},{1,2},{2,0},{2,1}};

            for (int t = 1; t <= config.iterations; t++) {
                // batch compute strategies before each traversal
                store.batch_compute_strategies();

                for (auto& d : deals) {
                    kuhn_soa::traverse(store, d[0], d[1], "", 1.0, 1.0,
                                       config.mode, t);
                }

                if (config.mode == Mode::CFR_PLUS)
                    store.batch_floor_regrets();

                if (t == 1 || t % config.eval_every == 0 || t == config.iterations) {
                    // compute exploitability using the InfoMap-based evaluator
                    // (build a temporary InfoMap from the SoA store)
                    InfoMap tmp;
                    for (auto& [key, loc] : store.all_keys()) {
                        int g = loc.first, idx = loc.second;
                        int na = store.num_actions(g);
                        InfoNode nd(na);
                        store.get_average_strategy(g, idx, nd.strategy_sum);
                        // hack: put avg strategy directly in strategy_sum with norm=1
                        // the get_average_strategy call will just renormalize
                        double sum = 0;
                        for (int a = 0; a < na; a++) sum += nd.strategy_sum[a];
                        if (sum > 0) {
                            for (int a = 0; a < na; a++)
                                nd.strategy_sum[a] = nd.strategy_sum[a]; // already normalized by get_average_strategy
                        }
                        tmp[key] = nd;
                    }
                    double expl = kuhn::exploitability(tmp);
                    result.exploit_curve.push_back({t, expl});
                }
            }
            result.num_infosets = store.total_nodes();
            result.final_exploitability = result.exploit_curve.empty()
                ? 0.0 : result.exploit_curve.back().second;
        } else {
            // leduc SoA: fallback to AoS
            InfoMap nodes;
            auto curve = leduc::train(nodes, config.iterations, config.mode, config.eval_every);
            result.exploit_curve = curve;
            result.num_infosets = nodes.size();
            result.final_exploitability = curve.empty() ? 0.0 : curve.back().second;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(t1 - t0).count();
    return result;
}

} // namespace cfr
