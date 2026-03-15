# cfr-edge
Poker CFR / CFR+ Solver in C++

One-liner: CFR/CFR+ poker solver with exploitability tracking, abstractions, and SIMD-optimized traversals.

"cfr" is Counterfactual Regret Minimization, and "edge" hints at finding a strategic edge via exploitability-driven optimization.

## Features

- Vanilla CFR and CFR+ for **Kuhn Poker** and **Leduc Hold'em**
- Exploitability (best-response gap) computed every N iterations
- SoA (Structure-of-Arrays) infoset store with AVX2 SIMD regret matching
- Card-strength bucketing and action abstraction experiments for Leduc
- CSV output of convergence curves + Python plotting script

## Build

Requires CMake ≥ 3.16 and a C++17 compiler (MSVC, GCC, or Clang).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

OpenMP is detected automatically; AVX2 is enabled on MSVC via `/arch:AVX2` and on GCC/Clang via `-march=native`.

## Run

```bash
# All experiments (Kuhn, Leduc, abstraction comparison)
./build/cfr_solver

# Kuhn only
./build/cfr_solver --kuhn-only

# Leduc only
./build/cfr_solver --leduc-only

# Skip abstraction experiments
./build/cfr_solver --no-abstract
```

Results are written to `results/`:
| File | Contents |
|------|----------|
| `kuhn_convergence.csv` | CFR / CFR+ / CFR-SoA exploitability curves |
| `leduc_convergence.csv` | CFR / CFR+ exploitability curves |
| `abstraction_comparison.csv` | Four abstraction schemes vs. exploitability |

## Plot

```bash
pip install matplotlib
python scripts/plot_convergence.py
```

Saves `results/*.png` and prints a text summary.

## Project Structure

```
include/
  kuhn.h          – Kuhn Poker game tree + CFR traversal
  leduc.h         – Leduc Hold'em game tree + CFR traversal
  cfr.h           – TrainConfig / TrainResult / run_training()
  infoset.h       – InfoNode (regret + strategy sum)
  soa_store.h     – SoA infoset store
  simd_utils.h    – AVX2 regret-matching helpers
  abstraction.h   – Card bucketing + action abstraction
src/
  kuhn.cpp
  leduc.cpp
  cfr.cpp
  best_response.cpp
  abstraction.cpp
  main.cpp        – Experiment driver + CSV output
scripts/
  plot_convergence.py
results/          – CSV / PNG output (git-ignored)
```
