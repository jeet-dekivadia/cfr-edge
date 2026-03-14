#pragma once

#include "infoset.h"
#include <string>
#include <vector>
#include <utility>

namespace cfr {
namespace kuhn {

constexpr int JACK = 0;
constexpr int QUEEN = 1;
constexpr int KING = 2;
constexpr int NUM_CARDS = 3;
constexpr int NUM_ACTIONS = 2; // pass, bet
constexpr int PASS = 0;
constexpr int BET = 1;

inline char card_name(int c) {
    constexpr char n[] = {'J', 'Q', 'K'};
    return n[c];
}

// CFR tree traversal. Returns EV for player 0.
double cfr_traverse(InfoMap& nodes, int p0_card, int p1_card,
                    const std::string& history,
                    double p0_reach, double p1_reach,
                    Mode mode, int iteration);

// Train for N iterations. Returns exploitability samples (iter, expl).
std::vector<std::pair<int,double>> train(InfoMap& nodes, int iterations,
                                         Mode mode, int eval_every = 100);

// Exact best-response exploitability of the average strategy
double exploitability(const InfoMap& nodes);

// Dump average strategy to stdout
void print_strategy(const InfoMap& nodes);

} // namespace kuhn
} // namespace cfr
