'use client';
import { useEffect, useState, useCallback } from 'react';
import { motion } from 'framer-motion';
import { useSolverStore } from '@/store/solver';
import ConvergenceChart from '@/components/charts/ConvergenceChart';
import StrategyHeatmap from '@/components/charts/StrategyHeatmap';
import type { GameStrategy, GameType, AlgoType } from '@/lib/types';
import { loadAllAlgos } from '@/lib/data';
import { fmt } from '@/lib/utils';

const GAMES: { id: GameType; label: string; desc: string }[] = [
  { id: 'kuhn',   label: 'Kuhn Poker',   desc: '12 infosets · exact Nash known' },
  { id: 'leduc',  label: 'Leduc Hold\'em', desc: '288 infosets · 2-round' },
  { id: 'holdem', label: 'Texas Hold\'em', desc: '33K+ infosets · MCCFR' },
];

const ALGOS: AlgoType[] = ['CFR', 'CFR+', 'DCFR'];

export default function SolvePage() {
  const [allStrats, setAllStrats] = useState<Record<string, GameStrategy>>({});
  const [loading, setLoading] = useState(true);
  const [game, setGame] = useState<GameType>('kuhn');
  const [logScale, setLogScale] = useState(true);
  const [currentIter, setCurrentIter] = useState(10000);
  const [isPlaying, setIsPlaying] = useState(false);
  const [snapIndex, setSnapIndex] = useState(0);

  useEffect(() => {
    loadAllAlgos(game).then(strats => {
      setAllStrats(strats);
      setLoading(false);
      // Set iteration to max
      const max = Object.values(strats)[0]?.iterations ?? 10000;
      setCurrentIter(max);
      setSnapIndex(0);
    }).catch(console.error);
  }, [game]);

  // Get snapshot keys sorted
  const dcfrStrat = allStrats['DCFR'];
  const snapKeys = dcfrStrat
    ? Object.keys(dcfrStrat.strategy_snapshots).map(Number).sort((a,b) => a-b)
    : [];

  // Auto-play: advance through snapshots
  useEffect(() => {
    if (!isPlaying) return;
    const interval = setInterval(() => {
      setSnapIndex(prev => {
        if (prev >= snapKeys.length - 1) { setIsPlaying(false); return prev; }
        const next = prev + 1;
        setCurrentIter(snapKeys[next]);
        return next;
      });
    }, 600);
    return () => clearInterval(interval);
  }, [isPlaying, snapKeys]);

  const currentStrategy = dcfrStrat
    ? (dcfrStrat.strategy_snapshots[String(snapKeys[snapIndex])] ?? dcfrStrat.final_strategy)
    : null;

  // Convergence chart data per algorithm
  const chartData = Object.fromEntries(
    Object.entries(allStrats).filter(([_, s]) => s.game === game)
  );

  return (
    <div className="min-h-screen bg-gray-950 text-white p-6">
      <div className="max-w-7xl mx-auto">

        {/* Header */}
        <motion.div initial={{ opacity: 0, y: -20 }} animate={{ opacity: 1, y: 0 }} className="mb-8">
          <h1 className="text-4xl font-black text-white mb-2">
            Live CFR Solver
          </h1>
          <p className="text-gray-400">
            Watch the algorithm converge to Nash equilibrium in real time. Scrub through iterations
            to see strategies evolve.
          </p>
        </motion.div>

        {/* Controls row */}
        <div className="flex flex-wrap gap-4 mb-8 items-center">
          {/* Game selector */}
          <div className="flex gap-2">
            {GAMES.map(g => (
              <button
                key={g.id}
                onClick={() => { setGame(g.id); setLoading(true); }}
                className={`px-4 py-2 rounded-lg text-sm font-medium transition-all ${
                  game === g.id
                    ? 'bg-emerald-500/20 text-emerald-400 ring-1 ring-emerald-500/30'
                    : 'bg-gray-800 text-gray-400 hover:text-white'
                }`}
              >
                {g.label}
              </button>
            ))}
          </div>

          {/* Log scale toggle */}
          <button
            onClick={() => setLogScale(!logScale)}
            className="px-3 py-2 rounded-lg text-xs font-mono border border-gray-700 text-gray-400 hover:text-white transition-colors"
          >
            {logScale ? 'log scale' : 'linear scale'}
          </button>
        </div>

        <div className="grid lg:grid-cols-5 gap-6">

          {/* Left: Convergence chart + controls */}
          <div className="lg:col-span-3 space-y-4">
            <div className="bg-gray-900/60 rounded-2xl border border-gray-800 p-6">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-lg font-bold text-white">Exploitability Convergence</h2>
                <div className="text-xs text-gray-500 font-mono">
                  ε(T) ~ C · T<sup>-α</sup>
                </div>
              </div>

              {loading ? (
                <div className="h-80 flex items-center justify-center text-gray-600">
                  Loading strategies...
                </div>
              ) : (
                <ConvergenceChart
                  strategies={chartData}
                  currentIter={currentIter}
                  logScale={logScale}
                />
              )}

              {/* Iteration scrubber */}
              {!loading && dcfrStrat && (
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
                        setCurrentIter(snapKeys[idx]);
                      }}
                      className="flex-1 accent-emerald-500"
                    />
                    <span className="text-xs text-gray-500 w-16 font-mono text-right">
                      {dcfrStrat.iterations.toLocaleString()}
                    </span>
                  </div>

                  <div className="flex items-center justify-between">
                    <div className="text-sm font-mono">
                      <span className="text-gray-500">iter </span>
                      <span className="text-emerald-400 font-bold">{currentIter.toLocaleString()}</span>
                    </div>

                    {/* Algorithm stats */}
                    <div className="flex gap-4">
                      {Object.entries(allStrats).map(([algo, s]) => {
                        const closest = s.convergence.reduce((a, b) =>
                          Math.abs(a.iter - currentIter) < Math.abs(b.iter - currentIter) ? a : b
                        );
                        return (
                          <div key={algo} className="text-center">
                            <div className="text-[10px] text-gray-500">{algo}</div>
                            <div className="text-xs font-mono text-amber-400">
                              {fmt(closest.exploitability)}
                            </div>
                          </div>
                        );
                      })}
                    </div>

                    {/* Play button */}
                    <button
                      onClick={() => {
                        if (snapIndex >= snapKeys.length - 1) setSnapIndex(0);
                        setIsPlaying(!isPlaying);
                      }}
                      className="px-4 py-1.5 rounded-lg bg-emerald-500/20 text-emerald-400 text-xs font-mono hover:bg-emerald-500/30 transition-colors ring-1 ring-emerald-500/30"
                    >
                      {isPlaying ? '⏸ Pause' : '▶ Replay'}
                    </button>
                  </div>
                </div>
              )}
            </div>

            {/* Algorithm comparison cards */}
            {!loading && (
              <div className="grid grid-cols-3 gap-3">
                {Object.entries(allStrats).map(([algo, s]) => (
                  <div key={algo} className="bg-gray-900/60 rounded-xl border border-gray-800 p-4">
                    <div className="text-xs text-gray-500 mb-1">{algo}</div>
                    <div className="text-2xl font-black text-white font-mono">
                      {fmt(s.final_exploitability)}
                    </div>
                    <div className="text-xs text-gray-600 mt-1">final ε</div>
                    <div className="mt-2 text-xs text-emerald-400 font-mono">
                      α = {s.convergence_rate.toFixed(3)}
                    </div>
                    <div className="text-[10px] text-gray-600">conv. rate</div>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* Right: Strategy heatmap */}
          <div className="lg:col-span-2">
            <div className="bg-gray-900/60 rounded-2xl border border-gray-800 p-6 h-full">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-lg font-bold text-white">Strategy at Iteration {currentIter.toLocaleString()}</h2>
                {dcfrStrat && (
                  <div className="text-xs text-gray-500 font-mono">
                    {Object.keys(currentStrategy ?? {}).length} infosets
                  </div>
                )}
              </div>

              {!currentStrategy ? (
                <div className="text-gray-600 text-sm">Select a game to view strategy.</div>
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

        {/* DCFR insights */}
        {!loading && dcfrStrat && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            transition={{ delay: 0.5 }}
            className="mt-6 grid sm:grid-cols-4 gap-4"
          >
            {[
              { label: 'Infosets', value: dcfrStrat.num_infosets.toLocaleString(), sub: 'unique decision points' },
              { label: 'Final Expl.', value: fmt(dcfrStrat.final_exploitability), sub: 'ε at Nash = 0' },
              { label: 'Convergence α', value: dcfrStrat.convergence_rate.toFixed(3), sub: 'ε ~ C · T⁻ᵅ' },
              { label: 'Solve time', value: `${dcfrStrat.elapsed_seconds.toFixed(1)}s`, sub: 'AVX2 C++ backend' },
            ].map((s) => (
              <div key={s.label} className="bg-gray-900/40 rounded-xl border border-gray-800 p-4 text-center">
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
