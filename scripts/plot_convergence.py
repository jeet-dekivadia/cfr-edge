#!/usr/bin/env python3
"""
Plot exploitability convergence curves from CSV outputs.
Usage: python scripts/plot_convergence.py
"""

import os
import sys
import csv
from collections import defaultdict

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
    HAS_MPL = True
except ImportError:
    HAS_MPL = False
    print("matplotlib not found, will output text summary only")


def read_csv(path):
    """Read convergence CSV, return dict of label -> [(iter, expl)]"""
    curves = defaultdict(list)
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            label = row.get('variant') or row.get('label') or row.get('scheme', 'unknown')
            it = int(row['iteration'])
            ex = float(row['exploitability'])
            curves[label].append((it, ex))
    for k in curves:
        curves[k].sort()
    return curves


def plot_curves(curves, title, outpath):
    if not HAS_MPL:
        return
    fig, ax = plt.subplots(figsize=(8, 5))
    for label, data in sorted(curves.items()):
        iters = [d[0] for d in data]
        expls = [d[1] for d in data]
        ax.plot(iters, expls, label=label, linewidth=1.5)

    ax.set_xlabel('Iteration')
    ax.set_ylabel('Exploitability')
    ax.set_title(title)
    ax.set_yscale('log')
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(outpath, dpi=150)
    plt.close(fig)
    print(f"  saved {outpath}")


def text_summary(curves, title):
    print(f"\n--- {title} ---")
    for label, data in sorted(curves.items()):
        if data:
            last = data[-1]
            print(f"  {label:25s} final_expl={last[1]:.6e}  (iter {last[0]})")


def main():
    results_dir = os.path.join(os.path.dirname(__file__), '..', 'results')
    plots_dir = results_dir

    files = {
        'kuhn_convergence.csv': ('Kuhn Poker: CFR Convergence', 'kuhn_convergence.png'),
        'leduc_convergence.csv': ('Leduc Hold\'em: CFR Convergence', 'leduc_convergence.png'),
        'abstraction_comparison.csv': ('Leduc Abstraction Comparison', 'abstraction_comparison.png'),
    }

    found_any = False
    for csv_name, (title, png_name) in files.items():
        csv_path = os.path.join(results_dir, csv_name)
        if not os.path.exists(csv_path):
            print(f"  skipping {csv_name} (not found)")
            continue
        found_any = True
        curves = read_csv(csv_path)
        text_summary(curves, title)
        plot_curves(curves, title, os.path.join(plots_dir, png_name))

    if not found_any:
        print("No CSV files found in results/. Run the solver first.")
        sys.exit(1)


if __name__ == '__main__':
    main()
