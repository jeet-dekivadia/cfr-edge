// Data loading utilities for strategy JSON bundles.
// All data is statically imported at build time (no server needed).

import type { GameStrategy, MetaFile } from './types';

// Cache fetched strategies to avoid redundant loads
const cache: Record<string, GameStrategy> = {};

export async function loadStrategy(filename: string): Promise<GameStrategy> {
  if (cache[filename]) return cache[filename];
  const res = await fetch(`/strategies/${filename}`);
  if (!res.ok) throw new Error(`Failed to load ${filename}: ${res.status}`);
  const data = await res.json();
  cache[filename] = data as GameStrategy;
  return cache[filename];
}

export async function loadMeta(): Promise<MetaFile> {
  const res = await fetch('/strategies/meta.json');
  if (!res.ok) throw new Error('Failed to load meta.json');
  return res.json() as Promise<MetaFile>;
}

// Load all three algorithms for a game in parallel
export async function loadAllAlgos(
  game: 'kuhn' | 'leduc' | 'holdem',
): Promise<Record<string, GameStrategy>> {
  const algos = game === 'holdem'
    ? ['dcfr']
    : ['cfr', 'cfr_plus', 'dcfr'];

  const results = await Promise.all(
    algos.map(a => loadStrategy(`${game}_${a}.json`))
  );

  return Object.fromEntries(
    algos.map((a, i) => [a === 'cfr_plus' ? 'CFR+' : a.toUpperCase(), results[i]])
  );
}

// Interpolate strategy at an arbitrary iteration from available snapshots
export function getSnapshotAtIter(
  strategy: GameStrategy,
  targetIter: number,
): import('./types').StrategyMap {
  const snapKeys = Object.keys(strategy.strategy_snapshots)
    .map(Number)
    .sort((a, b) => a - b);

  if (snapKeys.length === 0) return strategy.final_strategy;

  // Find closest snapshot <= targetIter
  let best = snapKeys[0];
  for (const k of snapKeys) {
    if (k <= targetIter) best = k;
    else break;
  }
  return strategy.strategy_snapshots[String(best)] ?? strategy.final_strategy;
}

// Kuhn Poker game constants
export const KUHN_CARDS = ['J', 'Q', 'K'] as const;
export const KUHN_CARD_VALUES = { J: 0, Q: 1, K: 2 } as const;

// Infoset key builder for Kuhn Poker
export function kuhnInfoKey(card: 'J' | 'Q' | 'K', history: string): string {
  return `${card}:${history}`;
}

// Sample a Nash action for Kuhn poker
export function sampleNashAction(
  strategy: import('./types').StrategyMap,
  infoKey: string,
): number {
  const node = strategy[infoKey];
  if (!node) return 0;
  const r = Math.random();
  let cum = 0;
  for (let i = 0; i < node.probs.length; i++) {
    cum += node.probs[i];
    if (r <= cum) return i;
  }
  return node.probs.length - 1;
}

// Kuhn Poker payoff for player 0 (+/- big blinds)
export function kuhnPayoff(p0Card: string, p1Card: string, history: string): number {
  const p0 = KUHN_CARD_VALUES[p0Card as 'J'|'Q'|'K'] ?? 0;
  const p1 = KUHN_CARD_VALUES[p1Card as 'J'|'Q'|'K'] ?? 0;
  const p0Wins = p0 > p1;
  if (history === 'pp')  return p0Wins ?  1 : -1;
  if (history === 'bp')  return  1;
  if (history === 'bb')  return p0Wins ?  2 : -2;
  if (history === 'pbp') return -1;
  if (history === 'pbb') return p0Wins ?  2 : -2;
  return 0;
}

// Leduc infoset key builder
export function leducInfoKey(
  card: string,
  boardCard: string | null,
  history: string,
): string {
  return `${card}:${boardCard ?? '?'}:${history}`;
}

// Human-readable action names per game
export const ACTION_LABELS: Record<string, string[]> = {
  kuhn:   ['Pass', 'Bet'],
  leduc:  ['Fold', 'Check/Call', 'Bet/Raise'],
  holdem: ['Fold', 'Check/Call', 'Bet 33%', 'Bet 75%', 'Bet 100%', 'All-in'],
};

// Color map for action types
export const ACTION_COLORS: Record<string, string> = {
  Pass: '#94a3b8',
  Fold: '#ef4444',
  Bet: '#22c55e',
  'Check/Call': '#3b82f6',
  'Bet/Raise': '#22c55e',
  'Bet 33%': '#84cc16',
  'Bet 75%': '#f59e0b',
  'Bet 100%': '#f97316',
  'All-in': '#ef4444',
};

export const ALGO_COLORS: Record<string, string> = {
  CFR:    '#4e79a7',
  'CFR+': '#f28e2b',
  DCFR:   '#e15759',
  CFR_SOA:'#76b7b2',
};
