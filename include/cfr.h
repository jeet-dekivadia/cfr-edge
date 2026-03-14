#pragma once

// Unified CFR / CFR+ engine with SoA store backend and optional parallel rollouts.
// Supports both Kuhn and Leduc via game-tag dispatch.

#include "infoset.h"
#include "soa_store.h"
#include <string>
#include <vector>
#include <utility>
#include <functional>

namespace cfr {

enum class Game { KUHN, LEDUC };

struct TrainConfig {
    Game game         = Game::KUHN;
    Mode mode         = Mode::CFR;
    int iterations    = 10000;
    int eval_every    = 100;
    bool use_soa      = false;   // use SoA store + SIMD batch ops
    bool parallel     = false;   // parallel deal enumeration (OpenMP)
    int num_threads   = 4;
};

struct TrainResult {
    std::vector<std::pair<int,double>> exploit_curve;  // (iteration, exploitability)
    double final_exploitability;
    double elapsed_seconds;
    size_t num_infosets;
};

// Run training and return results.
// Uses the InfoMap (AoS) path by default.
// If config.use_soa is true, uses SoAStore path.
TrainResult run_training(const TrainConfig& config);

} // namespace cfr
