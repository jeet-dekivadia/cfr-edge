#pragma once

// Shared CFR training-loop utilities.
//
// Extracts the boilerplate that every game's training loop repeats:
//   - strategy-sum weighting (1 / t / t^2 by variant)
//   - DCFR regret discounting (alpha = 1.5)
//   - CFR+ / DCFR regret flooring
//   - convergence-checkpoint predicate

#include "infoset.h"
#include <cmath>

namespace cfr {

// Strategy-sum weight for the current iteration.
//   CFR  -> 1,   CFR+ -> t,   DCFR -> t^2
inline double strategy_sum_weight(Mode mode, int iteration) {
    if      (mode == Mode::CFR_PLUS) return static_cast<double>(iteration);
    else if (mode == Mode::DCFR)     return static_cast<double>(iteration)
                                          * static_cast<double>(iteration);
    return 1.0;
}

// DCFR pre-iteration discount: positive regrets *= t^alpha / (t^alpha + 1),
// negative regrets floored to 0.  alpha = 1.5 (Brown & Sandholm 2019).
inline void dcfr_discount_regrets(InfoMap& nodes, int iteration) {
    constexpr double alpha = 1.5;
    double pt     = std::pow(static_cast<double>(iteration), alpha);
    double factor = pt / (pt + 1.0);
    for (auto& [key, node] : nodes)
        for (int a = 0; a < node.num_actions; a++)
            node.regret_sum[a] = std::max(node.regret_sum[a], 0.0) * factor;
}

// Floor every node's regrets at zero (CFR+ and DCFR post-iteration step).
inline void floor_all_regrets(InfoMap& nodes) {
    for (auto& [key, node] : nodes)
        node.floor_regrets();
}

// Should we record an exploitability sample at iteration t?
inline bool should_eval(int t, int eval_every, int total_iterations) {
    return t == 1 || t % eval_every == 0 || t == total_iterations;
}

} // namespace cfr
