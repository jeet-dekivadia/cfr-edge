// best_response.cpp
// standalone best-response evaluation utilities
// (game-specific BR logic lives in kuhn.cpp and leduc.cpp;
//  this file provides shared helpers for the main driver)

#include "kuhn.h"
#include "leduc.h"
#include <iostream>
#include <iomanip>

namespace cfr {

void print_exploitability_report(const std::string& game_name,
                                  const std::vector<std::pair<int,double>>& curve) {
    std::cout << "\n=== " << game_name << " Exploitability ===\n";
    std::cout << std::setw(10) << "Iter" << std::setw(16) << "Exploitability" << "\n";
    std::cout << std::string(26, '-') << "\n";
    std::cout << std::scientific << std::setprecision(6);
    for (auto& [iter, expl] : curve) {
        std::cout << std::setw(10) << iter << std::setw(16) << expl << "\n";
    }
}

} // namespace cfr
