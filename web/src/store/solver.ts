import { create } from 'zustand';
import type { GameStrategy, GameType, AlgoType, StrategyMap } from '@/lib/types';
import { loadAllAlgos, getSnapshotAtIter } from '@/lib/data';

interface SolverState {
  // Loaded data
  strategies: Partial<Record<string, GameStrategy>>;
  loadingGame: GameType | null;
  error: string | null;

  // UI state
  selectedGame: GameType;
  selectedAlgo: AlgoType;
  currentIteration: number;   // scrubber position
  isPlaying: boolean;         // auto-advance animation
  playSpeed: number;          // iterations per tick

  // Derived: current displayed strategy (snapshot at currentIteration)
  currentStrategy: StrategyMap | null;

  // Actions
  loadGame: (game: GameType) => Promise<void>;
  setGame: (game: GameType) => void;
  setAlgo: (algo: AlgoType) => void;
  setIteration: (iter: number) => void;
  togglePlay: () => void;
  setPlaySpeed: (speed: number) => void;
  tick: () => void;
}

export const useSolverStore = create<SolverState>((set, get) => ({
  strategies:       {},
  loadingGame:      null,
  error:            null,
  selectedGame:     'kuhn',
  selectedAlgo:     'DCFR',
  currentIteration: 10000,
  isPlaying:        false,
  playSpeed:        200,
  currentStrategy:  null,

  loadGame: async (game) => {
    const key = `${game}_dcfr`;
    if (get().strategies[key]) return;  // already loaded

    set({ loadingGame: game, error: null });
    try {
      const all = await loadAllAlgos(game);
      const updates: Partial<Record<string, GameStrategy>> = {};
      for (const [algo, strat] of Object.entries(all)) {
        updates[`${game}_${algo.toLowerCase().replace('+','_plus')}`] = strat;
      }
      set(s => ({
        strategies: { ...s.strategies, ...updates },
        loadingGame: null,
      }));
      // Update current strategy
      get().setIteration(get().currentIteration);
    } catch (e) {
      set({ loadingGame: null, error: String(e) });
    }
  },

  setGame: (game) => {
    set({ selectedGame: game });
    get().loadGame(game);
  },

  setAlgo: (algo) => {
    set({ selectedAlgo: algo });
    get().setIteration(get().currentIteration);
  },

  setIteration: (iter) => {
    const { selectedGame, selectedAlgo, strategies } = get();
    const key = `${selectedGame}_${selectedAlgo.toLowerCase().replace('+','_plus')}`;
    const strat = strategies[key];
    const snap = strat ? getSnapshotAtIter(strat, iter) : null;
    set({ currentIteration: iter, currentStrategy: snap });
  },

  togglePlay: () => set(s => ({ isPlaying: !s.isPlaying })),

  setPlaySpeed: (speed) => set({ playSpeed: speed }),

  tick: () => {
    const { isPlaying, currentIteration, selectedGame, strategies, selectedAlgo } = get();
    if (!isPlaying) return;

    const key = `${selectedGame}_${selectedAlgo.toLowerCase().replace('+','_plus')}`;
    const strat = strategies[key];
    if (!strat) return;

    const maxIter = strat.iterations;
    const snapKeys = Object.keys(strat.strategy_snapshots).map(Number).sort((a,b)=>a-b);

    // Find next snapshot iteration
    const nextSnap = snapKeys.find(k => k > currentIteration);
    if (nextSnap) {
      get().setIteration(nextSnap);
    } else {
      // Reached end — stop
      get().setIteration(maxIter);
      set({ isPlaying: false });
    }
  },
}));
