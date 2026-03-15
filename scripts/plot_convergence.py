#!/usr/bin/env python3
"""
Plot exploitability convergence curves from CSV outputs.
Usage: python scripts/plot_convergence.py

Produces per-game PNG plots (log-scale Y axis) in results/.
Also prints a text summary table.
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
    print("matplotlib not found — text summary only. pip install matplotlib to enable plots.")


COLORS = {
    'CFR':              '#4e79a7',
    'CFR+':             '#f28e2b',
    'DCFR':             '#e15759',
    'CFR_SoA':          '#76b7b2',
    'full':             '#4e79a7',
    'strength_buckets': '#f28e2b',
    'no_raise':         '#e15759',
    'strength+no_raise':'#76b7b2',
}

LINESTYLES = {
    'CFR':              '-',
    'CFR+':             '--',
    'DCFR':             '-.',
    'CFR_SoA':          ':',
}


def read_csv(path):
    """Return dict of label -> [(iter, expl)] sorted by iter."""
    curves = defaultdict(list)
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            label = (row.get('variant') or row.get('label')
                     or row.get('scheme') or 'unknown')
            curves[label].append((int(row['iteration']), float(row['exploitability'])))
    for k in curves:
        curves[k].sort()
    return curves


def plot_curves(curves, title, outpath, nash_ref=None):
    if not HAS_MPL:
        return
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))

    for ax, yscale in zip(axes, ('linear', 'log')):
        for label, data in sorted(curves.items()):
            iters = [d[0] for d in data]
            expls = [d[1] for d in data]
            color = COLORS.get(label, None)
            ls    = LINESTYLES.get(label, '-')
            ax.plot(iters, expls, label=label, linewidth=2,
                    color=color, linestyle=ls)

        if nash_ref is not None:
            ax.axhline(nash_ref, color='gray', linestyle=':', linewidth=1,
                       label=f'Nash ({nash_ref:.4f})')

        ax.set_xlabel('Iteration', fontsize=11)
        ax.set_ylabel('Exploitability', fontsize=11)
        ax.set_title(f'{title} — {"linear" if yscale == "linear" else "log"} scale',
                     fontsize=12)
        ax.set_yscale(yscale)
        ax.legend(fontsize=9)
        ax.grid(True, alpha=0.3, which='both')

    fig.tight_layout()
    fig.savefig(outpath, dpi=150)
    plt.close(fig)
    print(f"  saved {outpath}")


def text_summary(curves, title):
    print(f"\n{'─'*60}")
    print(f"  {title}")
    print(f"{'─'*60}")
    header = f"  {'Algorithm':<22} {'Final expl':>14}  {'Iters':>8}"
    print(header)
    print(f"  {'-'*54}")
    for label, data in sorted(curves.items()):
        if not data:
            continue
        last = data[-1]
        # find iteration where exploitability first dropped below 0.01
        milestone = next(((it, ex) for it, ex in data if ex < 0.01), None)
        ms_str = f"  (<0.01 @ iter {milestone[0]})" if milestone else ""
        print(f"  {label:<22} {last[1]:>14.6e}  {last[0]:>8}{ms_str}")
    print()


def main():
    results_dir = os.path.join(os.path.dirname(__file__), '..', 'results')

    files = {
        'kuhn_convergence.csv': (
            'Kuhn Poker — CFR/CFR+/DCFR Convergence',
            'kuhn_convergence.png',
            0.0,   # Nash exploitability = 0
        ),
        'leduc_convergence.csv': (
            "Leduc Hold'em — CFR/CFR+/DCFR Convergence",
            'leduc_convergence.png',
            None,
        ),
        'abstraction_comparison.csv': (
            'Leduc — Abstraction Scheme Comparison',
            'abstraction_comparison.png',
            None,
        ),
    }

    found_any = False
    for csv_name, (title, png_name, nash_ref) in files.items():
        csv_path = os.path.join(results_dir, csv_name)
        if not os.path.exists(csv_path):
            print(f"  skipping {csv_name} (not found — run the solver first)")
            continue
        found_any = True
        curves = read_csv(csv_path)
        text_summary(curves, title)
        plot_curves(curves, title, os.path.join(results_dir, png_name), nash_ref=nash_ref)

    if not found_any:
        print("No result CSVs found in results/. Build and run cfr_solver first.")
        sys.exit(1)


if __name__ == '__main__':
    main()
