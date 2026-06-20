// Data loading utilities for strategy JSON bundles.
// All data is statically imported at build time (no server needed).

import type { GameStrategy, MetaFile } from './types';

// Cache fetched strategies to avoid redundant loads
const cache: Record<string, GameStrategy> = {};

const SAFE_FILENAME = /^[a-z0-9_]+\.json$/;

export async function loadStrategy(filename: string): Promise<GameStrategy> {
  if (!SAFE_FILENAME.test(filename)) {
    throw new Error(`Invalid strategy filename: ${filename}`);
  }
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
export type KuhnCard = typeof KUHN_CARDS[number];

// Infoset key builder for Kuhn Poker
export function kuhnInfoKey(card: KuhnCard, history: string): string {
  return `${card}:${history}`;
}

// Sample a Nash action for Kuhn poker
export function sampleNashAction(
  strategy: import('./types').StrategyMap,
  infoKey: string,
): number {
  const node = strategy[infoKey];
  if (!node) {
    console.warn(`sampleNashAction: no strategy found for infoset '${infoKey}', defaulting to action 0`);
    return 0;
  }
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
  const p0 = KUHN_CARD_VALUES[p0Card as 'J'|'Q'|'K'];
  const p1 = KUHN_CARD_VALUES[p1Card as 'J'|'Q'|'K'];
  if (p0 === undefined || p1 === undefined) {
    throw new Error(`kuhnPayoff: invalid card(s) p0='${p0Card}', p1='${p1Card}'`);
  }
  const p0Wins = p0 > p1;
  if (history === 'pp')  return p0Wins ?  1 : -1;
  if (history === 'bp')  return  1;
  if (history === 'bb')  return p0Wins ?  2 : -2;
  if (history === 'pbp') return -1;
  if (history === 'pbb') return p0Wins ?  2 : -2;
  throw new Error(`kuhnPayoff: unrecognized terminal history '${history}'`);
}

export function isKuhnTerminal(history: string): boolean {
  return history === 'pp' || history === 'bp' || history === 'bb' ||
    history === 'pbp' || history === 'pbb';
}

export function kuhnPot(history: string): number {
  return 2 + history.split('').filter(c => c === 'b').length;
}

export function kuhnHistoryLabel(history: string): string {
  if (!history) return 'deal';
  const labels: Record<string, string> = {
    p: 'check',
    b: 'bet',
    pp: 'check / check',
    bp: 'bet / fold',
    bb: 'bet / call',
    pb: 'check / bet',
    pbp: 'check / bet / fold',
    pbb: 'check / bet / call',
  };
  return labels[history] ?? history;
}

export function kuhnExpectedValue(
  strategy: import('./types').StrategyMap,
  p0Card: KuhnCard,
  p1Card: KuhnCard,
  history = '',
  forcedRootAction?: number,
): number {
  if (isKuhnTerminal(history)) return kuhnPayoff(p0Card, p1Card, history);

  const player = history.length % 2;
  const card = player === 0 ? p0Card : p1Card;
  const infoKey = kuhnInfoKey(card, history);
  const node = strategy[infoKey];
  const probs = node?.probs ?? (() => {
    console.warn(`kuhnExpectedValue: no strategy node for '${infoKey}', using uniform`);
    return [0.5, 0.5];
  })();
  const actionChars = ['p', 'b'];

  if (forcedRootAction != null && player === 0) {
    return kuhnExpectedValue(
      strategy,
      p0Card,
      p1Card,
      history + actionChars[forcedRootAction],
    );
  }

  return probs.reduce((sum, prob, actionIdx) => (
    sum + prob * kuhnExpectedValue(
      strategy,
      p0Card,
      p1Card,
      history + actionChars[actionIdx],
    )
  ), 0);
}

export function kuhnActionValues(
  strategy: import('./types').StrategyMap,
  playerCard: KuhnCard,
  history: string,
): { actionValues: number[]; nashValue: number; actionDelta: number[] } {
  const possibleOppCards = KUHN_CARDS.filter(c => c !== playerCard);
  const actionValues = [0, 1].map(actionIdx => (
    possibleOppCards.reduce((sum, oppCard) => (
      sum + kuhnExpectedValue(strategy, playerCard, oppCard, history, actionIdx)
    ), 0) / possibleOppCards.length
  ));

  const actionInfoKey = kuhnInfoKey(playerCard, history);
  const node = strategy[actionInfoKey];
  const probs = node?.probs ?? (() => {
    console.warn(`kuhnActionValues: no strategy node for '${actionInfoKey}', using uniform`);
    return [0.5, 0.5];
  })();
  const nashValue = actionValues.reduce((sum, value, i) => sum + value * (probs[i] ?? 0), 0);

  return {
    actionValues,
    nashValue,
    actionDelta: actionValues.map(value => value - nashValue),
  };
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
  kuhn:   ['pass', 'bet'],
  leduc:  ['fold', 'check/call', 'bet/raise'],
  holdem: ['fold', 'check/call', 'bet33', 'bet75', 'bet100', 'allin'],
};

// Color map for action types
export const ACTION_COLORS: Record<string, string> = {
  pass: '#94a3b8',
  fold: '#ef4444',
  bet: '#22c55e',
  'check/call': '#3b82f6',
  'bet/raise': '#22c55e',
  bet33: '#84cc16',
  bet75: '#f59e0b',
  bet100: '#f97316',
  allin: '#ef4444',
};

export const ALGO_COLORS: Record<string, string> = {
  CFR:    '#6ee7b7',
  'CFR+': '#22c55e',
  DCFR:   '#a3e635',
  CFR_SOA:'#34d399',
};

export function actionDisplay(action: string): string {
  const labels: Record<string, string> = {
    pass: 'Pass',
    fold: 'Fold',
    bet: 'Bet',
    'check/call': 'Check / Call',
    'bet/raise': 'Bet / Raise',
    bet33: 'Bet 33%',
    bet75: 'Bet 75%',
    bet100: 'Bet 100%',
    allin: 'All-in',
  };
  return labels[action] ?? action;
}

export const HOLDEM_RANKS = ['A','K','Q','J','T','9','8','7','6','5','4','3','2'] as const;

function rankValue(rank: string): number {
  return 12 - HOLDEM_RANKS.indexOf(rank as typeof HOLDEM_RANKS[number]);
}

export function holdemBucketForHand(label: string): number {
  const first = label[0];
  const second = label[1];
  const high = Math.max(rankValue(first), rankValue(second));
  const low = Math.min(rankValue(first), rankValue(second));

  if (first === second) return 12 - high;

  let idx = 0;
  for (let h = 12; h > high; h--) idx += h;
  idx += high - 1 - low;

  return label.endsWith('s') ? 13 + idx : 91 + idx;
}

export function holdemHandForMatrixCell(row: number, col: number): {
  label: string;
  type: 'pair' | 'suited' | 'offsuit';
  bucket: number;
} {
  if (row === col) {
    const label = HOLDEM_RANKS[row] + HOLDEM_RANKS[col];
    return { label, type: 'pair', bucket: holdemBucketForHand(label) };
  }

  if (col > row) {
    const label = HOLDEM_RANKS[row] + HOLDEM_RANKS[col] + 's';
    return { label, type: 'suited', bucket: holdemBucketForHand(label) };
  }

  const label = HOLDEM_RANKS[col] + HOLDEM_RANKS[row] + 'o';
  return { label, type: 'offsuit', bucket: holdemBucketForHand(label) };
}
