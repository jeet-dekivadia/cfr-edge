#pragma once

// JSON serialization for CFR results.
// Produces the strategy snapshot bundles consumed by the Next.js frontend.
//
// Output schema (per game file):
// {
//   "game":       "kuhn" | "leduc" | "holdem",
//   "algorithm":  "CFR" | "CFR+" | "DCFR",
//   "iterations": N,
//   "convergence_rate": α,          // empirical ε ~ C·T^(-α)
//   "convergence": [
//     {"iter": T, "exploitability": ε}, ...
//   ],
//   "strategy_snapshots": {          // strategy at selected checkpoints
//     "500":  { "infoset_key": {"action": prob, ...}, ... },
//     "2000": { ... },
//     ...
//   },
//   "final_strategy": {              // full final strategy for viz
//     "infoset_key": {
//       "actions": ["pass","bet"],
//       "probs":   [0.333, 0.667],
//       "regrets": [0.0, 0.12]       // final regret sums (sign: positive = regret)
//     }, ...
//   }
// }

#include "infoset.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <utility>
#include <map>

namespace cfr {

using json = nlohmann::json;

// Serialise a single InfoMap to a json object of the form:
//   { "infoset_key": { "actions":[...], "probs":[...], "regrets":[...] }, ... }
// action_labels: for each game, the human-readable name of action index i.
inline json infomap_to_json(const InfoMap& nodes,
                             const std::vector<std::string>& action_labels) {
    json out = json::object();
    // Sort keys for deterministic output
    std::vector<std::string> keys;
    keys.reserve(nodes.size());
    for (const auto& [k, _] : nodes) keys.push_back(k);
    std::sort(keys.begin(), keys.end());

    for (const auto& k : keys) {
        const InfoNode& nd = nodes.at(k);
        double avg[MAX_ACTIONS];
        nd.get_average_strategy(avg);

        json node_obj;
        json acts = json::array();
        json probs = json::array();
        json regrets = json::array();
        for (int a = 0; a < nd.num_actions; a++) {
            acts.push_back(a < (int)action_labels.size() ? action_labels[a] : std::to_string(a));
            probs.push_back(std::round(avg[a] * 10000.0) / 10000.0);  // 4 decimal places
            regrets.push_back(std::round(nd.regret_sum[a] * 1e6) / 1e6);
        }
        node_obj["actions"]  = acts;
        node_obj["probs"]    = probs;
        node_obj["regrets"]  = regrets;
        out[k] = node_obj;
    }
    return out;
}

// Serialise a full training run to the frontend JSON format.
struct JsonOutputConfig {
    std::string game;        // "kuhn", "leduc", "holdem"
    std::string algorithm;   // "CFR", "CFR+", "DCFR"
    int total_iterations;
    double convergence_rate;
    std::vector<std::string> action_labels;
    // Which iteration checkpoints to snapshot (must be a subset of eval_every points)
    std::vector<int> snapshot_iters;
};

struct SnapshotEntry {
    int iter;
    InfoMap nodes;
};

inline json build_json_output(
        const JsonOutputConfig& cfg,
        const std::vector<std::pair<int,double>>& convergence,
        const std::vector<SnapshotEntry>& snapshots,
        const InfoMap& final_nodes) {

    json root;
    root["game"]             = cfg.game;
    root["algorithm"]        = cfg.algorithm;
    root["iterations"]       = cfg.total_iterations;
    root["convergence_rate"] = std::round(cfg.convergence_rate * 1000.0) / 1000.0;

    // Convergence curve
    json conv_arr = json::array();
    for (const auto& [it, ex] : convergence)
        conv_arr.push_back({{"iter", it}, {"exploitability", ex}});
    root["convergence"] = conv_arr;

    // Strategy snapshots at intermediate iterations
    json snaps = json::object();
    for (const auto& snap : snapshots)
        snaps[std::to_string(snap.iter)] = infomap_to_json(snap.nodes, cfg.action_labels);
    root["strategy_snapshots"] = snaps;

    // Final strategy
    root["final_strategy"] = infomap_to_json(final_nodes, cfg.action_labels);

    return root;
}

} // namespace cfr
