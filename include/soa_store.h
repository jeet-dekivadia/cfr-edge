#pragma once

// Structure-of-Arrays info set store for cache-friendly batch operations.
// Groups nodes by action count. Each group stores regrets and strategy
// sums in action-major, node-minor layout for SIMD vectorization.

#include "simd_utils.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>

namespace cfr {

class SoAStore {
public:
    // Register an info set. Returns (group_idx, node_idx_within_group).
    std::pair<int,int> get_or_add(const std::string& key, int num_actions) {
        auto it = key_map_.find(key);
        if (it != key_map_.end()) return it->second;

        int g = num_actions - 1; // group 0 = 1 action, group 1 = 2 actions, etc
        if (g >= (int)groups_.size()) groups_.resize(g + 1);

        auto& grp = groups_[g];
        int idx = grp.count;
        grp.count++;
        grp.keys.push_back(key);

        // extend each action's arrays
        for (int a = 0; a <= g; a++) {
            if ((int)grp.regrets.size() <= a) {
                grp.regrets.resize(a + 1);
                grp.strat_sums.resize(a + 1);
                grp.strategy.resize(a + 1);
            }
            grp.regrets[a].push_back(0.0);
            grp.strat_sums[a].push_back(0.0);
            grp.strategy[a].push_back(0.0);
        }

        auto loc = std::make_pair(g, idx);
        key_map_[key] = loc;
        return loc;
    }

    // Pre-compute all strategies from regrets using SIMD batch ops
    void batch_compute_strategies() {
        for (int g = 0; g < (int)groups_.size(); g++) {
            auto& grp = groups_[g];
            if (grp.count == 0) continue;
            int na = g + 1;
            size_t n = grp.count;

            if (na == 2) {
                simd::batch_regret_match_2actions(
                    grp.regrets[0].data(), grp.regrets[1].data(),
                    grp.strategy[0].data(), grp.strategy[1].data(), n);
            } else if (na == 3) {
                simd::batch_regret_match_3actions(
                    grp.regrets[0].data(), grp.regrets[1].data(), grp.regrets[2].data(),
                    grp.strategy[0].data(), grp.strategy[1].data(), grp.strategy[2].data(), n);
            } else {
                // general fallback
                for (size_t i = 0; i < n; i++) {
                    double sum = 0.0;
                    for (int a = 0; a < na; a++) {
                        double r = std::max(grp.regrets[a][i], 0.0);
                        grp.strategy[a][i] = r;
                        sum += r;
                    }
                    if (sum > 0) {
                        for (int a = 0; a < na; a++)
                            grp.strategy[a][i] /= sum;
                    } else {
                        for (int a = 0; a < na; a++)
                            grp.strategy[a][i] = 1.0 / na;
                    }
                }
            }
        }
    }

    // Floor all regrets at zero (CFR+)
    void batch_floor_regrets() {
        for (auto& grp : groups_)
            for (auto& rv : grp.regrets)
                simd::batch_floor_zero(rv.data(), rv.size());
    }

    // DCFR: multiply positive regrets by factor, floor negative regrets to 0.
    // factor = t^alpha / (t^alpha + 1)  with alpha=1.5
    void batch_discount_regrets(double factor) {
        for (auto& grp : groups_) {
            for (auto& rv : grp.regrets) {
                for (double& r : rv)
                    r = (r > 0.0) ? r * factor : 0.0;
            }
        }
    }

    // Access strategy for a specific node
    double get_strategy(int group, int idx, int action) const {
        return groups_[group].strategy[action][idx];
    }

    // Accumulate regret delta
    void add_regret(int group, int idx, int action, double delta) {
        groups_[group].regrets[action][idx] += delta;
    }

    // Accumulate strategy sum
    void add_strategy_sum(int group, int idx, int action, double delta) {
        groups_[group].strat_sums[action][idx] += delta;
    }

    // Get average strategy for a node
    void get_average_strategy(int group, int idx, double* out) const {
        auto& grp = groups_[group];
        int na = group + 1;
        double norm = 0.0;
        for (int a = 0; a < na; a++)
            norm += grp.strat_sums[a][idx];
        if (norm > 0) {
            for (int a = 0; a < na; a++)
                out[a] = grp.strat_sums[a][idx] / norm;
        } else {
            for (int a = 0; a < na; a++)
                out[a] = 1.0 / na;
        }
    }

    int num_actions(int group) const { return group + 1; }

    // lookup
    bool find(const std::string& key, int& group, int& idx) const {
        auto it = key_map_.find(key);
        if (it == key_map_.end()) return false;
        group = it->second.first;
        idx = it->second.second;
        return true;
    }

    size_t total_nodes() const {
        size_t t = 0;
        for (auto& g : groups_) t += g.count;
        return t;
    }

    // iterate over all nodes (for printing etc)
    const std::unordered_map<std::string, std::pair<int,int>>& all_keys() const {
        return key_map_;
    }

private:
    struct Group {
        int count = 0;
        std::vector<std::string> keys;
        std::vector<std::vector<double>> regrets;     // [action][node]
        std::vector<std::vector<double>> strat_sums;  // [action][node]
        std::vector<std::vector<double>> strategy;    // [action][node] (precomputed)
    };

    std::vector<Group> groups_;
    std::unordered_map<std::string, std::pair<int,int>> key_map_;
};

} // namespace cfr
