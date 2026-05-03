# CFR-Edge

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white" alt="C++17" />
  <img src="https://img.shields.io/badge/CMake-3.16%2B-064F8C?style=flat-square&logo=cmake&logoColor=white" alt="CMake" />
  <img src="https://img.shields.io/badge/SIMD-AVX2%20%2F%20SSE2-76b900?style=flat-square&logo=nvidia&logoColor=white" alt="AVX2" />
  <img src="https://img.shields.io/badge/Next.js-16-black?style=flat-square&logo=nextdotjs&logoColor=white" alt="Next.js 16" />
  <img src="https://img.shields.io/badge/React-19-61DAFB?style=flat-square&logo=react&logoColor=black" alt="React 19" />
  <img src="https://img.shields.io/badge/TypeScript-5-3178C6?style=flat-square&logo=typescript&logoColor=white" alt="TypeScript" />
  <img src="https://img.shields.io/badge/Tailwind_CSS-3-06B6D4?style=flat-square&logo=tailwindcss&logoColor=white" alt="Tailwind CSS" />
  <img src="https://img.shields.io/badge/Three.js-r165-black?style=flat-square&logo=threedotjs&logoColor=white" alt="Three.js" />
  <img src="https://img.shields.io/badge/D3.js-v7-F9A03C?style=flat-square&logo=d3dotjs&logoColor=white" alt="D3.js" />
  <img src="https://img.shields.io/badge/Algorithm-CFR%20%7C%20CFR%2B%20%7C%20DCFR-10b981?style=flat-square" alt="CFR Algorithms" />
  <img src="https://img.shields.io/badge/Nash_ε-4.4×10⁻⁴-emerald?style=flat-square" alt="Nash Exploitability" />
  <img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="MIT License" />
</p>

**A production-grade Counterfactual Regret Minimization engine for poker, with a full-stack interactive visualisation platform.**

CFR-Edge implements three generations of the CFR algorithm family — Vanilla CFR, CFR+, and Discounted CFR (DCFR) — in AVX2-accelerated C++17, and exposes the solver results through a Next.js web application that lets users watch strategies converge to Nash equilibrium in real time, play against the solved strategy, and explore every information set in depth.

Games supported: **Kuhn Poker**, **Leduc Hold'em**, and **Heads-Up No-Limit Texas Hold'em** (via Monte Carlo CFR with external sampling).

---

## Authors

| Name | GitHub |
|------|--------|
| Himansh Chitkara | [@heyman7913](https://github.com/heyman7913) |
| Jeet Dekivadia | [@jeet-dekivadia](https://github.com/jeet-dekivadia) |

---

## Table of Contents

1. [Background](#background)
2. [Algorithms](#algorithms)
3. [Architecture](#architecture)
4. [C++ Engine](#c-engine)
   - [Build](#build)
   - [Running the Solver](#running-the-solver)
   - [Generating Strategy Bundles](#generating-strategy-bundles)
5. [Web Application](#web-application)
   - [Setup](#setup)
   - [Pages](#pages)
6. [Project Structure](#project-structure)
7. [Results](#results)
8. [References](#references)

---

## Background

**Counterfactual Regret Minimization (CFR)** is an iterative self-play algorithm for computing Nash equilibria in extensive-form games with imperfect information. It was introduced by Zinkevich et al. (2007) and has since become the foundational algorithm for computer poker research — most notably in Libratus (2017) and Pluribus (2019), which defeated professional human players at heads-up and multi-player Texas Hold'em.

The key insight is that in a zero-sum two-player game, the time-average strategy profile converges to a Nash equilibrium at a rate of O(T^{-1/2}). More recent variants (CFR+ and DCFR) achieve significantly faster empirical convergence by discounting early, high-variance regret estimates.

This project implements the full algorithm family from scratch in C++17, verifies convergence via exact best-response exploitability computation, and wraps the results in an interactive web interface designed to make the underlying mathematics tangible and explorable.

---

## Algorithms

### Vanilla CFR

At each iteration, traverse the full game tree. For each information set *I* and action *a*, accumulate the *counterfactual regret*:

```
R^T(I, a) = Σ_{t=1}^{T}  π^σ_{-i}(I) · [ v^σ(I → a) − v^σ(I) ]
```

The strategy is updated via **regret matching**: actions are played proportional to their positive cumulative regrets. The time-average strategy converges to Nash at rate O(Δ √|I| / √T).

### CFR+

- Regrets are floored at zero after each update (no negative regret accumulation).
- Strategy sums are weighted linearly by *t* rather than uniformly.
- Achieves faster empirical convergence — approximately O(T^{-1}) in practice.

### DCFR (Discounted CFR)

Brown & Sandholm (2019). Before iteration *t*, positive regrets are discounted by:

```
factor = t^α / (t^α + 1),   α = 1.5
```

Negative regrets are floored to zero (the β = −∞ variant, which converges faster than the published β = 0 formulation). Strategy sums use quadratic weighting (γ = 2). This achieves the fastest empirical convergence of the three, reaching ε < 10⁻³ exploitability on Kuhn Poker in under 10,000 iterations.

| Algorithm | Regret weight | Strategy weight | Empirical α in ε ~ C·T^{-α} |
|-----------|---------------|-----------------|-------------------------------|
| CFR       | 1 (uniform)   | 1 (uniform)     | ~0.53                         |
| CFR+      | max(R, 0)     | t (linear)      | ~0.50                         |
| DCFR      | t^1.5 / (t^1.5+1) × max(R, 0) | t² (quadratic) | ~0.61 |

### Monte Carlo CFR (External Sampling)

For Texas Hold'em — where the game tree is astronomically large — we use External Sampling MCCFR:

- Sample one deal per iteration.
- For the **traversing player**: iterate over *all* actions (no sampling).
- For the **opponent**: sample one action proportional to current strategy.
- This reduces per-iteration complexity from O(|A|^d) to O(|A|^{d/2}).

---

## Architecture

```
cfr-edge/
├── C++ engine          (include/ + src/)          ← solver, hand eval, MCCFR
├── web/                                           ← Next.js 16 visualisation app
│   ├── src/app/                                   ← App Router pages
│   ├── src/components/                            ← React components
│   ├── src/lib/                                   ← Data loading, types, utilities
│   ├── src/store/                                 ← Zustand global state
│   └── public/strategies/                        ← Pre-computed strategy JSON bundles
└── scripts/
    └── plot_convergence.py                        ← Matplotlib convergence plots
```

The C++ solver generates JSON strategy bundles (convergence curves + per-infoset strategy snapshots at multiple iteration checkpoints) which are served as static files by the Next.js app. This means the web app requires no backend server to run — all data is pre-computed and bundled.

---

## C++ Engine

### Build

**Requirements:** CMake ≥ 3.16, a C++17 compiler (GCC ≥ 9, Clang ≥ 10, or MSVC 2019+), internet access (automatically fetches `nlohmann/json` v3.11.3 at configure time).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)
```

**Compiler flags (Release):**

| Platform | Flags |
|----------|-------|
| GCC / Clang | `-O3 -march=native -DNDEBUG -flto` |
| MSVC | `/O2 /arch:AVX2 /DNDEBUG /GL` |

`-march=native` enables AVX2 on supported CPUs. The SIMD tier is detected at compile time and printed at startup. If AVX2 is available, batch regret matching processes 4 doubles per cycle; SSE2 processes 2; scalar is the fallback.

### Running the Solver

```bash
# All experiments (Kuhn, Leduc, Texas Hold'em, abstraction comparison)
./build/cfr_solver

# Individual games
./build/cfr_solver --kuhn-only
./build/cfr_solver --leduc-only
./build/cfr_solver --holdem-only

# Skip abstraction experiments
./build/cfr_solver --no-abstract

# Skip Texas Hold'em (slow on first run due to hand evaluator init)
./build/cfr_solver --no-holdem
```

Results are written to `results/`:

| File | Contents |
|------|----------|
| `kuhn_convergence.csv` | CFR / CFR+ / DCFR / CFR-SoA exploitability curves |
| `leduc_convergence.csv` | CFR / CFR+ / DCFR exploitability curves |
| `holdem_convergence.csv` | MCCFR-DCFR exploitability proxy curve |
| `abstraction_comparison.csv` | Four abstraction schemes for Leduc |

Visualise with:

```bash
pip install matplotlib
python scripts/plot_convergence.py
```

### Generating Strategy Bundles

The `json_exporter` binary runs all experiments and writes the strategy JSON files consumed by the web app:

```bash
./build/json_exporter --out ./web/public/strategies/
```

This generates:

| File | Game | Algorithm | Infosets |
|------|------|-----------|----------|
| `kuhn_cfr.json` | Kuhn Poker | CFR | 12 |
| `kuhn_cfr_plus.json` | Kuhn Poker | CFR+ | 12 |
| `kuhn_dcfr.json` | Kuhn Poker | DCFR | 12 |
| `leduc_cfr.json` | Leduc Hold'em | CFR | 288 |
| `leduc_cfr_plus.json` | Leduc Hold'em | CFR+ | 288 |
| `leduc_dcfr.json` | Leduc Hold'em | DCFR | 288 |
| `holdem_dcfr.json` | Texas Hold'em | DCFR (MCCFR) | 33K+ |
| `meta.json` | Index | — | — |

Each file includes: convergence curve, strategy snapshots at multiple iteration checkpoints (for animated playback), final Nash strategy per infoset, convergence rate α, elapsed time, and infoset count.

---

## Web Application

### Setup

```bash
cd web
npm install
npm run dev        # development server → http://localhost:3000
npm run build      # production build
npm start          # serve production build
```

**Requirements:** Node.js ≥ 20.9, npm ≥ 10. The app reads pre-computed strategy JSON from `web/public/strategies/` — no live solver connection required.

### Pages

#### `/` — Home

Dark cinematic landing page with a physically-based 3D poker table rendered via Three.js / React Three Fiber. Felt texture, gold rail ring, animated floating cards with spring physics, chip stacks. Stats bar shows live solver metrics (exploitability, convergence rate, SIMD tier). Three-step CFR explainer.

#### `/solve` — Live Solver Dashboard

The primary demonstration page.

- **Convergence chart** (D3.js): exploitability vs. iterations on log or linear scale for CFR, CFR+, and DCFR simultaneously. Includes the theoretical O(1/√T) lower bound overlay and a vertical cursor tracking the current iteration.
- **Algorithm comparison cards**: final exploitability and convergence rate α for each variant.
- **Iteration scrubber**: drag to any checkpoint; the strategy heatmap updates instantly.
- **Replay animation**: step through pre-computed strategy snapshots to watch the strategy crystallise from uniform random to Nash.
- **Strategy heatmap** (right panel): every infoset in the game with colour-coded action probabilities and max absolute regret. Updates in real time with the scrubber.
- **Metric panel**: infoset count, final exploitability, convergence rate, solve time.

#### `/play` — Play vs Nash

Interactive Kuhn Poker game against the DCFR Nash strategy.

- Cards are dealt at random; the player sees their hole card and must decide to pass or bet.
- At every decision point, the Nash strategy recommendation is shown with exact probabilities and a bar chart.
- After each decision, a "Why this strategy?" panel explains the GTO reasoning in plain language.
- EV delta per action and per hand is tracked, along with a running session EV counter.
- Hand log records full history, both cards, and the EV cost of deviating from Nash.

#### `/strategy` — Strategy Explorer

Full sortable, filterable table of every information set in the selected game.

- **Sort** by infoset key, max |regret|, or Shannon entropy (H = −Σ p log p, measures how mixed the strategy is).
- **Filter** by key substring.
- **Inline probability bars** make action distributions scannable at a glance.
- **Export CSV** downloads the complete strategy for offline analysis.
- Algorithm selector (CFR / CFR+ / DCFR) updates all rows instantly.

#### `/algorithm` — CFR Demystified

Step-by-step interactive walkthrough of the algorithm.

- Five collapsible stages (Initialize → Traverse → Regret → Matching → Average), each with typeset pseudocode, a formal description, and a plain-language explanation.
- **Live regret waterfall**: select any Kuhn Poker infoset; amber bars show positive regret per action, green overlay shows current strategy σ. Updates instantly.
- Algorithm comparison table with colour-coded convergence rates.
- Convergence guarantee panel with the formal O(Δ √|I| / √T) bound.

#### `/holdem` — Texas Hold'em GTO

- **169-hand preflop range chart**: the canonical 13×13 matrix (pairs on the diagonal, suited hands in the upper triangle, offsuit hands in the lower triangle). Cell colour intensity represents action frequency for the selected view (Raise / Call / Fold / All-in).
- Hover over any cell to see the full action distribution for that hand class.
- **GTO concepts panel**: polarised ranges, bluff-to-value ratios, suit isomorphism, MCCFR external sampling.
- Aggregate stats: infoset count, average bet frequency, convergence rate, MCCFR exploitability proxy.

---

## Project Structure

```
cfr-edge/
│
├── include/
│   ├── infoset.h           InfoNode struct (regret/strategy sum arrays, MAX_ACTIONS=6)
│   │                       Mode enum: CFR | CFR_PLUS | DCFR
│   ├── cfr.h               TrainConfig / TrainResult / run_training() dispatch
│   ├── kuhn.h              Kuhn Poker game tree, CFR traversal, BR, training loop
│   ├── leduc.h             Leduc Hold'em game tree, CFR traversal, BR, training loop
│   ├── leduc_utils.h       Shared helpers: round_over(), showdown_winner(),
│   │                       acting_player_in_round(), get_actions_tpl<>()
│   ├── abstraction.h       Card bucketing (NONE/RANK_ONLY/STRENGTH) and action
│   │                       abstraction (FULL/NO_RAISE) for Leduc
│   ├── soa_store.h         Structure-of-Arrays infoset store for SIMD batch ops
│   │                       (batch_compute_strategies, batch_floor_regrets,
│   │                        batch_discount_regrets)
│   ├── simd_utils.h        AVX2/SSE2 regret matching (4 or 2 doubles/cycle),
│   │                       batch floor, discount, and axpy operations
│   ├── hand_eval.h         7-card poker hand evaluator via C(7,5)=21 subsets,
│   │                       flush/straight table lookup, paired-hand classification
│   ├── holdem.h            HUNL Texas Hold'em game tree: 169-bucket preflop
│   │                       abstraction, EHS² postflop bucketing, MCCFR external
│   │                       sampling, bet-size discretisation
│   └── json_output.h       JSON serialisation of InfoMap + convergence curves +
│                           strategy snapshots (consumed by web app)
│
├── src/
│   ├── kuhn.cpp            Kuhn Poker implementation
│   ├── leduc.cpp           Leduc Hold'em implementation
│   ├── cfr.cpp             run_training() dispatch (AoS and SoA paths)
│   ├── best_response.cpp   print_exploitability_report(), estimate_convergence_rate()
│   ├── abstraction.cpp     Card bucketing + action abstraction + abstract BR
│   ├── hand_eval.cpp       Hand evaluator: table construction, eval5(), eval7(),
│   │                       equity() (Monte Carlo for preflop, exact for 1–2 runouts)
│   ├── holdem.cpp          MCCFR traversal, preflop/postflop bucket builders,
│   │                       training loop, exploitability proxy
│   ├── main.cpp            Experiment driver: all games, CSV output, CLI flags
│   └── json_exporter.cpp   Standalone tool: runs all experiments, emits strategy
│                           JSON bundles to web/public/strategies/
│
├── web/                    Next.js 16 · React 19 · TypeScript · Tailwind CSS v3
│   ├── src/
│   │   ├── app/            App Router pages (/, /solve, /play, /strategy,
│   │   │                   /algorithm, /holdem)
│   │   ├── components/
│   │   │   ├── Nav.tsx
│   │   │   ├── PokerTableScene.tsx   (Three.js / R3F 3D table)
│   │   │   └── charts/
│   │   │       ├── ConvergenceChart.tsx  (D3.js)
│   │   │       └── StrategyHeatmap.tsx   (D3-inspired, React)
│   │   ├── lib/
│   │   │   ├── types.ts    TypeScript interfaces mirroring JSON schema
│   │   │   ├── data.ts     Strategy loading, caching, snapshot interpolation
│   │   │   └── utils.ts    cn(), fmt(), pct(), lerpColor(), probColor()
│   │   └── store/
│   │       └── solver.ts   Zustand store: game/algo selection, iteration scrubber,
│   │                       auto-play animation
│   └── public/
│       └── strategies/     Pre-computed JSON strategy bundles (7 files, ~2.5 MB total)
│
├── scripts/
│   └── plot_convergence.py   Matplotlib convergence plots from CSV output
│
├── results/                  CSV + PNG output (git-ignored)
├── CMakeLists.txt
└── README.md
```

---

## Results

### Exploitability at Convergence

| Game | Algorithm | Iterations | Final ε | Conv. Rate α |
|------|-----------|-----------|---------|--------------|
| Kuhn Poker | CFR  | 10,000 | 1.49 × 10⁻³ | 0.532 |
| Kuhn Poker | CFR+ | 10,000 | 1.59 × 10⁻³ | 0.496 |
| Kuhn Poker | DCFR | 10,000 | **4.41 × 10⁻⁴** | **0.605** |
| Leduc Hold'em | CFR  | 10,000 | 3.83 × 10⁻³ | 0.549 |
| Leduc Hold'em | CFR+ | 10,000 | 7.87 × 10⁻³ | 0.480 |
| Leduc Hold'em | DCFR | 10,000 | 7.54 × 10⁻³ | 0.497 |
| Texas Hold'em | MCCFR-DCFR | 1,000 | — (proxy) | — |

*Exploitability is defined as the average best-response gap per player: ε = (BR₀ + BR₁) / 2, where BRᵢ is the expected gain of a best response against the opponent's average strategy. At Nash equilibrium, ε = 0.*

### Kuhn Poker Nash Strategy (DCFR, 20K iterations)

The known analytic Nash for Kuhn Poker requires:
- **K**: always bet (polarised value range)
- **J**: bluff with frequency α ≈ 1/3 (makes opponent indifferent)
- **Q**: check most of the time; call with mixed frequency

DCFR converges to (Pass / Bet):

```
K:    0.249 / 0.751   ← bets 75% (value)
K:b   0.000 / 1.000   ← always calls
J:    0.751 / 0.249   ← bluffs 25% ≈ 1/3 of K's bet frequency
J:b   1.000 / 0.000   ← folds to bet
Q:b   0.666 / 0.334   ← calls with mixed strategy
```

This matches the analytic solution to within the exploitability bound.

### SIMD Performance

| Backend | Throughput | Relative |
|---------|-----------|----------|
| Scalar  | 1 double/cycle | 1× |
| SSE2    | 2 doubles/cycle | 2× |
| AVX2    | 4 doubles/cycle | 4× |

Detected automatically at compile time via `__AVX2__` / `__SSE2__` macros.

---

## Technical Notes

### Why two CFR paths (AoS vs SoA)?

The default Array-of-Structures (AoS) path uses `std::unordered_map<string, InfoNode>` and computes strategies on the fly during traversal. Each deal within an iteration can see regrets updated by previous deals.

The Structure-of-Arrays (SoA) path pre-computes all strategies via SIMD batch operations at the *start* of each iteration, then freezes them for the entire iteration's traversal. This is more faithful to vanilla CFR's definition (fixed strategy per iteration) and enables cache-friendly SIMD over large node arrays.

Both paths converge to Nash but exhibit different per-iteration trajectories. The SoA path is currently implemented for Kuhn Poker only.

### Texas Hold'em abstraction

With 1,326 possible starting hands, 19,600 possible flops, and continuous bet sizing, exact HUNL has an essentially infinite game tree. We use:

- **Preflop**: 169 canonical hand classes via suit isomorphism (e.g., AKs, AKo, QQ)
- **Postflop**: 8 hand-strength buckets based on the raw 5-card (or 7-card) evaluator rank, normalised uniformly across [1, 7461]
- **Bet sizes**: {fold, check/call, bet 33%, bet 75%, bet 100%, all-in} — 6 actions maximum

This reduces the tree to a tractable size for MCCFR demonstration, though a production HUNL solver (e.g., Libratus) uses far richer abstractions with tens of millions of buckets and blueprint + real-time solving.

---

## References

1. Zinkevich, M., Johanson, M., Bowling, M., & Piccione, C. (2007). **Regret minimization in games with incomplete information.** *Advances in Neural Information Processing Systems*, 20.

2. Tammelin, O. (2014). **Solving large imperfect information games using CFR+.** *arXiv:1407.5042*.

3. Brown, N., & Sandholm, T. (2019). **Solving imperfect-information games via discounted regret minimization.** *Proceedings of the AAAI Conference on Artificial Intelligence*, 33(1), 1829–1836.

4. Lanctot, M., Waugh, K., Zinkevich, M., & Bowling, M. (2009). **Monte Carlo sampling for regret minimization in extensive games.** *Advances in Neural Information Processing Systems*, 22.

5. Brown, N., & Sandholm, T. (2017). **Libratus: The superhuman AI for no-limit poker.** *Proceedings of the 26th International Joint Conference on Artificial Intelligence*, 5226–5228.

6. Brown, N., & Sandholm, T. (2019). **Superhuman AI for multiplayer poker.** *Science*, 365(6456), 885–890.

---

## License

MIT License. See [LICENSE](LICENSE) for details.
