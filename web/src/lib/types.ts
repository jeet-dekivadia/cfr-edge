// Core data types mirroring the C++ JSON output schema.

export type GameType = 'kuhn' | 'leduc' | 'holdem';
export type AlgoType = 'CFR' | 'CFR+' | 'DCFR';

export interface ConvergencePoint {
  iter: number;
  exploitability: number;
}

export interface ActionDist {
  actions: string[];
  probs: number[];
  regrets: number[];
}

export type StrategyMap = Record<string, ActionDist>;

export interface StrategySnapshot {
  [iter: string]: StrategyMap;
}

export interface GameStrategy {
  game: GameType;
  algorithm: AlgoType;
  iterations: number;
  convergence_rate: number;
  convergence: ConvergencePoint[];
  strategy_snapshots: StrategySnapshot;
  final_strategy: StrategyMap;
  num_infosets: number;
  elapsed_seconds: number;
  final_exploitability: number;
  // Holdem-specific
  preflop_summary?: Record<string, Record<string, number>>;
}

export interface MetaFile {
  version: string;
  generated_at: string;
  games: {
    game: GameType;
    description: string;
    nash_exploitability?: number;
    files: { file: string; algorithm: AlgoType; final_exploitability: number }[];
  }[];
}

// For the /play game mode
export interface KuhnGameState {
  playerCard: 'J' | 'Q' | 'K';
  opponentCard: 'J' | 'Q' | 'K';
  history: string;
  pot: number;
  playerTurn: boolean;
  phase: 'deal' | 'action' | 'showdown';
  result: 'win' | 'lose' | 'tie' | null;
  evDelta: number;       // EV lost vs Nash for this hand
  sessionEV: number;     // cumulative EV delta this session
  handsPlayed: number;
}

export interface LeducGameState {
  playerCard: string;  // "J", "Q", "K"
  opponentCard: string;
  boardCard: string | null;
  round: number;
  history: string;
  pot: number;
  playerTurn: boolean;
  phase: 'deal' | 'action' | 'showdown';
  result: 'win' | 'lose' | 'tie' | null;
  evDelta: number;
  sessionEV: number;
  handsPlayed: number;
}

// Nash strategy lookup for a given infoset
export function getNashStrategy(
  strategy: StrategyMap,
  infosetKey: string,
): ActionDist | null {
  return strategy[infosetKey] ?? null;
}

// EV of each action given the Nash strategy at an infoset
export function getActionEVs(
  strategy: StrategyMap,
  infosetKey: string,
): { action: string; prob: number; ev: number }[] {
  const node = strategy[infosetKey];
  if (!node) return [];
  return node.actions.map((action, i) => ({
    action,
    prob: node.probs[i],
    ev: node.regrets[i],  // regret as proxy for EV advantage
  }));
}
