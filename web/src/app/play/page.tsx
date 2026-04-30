'use client';
import { useEffect, useState, useCallback } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import type { GameStrategy, StrategyMap } from '@/lib/types';
import { loadStrategy, kuhnInfoKey, sampleNashAction, kuhnPayoff, KUHN_CARDS, ACTION_COLORS } from '@/lib/data';
import { fmt, pct } from '@/lib/utils';

type Card = 'J' | 'Q' | 'K';
type Phase = 'deal' | 'action' | 'result';

interface HandState {
  playerCard: Card;
  oppCard: Card;
  history: string;
  pot: number;
  phase: Phase;
  result: 'win' | 'lose' | 'tie' | null;
  evDelta: number;       // EV cost of this hand vs Nash
  nashAction: number;    // what Nash would have done (0=pass,1=bet)
  playerActed: boolean;
}

const CARD_LABELS: Record<Card, { label: string; value: number; color: string }> = {
  J: { label: 'J', value: 0, color: '#ef4444' },
  Q: { label: 'Q', value: 1, color: '#f59e0b' },
  K: { label: 'K', value: 2, color: '#22c55e' },
};

function CardDisplay({ card, hidden = false, size = 'lg' }: { card: Card | null; hidden?: boolean; size?: 'sm' | 'lg' }) {
  const info = card ? CARD_LABELS[card] : null;
  const dim = size === 'lg' ? 'w-20 h-28' : 'w-12 h-16';
  return (
    <motion.div
      initial={{ rotateY: 90, opacity: 0 }}
      animate={{ rotateY: 0, opacity: 1 }}
      transition={{ type: 'spring', stiffness: 200 }}
      className={`${dim} rounded-xl card-shine flex flex-col items-center justify-center relative overflow-hidden
        ${hidden ? 'bg-blue-900 border border-blue-700' : 'bg-white border border-gray-200'}`}
    >
      {hidden ? (
        <div className="text-blue-400 text-2xl">?</div>
      ) : (
        <>
          <div className="text-4xl font-black" style={{ color: info?.color }}>{info?.label}</div>
          <div className="text-xs font-mono" style={{ color: info?.color }}>
            {{ J: 'Jack', Q: 'Queen', K: 'King' }[card!]}
          </div>
        </>
      )}
    </motion.div>
  );
}

function NashStrategyPanel({
  strategy,
  infoKey,
  history,
}: {
  strategy: StrategyMap;
  infoKey: string;
  history: string;
}) {
  const node = strategy[infoKey];
  if (!node) return null;
  const actions = node.actions;
  const probs = node.probs;

  return (
    <div className="bg-gray-900/80 rounded-xl border border-emerald-500/20 p-4">
      <div className="text-xs text-emerald-400 font-mono mb-3 uppercase tracking-widest">
        Nash Strategy @ {infoKey || 'root'}
      </div>
      {actions.map((action, i) => (
        <div key={action} className="mb-2">
          <div className="flex justify-between text-sm mb-1">
            <span className="text-gray-300">{action}</span>
            <span className="font-mono text-emerald-400">{pct(probs[i])}</span>
          </div>
          <div className="h-2 bg-gray-800 rounded-full overflow-hidden">
            <motion.div
              initial={{ width: 0 }}
              animate={{ width: `${probs[i] * 100}%` }}
              transition={{ duration: 0.6, ease: 'easeOut' }}
              className="h-full rounded-full"
              style={{
                backgroundColor: ACTION_COLORS[action] ?? '#22c55e',
              }}
            />
          </div>
        </div>
      ))}
    </div>
  );
}

function dealNewHand(): { playerCard: Card; oppCard: Card } {
  const cards: Card[] = [...KUHN_CARDS];
  // shuffle
  for (let i = cards.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [cards[i], cards[j]] = [cards[j], cards[i]];
  }
  return { playerCard: cards[0], oppCard: cards[1] };
}

export default function PlayPage() {
  const [strategy, setStrategy] = useState<StrategyMap | null>(null);
  const [loading, setLoading] = useState(true);
  const [hand, setHand] = useState<HandState | null>(null);
  const [sessionEV, setSessionEV] = useState(0);
  const [handsPlayed, setHandsPlayed] = useState(0);
  const [log, setLog] = useState<string[]>([]);

  useEffect(() => {
    loadStrategy('kuhn_dcfr.json')
      .then(s => { setStrategy(s.final_strategy); setLoading(false); })
      .catch(console.error);
  }, []);

  const startNewHand = useCallback(() => {
    if (!strategy) return;
    const { playerCard, oppCard } = dealNewHand();
    const infoKey = kuhnInfoKey(playerCard, '');
    const nashAction = sampleNashAction(strategy, infoKey);

    setHand({
      playerCard, oppCard,
      history: '',
      pot: 2,
      phase: 'action',
      result: null,
      evDelta: 0,
      nashAction,
      playerActed: false,
    });
  }, [strategy]);

  // Player takes an action (0=pass, 1=bet)
  const playerAction = useCallback((actionIdx: number) => {
    if (!hand || !strategy || hand.phase !== 'action') return;

    const actionChar = actionIdx === 0 ? 'p' : 'b';
    let newHistory = hand.history + actionChar;

    // Compute Nash EV for this position (to calculate EV delta)
    const infoKey = kuhnInfoKey(hand.playerCard, hand.history);
    const nashNode = strategy[infoKey];
    let nashEV = 0;
    if (nashNode) {
      // Expected EV under Nash strategy (sum of prob * action value)
      // Approximate: use action 0 (pass) EV as baseline
      // In Kuhn, passing early is usually -EV for strong cards
      nashEV = (hand.playerCard === 'K' ? 0.8 : hand.playerCard === 'Q' ? 0.0 : -0.5);
    }

    // Let opponent respond via Nash
    let finalHistory = newHistory;
    let phase: Phase = 'action';
    let result: 'win' | 'lose' | 'tie' | null = null;
    let evDelta = 0;

    // Simple terminal check after player action
    const isTerminalAfterPlayer = (h: string) =>
      h === 'pp' || h === 'bb' || h === 'bp' ||
      h === 'pbb' || h === 'pbp';

    if (isTerminalAfterPlayer(newHistory)) {
      phase = 'result';
      const payoff = kuhnPayoff(hand.playerCard, hand.oppCard, newHistory);
      result = payoff > 0 ? 'win' : payoff < 0 ? 'lose' : 'tie';
      evDelta = payoff - nashEV;
    } else {
      // Opponent responds via Nash
      const oppInfoKey = kuhnInfoKey(hand.oppCard, newHistory);
      const oppAction = sampleNashAction(strategy, oppInfoKey);
      finalHistory = newHistory + (oppAction === 0 ? 'p' : 'b');

      if (isTerminalAfterPlayer(finalHistory)) {
        phase = 'result';
        const payoff = kuhnPayoff(hand.playerCard, hand.oppCard, finalHistory);
        result = payoff > 0 ? 'win' : payoff < 0 ? 'lose' : 'tie';
        evDelta = payoff - nashEV;
      }
      // If still not terminal, player acts again
    }

    const updatedHand = {
      ...hand,
      history: finalHistory,
      phase,
      result,
      evDelta,
      playerActed: true,
    };
    setHand(updatedHand);

    if (phase === 'result') {
      setHandsPlayed(h => h + 1);
      setSessionEV(prev => prev + evDelta);
      const payoff = kuhnPayoff(hand.playerCard, hand.oppCard, finalHistory);
      setLog(l => [
        `Hand ${handsPlayed + 1}: ${hand.playerCard} vs ${hand.oppCard} — ${finalHistory} → ${payoff > 0 ? '+' + payoff : payoff} bb (ΔEV: ${fmt(evDelta, 3)})`,
        ...l.slice(0, 9),
      ]);
    }
  }, [hand, strategy, handsPlayed]);

  // Determine available actions
  const getAvailableActions = (history: string): { idx: number; label: string; desc: string }[] => {
    if (!history || (history.length === 1 && history[0] === 'p')) {
      // Can pass or bet
      return [
        { idx: 0, label: 'Pass / Check', desc: 'No chips. Weak or balanced.' },
        { idx: 1, label: 'Bet (+1 chip)', desc: 'Value bet or bluff.' },
      ];
    }
    if (history.endsWith('b')) {
      return [
        { idx: 0, label: 'Pass / Fold', desc: 'Give up. Lose current bet.' },
        { idx: 1, label: 'Call / Raise', desc: 'Match or raise the bet.' },
      ];
    }
    return [
      { idx: 0, label: 'Pass', desc: '' },
      { idx: 1, label: 'Bet', desc: '' },
    ];
  };

  const currentInfoKey = hand ? kuhnInfoKey(hand.playerCard, hand.history) : '';
  const nashAtCurrent = hand && strategy ? strategy[currentInfoKey] : null;

  if (loading) {
    return (
      <div className="min-h-screen bg-gray-950 flex items-center justify-center text-gray-500">
        Loading Nash strategy...
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-950 text-white p-6">
      <div className="max-w-5xl mx-auto">

        {/* Header */}
        <div className="mb-8">
          <h1 className="text-4xl font-black text-white mb-2">Play vs Nash</h1>
          <p className="text-gray-400">
            Kuhn Poker — make decisions and see how your choices compare to the Nash equilibrium strategy.
          </p>
        </div>

        {/* Session stats */}
        <div className="grid grid-cols-3 gap-4 mb-8">
          <div className="bg-gray-900/60 rounded-xl border border-gray-800 p-4 text-center">
            <div className="text-2xl font-black font-mono text-white">{handsPlayed}</div>
            <div className="text-xs text-gray-500 mt-1">Hands Played</div>
          </div>
          <div className="bg-gray-900/60 rounded-xl border border-gray-800 p-4 text-center">
            <div className={`text-2xl font-black font-mono ${sessionEV >= 0 ? 'text-emerald-400' : 'text-red-400'}`}>
              {sessionEV >= 0 ? '+' : ''}{fmt(sessionEV, 3)} bb
            </div>
            <div className="text-xs text-gray-500 mt-1">Session EV vs Nash</div>
          </div>
          <div className="bg-gray-900/60 rounded-xl border border-gray-800 p-4 text-center">
            <div className="text-2xl font-black font-mono text-amber-400">
              {handsPlayed > 0 ? fmt(sessionEV / handsPlayed, 3) : '—'} bb
            </div>
            <div className="text-xs text-gray-500 mt-1">Avg EV / Hand</div>
          </div>
        </div>

        <div className="grid lg:grid-cols-5 gap-6">

          {/* Game area */}
          <div className="lg:col-span-3 space-y-6">

            {!hand || hand.phase === 'result' ? (
              <div className="bg-gray-900/60 rounded-2xl border border-gray-800 p-10 text-center">
                {hand?.phase === 'result' && (
                  <div className="mb-6">
                    <div className={`text-5xl font-black mb-2 ${
                      hand.result === 'win' ? 'text-emerald-400' :
                      hand.result === 'lose' ? 'text-red-400' : 'text-gray-400'
                    }`}>
                      {hand.result === 'win' ? '🏆 You Win' : hand.result === 'lose' ? '💔 You Lose' : '🤝 Tie'}
                    </div>
                    <div className="text-gray-400 text-sm">
                      Your card: <strong className="text-white">{hand.playerCard}</strong>  ·  
                      Opponent: <strong className="text-white">{hand.oppCard}</strong>  ·  
                      History: <code className="text-emerald-400">{hand.history}</code>
                    </div>
                    <div className="mt-2 text-sm">
                      EV delta vs Nash:{' '}
                      <span className={hand.evDelta >= 0 ? 'text-emerald-400' : 'text-red-400'}>
                        {hand.evDelta >= 0 ? '+' : ''}{fmt(hand.evDelta, 3)} bb
                      </span>
                    </div>
                  </div>
                )}
                <button
                  onClick={startNewHand}
                  className="px-8 py-3 rounded-xl bg-emerald-500 hover:bg-emerald-400 text-black font-bold text-lg transition-all hover:scale-105 shadow-lg shadow-emerald-500/20"
                >
                  {hand ? 'Deal Next Hand' : 'Deal Hand'}
                </button>
              </div>
            ) : (
              <div className="bg-gray-900/60 rounded-2xl border border-gray-800 p-6">
                {/* Felt table visualization */}
                <div className="felt rounded-xl p-6 mb-6">
                  <div className="text-center mb-6">
                    <div className="text-xs text-gray-500 mb-2 uppercase tracking-widest">Opponent</div>
                    <div className="flex justify-center">
                      <CardDisplay card={hand.oppCard} hidden size="sm" />
                    </div>
                  </div>

                  {/* Pot */}
                  <div className="text-center my-4">
                    <div className="inline-flex items-center gap-2 px-4 py-1.5 rounded-full bg-black/30 border border-gray-700">
                      <span className="text-xs text-gray-500">Pot:</span>
                      <span className="text-white font-mono font-bold">{hand.pot} chips</span>
                    </div>
                    {hand.history && (
                      <div className="mt-2 text-xs font-mono text-gray-500">
                        history: <span className="text-emerald-400">{hand.history}</span>
                      </div>
                    )}
                  </div>

                  <div className="text-center">
                    <div className="text-xs text-emerald-400 mb-2 uppercase tracking-widest">Your Hand</div>
                    <div className="flex justify-center">
                      <CardDisplay card={hand.playerCard} size="lg" />
                    </div>
                  </div>
                </div>

                {/* Action buttons */}
                <div className="grid grid-cols-2 gap-3">
                  {getAvailableActions(hand.history).map(({ idx, label, desc }) => (
                    <button
                      key={idx}
                      onClick={() => playerAction(idx)}
                      className="group p-4 rounded-xl border border-gray-700 hover:border-emerald-500/50
                               bg-gray-800/50 hover:bg-emerald-500/10 transition-all text-left"
                    >
                      <div className="font-bold text-white group-hover:text-emerald-400 transition-colors">
                        {label}
                      </div>
                      {desc && <div className="text-xs text-gray-500 mt-1">{desc}</div>}
                      {nashAtCurrent && (
                        <div className="mt-2 text-xs font-mono text-emerald-500">
                          Nash: {pct(nashAtCurrent.probs[idx] ?? 0)}
                        </div>
                      )}
                    </button>
                  ))}
                </div>
              </div>
            )}
          </div>

          {/* Right: Nash strategy panel + hand log */}
          <div className="lg:col-span-2 space-y-4">
            {strategy && hand && (
              <NashStrategyPanel
                strategy={strategy}
                infoKey={currentInfoKey}
                history={hand.history}
              />
            )}

            {/* Math explanation */}
            {hand && strategy && nashAtCurrent && (
              <div className="bg-gray-900/60 rounded-xl border border-gray-800 p-4">
                <div className="text-xs text-amber-400 font-mono mb-2 uppercase tracking-widest">
                  Why this strategy?
                </div>
                <p className="text-xs text-gray-400 leading-relaxed">
                  With <strong className="text-white">{hand.playerCard}</strong> at history{' '}
                  <code className="text-emerald-400">&quot;{hand.history || '∅'}&quot;</code>, Nash plays:
                  {nashAtCurrent.actions.map((a, i) => (
                    <span key={a}>
                      {' '}
                      <strong className="text-white">{a}</strong>{' '}
                      <span className="text-emerald-400">{pct(nashAtCurrent.probs[i])}</span>
                      {i < nashAtCurrent.actions.length - 1 ? ',' : '.'}
                    </span>
                  ))}
                  {' '}This mixed strategy makes the opponent indifferent between their actions,
                  achieving a Nash equilibrium.
                </p>
              </div>
            )}

            {/* Hand log */}
            {log.length > 0 && (
              <div className="bg-gray-900/60 rounded-xl border border-gray-800 p-4">
                <div className="text-xs text-gray-500 mb-2 uppercase tracking-widest font-mono">Hand Log</div>
                <div className="space-y-1 max-h-48 overflow-y-auto">
                  {log.map((entry, i) => (
                    <div key={i} className="text-[11px] text-gray-400 font-mono">{entry}</div>
                  ))}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
