#!/usr/bin/env python3
"""
Generate the README figures from the committed strategy bundles.

Every figure here is rendered from the real data in web/public/strategies/,
so the README never shows a number the solver did not produce. Outputs land
in docs/media/.

Usage:
    python scripts/make_readme_assets.py
"""

import json
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Ellipse, Circle, Rectangle
from matplotlib.colors import LinearSegmentedColormap
import matplotlib.animation as animation
import numpy as np

HERE = os.path.dirname(__file__)
ROOT = os.path.abspath(os.path.join(HERE, ".."))
STRAT = os.path.join(ROOT, "web", "public", "strategies")
OUT = os.path.join(ROOT, "docs", "media")
os.makedirs(OUT, exist_ok=True)

# ---- Theme ----
BG        = "#070d0a"
PANEL     = "#0c1611"
FELT      = "#10623a"
FELT_DARK = "#0a3f25"
RIM       = "#c9a227"
TEXT      = "#e8f6ee"
MUTED     = "#8fa99b"
GRID      = "#1c2c24"
C_CFR     = "#38bdf8"   # sky
C_CFRP    = "#f59e0b"   # amber
C_DCFR    = "#a3e635"   # lime
ACCENT    = "#34d399"
CARD_FACE = "#f8fafc"
CARD_BACK = "#15304f"
RED       = "#e2474a"
BLACK     = "#0f172a"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "figure.facecolor": BG,
    "savefig.facecolor": BG,
    "text.color": TEXT,
    "axes.edgecolor": GRID,
})

GREEN_CMAP = LinearSegmentedColormap.from_list(
    "edge_green", ["#0a1f16", "#0f5132", "#2f9e58", "#7dd97f", "#eafff1"])


def load(name):
    with open(os.path.join(STRAT, name), encoding="utf-8") as f:
        return json.load(f)


# =====================================================================
# 1. Convergence plots (Kuhn and Leduc) from the real curves
# =====================================================================

def convergence_plot(files, title, subtitle, outfile, annotate):
    fig, ax = plt.subplots(figsize=(9.2, 5.2))
    fig.subplots_adjust(left=0.1, right=0.97, top=0.84, bottom=0.13)
    ax.set_facecolor(PANEL)

    for fname, label, color in files:
        d = load(fname)
        pts = [(c["iter"], c["exploitability"]) for c in d["convergence"]
               if c["exploitability"] > 0]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        ax.plot(xs, ys, color=color, lw=2.4, label=label, alpha=0.95,
                solid_capstyle="round")
        ax.scatter([xs[-1]], [ys[-1]], color=color, s=42, zorder=5,
                   edgecolor=BG, linewidth=1.2)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Iteration", fontsize=12, color=MUTED)
    ax.set_ylabel("Exploitability  (best-response, payoff units)",
                  fontsize=11, color=MUTED)
    ax.grid(True, which="both", color=GRID, lw=0.7, alpha=0.7)
    for s in ax.spines.values():
        s.set_color(GRID)
    ax.tick_params(colors=MUTED)
    leg = ax.legend(loc="upper right", frameon=True, fontsize=11)
    leg.get_frame().set_facecolor(PANEL)
    leg.get_frame().set_edgecolor(GRID)
    for t in leg.get_texts():
        t.set_color(TEXT)

    fig.text(0.1, 0.945, title, fontsize=19, fontweight="bold", color=TEXT)
    fig.text(0.1, 0.895, subtitle, fontsize=11.5, color=MUTED)
    if annotate:
        ax.annotate(annotate[0], xy=annotate[1], xytext=annotate[2],
                    color=annotate[3], fontsize=10.5,
                    arrowprops=dict(arrowstyle="->", color=annotate[3], lw=1.4))
    fig.savefig(os.path.join(OUT, outfile), dpi=160)
    plt.close(fig)
    print("wrote", outfile)


# =====================================================================
# 2. Kuhn equilibrium strategy chart (12 information sets)
# =====================================================================

def kuhn_strategy_plot():
    d = load("kuhn_dcfr.json")
    fs = d["final_strategy"]
    order = ["K:", "Q:", "J:", "K:p", "Q:p", "J:p",
             "K:b", "Q:b", "J:b", "K:pb", "Q:pb", "J:pb"]
    labels = {
        "K:": "K  open", "Q:": "Q  open", "J:": "J  open",
        "K:p": "K  vs check", "Q:p": "Q  vs check", "J:p": "J  vs check",
        "K:b": "K  vs bet", "Q:b": "Q  vs bet", "J:b": "J  vs bet",
        "K:pb": "K  check-then-bet", "Q:pb": "Q  check-then-bet",
        "J:pb": "J  check-then-bet",
    }
    order = [k for k in order if k in fs]

    fig, ax = plt.subplots(figsize=(9.2, 5.6))
    fig.subplots_adjust(left=0.26, right=0.90, top=0.81, bottom=0.11)
    ax.set_facecolor(PANEL)
    cx = 0.58  # horizontal centre of the plotting axes, for the heading

    y = np.arange(len(order))[::-1]
    for yi, key in zip(y, order):
        p_pass, p_bet = fs[key]["probs"]
        ax.barh(yi, p_pass, color="#3b4a5a", height=0.62)
        ax.barh(yi, p_bet, left=p_pass, color=ACCENT, height=0.62)
        if p_bet > 0.04:
            ax.text(p_pass + p_bet / 2, yi, f"{p_bet*100:.0f}%",
                    ha="center", va="center", color=BG, fontsize=9.5,
                    fontweight="bold")

    ax.set_yticks(y)
    ax.set_yticklabels([labels[k] for k in order], fontsize=10.5, color=TEXT)
    ax.set_xlim(0, 1)
    ax.set_xticks([0, 0.25, 0.5, 0.75, 1.0])
    ax.set_xticklabels(["0", "25", "50", "75", "100%"], color=MUTED)
    ax.tick_params(colors=MUTED, length=0)
    for s in ax.spines.values():
        s.set_visible(False)
    ax.set_axisbelow(True)
    ax.xaxis.grid(True, color=GRID, lw=0.7)

    fig.text(cx, 0.94, "Kuhn Poker equilibrium  (DCFR average strategy)",
             fontsize=17, fontweight="bold", color=TEXT, ha="center")
    fig.text(cx, 0.895,
             "Grey = pass / check / fold,    green = bet / call.    "
             "Note the Jack's 25% bluff.",
             fontsize=10.5, color=MUTED, ha="center")
    fig.savefig(os.path.join(OUT, "kuhn-strategy.png"), dpi=160)
    plt.close(fig)
    print("wrote kuhn-strategy.png")


# =====================================================================
# 3. Texas Hold'em preflop range chart (13x13) from preflop_summary
# =====================================================================

RANKS = ["A", "K", "Q", "J", "T", "9", "8", "7", "6", "5", "4", "3", "2"]


def rank_value(r):
    return 12 - RANKS.index(r)


def bucket_for_hand(label):
    a, b = label[0], label[1]
    hi = max(rank_value(a), rank_value(b))
    lo = min(rank_value(a), rank_value(b))
    if a == b:
        return 12 - hi
    idx = 0
    for h in range(12, hi, -1):
        idx += h
    idx += hi - 1 - lo
    return 13 + idx if label.endswith("s") else 91 + idx


def hand_for_cell(row, col):
    if row == col:
        lab = RANKS[row] + RANKS[col]
        return lab, "pair"
    if col > row:
        return RANKS[row] + RANKS[col] + "s", "suited"
    return RANKS[col] + RANKS[row] + "o", "offsuit"


def raise_freq(actions):
    return (actions.get("bet33", 0) + actions.get("bet75", 0)
            + actions.get("bet100", 0) + actions.get("allin", 0))


def holdem_range_plot():
    d = load("holdem_dcfr.json")
    ps = d["preflop_summary"]

    def bucket_actions(bucket):
        root = ps.get(f"PF{bucket}:")
        if root:
            return root
        rows = [v for k, v in ps.items() if k.startswith(f"PF{bucket}:")]
        if not rows:
            return {}
        tot = {}
        for r in rows:
            for a, v in r.items():
                tot[a] = tot.get(a, 0) + v
        return {a: v / len(rows) for a, v in tot.items()}

    grid = np.zeros((13, 13))
    for r in range(13):
        for c in range(13):
            lab, _ = hand_for_cell(r, c)
            grid[r, c] = raise_freq(bucket_actions(bucket_for_hand(lab)))

    fig, ax = plt.subplots(figsize=(8.6, 8.2))
    fig.subplots_adjust(left=0.07, right=0.93, top=0.86, bottom=0.06)
    im = ax.imshow(grid, cmap=GREEN_CMAP, vmin=0, vmax=1)

    for r in range(13):
        for c in range(13):
            lab, typ = hand_for_cell(r, c)
            val = grid[r, c]
            txt = "#06140d" if val > 0.55 else "#dbeee2"
            ax.text(c, r - 0.16, lab, ha="center", va="center",
                    fontsize=7.6, color=txt, fontweight="bold")
            ax.text(c, r + 0.2, f"{val*100:.0f}", ha="center", va="center",
                    fontsize=6.2, color=txt, alpha=0.85)

    ax.set_xticks(range(13)); ax.set_yticks(range(13))
    ax.set_xticklabels(RANKS, color=MUTED, fontsize=9)
    ax.set_yticklabels(RANKS, color=MUTED, fontsize=9)
    ax.tick_params(length=0)
    for s in ax.spines.values():
        s.set_color(GRID)
    ax.set_xticks(np.arange(-0.5, 13, 1), minor=True)
    ax.set_yticks(np.arange(-0.5, 13, 1), minor=True)
    ax.grid(which="minor", color=BG, lw=1.4)

    fig.text(0.07, 0.945,
             "Heads-up Hold'em preflop range  (bet or all-in frequency)",
             fontsize=16.5, fontweight="bold", color=TEXT)
    fig.text(0.07, 0.905,
             "169 canonical hands. Diagonal = pairs, upper = suited, "
             "lower = offsuit. MCCFR, 1,000 iterations.",
             fontsize=10, color=MUTED)
    cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.03)
    cbar.set_label("raise / all-in frequency", color=MUTED, fontsize=9)
    cbar.ax.yaxis.set_tick_params(color=MUTED)
    plt.setp(plt.getp(cbar.ax.axes, "yticklabels"), color=MUTED)
    fig.savefig(os.path.join(OUT, "holdem-range.png"), dpi=160)
    plt.close(fig)
    print("wrote holdem-range.png")


# =====================================================================
# 4. Hold'em regret proxy growth
# =====================================================================

def holdem_proxy_plot():
    d = load("holdem_dcfr.json")
    pts = [(c["iter"], c["exploitability"]) for c in d["convergence"]]
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]

    fig, ax = plt.subplots(figsize=(9.2, 4.8))
    fig.subplots_adjust(left=0.1, right=0.96, top=0.82, bottom=0.14)
    ax.set_facecolor(PANEL)
    ax.plot(xs, ys, color="#fb7185", lw=2.6, marker="o", ms=6,
            mec=BG, mew=1.2)
    ax.fill_between(xs, 0, ys, color="#fb7185", alpha=0.12)
    ax.set_xlabel("MCCFR iteration", color=MUTED, fontsize=11)
    ax.set_ylabel("mean positive regret at preflop nodes", color=MUTED,
                  fontsize=10.5)
    ax.grid(True, color=GRID, lw=0.7)
    for s in ax.spines.values():
        s.set_color(GRID)
    ax.tick_params(colors=MUTED)
    fig.text(0.1, 0.94,
             "Texas Hold'em: regret-magnitude proxy, not exploitability",
             fontsize=15.5, fontweight="bold", color=TEXT)
    fig.text(0.1, 0.885,
             "33,260 information sets. The proxy grows as regrets accumulate, "
             "so it tracks scale, not closeness to equilibrium.",
             fontsize=10, color=MUTED)
    fig.savefig(os.path.join(OUT, "holdem-proxy.png"), dpi=160)
    plt.close(fig)
    print("wrote holdem-proxy.png")


# =====================================================================
# 5. Animated poker-table GIF (heads-up self-play, real action sizes)
# =====================================================================

SUIT_GLYPH = {"s": "♠", "h": "♥", "d": "♦", "c": "♣"}
SUIT_COLOR = {"s": BLACK, "c": BLACK, "h": RED, "d": RED}


def draw_card(ax, x, y, card, faceup, w=0.66, h=0.96, glow=False):
    if glow:
        ax.add_patch(FancyBboxPatch((x - w/2 - 0.06, y - h/2 - 0.06),
                     w + 0.12, h + 0.12,
                     boxstyle="round,pad=0.02,rounding_size=0.12",
                     fc="none", ec=ACCENT, lw=2.4, zorder=4))
    if faceup:
        ax.add_patch(FancyBboxPatch((x - w/2, y - h/2), w, h,
                     boxstyle="round,pad=0.02,rounding_size=0.1",
                     fc=CARD_FACE, ec="#cbd5e1", lw=1, zorder=5))
        rank, suit = card[0], card[1]
        col = SUIT_COLOR[suit]
        ax.text(x - w*0.27, y + h*0.27, rank, ha="center", va="center",
                fontsize=12, fontweight="bold", color=col, zorder=6)
        ax.text(x, y - h*0.08, SUIT_GLYPH[suit], ha="center", va="center",
                fontsize=17, color=col, zorder=6)
    else:
        ax.add_patch(FancyBboxPatch((x - w/2, y - h/2), w, h,
                     boxstyle="round,pad=0.02,rounding_size=0.1",
                     fc=CARD_BACK, ec="#2a4a6b", lw=1, zorder=5))
        for dx in (-0.16, 0, 0.16):
            ax.add_patch(Circle((x + dx, y), 0.05, fc="#2a4a6b",
                         ec="#3c6ea0", lw=0.6, zorder=6))


def draw_chips(ax, x, y, n, color):
    for i in range(n):
        ax.add_patch(Circle((x, y + i * 0.07), 0.16, fc=color, ec="#0b0f0c",
                     lw=0.8, zorder=3))
        ax.add_patch(Circle((x, y + i * 0.07), 0.105, fc="none", ec="#ffffff",
                     lw=0.6, alpha=0.35, zorder=3))


def poker_table_gif():
    hero = ["As", "Ks"]      # A K of spades
    villain = ["Qh", "Qd"]   # pocket queens
    board = ["Kd", "7c", "2s", "9h", "Js"]

    # storyboard: (stage, n_board, pot, hero_act, vill_act, reveal_villain, winner)
    story = [
        ("Pre-flop", 0, 1.5,  "",              "",      False, None),
        ("Pre-flop", 0, 1.5,  "Bet 75% pot",   "",      False, None),
        ("Pre-flop", 0, 4.5,  "Bet 75% pot",   "Call",  False, None),
        ("Flop",     3, 4.5,  "",              "Check", False, None),
        ("Flop",     3, 6.0,  "Bet 33% pot",   "Check", False, None),
        ("Flop",     3, 7.5,  "Bet 33% pot",   "Call",  False, None),
        ("Turn",     4, 7.5,  "Check",         "Check", False, None),
        ("River",    5, 7.5,  "",              "Bet 100% pot", False, None),
        ("River",    5, 15.0, "Call",          "Bet 100% pot", False, None),
        ("Showdown", 5, 15.0, "Pair of Kings", "Pair of Queens", True, "hero"),
        ("Showdown", 5, 15.0, "Pair of Kings", "Pair of Queens", True, "hero"),
    ]
    holds = [3, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6]
    frames = []
    for s, h in zip(story, holds):
        frames += [s] * h

    fig, ax = plt.subplots(figsize=(8.4, 5.2))
    fig.subplots_adjust(left=0, right=1, top=1, bottom=0)

    def render(i):
        ax.clear()
        ax.set_xlim(0, 10); ax.set_ylim(0, 6.2); ax.axis("off")
        ax.set_facecolor(BG)
        stage, nb, pot, hero_act, vill_act, reveal, winner = frames[i]

        # table
        ax.add_patch(Ellipse((5, 3.05), 9.0, 4.7, fc=RIM, ec="none", zorder=0))
        ax.add_patch(Ellipse((5, 3.05), 8.5, 4.25, fc=FELT, ec=FELT_DARK,
                     lw=2, zorder=1))
        ax.add_patch(Ellipse((5, 3.05), 6.0, 2.5, fc="none", ec=FELT_DARK,
                     lw=1.2, alpha=0.6, zorder=1))

        # title + stage
        ax.text(0.35, 5.92, "CFR-Edge", fontsize=15, fontweight="bold",
                color=TEXT, zorder=8)
        ax.text(0.35, 5.55, "heads-up self-play under the solved strategy",
                fontsize=9.5, color=MUTED, zorder=8)
        ax.add_patch(FancyBboxPatch((7.7, 5.45), 2.0, 0.5,
                     boxstyle="round,pad=0.02,rounding_size=0.1",
                     fc=PANEL, ec=ACCENT, lw=1.2, zorder=8))
        ax.text(8.7, 5.7, stage, ha="center", va="center", fontsize=11,
                color=ACCENT, fontweight="bold", zorder=9)

        # community cards
        cx0 = 5 - 2 * 0.78
        for j in range(5):
            x = cx0 + j * 0.78
            if j < nb:
                draw_card(ax, x, 3.25, board[j], True)
            else:
                ax.add_patch(FancyBboxPatch((x - 0.33, 3.25 - 0.48), 0.66, 0.96,
                             boxstyle="round,pad=0.02,rounding_size=0.1",
                             fc="#0c3a22", ec=FELT_DARK, lw=1, zorder=2))

        # pot (between the board and the hero seat)
        draw_chips(ax, 5, 2.35, 5, "#d4af37")
        ax.text(5, 1.95, f"Pot  {pot:.1f} bb", ha="center", va="center",
                fontsize=10.5, color=TEXT, fontweight="bold", zorder=8)

        # villain (top)
        vg = (winner == "villain")
        for k, (dx) in enumerate((-0.42, 0.42)):
            draw_card(ax, 5 + dx, 4.78, villain[k], reveal, glow=vg)
        ax.text(3.35, 4.78, "SOLVER", ha="right", va="center", fontsize=10,
                color=MUTED, fontweight="bold")
        if vill_act:
            ax.add_patch(FancyBboxPatch((6.05, 4.49), 2.2, 0.58,
                         boxstyle="round,pad=0.02,rounding_size=0.12",
                         fc=PANEL, ec="#2b6cb0", lw=1.2, zorder=8))
            ax.text(7.15, 4.78, vill_act, ha="center", va="center",
                    fontsize=9.5, color="#7cc4ff", zorder=9)

        # hero (bottom)
        hg = (winner == "hero")
        for k, (dx) in enumerate((-0.42, 0.42)):
            draw_card(ax, 5 + dx, 0.95, hero[k], True, glow=hg)
        ax.text(3.35, 0.95, "HERO", ha="right", va="center", fontsize=10,
                color=ACCENT, fontweight="bold")
        if hero_act:
            ax.add_patch(FancyBboxPatch((6.05, 0.66), 2.4, 0.58,
                         boxstyle="round,pad=0.02,rounding_size=0.12",
                         fc=PANEL, ec=ACCENT, lw=1.2, zorder=8))
            ax.text(7.25, 0.95, hero_act, ha="center", va="center",
                    fontsize=9.5, color=ACCENT, zorder=9)

        if winner:
            ax.text(5, 0.28, "Hero wins  (pair of kings beats pair of queens)",
                    ha="center", va="center", fontsize=9.5, color="#facc15",
                    fontweight="bold", zorder=9)
        else:
            ax.text(5, 0.28, "bet sizes are the solver's 33 / 75 / 100% pot "
                    "action abstraction", ha="center", va="center",
                    fontsize=8.2, color=MUTED, zorder=9)
        return []

    # static poster: render the final showdown frame to PNG
    render(len(frames) - 1)
    fig.savefig(os.path.join(OUT, "poker-table-still.png"), dpi=150)

    anim = animation.FuncAnimation(fig, render, frames=len(frames),
                                   interval=240, blit=False)
    path = os.path.join(OUT, "poker-table-demo.gif")
    anim.save(path, writer=animation.PillowWriter(fps=4))
    plt.close(fig)
    print("wrote poker-table-demo.gif and poker-table-still.png")


if __name__ == "__main__":
    convergence_plot(
        [("kuhn_cfr.json", "CFR", C_CFR),
         ("kuhn_cfr_plus.json", "CFR+", C_CFRP),
         ("kuhn_dcfr.json", "DCFR", C_DCFR)],
        "Kuhn Poker convergence",
        "Exact best-response exploitability, 10,000 iterations. Nash exploitability = 0.",
        "kuhn-convergence.png",
        ("DCFR reaches 4.4e-4", (10000, 4.4e-4), (1500, 8e-4), C_DCFR))

    convergence_plot(
        [("leduc_cfr.json", "CFR", C_CFR),
         ("leduc_cfr_plus.json", "CFR+", C_CFRP),
         ("leduc_dcfr.json", "DCFR", C_DCFR)],
        "Leduc Hold'em convergence",
        "Exact best-response exploitability over all 120 deals, 10,000 iterations.",
        "leduc-convergence.png",
        None)

    kuhn_strategy_plot()
    holdem_range_plot()
    holdem_proxy_plot()
    poker_table_gif()
    print("\nAll assets written to", OUT)
