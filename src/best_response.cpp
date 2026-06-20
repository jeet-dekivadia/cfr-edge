// best_response.cpp
// Shared reporting utilities used by the main driver.
// Game-specific best-response traversal lives in kuhn.cpp and leduc.cpp.

#include "kuhn.h"
#include "leduc.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include <utility>
#include <string>
#include <cmath>

namespace cfr {

// Print a formatted exploitability table for a single algorithm run.
void print_exploitability_report(const std::string& game_name,
                                  const std::string& algo_name,
                                  const std::vector<std::pair<int,double>>& curve) {
    std::cout << "\n=== " << game_name << " | " << algo_name << " ===\n";
    std::cout << std::setw(10) << "Iter"
              << std::setw(18) << "Exploitability"
              << std::setw(14) << "vs prev\n";
    std::cout << std::string(42, '-') << "\n";
    std::cout << std::scientific << std::setprecision(5);
    double prev = -1.0;
    for (auto& [iter, expl] : curve) {
        std::cout << std::setw(10) << iter
                  << std::setw(18) << expl;
        if (prev > 0.0)
            std::cout << std::setw(12) << std::fixed << std::setprecision(1)
                      << (expl / prev) << "x";
        std::cout << "\n";
        prev = expl;
        std::cout << std::scientific << std::setprecision(5);
    }
}

// Estimate the empirical convergence rate α such that ε(T) ≈ C · T^(-α).
// Uses a least-squares fit on log(ε) ~ -α · log(T).
// Returns α (positive: faster is higher).
double estimate_convergence_rate(const std::vector<std::pair<int,double>>& curve) {
    if (curve.size() < 4) return 0.0;
    // Only use the latter half to get the asymptotic rate.
    const size_t start = curve.size() / 2;
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    int m = 0;
    for (size_t i = start; i < curve.size(); i++) {
        if (curve[i].second <= 0.0) continue;
        double lx = std::log(static_cast<double>(curve[i].first));
        double ly = std::log(curve[i].second);
        sx  += lx; sy  += ly;
        sxx += lx * lx;
        sxy += lx * ly;
        m++;
    }
    if (m < 2) return 0.0;
    double denom = m * sxx - sx * sx;
    if (std::abs(denom) < 1e-15) return 0.0;
    double slope = (m * sxy - sx * sy) / denom;
    return -slope;  // positive when exploitability decreases with iterations
}

} // namespace cfr
