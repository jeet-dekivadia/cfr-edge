#pragma once

#include "infoset.h"
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

namespace cfr {
namespace leduc {

// ranks
constexpr int JACK = 0;
constexpr int QUEEN = 1;
constexpr int KING = 2;
constexpr int NUM_RANKS = 3;
constexpr int CARDS_PER_RANK = 2;
constexpr int DECK_SIZE = 6;

// actions within a round
constexpr int FOLD  = 0;
constexpr int CHECK_CALL = 1;  // check if no bet, call if facing bet
constexpr int BET_RAISE   = 2;  // bet if no bet, raise if facing bet

constexpr int BET_SIZE[2] = {2, 4};  // round 0 and round 1
constexpr int MAX_RAISES = 2;        // max bets per round (1 bet + 1 raise)

inline char rank_name(int r) {
    if (r < 0 || r >= NUM_RANKS) {
        throw std::out_of_range("rank_name: rank index " + std::to_string(r) +
                                " out of range [0, " + std::to_string(NUM_RANKS) + ")");
    }
    constexpr char n[] = {'J', 'Q', 'K'};
    return n[r];
}

// rank of the i-th card in the 6-card deck (0..5)
inline int card_rank(int card_id) { return card_id / CARDS_PER_RANK; }

// A deal: cards[0]=P0 card_id, cards[1]=P1 card_id, cards[2]=board card_id
struct Deal {
    int cards[3];
    double prob;  // probability of this deal
};

// enumerate all possible deals (6*5*4 / ... but we pick 3 distinct cards)
std::vector<Deal> all_deals();

// CFR traversal for leduc, returns EV for player 0
double cfr_traverse(InfoMap& nodes,
                    const int cards[3],   // p0, p1, board
                    int round,            // 0 or 1
                    const std::string& history,
                    int num_bets,         // bets placed in current round
                    int chips_in[2],      // chips each player has in the pot
                    double p0_reach, double p1_reach,
                    Mode mode, int iteration);

// training loop
std::vector<std::pair<int,double>> train(InfoMap& nodes, int iterations,
                                          Mode mode, int eval_every = 200);

// exact best-response exploitability
double exploitability(const InfoMap& nodes);

void print_strategy(const InfoMap& nodes);

} // namespace leduc
} // namespace cfr
