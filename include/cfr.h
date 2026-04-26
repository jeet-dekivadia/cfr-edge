#pragma once

// Unified CFR / CFR+ / DCFR engine.
// Supports Kuhn Poker, Leduc Hold'em, and Texas Hold'em via game-tag dispatch.

#include "infoset.h"
#include "soa_store.h"
#include <string>
#include <vector>
#include <utility>

namespace cfr {

enum class Game { KUHN, LEDUC, HOLDEM };

struct TrainConfig {
    Game game         = Game::KUHN;
    Mode mode         = Mode::CFR;
    int iterations    = 10000;
    int eval_every    = 100;
    bool use_soa      = false;   // use SoA store + SIMD batch ops (Kuhn only)
};

struct TrainResult {
    std::vector<std::pair<int,double>> exploit_curve;  // (iteration, exploitability)
    double final_exploitability;
    double elapsed_seconds;
    size_t num_infosets;
    double convergence_rate;  // empirical α in ε ~ C·T^(-α)
};

// Run training and return results.
TrainResult run_training(const TrainConfig& config);

// ---- Shared reporting (best_response.cpp) ----
void print_exploitability_report(const std::string& game_name,
                                  const std::string& algo_name,
                                  const std::vector<std::pair<int,double>>& curve);

double estimate_convergence_rate(const std::vector<std::pair<int,double>>& curve);

} // namespace cfr
