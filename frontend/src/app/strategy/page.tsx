'use client';
import { useEffect, useState, useMemo } from 'react';
import { motion } from 'framer-motion';
import type { GameStrategy, GameType, StrategyMap } from '@/lib/types';
import { loadAllAlgos, ACTION_LABELS } from '@/lib/data';
import { pct, fmt } from '@/lib/utils';

const GAMES: { id: GameType; label: string }[] = [
  { id: 'kuhn',   label: 'Kuhn Poker'   },
  { id: 'leduc',  label: 'Leduc Hold\'em' },
];

function exportCSV(strategy: StrategyMap, game: GameType) {
  const actions = ACTION_LABELS[game] ?? [];
  const header = ['infoset', ...actions, 'max_regret'].join(',');
  const rows = Object.entries(strategy).map(([key, node]) => {
    const probCols = node.probs.map(p => p.toFixed(6));
    const maxRegret = Math.max(...node.regrets.map(Math.abs)).toFixed(6);
    return [key, ...probCols, maxRegret].join(',');
  });
  const csv = [header, ...rows].join('\n');
  const blob = new Blob([csv], { type: 'text/csv' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a'); a.href = url; a.download = `${game}_strategy.csv`;
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

export default function StrategyPage() {
  const [allStrats, setAllStrats] = useState<Record<string, GameStrategy>>({});
  const [game, setGame] = useState<GameType>('kuhn');
  const [algo, setAlgo] = useState('DCFR');
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState('');
  const [sortBy, setSortBy] = useState<'key' | 'regret' | 'entropy'>('key');

  useEffect(() => {
    setLoading(true);
    loadAllAlgos(game).then(s => {
      setAllStrats(s);
      setLoading(false);
    });
  }, [game]);

  const currentStrat = allStrats[algo]?.final_strategy ?? {};
  const actions = ACTION_LABELS[game] ?? [];

  // Compute entropy for each infoset (measures how mixed the strategy is)
  const entropy = (probs: number[]) => {
    return -probs.reduce((s, p) => s + (p > 1e-9 ? p * Math.log2(p) : 0), 0);
  };

  const rows = useMemo(() => {
    return Object.entries(currentStrat)
      .filter(([k]) => filter === '' || k.toLowerCase().includes(filter.toLowerCase()))
      .sort(([ka, a], [kb, b]) => {
        if (sortBy === 'key') return ka.localeCompare(kb);
        if (sortBy === 'regret') {
          return Math.max(...b.regrets.map(Math.abs)) - Math.max(...a.regrets.map(Math.abs));
        }
        return entropy(b.probs) - entropy(a.probs);
      });
  }, [currentStrat, filter, sortBy]);

  return (
    <div className="min-h-screen bg-gray-950 text-white p-6">
      <div className="max-w-7xl mx-auto">

        <motion.div initial={{ opacity: 0, y: -20 }} animate={{ opacity: 1, y: 0 }} className="mb-8">
          <h1 className="text-4xl font-black text-white mb-2">Strategy Explorer</h1>
          <p className="text-gray-400">
            Browse every information set and its Nash equilibrium action probabilities.
          </p>
        </motion.div>

        {/* Controls */}
        <div className="flex flex-wrap gap-4 mb-6 items-center">
          <div className="flex gap-2">
            {GAMES.map(g => (
              <button key={g.id} onClick={() => setGame(g.id)}
                className={`px-4 py-2 rounded-lg text-sm font-medium transition-all ${
                  game === g.id ? 'bg-emerald-500/20 text-emerald-400 ring-1 ring-emerald-500/30' : 'bg-gray-800 text-gray-400 hover:text-white'
                }`}
              >{g.label}</button>
            ))}
          </div>

          <div className="flex gap-2">
            {['CFR', 'CFR+', 'DCFR'].map(a => (
              <button key={a} onClick={() => setAlgo(a)}
                className={`px-3 py-2 rounded-lg text-xs font-mono transition-all ${
                  algo === a ? 'bg-blue-500/20 text-blue-400 ring-1 ring-blue-500/30' : 'bg-gray-800 text-gray-400 hover:text-white'
                }`}
              >{a}</button>
            ))}
          </div>

          <input
            type="text"
            placeholder="Filter infosets..."
            value={filter}
            onChange={e => setFilter(e.target.value)}
            className="px-3 py-2 rounded-lg bg-gray-800 text-sm text-gray-300 placeholder-gray-600 border border-gray-700 focus:outline-none focus:border-emerald-500/50 w-48"
          />

          <select
            value={sortBy}
            onChange={e => setSortBy(e.target.value as 'key'|'regret'|'entropy')}
            className="px-3 py-2 rounded-lg bg-gray-800 text-sm text-gray-300 border border-gray-700 focus:outline-none"
          >
            <option value="key">Sort: Key</option>
            <option value="regret">Sort: Max Regret ↓</option>
            <option value="entropy">Sort: Entropy ↓</option>
          </select>

          <button
            onClick={() => exportCSV(currentStrat, game)}
            className="px-4 py-2 rounded-lg text-xs font-mono border border-gray-700 text-gray-400 hover:text-white hover:border-emerald-500/40 transition-all ml-auto"
          >
            ↓ Export CSV
          </button>
        </div>

        {/* Stats bar */}
        {allStrats[algo] && (
          <div className="grid grid-cols-4 gap-3 mb-6">
            {[
              { label: 'Infosets', value: allStrats[algo].num_infosets.toLocaleString() },
              { label: 'Final Expl.', value: fmt(allStrats[algo].final_exploitability) },
              { label: 'Conv. Rate α', value: allStrats[algo].convergence_rate.toFixed(3) },
              { label: 'Solve time', value: `${allStrats[algo].elapsed_seconds?.toFixed(1) ?? '—'}s` },
            ].map(s => (
              <div key={s.label} className="bg-gray-900/60 rounded-xl border border-gray-800 p-3 text-center">
                <div className="text-lg font-black text-emerald-400 font-mono">{s.value}</div>
                <div className="text-xs text-gray-500 mt-0.5">{s.label}</div>
              </div>
            ))}
          </div>
        )}

        {/* Table */}
        <div className="bg-gray-900/60 rounded-2xl border border-gray-800 overflow-hidden">
          {loading ? (
            <div className="p-10 text-center text-gray-600">Loading strategies...</div>
          ) : (
            <div className="overflow-x-auto">
              <table className="w-full text-sm">
                <thead className="border-b border-gray-800">
                  <tr>
                    <th className="text-left py-3 px-4 text-gray-500 font-medium">Infoset</th>
                    {actions.map(a => (
                      <th key={a} className="py-3 px-4 text-gray-500 font-medium text-center">{a}</th>
                    ))}
                    <th className="py-3 px-4 text-gray-500 font-medium text-center">Entropy</th>
                    <th className="py-3 px-4 text-gray-500 font-medium text-center">Max |Regret|</th>
                  </tr>
                </thead>
                <tbody>
                  {rows.map(([key, node], i) => {
                    const H = entropy(node.probs);
                    const maxR = Math.max(...node.regrets.map(Math.abs));
                    return (
                      <motion.tr
                        key={key}
                        initial={{ opacity: 0 }}
                        animate={{ opacity: 1 }}
                        transition={{ delay: Math.min(i * 0.01, 0.3) }}
                        className="border-b border-gray-900 hover:bg-gray-800/30 transition-colors"
                      >
                        <td className="py-2 px-4 font-mono text-xs text-gray-300">{key}</td>
                        {node.probs.map((p, ai) => (
                          <td key={ai} className="py-2 px-4 text-center">
                            <div className="relative h-5 flex items-center justify-center">
                              <div
                                className="absolute inset-0 rounded opacity-25"
                                style={{ backgroundColor: p > 0.5 ? '#10b981' : '#475569', width: `${p*100}%` }}
                              />
                              <span className="relative text-xs font-mono text-gray-200">{pct(p)}</span>
                            </div>
                          </td>
                        ))}
                        <td className="py-2 px-4 text-center">
                          <span className="text-xs font-mono text-blue-400">{H.toFixed(3)}</span>
                        </td>
                        <td className="py-2 px-4 text-center">
                          <span className={`text-xs font-mono ${maxR > 0.1 ? 'text-amber-400' : 'text-gray-600'}`}>
                            {fmt(maxR, 4)}
                          </span>
                        </td>
                      </motion.tr>
                    );
                  })}
                </tbody>
              </table>
              <div className="px-4 py-3 text-xs text-gray-600 border-t border-gray-900">
                Showing {rows.length} infosets {filter && `matching "${filter}"`}
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
