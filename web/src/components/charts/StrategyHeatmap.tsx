'use client';
import { useMemo } from 'react';
import { motion } from 'framer-motion';
import type { StrategyMap } from '@/lib/types';
import { probColor, pct, fmt } from '@/lib/utils';

interface Props {
  strategy: StrategyMap;
  game: 'kuhn' | 'leduc' | 'holdem';
  maxRows?: number;
}

const GAME_ACTIONS: Record<string, string[]> = {
  kuhn:   ['Pass', 'Bet'],
  leduc:  ['Fold', 'Chk/Call', 'Bet/Raise'],
  holdem: ['Fold', 'Chk/Call', 'Bet33', 'Bet75', 'Bet100', 'Allin'],
};

export default function StrategyHeatmap({ strategy, game, maxRows = 50 }: Props) {
  const rows = useMemo(() => {
    const entries = Object.entries(strategy)
      .slice(0, maxRows)
      .sort(([a], [b]) => a.localeCompare(b));
    return entries;
  }, [strategy, maxRows]);

  const actions = GAME_ACTIONS[game] ?? ['A0', 'A1'];

  return (
    <div className="overflow-auto">
      <table className="w-full text-xs font-mono border-collapse">
        <thead>
          <tr className="border-b border-gray-800">
            <th className="text-left py-2 px-3 text-gray-500 font-medium w-40">Infoset</th>
            {actions.map(a => (
              <th key={a} className="py-2 px-2 text-gray-500 font-medium">{a}</th>
            ))}
            <th className="py-2 px-2 text-gray-500 font-medium">Regret</th>
          </tr>
        </thead>
        <tbody>
          {rows.map(([key, node], rowIdx) => {
            const maxRegret = Math.max(...node.regrets.map(Math.abs), 1e-9);
            return (
              <motion.tr
                key={key}
                initial={{ opacity: 0, x: -10 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: rowIdx * 0.015 }}
                className="border-b border-gray-900 hover:bg-gray-900/50 transition-colors group"
              >
                {/* Infoset key */}
                <td className="py-1.5 px-3 text-gray-400 truncate max-w-[160px] group-hover:text-white transition-colors">
                  {key}
                </td>

                {/* Action probability cells */}
                {actions.map((_, i) => {
                  const prob = node.probs[i] ?? 0;
                  const bg = probColor(prob);
                  return (
                    <td key={i} className="py-1 px-2 text-center relative" title={`${pct(prob)}`}>
                      {/* Background fill bar */}
                      <div
                        className="absolute bottom-0 left-0 right-0 rounded-sm opacity-30 prob-bar"
                        style={{
                          height: `${prob * 100}%`,
                          backgroundColor: bg,
                        }}
                      />
                      <span
                        className="relative z-10 font-mono text-xs"
                        style={{ color: prob > 0.5 ? '#a7f3d0' : '#64748b' }}
                      >
                        {pct(prob)}
                      </span>
                    </td>
                  );
                })}

                {/* Max absolute regret */}
                <td className="py-1 px-2 text-center">
                  <span className="text-amber-500/70 text-[10px]">
                    {fmt(maxRegret, 3)}
                  </span>
                </td>
              </motion.tr>
            );
          })}
        </tbody>
      </table>

      {Object.keys(strategy).length > maxRows && (
        <div className="text-center py-3 text-gray-600 text-xs">
          Showing {maxRows} of {Object.keys(strategy).length} infosets
        </div>
      )}
    </div>
  );
}
