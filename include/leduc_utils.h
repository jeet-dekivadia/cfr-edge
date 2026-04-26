#pragma once

// Shared game-tree helpers for Leduc Hold'em.
// Extracted to avoid duplication between leduc.cpp and abstraction.cpp.
//
// Design note: all functions are inline so this can be a header-only include.

#include <string>
#include <cassert>

namespace cfr {
namespace leduc {

// ---- Card rank utilities ----

// Deck layout: cards 0-1 are Jacks, 2-3 are Queens, 4-5 are Kings.
inline int card_rank(int card_id) { return card_id / 2; }  // 0=J, 1=Q, 2=K

inline char rank_char(int rank) {
    constexpr char n[] = {'J', 'Q', 'K'};
    assert(rank >= 0 && rank < 3);
    return n[rank];
}

// ---- Acting player ----
// Within a single round, player 0 always acts first.
// round_hist contains only the actions within that round (no '/' separator).
inline int acting_player_in_round(const std::string& round_hist) {
    return (round_hist.size() % 2 == 0) ? 0 : 1;
}

// ---- Round termination ----
// A round ends when:
//   (a) someone folds ('f' is last char), or
//   (b) both players check ("cc"), or
//   (c) a bet is followed by a call ("bc", "bbc" treated as "...c" after "...b")
//
// Returns true if round_hist represents a terminal round state.
// Sets folded=true if a fold ended the round.
inline bool round_over(const std::string& rh, bool& folded) {
    folded = false;
    const int n = static_cast<int>(rh.size());
    if (n < 2) return false;

    // Fold always terminates
    if (rh.back() == 'f') { folded = true; return true; }

    // A call terminates if there was a preceding bet or check
    // i.e. last char 'c' and second-to-last is 'c' (check-check) or 'b' (bet-call)
    if (rh[n - 1] == 'c' && (rh[n - 2] == 'c' || rh[n - 2] == 'b')) return true;

    return false;
}

// ---- Showdown ----
// Returns +1 if P0 wins, -1 if P1 wins, 0 if tie.
// Pair (private rank == board rank) beats any high card.
// Tie-break: higher rank wins.
inline int showdown_winner(const int cards[3]) {
    const int r0    = card_rank(cards[0]);
    const int r1    = card_rank(cards[1]);
    const int board = card_rank(cards[2]);
    const bool pair0 = (r0 == board);
    const bool pair1 = (r1 == board);
    if (pair0 && !pair1) return +1;
    if (!pair0 && pair1) return -1;
    if (r0 > r1) return +1;
    if (r0 < r1) return -1;
    return 0;
}

// ---- Action set ----
// Actions available at a position defined by (num_bets, round_hist).
// Populates actions[0..n_actions-1] with FOLD/CHECK_CALL/BET_RAISE constants.
// Caller must have CHECK_CALL=1, BET_RAISE=2, FOLD=0 in scope.
//   num_bets: number of bets already placed in this round.
//   round_hist: action string for the current round only.
//   max_raises: maximum allowed bets per round.
template<int FOLD_VAL, int CHECK_CALL_VAL, int BET_RAISE_VAL, int MAX_RAISES_VAL>
inline void get_actions_tpl(int num_bets, const std::string& round_hist,
                             int* actions, int& n_actions) {
    n_actions = 0;
    // "Open" position: no unmatched bet outstanding.
    // This is true at start of round or after both players checked.
    // We detect it by: history is empty, OR consists only of checks so far.
    // More robustly: last char (if any) is 'c' from a check (not a call).
    // Simplest: we're not facing a bet if the net bets == net calls.
    bool facing_bet = false;
    if (!round_hist.empty()) {
        int bets = 0, calls = 0;
        for (char c : round_hist) {
            if (c == 'b') { bets++; }
            // A 'c' counts as a call only when there's an uncovered bet
            if (c == 'c' && bets > calls) { calls++; }
        }
        facing_bet = (bets > calls);
    }

    if (!facing_bet) {
        // Can check (or it's the first action: equivalent to check/bet)
        actions[n_actions++] = CHECK_CALL_VAL;
        if (num_bets < MAX_RAISES_VAL)
            actions[n_actions++] = BET_RAISE_VAL;
    } else {
        actions[n_actions++] = FOLD_VAL;
        actions[n_actions++] = CHECK_CALL_VAL;
        if (num_bets < MAX_RAISES_VAL)
            actions[n_actions++] = BET_RAISE_VAL;
    }
}

} // namespace leduc
} // namespace cfr
