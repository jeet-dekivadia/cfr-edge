#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstring>

namespace cfr {

// Solver variant
enum class Mode { CFR, CFR_PLUS };

struct InfoNode {
    double regret_sum[4];
    double strategy_sum[4];
    int num_actions;

    explicit InfoNode(int n = 2) : num_actions(n) {
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
