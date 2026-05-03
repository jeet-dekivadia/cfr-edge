# CFR-Edge

A production-grade Counterfactual Regret Minimization engine for poker, with a full-stack interactive visualisation platform.

CFR-Edge implements three generations of the CFR algorithm family: Vanilla CFR, CFR+, and Discounted CFR (DCFR). The solver is written in AVX2-accelerated C++17, and the results are exposed through a Next.js web application that lets users watch strategies converge to Nash equilibrium in real time, play against the solved strategy, and explore every information set in depth.

Games supported: Kuhn Poker, Leduc Hold'em, and Heads-Up No-Limit Texas Hold'em using Monte Carlo CFR with external sampling.

## Table of Contents

1. Background
2. Algorithms
3. Architecture
4. C++ Engine
5. Web Application
6. Project Structure
7. Results
8. References

## Background

Counterfactual Regret Minimization (CFR) is an iterative self-play algorithm for computing Nash equilibria in extensive-form games with imperfect information. It was introduced by Zinkevich et al. (2007) and has since become the foundational algorithm for computer poker research, most notably in Libratus (2017) and Pluribus (2019), which defeated professional human players at heads-up and multi-player Texas Hold'em.

The key insight is that in a zero-sum two-player game, the time-average strategy profile converges to a Nash equilibrium at a rate of O(T^{-1/2}). More recent variants, such as CFR+ and DCFR, achieve significantly faster empirical convergence by discounting early, high-variance regret estimates.

This project implements the full algorithm family from scratch in C++17, verifies convergence via exact best-response exploitability computation, and wraps the results in an interactive web interface designed to make the underlying mathematics tangible and explorable.

## Algorithms

### Vanilla CFR

At each iteration, the solver traverses the full game tree. For each information set I and action a, it accumulates counterfactual regret as the difference between the value of taking action a and the value of following the current strategy, weighted by the opponent reach probability.

The strategy is updated via regret matching, where actions are played proportional to their positive cumulative regrets. The time-average strategy converges to Nash at rate O(Δ sqrt(|I|) / sqrt(T)).

### CFR+

CFR+ floors regrets at zero after each update, weights strategy sums linearly by iteration number, and converges faster in practice than vanilla CFR.

### DCFR

Brown and Sandholm (2019) discount positive regrets before each iteration using a factor of t^α / (t^α + 1), with α = 1.5. Negative regrets are floored to zero in this implementation, and strategy sums use quadratic weighting. In this project, DCFR delivers the strongest empirical convergence on Kuhn Poker.

| Algorithm | Regret weight | Strategy weight | Empirical α in ε ~ C·T^{-α} |
|-----------|---------------|-----------------|-------------------------------|
| CFR | 1 (uniform) | 1 (uniform) | ~0.53 |
| CFR+ | max(R, 0) | t (linear) | ~0.50 |
| DCFR | t^1.5 / (t^1.5 + 1) x max(R, 0) | t² (quadratic) | ~0.61 |

### Monte Carlo CFR

For Texas Hold'em, where the game tree is extremely large, the project uses External Sampling MCCFR.

- One deal is sampled per iteration.
- The traversing player evaluates all actions.
- The opponent samples one action proportional to the current strategy.
- This reduces per-iteration complexity substantially compared with full-tree traversal.

## Architecture

The repository is split into a C++ engine, a web application, and supporting scripts.

- C++ engine in include/ and src/ for solver logic, hand evaluation, and MCCFR.
- Next.js application in web/ for visualisation, exploration, and gameplay.
- Scripts in scripts/ for convergence plotting.
- Generated strategy bundles in web/public/strategies/ for static consumption by the app.

The C++ solver generates JSON strategy bundles with convergence curves and per-infoset strategy snapshots at multiple iteration checkpoints. The web app serves them as static assets, so it does not require a backend server.

## C++ Engine

The engine includes implementations for Kuhn Poker, Leduc Hold'em, Texas Hold'em abstractions, best-response evaluation, hand evaluation, and JSON export. It also includes SIMD helpers and a structure-of-arrays store for faster batch processing.

Compiler support is centered on C++17, with AVX2 and SSE2 paths when available. The build is designed to fetch nlohmann/json automatically at configure time.

## Web Application

The web app is a Next.js 16 and React 19 interface with Tailwind CSS styling. It consumes the pre-computed strategy bundles from web/public/strategies/ and presents several views:

- Home: a cinematic landing page with a 3D poker table scene.
- Live Solver Dashboard: convergence charts, iteration scrubbing, strategy heatmaps, and replay playback.
- Play vs Nash: an interactive Kuhn Poker game against the solved strategy.
- Strategy Explorer: a sortable and filterable table of information sets.
- CFR Demystified: an interactive walkthrough of the algorithm.
- Texas Hold'em GTO: a preflop range explorer with abstraction and strategy summaries.

## Project Structure

| Path | Purpose |
|------|---------|
| include/ | Public headers for CFR, game trees, abstractions, SIMD utilities, and JSON output |
| src/ | Solver implementations, experiment driver, best response, and exporter binaries |
| web/ | Next.js application, components, state, and static strategy assets |
| scripts/ | Analysis and plotting scripts |
| results/ | CSV and plot outputs generated by experiments |

## Results

### Exploitability at Convergence

| Game | Algorithm | Iterations | Final ε | Conv. Rate α |
|------|-----------|-----------|---------|--------------|
| Kuhn Poker | CFR | 10,000 | 1.49 × 10⁻³ | 0.532 |
| Kuhn Poker | CFR+ | 10,000 | 1.59 × 10⁻³ | 0.496 |
| Kuhn Poker | DCFR | 10,000 | 4.41 × 10⁻⁴ | 0.605 |
| Leduc Hold'em | CFR | 10,000 | 3.83 × 10⁻³ | 0.549 |
| Leduc Hold'em | CFR+ | 10,000 | 7.87 × 10⁻³ | 0.480 |
| Leduc Hold'em | DCFR | 10,000 | 7.54 × 10⁻³ | 0.497 |
| Texas Hold'em | MCCFR-DCFR | 1,000 | proxy | proxy |

Exploitability is defined as the average best-response gap per player: ε = (BR₀ + BR₁) / 2, where BRᵢ is the expected gain of a best response against the opponent's average strategy. At Nash equilibrium, ε = 0.

### Kuhn Poker Nash Strategy

The known analytic Nash for Kuhn Poker requires K to always bet, J to bluff with frequency near one third of K's betting frequency, and Q to check most of the time while mixing on calls. The learned DCFR strategy converges closely to the analytic solution.

### SIMD Performance

| Backend | Throughput | Relative |
|---------|------------|----------|
| Scalar | 1 double/cycle | 1x |
| SSE2 | 2 doubles/cycle | 2x |
| AVX2 | 4 doubles/cycle | 4x |

## Technical Notes

The project keeps two CFR paths. The default Array-of-Structures path uses std::unordered_map<string, InfoNode> and computes strategies during traversal, while the Structure-of-Arrays path pre-computes strategies at the start of each iteration and freezes them for the remainder of that iteration. The SoA path is currently implemented for Kuhn Poker only.

The Texas Hold'em abstraction uses 169 canonical starting-hand classes, eight postflop hand-strength buckets, and a six-action cap for tractability. This keeps the demonstration manageable while still showing how abstraction and external sampling interact.

## References

1. Zinkevich, M., Johanson, M., Bowling, M., and Piccione, C. (2007). Regret minimization in games with incomplete information. Advances in Neural Information Processing Systems, 20.
2. Tammelin, O. (2014). Solving large imperfect information games using CFR+. arXiv:1407.5042.
3. Brown, N., and Sandholm, T. (2019). Solving imperfect-information games via discounted regret minimization. Proceedings of the AAAI Conference on Artificial Intelligence, 33(1), 1829-1836.
4. Lanctot, M., Waugh, K., Zinkevich, M., and Bowling, M. (2009). Monte Carlo sampling for regret minimization in extensive games. Advances in Neural Information Processing Systems, 22.
