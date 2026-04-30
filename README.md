# cfr-edge

A full-stack poker GTO solver — CFR / CFR+ / DCFR engine in C++17 with a Next.js visualisation frontend.

## What it does

- Solves **Kuhn Poker** (12 infosets) and **Leduc Hold'em** (288 infosets) to provable near-Nash strategies
- **MCCFR External Sampling** on heads-up no-limit **Texas Hold'em** (33K+ infosets)
- Exploitability tracking via exact best-response computation
- AVX2-vectorised batch regret matching (~4 doubles/cycle)
- SoA (Structure-of-Arrays) infoset store for cache-friendly SIMD traversal
- Interactive Next.js frontend: convergence chart, strategy heatmap, play vs Nash, preflop range chart

## Algorithms

| Algorithm | Strategy weight | Regret discount | Empirical α in ε~T^(-α) |
|-----------|-----------------|-----------------|--------------------------|
| CFR       | 1 (uniform)     | none            | ~0.53                    |
| CFR+      | t (linear)      | floor at 0      | ~0.50                    |
| DCFR      | t² (quadratic)  | pos×t^1.5/(t^1.5+1), neg→0 | ~0.61        |

## Build (C++)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j4
```

Requires CMake ≥ 3.16, C++17 compiler, internet access to auto-fetch `nlohmann/json`.

### Run the solver

```bash
./build/cfr_solver                  # all experiments
./build/cfr_solver --kuhn-only      # Kuhn only
./build/cfr_solver --leduc-only     # Leduc only
./build/cfr_solver --holdem-only    # Texas Hold'em (MCCFR)
```

### Generate strategy JSON for the frontend

```bash
./build/json_exporter --out ./frontend/public/strategies/
```

## Frontend

```bash
cd frontend
npm install
npm run dev          # http://localhost:3000
npm run build        # production build
```

### Pages

| Route         | Description |
|---------------|-------------|
| `/`           | Landing page with 3D poker table |
| `/solve`      | Live convergence chart + strategy heatmap with iteration scrubber |
| `/play`       | Play Kuhn Poker against the Nash strategy |
| `/strategy`   | Browse all infosets, sort by entropy / regret, export CSV |
| `/algorithm`  | Step-by-step CFR walkthrough + live regret waterfall |
| `/holdem`     | Texas Hold'em preflop range chart (169-hand GTO frequencies) |

## Project structure

```
include/
  kuhn.h            Kuhn Poker game tree + CFR
  leduc.h           Leduc Hold'em game tree + CFR
  leduc_utils.h     Shared helpers (round_over, showdown_winner, ...)
  cfr.h             TrainConfig / TrainResult / run_training()
  infoset.h         InfoNode (regret + strategy sum, MAX_ACTIONS=6)
  soa_store.h       SoA infoset store (SIMD batch ops)
  simd_utils.h      AVX2/SSE2 regret matching helpers
  abstraction.h     Card bucketing + action abstraction for Leduc
  hand_eval.h       7-card poker hand evaluator
  holdem.h          HUNL Texas Hold'em + MCCFR
  json_output.h     Strategy JSON serialisation

src/
  kuhn.cpp  leduc.cpp  cfr.cpp  abstraction.cpp
  best_response.cpp   hand_eval.cpp  holdem.cpp
  main.cpp            json_exporter.cpp

frontend/           Next.js 14 + TypeScript + Tailwind + D3 + Three.js
```

## Results

```
Kuhn Poker  DCFR  10K iters: ε = 4.4×10⁻⁴  (Nash is ε=0)
Leduc       DCFR  10K iters: ε = 7.5×10⁻³
HoldEm MCCFR 1K iters: 33K infosets created
```
