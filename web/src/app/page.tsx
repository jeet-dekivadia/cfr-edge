'use client';

import Link from 'next/link';
import dynamic from 'next/dynamic';
import { useEffect, useMemo, useState } from 'react';
import ConvergenceChart from '@/components/charts/ConvergenceChart';
import type { GameStrategy, MetaFile } from '@/lib/types';
import { HOLDEM_RANKS, holdemHandForMatrixCell, loadAllAlgos, loadMeta } from '@/lib/data';
import { fmt, lerpColor, pct } from '@/lib/utils';

const PokerTableScene = dynamic(() => import('@/components/PokerTableScene'), { ssr: false });

type StrategyGroups = {
  kuhn?: Record<string, GameStrategy>;
  leduc?: Record<string, GameStrategy>;
  holdem?: Record<string, GameStrategy>;
};

const ACTIONS = [
  { href: '/solve', label: 'Run Solver' },
  { href: '/play', label: 'Play Kuhn' },
  { href: '/strategy', label: 'Browse Strategy' },
  { href: '/cpp', label: 'C++' },
];

function rootBucketActions(summary: Record<string, Record<string, number>>, bucket: number) {
  const root = summary[`PF${bucket}:`];
  if (root) return root;

  const entries = Object.entries(summary).filter(([key]) => Number(key.match(/^PF(\d+):/)?.[1]) === bucket);
  const total: Record<string, number> = {};
  for (const [, actions] of entries) {
    for (const [action, value] of Object.entries(actions)) total[action] = (total[action] ?? 0) + value;
  }
  return Object.fromEntries(Object.entries(total).map(([action, value]) => [action, entries.length ? value / entries.length : 0]));
}

function MiniHoldemRange({ strategy }: { strategy?: GameStrategy }) {
  const summary = strategy?.preflop_summary ?? {};

  return (
    <div className="grid grid-cols-[auto_repeat(13,minmax(0,1fr))] gap-px">
      <div />
      {HOLDEM_RANKS.map(rank => (
        <div key={rank} className="pb-1 text-center font-mono text-[10px] text-emerald-700">{rank}</div>
      ))}
      {HOLDEM_RANKS.map((rowRank, row) => (
        <div key={rowRank} className="contents">
          <div className="flex items-center justify-end pr-2 font-mono text-[10px] text-emerald-700">{rowRank}</div>
          {HOLDEM_RANKS.map((_, col) => {
            const hand = holdemHandForMatrixCell(row, col);
            const actions = rootBucketActions(summary, hand.bucket);
            const raise = (actions.bet33 ?? 0) + (actions.bet75 ?? 0) + (actions.bet100 ?? 0) + (actions.allin ?? 0);
            return (
              <div
                key={`${row}-${col}`}
                title={`${hand.label} ${pct(raise)} raise/all-in`}
                className="aspect-square"
                style={{ backgroundColor: lerpColor('#07120b', '#7df2a5', Math.min(raise, 1)) }}
              />
            );
          })}
        </div>
      ))}
    </div>
  );
}

function Section({
  number,
  title,
  children,
}: {
  number: string;
  title: string;
  children: React.ReactNode;
}) {
  return (
    <section className="border-t border-emerald-900/70 pt-8">
      <p className="font-mono text-xs uppercase tracking-[0.24em] text-emerald-500">{number}</p>
      <h2 className="mt-3 font-display text-4xl font-semibold text-emerald-50">{title}</h2>
      <div className="mt-5 space-y-5 text-[17px] leading-8 text-slate-300">{children}</div>
    </section>
  );
}

function Metric({ label, value, detail }: { label: string; value: string; detail: string }) {
  return (
    <div className="border-t border-emerald-900/70 py-5">
      <div className="font-mono text-2xl font-semibold tracking-tight text-emerald-200">{value}</div>
      <div className="mt-1 text-sm text-slate-300">{label}</div>
      <div className="mt-1 text-xs text-slate-500">{detail}</div>
    </div>
  );
}

export default function Home() {
  const [meta, setMeta] = useState<MetaFile | null>(null);
  const [groups, setGroups] = useState<StrategyGroups>({});

  useEffect(() => {
    let mounted = true;
    Promise.all([
      loadMeta(),
      loadAllAlgos('kuhn'),
      loadAllAlgos('leduc'),
      loadAllAlgos('holdem'),
    ]).then(([metaFile, kuhn, leduc, holdem]) => {
      if (!mounted) return;
      setMeta(metaFile);
      setGroups({ kuhn, leduc, holdem });
    });
    return () => { mounted = false; };
  }, []);

  const kuhn = groups.kuhn?.DCFR;
  const leduc = groups.leduc?.DCFR;
  const holdem = groups.holdem?.DCFR;

  const metrics = useMemo(() => [
    {
      label: 'Kuhn DCFR final epsilon',
      value: kuhn ? fmt(kuhn.final_exploitability) : '...',
      detail: kuhn ? `${kuhn.iterations.toLocaleString()} iterations, ${kuhn.num_infosets} infosets` : 'loading',
    },
    {
      label: "Leduc Hold'em",
      value: leduc ? leduc.num_infosets.toLocaleString() : '...',
      detail: leduc ? `final epsilon ${fmt(leduc.final_exploitability)}` : 'loading',
    },
    {
      label: "Hold'em MCCFR",
      value: holdem ? holdem.num_infosets.toLocaleString() : '...',
      detail: holdem ? `${Object.keys(holdem.preflop_summary ?? {}).length.toLocaleString()} preflop summary nodes` : 'loading',
    },
    {
      label: 'Generated solver bundle',
      value: meta?.generated_at ?? '...',
      detail: meta ? `schema v${meta.version}` : 'loading',
    },
  ], [kuhn, leduc, holdem, meta]);

  return (
    <div className="min-h-screen bg-[#050806] text-slate-100">
      <main className="mx-auto max-w-5xl px-5 pb-24 pt-28">
        <header className="border-b border-emerald-900/70 pb-12">
          <p className="font-mono text-xs uppercase tracking-[0.24em] text-emerald-500">By Jeet Dekivadia</p>
          <h1 className="mt-5 max-w-4xl font-display text-7xl font-semibold leading-[0.9] tracking-tight text-emerald-50 sm:text-8xl">
            Building CFR-Edge
          </h1>
          <p className="mt-5 font-display text-3xl text-emerald-200">Poker Solver in C++</p>
          <p className="mt-6 max-w-2xl text-lg leading-8 text-slate-400">
            A private C++ poker solver and visualization system for studying decision-making under hidden information.
          </p>
          <div className="mt-8 flex flex-wrap gap-2">
            {ACTIONS.map(action => (
              <Link
                key={action.href}
                href={action.href}
                className="rounded-full border border-emerald-900 px-4 py-2 text-sm text-slate-300 transition hover:border-emerald-300 hover:bg-emerald-300 hover:text-emerald-950"
              >
                {action.label}
              </Link>
            ))}
          </div>
        </header>

        <article className="grid gap-16 py-14 lg:grid-cols-[minmax(0,1fr)_260px]">
          <div className="space-y-14">
            <Section number="01" title="The tournament">
              <p>After a Jane Street recruiting event, there was a small poker tournament at the end of the night. I got knocked out pretty early and it bothered me in a specific way. Not because I lost, but because I genuinely had no idea what I was supposed to be doing at any point.</p>
              <p>
                Chess felt approachable by comparison. Both players see the whole board. Poker was different in a way I couldn&apos;t quite articulate yet. The hidden cards weren&apos;t just an inconvenience. They changed the structure of the problem entirely. And I had no framework for reasoning about it.
              </p>
            </Section>

            <Section number="02" title="The thing I kept thinking about">
              <p>
                When you can&apos;t see the full state of a system, what does it mean to act well? The clean idea of a single best move stops working. You have to think about what your opponent might be holding, what they think you might be holding, and how to mix your actions so you don&apos;t just telegraph everything you do. It&apos;s a different category of problem than the search and optimization stuff I was used to.
              </p>
              <p>I wanted to understand it properly, not just read a Wikipedia summary of it.</p>
            </Section>

            <Section number="03" title="Finding CFR">
              <p>
                I went looking for how real poker solvers work and kept running into Counterfactual Regret Minimization. The core idea is pretty intuitive once you sit with it: at every decision point, track what you regret not doing. Do that across thousands of iterations. Let the regrets shape the strategy. Average it all out. The math is surprisingly clean for something that actually solves a hard problem.
              </p>
              <p>Reading the theory is one thing. Writing code that does what the theory describes is a much longer project.</p>
            </Section>

            <figure className="overflow-hidden border border-emerald-900/70 bg-[#061008]">
              <div className="h-[460px]">
                <PokerTableScene />
              </div>
              <figcaption className="border-t border-emerald-900/70 px-4 py-3 text-sm text-slate-500">
                The visual shell is just a window into the solver output.
              </figcaption>
            </figure>

            <Section number="04" title="Starting with the smallest possible game">
              <p>
                Kuhn Poker has three cards, two players, and maybe six decision points total. I picked it on purpose. It&apos;s small enough to traverse completely, which means you can verify that regret accumulation is actually doing what you think it is. When the strategies started converging toward equilibrium across iterations, that was the first moment the algorithm felt real rather than theoretical.
              </p>
              <p>It also immediately made clear that most of the work was in the bookkeeping, not the math.</p>
            </Section>

            <Section number="05" title="When a function becomes a system">
              <p>
                Once Kuhn was working, I started caring about the structure. A recursive function is not a solver. I needed game logic separated from traversal, an information-set representation that would hold up as games got bigger, a storage layer for regrets and accumulated strategies that wouldn&apos;t become the bottleneck, and an export format I could actually read. The C++ side and the Next.js visualization needed to stay independent so neither would drag the other down when I changed something.
              </p>
              <p>Getting those boundaries right early saved me from a lot of bad refactors later.</p>
            </Section>

            <Section number="06" title="The part that took the longest to get right">
              <p>
                Information set keys encode exactly what a player can observe: hole cards, community cards, betting history. Nothing else. No peeking at opponent cards, even in simulation. Getting that encoding right is what makes the solver correct. But the key structure also affects memory layout, lookup speed, how well the data fits in cache, and how readable the output is when something goes wrong. A sloppy representation does not just make things slower. It compounds across every single iteration of the algorithm.
              </p>
              <p>This is where the project stopped feeling like an exercise and started feeling like real software.</p>
            </Section>

            <Section number="07" title="Trying different algorithms">
              <p>
                Vanilla CFR gave me the baseline and showed me what convergence actually looks like in practice. It&apos;s not as smooth as the theory implies. CFR+ introduced regret clipping and changed the convergence profile in ways I had to see to believe. DCFR made the down-weighting of early iterations explicit. Then games got big enough that full traversal stopped being feasible, so I added MCCFR with external sampling. Each variant taught me something different about the tradeoffs between iteration cost, memory pressure, and how fast the strategies actually converge.
              </p>
            </Section>

            <Section number="08" title="Scaling up">
              <p>
                Leduc Hold&apos;em has a public card and pushed the exported infoset count to 288. That felt manageable. When I moved to an abstracted heads-up no-limit Hold&apos;em with 33,260 infosets, the difference between slow and unusable became very concrete very fast. Sampling stopped being optional. So did thinking carefully about what I was allocating and how often.
              </p>
            </Section>

            <Section number="09" title="Why I built the visualizer">
              <p>
                Log files are a terrible way to debug a solver. You can&apos;t see whether a mixed strategy is converging, whether regrets are moving the right direction, or whether something is subtly wrong until it shows up as a bad number at the end. I built the strategy browser, convergence plots, and interactive demo because I needed to actually see what the solver was doing. It started as a debugging tool and turned into the most useful part of the whole project.
              </p>
              <div className="grid gap-4 md:grid-cols-2">
                <figure className="border border-emerald-900/70 bg-[#07110a] p-4">
                  <figcaption className="mb-3 flex items-center justify-between border-b border-emerald-900/70 pb-3 text-sm text-slate-500">
                    <span>Kuhn convergence</span>
                    <Link href="/solve" className="text-emerald-300 underline underline-offset-4">open</Link>
                  </figcaption>
                  {groups.kuhn ? <ConvergenceChart strategies={groups.kuhn} currentIter={kuhn?.iterations ?? 10000} logScale /> : <div className="h-80" />}
                </figure>

                <figure className="border border-emerald-900/70 bg-[#07110a] p-4">
                  <figcaption className="mb-4 flex items-center justify-between border-b border-emerald-900/70 pb-3 text-sm text-slate-500">
                    <span>Hold&apos;em abstraction</span>
                    <Link href="/holdem" className="text-emerald-300 underline underline-offset-4">open</Link>
                  </figcaption>
                  <MiniHoldemRange strategy={holdem} />
                </figure>
              </div>
            </Section>

            <Section number="10" title="Where things stand">
              <p>
                These numbers are not a claim about anything state-of-the-art. They are a snapshot of what this project has actually shipped and what I can inspect right now. I would rather show something honest than make preliminary work look shinier than it is.
              </p>
              <div className="grid gap-x-8 sm:grid-cols-2">
                {metrics.map(metric => (
                  <Metric key={metric.label} {...metric} />
                ))}
              </div>
            </Section>

            <Section number="11" title="What actually got better">
              <p>
                The biggest shift was not any single feature. It was the point where I stopped guessing about what the solver was doing and started being able to check. Cleaner separation between C++ and the frontend meant changes in one place stopped breaking the other. Better export formats meant the strategy data was actually readable. More algorithm variants meant I could compare convergence behavior instead of just trusting the default. Each of those was a small thing, but together they changed how I worked on the project day to day.
              </p>
            </Section>

            <Section number="12" title="What I took away from this">
              <p>
                Theory that looks clean on paper gets complicated the moment you have to lay it out in memory. The choice of data structure for an information set matters more than the update rule in practice, because a bad layout shows up in the profiler almost immediately. Small games are genuinely useful for building confidence in a design before you commit it to something bigger. And the visualizer caught bugs that no amount of log-reading would have found. If I were starting over, I would build it first.
              </p>
            </Section>

            <Section number="13" title="How this connects to other things I care about">
              <p>
                I have spent time around systems that make decisions quickly under real constraints, at DeepMind, through my incoming work at NVIDIA, and in competitive settings like IMC Prosperity where my team placed Top 25 out of 22,000 teams. The thread connecting all of it is the gap between a correct algorithm and a fast, inspectable, reliable system. That gap is where the interesting engineering problems live, and CFR-Edge sits right in the middle of it.
              </p>
            </Section>

            <Section number="14" title="What comes next">
              <p>
                Proper exploitability evaluation against a known Nash strategy. A benchmark harness that gives variance-aware results rather than single-run numbers. Parallel traversal is the most obvious thing the current design is missing, and figuring out the right synchronization model for concurrent regret updates is a problem I have been putting off because it deserves real attention. Allocator-aware infoset storage to reduce pressure during traversal. Strategy diffing across algorithm runs. Tighter invariants around game-state transitions so bugs surface at construction time rather than quietly mid-run.
              </p>
            </Section>

            <Section number="15" title="Why I kept working on it">
              <p>
                Getting knocked out of that poker tournament was annoying for about a day. Then it became useful, because it pointed me at a problem I had never thought carefully about. CFR-Edge is what happened when I decided to stop reading about the problem and start building something. It is still in progress. Probably always will be. But it is the project I keep coming back to when I want to actually think, and that is the best thing I can say about any side project.
              </p>
            </Section>
          </div>

          <aside className="space-y-8 lg:sticky lg:top-24 lg:self-start">
            <div>
              <p className="font-mono text-xs uppercase tracking-[0.2em] text-emerald-600">Project</p>
              <p className="mt-3 font-display text-3xl font-semibold text-emerald-50">CFR-Edge</p>
              <p className="mt-3 text-sm leading-6 text-slate-500">
                A private C++ poker solver and Next.js visualization project by Jeet Dekivadia.
              </p>
            </div>

            <div className="border-t border-emerald-900/70 pt-5">
              <p className="text-sm font-medium text-emerald-50">Stack</p>
              <p className="mt-2 text-sm leading-6 text-slate-500">
                C++17, CMake, SIMD regret matching, Next.js, TypeScript, D3, Three.js.
              </p>
            </div>

            <div className="border-t border-emerald-900/70 pt-5">
              <p className="text-sm font-medium text-emerald-50">Games</p>
              <p className="mt-2 text-sm leading-6 text-slate-500">
                Kuhn Poker, Leduc Hold&apos;em, and abstracted heads-up no-limit Hold&apos;em.
              </p>
            </div>

            <div className="border-t border-emerald-900/70 pt-5">
              <p className="text-sm font-medium text-emerald-50">Contact</p>
              <p className="mt-2 text-sm leading-6 text-slate-500">
                Reach out at <a className="text-emerald-300 underline underline-offset-4" href="mailto:jeet.university@gmail.com">jeet.university@gmail.com</a> to get access to the GitHub repo. It is private and still under improvement.
              </p>
            </div>
          </aside>
        </article>

        <footer className="border-t border-emerald-900/70 pt-8 text-sm text-slate-500">
          Built and written by Jeet Dekivadia. For repository access, email{' '}
          <a className="text-emerald-300 underline underline-offset-4" href="mailto:jeet.university@gmail.com">jeet.university@gmail.com</a>.
        </footer>
      </main>
    </div>
  );
}