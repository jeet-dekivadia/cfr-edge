# CFR-Edge

CFR-Edge is a poker solver and visualization project built around Counterfactual Regret Minimization. It started as a way to make poker theory feel less abstract and ended up becoming a full stack system for training strategies, checking convergence, and exploring the output in a browser.

The core engine is written in C++17. The interface is a Next.js app that turns solver output into something you can actually inspect: convergence curves, strategy heatmaps, poker tables, and a play-against-the-solution demo.

## What’s here

- A C++ solver for Kuhn Poker, Leduc Hold’em, and abstracted heads-up no-limit Texas Hold’em.
- CFR, CFR+, and DCFR implementations, plus MCCFR with external sampling for the larger game.
- A Next.js dashboard that loads precomputed strategy bundles from static JSON files.
- A small set of visual tools for comparing algorithms, browsing infosets, and replaying how a strategy settles over time.

## The short version

The project is split into two parts that work together but stay independent:

- The C++ side does the heavy lifting: tree traversal, regret updates, exploitability checks, and JSON export.
- The web side makes the result easier to read: charts, tables, heatmaps, an interactive poker table, and a simple play mode against a solved strategy.

That separation matters. It keeps the solver honest and keeps the UI lightweight.

## Games

The current build covers three poker settings:

- Kuhn Poker, which is small enough to fully traverse and use as a sanity check.
- Leduc Hold’em, which adds a public card and a much larger information set space.
- Heads-up no-limit Texas Hold’em, handled with abstraction and Monte Carlo CFR so the state space stays tractable.

## Algorithms

The engine includes three closely related CFR variants:

- Vanilla CFR, the baseline form.
- CFR+, which clips negative regret and usually converges faster in practice.
- DCFR, which discounts early regret and is the variant this project leans on most heavily.

For the larger Hold’em setup, the solver switches to external-sampling MCCFR so the tree can be explored without traversing every branch every time.

## Web experience

The browser app is meant to feel more like an explorable notebook than a dashboard full of metrics.

- `/` is the home page and reads like a project note: why the solver exists and what changed while building it.
- `/solve` shows convergence, algorithm comparisons, and strategy snapshots.
- `/play` lets you play Kuhn Poker against the solved Nash strategy.
- `/strategy` is a table view of infosets and action probabilities.
- `/algorithm` walks through CFR step by step.
- `/holdem` shows the abstracted Hold’em range view.

## Data flow

Solver runs produce JSON strategy bundles in `web/public/strategies/`. The app reads those files directly, so the UI does not depend on a live backend. That keeps the whole thing easy to move around, easy to deploy, and easy to inspect later.

## Project layout

- `include/` and `src/` contain the C++ engine.
- `web/` contains the Next.js app and its visual components.
- `scripts/` contains plotting helpers for convergence data.
- `results/` stores generated CSV output.

## A few notes

- This project is meant to be educational and inspectable, not a claim that the solver is state of the art.
- The numbers and charts in the repo are snapshots from the current implementation.
- The best part of the project is probably the visual layer, because it makes it much easier to see when the solver is doing something sensible and when it is not.

## Authors

- Himansh Chitkara
- Jeet Dekivadia
