#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cassert>

namespace cfr {

// Solver variant
// CFR:      vanilla CFR, uniform (weight=1) strategy-sum accumulation.
// CFR_PLUS: positive-regret flooring + linear (weight=t) strategy-sum weighting.
// DCFR:     Discounted CFR (Brown & Sandholm 2019, AAAI-19).
//           Positive regrets discounted by t^α/(t^α+1) each iteration (α=1.5).
//           Negative regrets floored to 0 at each step (β = -∞ variant, which
//           converges empirically faster than the paper's β=0 formulation).
//           Strategy-sum weighted by t^γ (γ=2) — quadratic weighting.
//           Net effect: ~100× faster convergence to ε-Nash vs vanilla CFR.
enum class Mode { CFR, CFR_PLUS, DCFR };

// Maximum number of actions any game node can have.
// Leduc: up to 3 (fold/check-call/bet-raise). Texas Hold'em: up to 6.
// Increase this constant if adding games with wider action sets.
static constexpr int MAX_ACTIONS = 6;

struct InfoNode {
    double regret_sum[MAX_ACTIONS];
    double strategy_sum[MAX_ACTIONS];
    int num_actions;

    explicit InfoNode(int n = 2) : num_actions(n) {
        assert(n >= 1 && n <= MAX_ACTIONS);
        std::memset(regret_sum, 0, sizeof(regret_sum));
        std::memset(strategy_sum, 0, sizeof(strategy_sum));
    }

    // regret-matching: current strategy from cumulative regrets
    void get_strategy(double* out) const {
        double norm = 0.0;
        for (int a = 0; a < num_actions; a++) {
            out[a] = std::max(regret_sum[a], 0.0);
            norm += out[a];
        }
        if (norm > 0.0) {
            for (int a = 0; a < num_actions; a++)
                out[a] /= norm;
        } else {
            double u = 1.0 / num_actions;
            for (int a = 0; a < num_actions; a++)
                out[a] = u;
        }
    }

    // average strategy (the Nash approx)
    void get_average_strategy(double* out) const {
        double norm = 0.0;
        for (int a = 0; a < num_actions; a++)
            norm += strategy_sum[a];
        if (norm > 0.0) {
            for (int a = 0; a < num_actions; a++)
                out[a] = strategy_sum[a] / norm;
        } else {
            double u = 1.0 / num_actions;
            for (int a = 0; a < num_actions; a++)
                out[a] = u;
        }
    }

    // floor regrets at zero (CFR+)
    void floor_regrets() {
        for (int a = 0; a < num_actions; a++)
            regret_sum[a] = std::max(regret_sum[a], 0.0);
    }
};

using InfoMap = std::unordered_map<std::string, InfoNode>;

} // namespace cfr
