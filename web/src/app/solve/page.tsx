'use client';
import { useEffect, useMemo, useState } from 'react';
import { motion } from 'framer-motion';
import ConvergenceChart from '@/components/charts/ConvergenceChart';
import StrategyHeatmap from '@/components/charts/StrategyHeatmap';
import type { GameStrategy, GameType, StrategyMap } from '@/lib/types';
import { loadAllAlgos } from '@/lib/data';
import { fmt } from '@/lib/utils';

const GAMES: { id: GameType; label: string; desc: string }[] = [
  { id: 'kuhn',   label: 'Kuhn Poker',    desc: '12 infosets · exact Nash known' },
  { id: 'leduc',  label: "Leduc Hold'em", desc: '288 infosets · 2-round' },
  { id: 'holdem', label: 'Texas Hold\'em', desc: '33K+ infosets · MCCFR' },
];

export default function SolvePage() {
  const [allStrats, setAllStrats] = useState<Record<string, GameStrategy>>({});
  const [loadedGame, setLoadedGame] = useState<GameType | null>(null);
  const [game, setGame]           = useState<GameType>('kuhn');
  const [logScale, setLogScale]   = useState(true);
  const [currentIter, setCurrentIter] = useState(10000);
  const [isPlaying, setIsPlaying] = useState(false);
  const [snapIndex, setSnapIndex] = useState(0);

  const loading = loadedGame !== game;

  // ── Load strategies whenever the selected game changes ─────────────────
  useEffect(() => {
    let active = true;
    loadAllAlgos(game)
      .then(strats => {
        if (!active) return;
        setAllStrats(strats);
        const first = Object.values(strats)[0];
        setCurrentIter(first?.iterations ?? 10000);
        setLoadedGame(game);
      })
      .catch(err => {
        if (!active) return;
        console.error(err);
        setAllStrats({});
        setLoadedGame(game);
      });
    return () => { active = false; };
  }, [game]);

  // ── Derived: DCFR strategy + snapshot keys ──────────────────────────────
  const dcfrStrat  = allStrats['DCFR'] ?? null;

  // Guard: strategy_snapshots may be absent (e.g. trimmed holdem JSON)
  const snapshots  = useMemo(() => dcfrStrat?.strategy_snapshots ?? {}, [dcfrStrat]);
  const snapKeys   = useMemo(() => Object.keys(snapshots).map(Number).sort((a, b) => a - b), [snapshots]);
  const hasSnaps   = snapKeys.length > 0;

  // Current strategy to display in the heatmap
  const currentStrategy: StrategyMap | null = (() => {
    if (!dcfrStrat) return null;
    if (hasSnaps) {
      const key = String(snapKeys[Math.min(snapIndex, snapKeys.length - 1)]);
      return snapshots[key] ?? dcfrStrat.final_strategy ?? null;
    }
    return Object.keys(dcfrStrat.final_strategy ?? {}).length > 0 ? dcfrStrat.final_strategy : null;
  })();

  // ── Auto-play through snapshots ─────────────────────────────────────────
  useEffect(() => {
    if (!isPlaying || !hasSnaps) return;
    const id = setInterval(() => {
      setSnapIndex(prev => {
        if (prev >= snapKeys.length - 1) { setIsPlaying(false); return prev; }
        const next = prev + 1;
        setCurrentIter(snapKeys[next]);
        return next;
      });
    }, 600);
    return () => clearInterval(id);
  }, [isPlaying, hasSnaps, snapKeys]);

  // ── Convergence chart data (all loaded algorithms) ──────────────────────
  const chartData = Object.fromEntries(
    Object.entries(allStrats).filter(([, s]) => s != null)
  );

  // ── Exploitability at current iteration for each algo ───────────────────
  function closestExpl(s: GameStrategy) {
    if (!s.convergence?.length) return 0;
    return s.convergence.reduce((a, b) =>
      Math.abs(a.iter - currentIter) < Math.abs(b.iter - currentIter) ? a : b
    ).exploitability;
  }

  return (
    <div className="min-h-screen bg-[#050806] text-white p-4 sm:p-6">
      <div className="max-w-7xl mx-auto">

        {/* ── Header ── */}
        <motion.div initial={{ opacity: 0, y: -20 }} animate={{ opacity: 1, y: 0 }} className="mb-8 rounded-md border border-emerald-900/70 bg-[#07110a] p-5 sm:p-6">
          <div className="font-mono text-xs uppercase tracking-[0.2em] text-emerald-300">C++ Export Playback</div>
          <h1 className="mt-2 mb-2 font-display text-5xl font-semibold text-emerald-50">Live CFR Solver</h1>
          <p className="max-w-3xl text-gray-400">
            Watch the algorithm converge to Nash equilibrium. Scrub through iterations to see
            strategies evolve from uniform random to near-optimal.
          </p>
        </motion.div>

        {/* ── Controls ── */}
        <div className="flex flex-wrap gap-3 mb-8 items-center">
          {GAMES.map(g => (
            <button
              key={g.id}
              onClick={() => {
                setGame(g.id);
                setSnapIndex(0);
                setIsPlaying(false);
              }}
              className={`px-4 py-2 rounded-lg text-sm font-medium transition-all ${
                game === g.id
                  ? 'bg-emerald-500/20 text-emerald-400 ring-1 ring-emerald-500/30'
                  : 'bg-[#07110a] text-gray-400 hover:text-white hover:bg-emerald-300/[0.08]'
              }`}
            >
              {g.label}
              <span className="ml-2 text-[10px] text-gray-600 hidden sm:inline">{g.desc}</span>
            </button>
          ))}
          <button
            onClick={() => setLogScale(v => !v)}
            className="px-3 py-2 rounded-lg text-xs font-mono border border-emerald-900/70 text-gray-400
                       hover:border-emerald-400/50 hover:text-white transition-colors ml-auto"
          >
            {logScale ? 'log scale' : 'linear scale'}
          </button>
        </div>

        <div className="grid lg:grid-cols-5 gap-6">

          {/* ── Left: chart + scrubber ── */}
          <div className="lg:col-span-3 space-y-4">
            <div className="surface rounded-md p-5 sm:p-6">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-lg font-bold text-white">Exploitability Convergence</h2>
                <span className="text-xs text-gray-500 font-mono">ε(T) ~ C · T<sup>-α</sup></span>
              </div>

              {loading ? (
                <div className="h-80 flex items-center justify-center text-gray-600 text-sm">
                  Loading strategies…
                </div>
              ) : (
                <ConvergenceChart
                  strategies={chartData}
                  currentIter={currentIter}
                  logScale={logScale}
                />
              )}

              {/* Iteration scrubber - only shown when snapshots exist */}
              {!loading && dcfrStrat && hasSnaps && (
                <div className="mt-4 space-y-3">
                  <div className="flex items-center gap-3">
                    <span className="text-xs text-gray-500 w-8 font-mono">1</span>
                    <input
                      type="range"
                      min={0}
                      max={snapKeys.length - 1}
                      value={snapIndex}
                      onChange={e => {
                        const idx = Number(e.target.value);
                        setSnapIndex(idx);
                        setCurrentIter(snapKeys[idx] ?? currentIter);
                      }}
                      className="flex-1 accent-emerald-500"
                    />
                    <span className="text-xs text-gray-500 w-16 font-mono text-right">
                      {(dcfrStrat.iterations ?? 0).toLocaleString()}
                    </span>
                  </div>

                  <div className="flex items-center justify-between">
                    <div className="text-sm font-mono">
                      <span className="text-gray-500">iter </span>
                      <span className="text-emerald-400 font-bold">{currentIter.toLocaleString()}</span>
                    </div>

                    {/* Per-algo exploitability at current iteration */}
                    <div className="flex gap-4">
                      {Object.entries(allStrats).map(([algo, s]) => (
                        <div key={algo} className="text-center">
                          <div className="text-[10px] text-gray-500">{algo}</div>
                          <div className="text-xs font-mono text-amber-400">
                            {fmt(closestExpl(s))}
                          </div>
                        </div>
                      ))}
                    </div>

                    <button
                      onClick={() => {
                        if (snapIndex >= snapKeys.length - 1) setSnapIndex(0);
                        setIsPlaying(v => !v);
                      }}
                      className="px-4 py-1.5 rounded-lg bg-emerald-500/20 text-emerald-400 text-xs
                                 font-mono hover:bg-emerald-500/30 transition-colors ring-1 ring-emerald-500/30"
                    >
                      {isPlaying ? '⏸ Pause' : '▶ Replay'}
                    </button>
                  </div>
                </div>
              )}

              {/* Holdem: no snapshot scrubber - show a note instead */}
              {!loading && game === 'holdem' && (
                <p className="mt-3 text-xs text-gray-600">
                  Texas Hold&apos;em uses MCCFR - strategy snapshots are not available
                  (tree too large to checkpoint). Convergence curve shows exploitability proxy.
                </p>
              )}
            </div>

            {/* Algorithm comparison cards */}
            {!loading && Object.keys(allStrats).length > 0 && (
              <div
                className="grid gap-3"
                style={{ gridTemplateColumns: 'repeat(auto-fit, minmax(150px, 1fr))' }}
              >
                {Object.entries(allStrats).map(([algo, s]) => (
                  <div key={algo} className="surface-quiet rounded-md p-4">
                    <div className="text-xs text-gray-500 mb-1">{algo}</div>
                    <div className="text-2xl font-black text-white font-mono">
                      {fmt(s.final_exploitability ?? 0)}
                    </div>
                    <div className="text-xs text-gray-600 mt-1">final ε</div>
                    <div className="mt-2 text-xs text-emerald-400 font-mono">
                      α = {(s.convergence_rate ?? 0).toFixed(3)}
                    </div>
                    <div className="text-[10px] text-gray-600">conv. rate</div>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* ── Right: strategy heatmap ── */}
          <div className="lg:col-span-2">
            <div className="surface rounded-md p-5 sm:p-6 h-full">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-lg font-bold text-white">
                  Strategy
                  {hasSnaps
                    ? ` @ iter ${currentIter.toLocaleString()}`
                    : ` (final)`}
                </h2>
                {currentStrategy && (
                  <span className="text-xs text-gray-500 font-mono">
                    {Object.keys(currentStrategy).length} infosets
                  </span>
                )}
              </div>

              {loading ? (
                <div className="text-gray-600 text-sm">Loading…</div>
              ) : !currentStrategy ? (
                <div className="text-gray-600 text-sm">
                  {game === 'holdem'
                    ? 'Texas Hold\'em exports a preflop summary for the range chart.'
                    : 'Select a game to view the strategy.'}
                </div>
              ) : (
                <StrategyHeatmap
                  strategy={currentStrategy}
                  game={game}
                  maxRows={game === 'kuhn' ? 12 : game === 'leduc' ? 40 : 30}
                />
              )}
            </div>
          </div>
        </div>

        {/* ── Bottom metric strip ── */}
        {!loading && dcfrStrat && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            transition={{ delay: 0.4 }}
            className="mt-6 grid sm:grid-cols-4 gap-4"
          >
            {[
              { label: 'Infosets',      value: (dcfrStrat.num_infosets ?? 0).toLocaleString(), sub: 'unique decision points' },
              { label: 'Final Expl.',   value: fmt(dcfrStrat.final_exploitability ?? 0),        sub: 'ε at Nash = 0' },
              { label: 'Convergence α', value: (dcfrStrat.convergence_rate ?? 0).toFixed(3),   sub: 'ε ~ C · T⁻ᵅ' },
              { label: 'Solve time',    value: dcfrStrat.elapsed_seconds != null ? `${dcfrStrat.elapsed_seconds.toFixed(1)}s` : '-', sub: 'AVX2 C++ backend' },
            ].map(s => (
              <div key={s.label} className="surface-quiet rounded-md p-4 text-center">
                <div className="text-2xl font-black text-emerald-400 font-mono">{s.value}</div>
                <div className="text-sm font-semibold text-white mt-1">{s.label}</div>
                <div className="text-xs text-gray-600">{s.sub}</div>
              </div>
            ))}
          </motion.div>
        )}
      </div>
    </div>
  );
}
