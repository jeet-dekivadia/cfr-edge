#pragma once

// 7-card poker hand evaluator.
// Evaluates the best 5-card hand from any 7 cards in O(21) comparisons.
//
// Hand rank: lower integer = stronger hand (0 is the best possible hand).
// Rank breakdown (approximate ranges):
//   0     – 10     : Royal/Straight Flush
//   10    – 166    : Four of a Kind
//   167   – 322    : Full House
//   323   – 1599   : Flush
//   1600  – 1609   : Straight
//   1610  – 2467   : Three of a Kind
//   2468  – 3325   : Two Pair
//   3326  – 6185   : One Pair
//   6186  – 7461   : High Card
//
// Card encoding:
//   card = rank * 4 + suit
//   rank: 0=2, 1=3, ..., 12=Ace
//   suit: 0=clubs, 1=diamonds, 2=hearts, 3=spades
//
// Usage:
//   int rank = eval_7(hole[0], hole[1], board[0], board[1], board[2], board[3], board[4]);

#include <cstdint>
#include <array>
#include <algorithm>
#include <stdexcept>

namespace cfr {
namespace handeval {

// ---- Card encoding helpers ----
inline int make_card(int rank, int suit) { return rank * 4 + suit; }
inline int card_rank(int c) { return c / 4; }
inline int card_suit(int c) { return c % 4; }

static constexpr const char* RANK_STR[] = {
    "2","3","4","5","6","7","8","9","T","J","Q","K","A"
};
static constexpr const char* SUIT_STR[] = { "c","d","h","s" };

inline std::string card_str(int c) {
    return std::string(RANK_STR[card_rank(c)]) + SUIT_STR[card_suit(c)];
}

// ---- 5-card evaluator (Cactus Kev / Senzee approach) ----
// Each card is represented as a 32-bit prime-product key for flush / rank checks.
// This is a lightweight implementation using lookup tables built at startup.

// Prime for each rank (for prime-product hand hashing)
static constexpr int PRIMES[] = {2,3,5,7,11,13,17,19,23,29,31,37,41};

// Encode card as 32-bit value:
//  bits 0-7  : 8-bit prime
//  bits 8-11 : rank (0-12)
//  bit 12-15 : bitrank (1 << rank) lo
//  bits 16-19: suit bit (club=1,diamond=2,heart=4,spade=8) as nibble
//  bits 20-31: (1 << rank) hi portion is bits 16-28
inline uint32_t encode_card(int c) {
    int r = card_rank(c);
    int s = card_suit(c);
    return static_cast<uint32_t>(
        PRIMES[r] |
        (r << 8) |
        (1 << (r + 16)) |
        (0x8000 >> s) << 12    // suit bit in bits 12-15: clubs=0x8,diamonds=0x4,...
    );
}

// ---- Lookup tables for 5-card evaluation ----
// We use a two-table scheme:
//   flush_table[bitrank_mask]  -> hand rank for 5-card flushes
//   unique5_table[bitrank_mask]-> hand rank for 5-card straights / high cards (no pair)
//   pairs_table[prime_product] -> hand rank for hands with pairs (via hash)
//
// For a complete implementation we generate these tables programmatically.

class HandEvaluator {
public:
    HandEvaluator();

    // Evaluate best 5-card hand from exactly 7 cards. Returns rank (lower=better).
    int eval7(const int cards[7]) const;

    // Evaluate exactly 5 cards.
    int eval5(const int cards[5]) const;

    // Equity of hand h1 vs h2 against a given board (0-5 board cards).
    // Returns fraction of outcomes where h1 wins (ties count 0.5).
    float equity(const int h1[2], const int h2[2],
                 const int board[], int board_count) const;

    // Hand category string
    static std::string category(int rank) {
        if (rank <=  9) return "Straight Flush";
        if (rank <= 165) return "Four of a Kind";
        if (rank <= 321) return "Full House";
        if (rank <= 1598) return "Flush";
        if (rank <= 1608) return "Straight";
        if (rank <= 2466) return "Three of a Kind";
        if (rank <= 3324) return "Two Pair";
        if (rank <= 6184) return "One Pair";
        return "High Card";
    }

private:
    // flush_ranks[13-bit hand mask] -> rank (only meaningful for flushes)
    std::array<int16_t, 8192> flush_table_;
    // noflush5[13-bit hand mask or prime product hash] for unique-rank 5-card hands
    std::array<int16_t, 8192> unique5_table_;
    // hash table for hands with repeated ranks (pairs, trips, quads, boats)
    std::array<int16_t, 49205> hash_adjust_;
    std::array<int16_t, 8192>  hash_values_;

    void build_flush_table();
    void build_unique5_table();
    void build_pairs_table();

    int eval5_impl(uint32_t c0, uint32_t c1, uint32_t c2,
                   uint32_t c3, uint32_t c4) const;
};

// ---- Global singleton (initialised once) ----
const HandEvaluator& get_evaluator();

// Convenience wrappers
inline int eval7(const int cards[7]) { return get_evaluator().eval7(cards); }
inline int eval5(const int cards[5]) { return get_evaluator().eval5(cards); }

} // namespace handeval
} // namespace cfr
