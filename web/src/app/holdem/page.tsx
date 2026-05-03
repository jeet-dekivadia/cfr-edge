'use client';
import { useEffect, useState, useMemo } from 'react';
import { motion } from 'framer-motion';
import type { GameStrategy } from '@/lib/types';
import { HOLDEM_RANKS, holdemHandForMatrixCell, loadStrategy } from '@/lib/data';
import { pct, lerpColor } from '@/lib/utils';

type SummaryActions = Record<string, number>;

function bucketActions(preflopSummary: Record<string, SummaryActions>, bucket: number) {
  const rootKey = `PF${bucket}:`;
  if (preflopSummary[rootKey]) {
    return {
      actions: preflopSummary[rootKey],
      source: 'root' as const,
      nodes: 1,
      key: rootKey,
    };
  }

  const entries = Object.entries(preflopSummary).filter(([key]) => {
    const match = key.match(/^PF(\d+):/);
    return match ? Number(match[1]) === bucket : false;
  });

  const totals: SummaryActions = {};
  for (const [, actions] of entries) {
    for (const [action, value] of Object.entries(actions)) {
      totals[action] = (totals[action] ?? 0) + value;
    }
  }

  const actions = Object.fromEntries(
    Object.entries(totals).map(([action, value]) => [action, entries.length ? value / entries.length : 0]),
  );

  return {
    actions,
    source: 'sampled' as const,
    nodes: entries.length,
    key: `PF${bucket}:*`,
  };
}

// Build the 13×13 preflop matrix (row = high rank, col = low rank)
// Upper-triangle (row < col) = suited, lower-triangle = offsuit, diagonal = pairs
function buildMatrix(preflopSummary: Record<string, Record<string, number>>) {
  const matrix: {
    label: string;
    type: 'pair' | 'suited' | 'offsuit';
    fold: number;
    call: number;
    bet: number;
    allin: number;
    bucket: number;
    nodes: number;
    source: 'root' | 'sampled';
    key: string;
  }[][] = [];

  for (let row = 0; row < 13; row++) {
    matrix[row] = [];
    for (let col = 0; col < 13; col++) {
      const hand = holdemHandForMatrixCell(row, col);
      const summary = bucketActions(preflopSummary, hand.bucket);
      const actions = summary.actions;
      matrix[row][col] = {
        label: hand.label,
        type: hand.type,
        fold:  actions['fold']         ?? 0,
        call:  actions['check/call']   ?? 0,
        bet:   (actions['bet33'] ?? 0) + (actions['bet75'] ?? 0) + (actions['bet100'] ?? 0),
        allin: actions['allin']        ?? 0,
        bucket: hand.bucket,
        nodes: summary.nodes,
        source: summary.source,
        key: summary.key,
      };
    }
  }
  return matrix;
}

type ActionView = 'raise' | 'call' | 'fold' | 'allin';

function cellColor(cell: ReturnType<typeof buildMatrix>[0][0], view: ActionView): string {
  let intensity = 0;
  if (view === 'raise') intensity = cell.bet;
  if (view === 'call')  intensity = cell.call;
  if (view === 'fold')  intensity = cell.fold;
  if (view === 'allin') intensity = cell.allin;

  if (view === 'raise') return lerpColor('#1e293b', '#22c55e', intensity);
  if (view === 'call')  return lerpColor('#1e293b', '#3b82f6', intensity);
  if (view === 'fold')  return lerpColor('#1e293b', '#ef4444', intensity);
  return lerpColor('#1e293b', '#f59e0b', intensity);
}

export default function HoldemPage() {
  const [strategy, setStrategy] = useState<GameStrategy | null>(null);
  const [loading, setLoading] = useState(true);
  const [view, setView] = useState<ActionView>('raise');
  const [hoveredCell, setHoveredCell] = useState<{ row: number; col: number } | null>(null);

  useEffect(() => {
    loadStrategy('holdem_dcfr.json')
      .then(s => { setStrategy(s); setLoading(false); })
      .catch(console.error);
  }, []);

  const preflopSummary = useMemo(() => strategy?.preflop_summary ?? {}, [strategy]);
  const matrix = useMemo(() => buildMatrix(preflopSummary), [preflopSummary]);

  const hoveredData = hoveredCell ? matrix[hoveredCell.row][hoveredCell.col] : null;

  // Aggregate stats — derived from preflop_summary (final_strategy is stripped from holdem JSON)
  const stats = useMemo(() => {
    if (!strategy) return null;
    const pf = Object.values(strategy.preflop_summary ?? {});
    const avgBet = pf.length > 0
      ? pf.reduce((s: number, actions: Record<string, number>) => {
          return s + (actions['bet33'] ?? 0) + (actions['bet75'] ?? 0) + (actions['bet100'] ?? 0);
        }, 0) / pf.length
      : 0;
    return {
      infosets: strategy.num_infosets ?? Object.keys(strategy.preflop_summary ?? {}).length,
      avgBetFreq: avgBet,
      exploitability: strategy.final_exploitability ?? 0,
      rate: strategy.convergence_rate ?? 0,
    };
  }, [strategy]);

  const ACTION_VIEWS: { id: ActionView; label: string; color: string }[] = [
    { id: 'raise', label: 'Raise / Bet', color: 'text-emerald-400' },
    { id: 'call',  label: 'Call / Check', color: 'text-blue-400' },
    { id: 'fold',  label: 'Fold',         color: 'text-red-400' },
    { id: 'allin', label: 'All-in',       color: 'text-amber-400' },
  ];

  return (
    <div className="min-h-screen bg-[#050806] text-white p-4 sm:p-6">
      <div className="max-w-7xl mx-auto">

        <motion.div initial={{ opacity: 0, y: -20 }} animate={{ opacity: 1, y: 0 }} className="mb-8 rounded-md border border-emerald-900/70 bg-[#07110a] p-5 sm:p-6">
          <div className="flex flex-col gap-5 md:flex-row md:items-start md:justify-between">
            <div>
              <div className="font-mono text-xs uppercase tracking-[0.2em] text-emerald-300">External Sampling MCCFR</div>
              <h1 className="mb-2 font-display text-5xl font-semibold text-emerald-50">Texas Hold&apos;em GTO</h1>
              <p className="text-gray-400 max-w-2xl">
                MCCFR external sampling on Heads-Up No-Limit Hold&apos;em. 169-bucket preflop
                abstraction with suit isomorphism, using the same bucket numbering as the C++ engine.
              </p>
            </div>
            <div className="text-left text-xs text-gray-500 font-mono shrink-0 md:text-right">
              <div>DCFR · {strategy?.iterations?.toLocaleString() ?? '...'} iters</div>
              <div>External Sampling MCCFR</div>
              <div className="text-emerald-400 mt-1">
                {strategy?.num_infosets?.toLocaleString() ?? '...'} infosets
              </div>
            </div>
          </div>
        </motion.div>

        {/* Stats */}
        {stats && (
          <div className="grid grid-cols-2 gap-3 mb-8 lg:grid-cols-4">
            {[
              { label: 'Infosets',    value: stats.infosets.toLocaleString(),          color: 'text-emerald-400' },
              { label: 'Proxy Rate α',value: stats.rate.toFixed(3),                    color: 'text-blue-400' },
              { label: 'Avg Bet Freq',value: pct(stats.avgBetFreq),                    color: 'text-amber-400' },
              { label: 'Regret Proxy', value: stats.exploitability.toFixed(2) + ' bb',  color: 'text-red-400' },
            ].map(s => (
              <div key={s.label} className="surface-quiet rounded-md p-4 text-center">
                <div className={`text-2xl font-black font-mono ${s.color}`}>{s.value}</div>
                <div className="text-xs text-gray-500 mt-1">{s.label}</div>
              </div>
            ))}
          </div>
        )}

        <div className="grid lg:grid-cols-3 gap-8">

          {/* Range chart */}
          <div className="lg:col-span-2">
            <div className="surface rounded-md p-4 sm:p-6">
              <div className="flex flex-col gap-4 mb-4 lg:flex-row lg:items-center lg:justify-between">
                <h2 className="text-lg font-bold text-white">Pre-flop Range Chart</h2>

                {/* Action selector */}
                <div className="flex flex-wrap gap-2">
                  {ACTION_VIEWS.map(v => (
                    <button
                      key={v.id}
                      onClick={() => setView(v.id)}
                      className={`px-3 py-1 rounded text-xs font-mono transition-all ${
                        view === v.id
                          ? `bg-white/[0.08] ${v.color} ring-1 ring-white/10`
                          : 'text-gray-600 hover:text-gray-400'
                      }`}
                    >
                      {v.label}
                    </button>
                  ))}
                </div>
              </div>

              {loading ? (
                <div className="h-80 flex items-center justify-center text-gray-600">
                  Loading Hold&apos;em strategy...
                </div>
              ) : (
                <>
                  {/* Rank labels top */}
                  <div className="grid grid-cols-13 gap-0.5 mb-0.5" style={{ gridTemplateColumns: `auto repeat(13, 1fr)` }}>
                    <div /> {/* spacer */}
                    {HOLDEM_RANKS.map(r => (
                      <div key={r} className="text-center text-[10px] text-gray-600 font-mono">{r}</div>
                    ))}
                  </div>

                  {/* Matrix */}
                  <div className="space-y-0.5">
                    {HOLDEM_RANKS.map((rowRank, row) => (
                      <div key={row} className="grid gap-0.5" style={{ gridTemplateColumns: `auto repeat(13, 1fr)` }}>
                        {/* Row label */}
                        <div className="text-[10px] text-gray-600 font-mono w-4 flex items-center justify-end pr-1">
                          {rowRank}
                        </div>
                        {HOLDEM_RANKS.map((_, col) => {
                          const cell = matrix[row][col];
                          const isHovered = hoveredCell?.row === row && hoveredCell?.col === col;
                          const bg = cellColor(cell, view);
                          return (
                            <motion.div
                              key={col}
                              className="aspect-square rounded-[2px] cursor-pointer relative overflow-hidden"
                              style={{ backgroundColor: bg }}
                              whileHover={{ scale: 1.15, zIndex: 10 }}
                              onMouseEnter={() => setHoveredCell({ row, col })}
                              onMouseLeave={() => setHoveredCell(null)}
                            >
                              {isHovered && (
                                <div className="absolute inset-0 flex items-center justify-center">
                                  <span className="text-[6px] font-bold text-white/80">{cell.label}</span>
                                </div>
                              )}
                            </motion.div>
                          );
                        })}
                      </div>
                    ))}
                  </div>

                  {/* Legend */}
                  <div className="flex items-center gap-4 mt-4 text-xs text-gray-500">
                    <div className="flex items-center gap-1.5">
                      <div className="w-3 h-3 rounded bg-gray-800" /> Low freq
                    </div>
                    <div className="flex items-center gap-1.5">
                      <div className="w-3 h-3 rounded" style={{
                        backgroundColor: view === 'raise' ? '#22c55e' : view === 'call' ? '#3b82f6' : view === 'fold' ? '#ef4444' : '#f59e0b'
                      }} /> High freq
                    </div>
                  </div>
                </>
              )}
            </div>
          </div>

          {/* Right panel: hover details + GTO concepts */}
          <div className="space-y-4">

            {/* Hover details */}
            <div className="surface rounded-md p-5 min-h-40">
              <h3 className="font-bold text-white mb-3">Hand Details</h3>
              {hoveredData ? (
                <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }}>
                  <div className="text-2xl font-black text-white mb-4 font-mono">
                    {hoveredData.label}
                    <span className="text-sm text-gray-500 ml-2 font-normal">
                      {hoveredData.type === 'pair' ? 'Pocket pair' :
                       hoveredData.type === 'suited' ? 'Suited' : 'Offsuit'} · bucket {hoveredData.bucket}
                    </span>
                  </div>
                  <div className="mb-3 font-mono text-[11px] uppercase tracking-[0.14em] text-slate-500">
                    {hoveredData.key} · {hoveredData.source === 'root' ? 'root node' : `${hoveredData.nodes} sampled nodes`}
                  </div>
                  {[
                    { label: 'Raise / Bet', value: hoveredData.bet, color: '#22c55e' },
                    { label: 'Call / Check', value: hoveredData.call, color: '#3b82f6' },
                    { label: 'Fold',         value: hoveredData.fold, color: '#ef4444' },
                    { label: 'All-in',       value: hoveredData.allin, color: '#f59e0b' },
                  ].map(a => (
                    <div key={a.label} className="mb-2">
                      <div className="flex justify-between text-xs mb-0.5">
                        <span className="text-gray-400">{a.label}</span>
                        <span className="font-mono" style={{ color: a.color }}>{pct(a.value)}</span>
                      </div>
                      <div className="h-1.5 bg-gray-800 rounded overflow-hidden">
                        <motion.div
                          animate={{ width: `${a.value * 100}%` }}
                          className="h-full rounded"
                          style={{ backgroundColor: a.color + '99' }}
                        />
                      </div>
                    </div>
                  ))}
                </motion.div>
              ) : (
                <div className="text-gray-600 text-sm">Select a hand class in the matrix.</div>
              )}
            </div>

            {/* GTO Concepts */}
            <div className="surface rounded-md p-5">
              <h3 className="font-bold text-white mb-4">GTO Concepts</h3>
              <div className="space-y-4 text-xs">
                {[
                  {
                    title: 'Polarised Range',
                    desc: 'A balanced betting range mixes very strong hands (for value) with weak hands (bluffs), making the opponent indifferent to calling.',
                    color: 'text-emerald-400',
                  },
                  {
                    title: 'Bluff-to-Value Ratio',
                    desc: 'At Nash, the bluff frequency is calibrated so opponent\'s fold equity exactly offsets their calling EV. Pot odds determine the ratio.',
                    color: 'text-amber-400',
                  },
                  {
                    title: 'Suit Isomorphism',
                    desc: 'AKs has the same strategy regardless of specific suits. This abstraction reduces 1326 starting hands to 169 canonical classes.',
                    color: 'text-blue-400',
                  },
                  {
                    title: 'MCCFR External Sampling',
                    desc: 'Per iteration: sample one deal, traverse ALL actions for the traversing player, sample ONE action for the opponent. O(|A|^d/2) vs O(|A|^d).',
                    color: 'text-purple-400',
                  },
                ].map(c => (
                  <div key={c.title}>
                    <div className={`font-semibold ${c.color} mb-1`}>{c.title}</div>
                    <p className="text-gray-500 leading-relaxed">{c.desc}</p>
                  </div>
                ))}
              </div>
            </div>

            {/* Exploitability note */}
            <div className="bg-amber-500/5 border border-amber-500/20 rounded-md p-4 text-xs text-amber-400/80">
              <strong className="text-amber-400">Note:</strong> With only 1,000 MCCFR iterations,
              the strategy is still converging. Full HUNL requires millions of iterations with
              more sophisticated abstractions (e.g., the Libratus / Pluribus approach).
              This demonstrates the architecture and GTO concepts, not a production solver.
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
