'use client';

import { useCallback, useEffect, useMemo, useState } from 'react';
import { motion } from 'framer-motion';
import { BrainCircuit, CircleDollarSign, History, RotateCcw, ShieldCheck } from 'lucide-react';
import type { StrategyMap } from '@/lib/types';
import {
  ACTION_COLORS,
  KUHN_CARDS,
  actionDisplay,
  isKuhnTerminal,
  kuhnActionValues,
  kuhnHistoryLabel,
  kuhnInfoKey,
  kuhnPayoff,
  kuhnPot,
  loadStrategy,
  sampleNashAction,
  type KuhnCard,
} from '@/lib/data';
import { fmt, pct } from '@/lib/utils';

type Phase = 'action' | 'result';

interface HandState {
  playerCard: KuhnCard;
  oppCard: KuhnCard;
  history: string;
  phase: Phase;
  result: 'win' | 'lose' | 'tie' | null;
  evDelta: number;
}

const CARD_META: Record<KuhnCard, { name: string; color: string }> = {
  J: { name: 'Jack', color: '#ef4444' },
  Q: { name: 'Queen', color: '#f59e0b' },
  K: { name: 'King', color: '#22c55e' },
};

function dealNewHand(): { playerCard: KuhnCard; oppCard: KuhnCard } {
  const cards: KuhnCard[] = [...KUHN_CARDS];
  for (let i = cards.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [cards[i], cards[j]] = [cards[j], cards[i]];
  }
  return { playerCard: cards[0], oppCard: cards[1] };
}

function CardDisplay({ card, hidden = false, size = 'lg' }: { card: KuhnCard; hidden?: boolean; size?: 'sm' | 'lg' }) {
  const dim = size === 'lg' ? 'h-28 w-20' : 'h-20 w-14';
  const meta = CARD_META[card];
  return (
    <motion.div
      initial={{ rotateY: 80, opacity: 0 }}
      animate={{ rotateY: 0, opacity: 1 }}
      transition={{ type: 'spring', stiffness: 190, damping: 18 }}
      className={`${dim} card-shine relative flex flex-col items-center justify-center overflow-hidden rounded-lg border ${
        hidden ? 'border-blue-700 bg-blue-950' : 'border-slate-200 bg-white'
      }`}
    >
      {hidden ? (
        <div className="h-9 w-9 rounded-md border border-blue-300/30 bg-blue-300/10" />
      ) : (
        <>
          <div className="text-4xl font-black" style={{ color: meta.color }}>{card}</div>
          <div className="mt-1 text-[10px] font-mono uppercase tracking-[0.14em]" style={{ color: meta.color }}>
            {meta.name}
          </div>
        </>
      )}
    </motion.div>
  );
}

function availableActions(history: string) {
  if (!history || history === 'p') {
    return [
      { idx: 0, label: history ? 'Check back' : 'Pass / Check', detail: 'No bet added' },
      { idx: 1, label: 'Bet', detail: '+1 chip' },
    ];
  }
  return [
    { idx: 0, label: 'Fold', detail: 'Decline the bet' },
    { idx: 1, label: 'Call', detail: '+1 chip' },
  ];
}

function finishHand(hand: HandState, history: string, evDelta: number): HandState {
  const payoff = kuhnPayoff(hand.playerCard, hand.oppCard, history);
  return {
    ...hand,
    history,
    phase: 'result',
    result: payoff > 0 ? 'win' : payoff < 0 ? 'lose' : 'tie',
    evDelta,
  };
}

function NashPanel({ strategy, hand }: { strategy: StrategyMap; hand: HandState }) {
  const infoKey = kuhnInfoKey(hand.playerCard, hand.history);
  const node = strategy[infoKey];
  const values = kuhnActionValues(strategy, hand.playerCard, hand.history);
  if (!node || hand.phase !== 'action') return null;

  return (
    <div className="surface rounded-md p-5">
      <div className="mb-4 flex items-center gap-2">
        <BrainCircuit size={18} className="text-emerald-300" />
        <div>
          <div className="text-sm font-bold text-white">Nash Strategy</div>
          <div className="font-mono text-xs text-slate-500">{infoKey}</div>
        </div>
      </div>

      <div className="space-y-3">
        {node.actions.map((action, i) => (
          <div key={action}>
            <div className="mb-1 flex items-center justify-between text-sm">
              <span className="text-slate-300">{actionDisplay(action)}</span>
              <span className="font-mono text-emerald-300">{pct(node.probs[i] ?? 0)}</span>
            </div>
            <div className="h-2 overflow-hidden rounded-full bg-black/30">
              <motion.div
                initial={{ width: 0 }}
                animate={{ width: `${(node.probs[i] ?? 0) * 100}%` }}
                className="h-full rounded-full"
                style={{ backgroundColor: ACTION_COLORS[action] ?? '#22c55e' }}
              />
            </div>
            <div className="mt-1 flex justify-between font-mono text-[11px] text-slate-500">
              <span>EV {fmt(values.actionValues[i] ?? 0, 3)} bb</span>
              <span className={(values.actionDelta[i] ?? 0) >= 0 ? 'text-emerald-300' : 'text-red-300'}>
                {values.actionDelta[i] >= 0 ? '+' : ''}{fmt(values.actionDelta[i] ?? 0, 3)}
              </span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
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
      .then(s => {
        setStrategy(s.final_strategy);
        setLoading(false);
      });
  }, []);

  const startNewHand = useCallback(() => {
    const dealt = dealNewHand();
    setHand({
      ...dealt,
      history: '',
      phase: 'action',
      result: null,
      evDelta: 0,
    });
  }, []);

  const currentDecision = useMemo(() => {
    if (!strategy || !hand || hand.phase !== 'action') return null;
    return kuhnActionValues(strategy, hand.playerCard, hand.history);
  }, [strategy, hand]);

  const playerAction = useCallback((actionIdx: number) => {
    if (!hand || !strategy || hand.phase !== 'action') return;

    const decision = kuhnActionValues(strategy, hand.playerCard, hand.history);
    const actionChar = actionIdx === 0 ? 'p' : 'b';
    const afterPlayer = hand.history + actionChar;
    const nextEvDelta = hand.evDelta + (decision.actionDelta[actionIdx] ?? 0);

    let updated: HandState;
    if (isKuhnTerminal(afterPlayer)) {
      updated = finishHand(hand, afterPlayer, nextEvDelta);
    } else {
      const oppInfoKey = kuhnInfoKey(hand.oppCard, afterPlayer);
      const oppAction = sampleNashAction(strategy, oppInfoKey);
      const afterOpponent = afterPlayer + (oppAction === 0 ? 'p' : 'b');
      updated = isKuhnTerminal(afterOpponent)
        ? finishHand(hand, afterOpponent, nextEvDelta)
        : { ...hand, history: afterOpponent, evDelta: nextEvDelta };
    }

    setHand(updated);

    if (updated.phase === 'result') {
      const payoff = kuhnPayoff(updated.playerCard, updated.oppCard, updated.history);
      setHandsPlayed(count => count + 1);
      setSessionEV(total => total + updated.evDelta);
      setLog(entries => [
        `Hand ${handsPlayed + 1}: ${updated.playerCard} vs ${updated.oppCard}, ${kuhnHistoryLabel(updated.history)}, payoff ${payoff >= 0 ? '+' : ''}${payoff} bb, EV ${updated.evDelta >= 0 ? '+' : ''}${fmt(updated.evDelta, 3)}`,
        ...entries.slice(0, 9),
      ]);
    }
  }, [hand, handsPlayed, strategy]);

  if (loading) {
    return (
      <div className="min-h-screen bg-[#050806] p-6 text-slate-500">
        Loading Kuhn DCFR strategy...
      </div>
    );
  }

  if (!strategy) return null;

  const currentInfoKey = hand ? kuhnInfoKey(hand.playerCard, hand.history) : '';
  const currentNode = currentInfoKey ? strategy[currentInfoKey] : null;
  const payoff = hand?.phase === 'result' ? kuhnPayoff(hand.playerCard, hand.oppCard, hand.history) : null;

  return (
    <div className="min-h-screen bg-[#050806] p-4 text-white sm:p-6">
      <div className="mx-auto max-w-6xl">
        <motion.div initial={{ opacity: 0, y: -18 }} animate={{ opacity: 1, y: 0 }} className="mb-6 rounded-md border border-emerald-900/70 bg-[#07110a] p-5 sm:p-6">
          <div className="font-mono text-xs uppercase tracking-[0.2em] text-emerald-300">Exact Kuhn EV</div>
          <h1 className="mt-2 font-display text-5xl font-semibold text-emerald-50">Play vs Nash</h1>
          <p className="mt-2 max-w-3xl text-slate-400">
            Decisions are compared against the shipped DCFR average strategy by recursively evaluating the Kuhn game tree.
          </p>
        </motion.div>

        <div className="mb-6 grid gap-3 sm:grid-cols-3">
          {[
            { label: 'Hands', value: handsPlayed.toLocaleString(), color: 'text-white' },
            { label: 'Session EV', value: `${sessionEV >= 0 ? '+' : ''}${fmt(sessionEV, 3)} bb`, color: sessionEV >= 0 ? 'text-emerald-300' : 'text-red-300' },
            { label: 'Average EV', value: handsPlayed ? `${fmt(sessionEV / handsPlayed, 3)} bb` : '0 bb', color: 'text-amber-300' },
          ].map(stat => (
            <div key={stat.label} className="surface-quiet rounded-md p-4 text-center">
              <div className={`font-mono text-2xl font-black ${stat.color}`}>{stat.value}</div>
              <div className="mt-1 text-xs uppercase tracking-[0.14em] text-slate-500">{stat.label}</div>
            </div>
          ))}
        </div>

        <div className="grid gap-5 lg:grid-cols-[minmax(0,1.3fr)_minmax(310px,0.7fr)]">
          <div className="space-y-5">
            <div className="surface rounded-md p-5">
              {!hand || hand.phase === 'result' ? (
                <div className="py-8 text-center">
                  {hand?.phase === 'result' && (
                    <div className="mb-7">
                      <div className={`text-4xl font-black ${
                        hand.result === 'win' ? 'text-emerald-300' :
                        hand.result === 'lose' ? 'text-red-300' : 'text-slate-300'
                      }`}>
                        {hand.result === 'win' ? 'You Win' : hand.result === 'lose' ? 'You Lose' : 'Tie'}
                      </div>
                      <div className="mt-3 text-sm text-slate-400">
                        {hand.playerCard} vs {hand.oppCard} · {kuhnHistoryLabel(hand.history)} · payoff{' '}
                        <span className="font-mono text-white">{payoff != null && payoff >= 0 ? '+' : ''}{payoff} bb</span>
                      </div>
                      <div className={`mt-2 font-mono text-sm ${hand.evDelta >= 0 ? 'text-emerald-300' : 'text-red-300'}`}>
                        EV vs Nash {hand.evDelta >= 0 ? '+' : ''}{fmt(hand.evDelta, 3)} bb
                      </div>
                    </div>
                  )}

                  <button
                    type="button"
                    onClick={startNewHand}
                    className="inline-flex items-center gap-2 rounded-md bg-emerald-300 px-6 py-3 font-black text-slate-950 transition hover:bg-emerald-200"
                  >
                    <RotateCcw size={17} />
                    {hand ? 'Deal Next Hand' : 'Deal Hand'}
                  </button>
                </div>
              ) : (
                <>
                  <div className="felt rounded-md border border-emerald-300/15 p-6">
                    <div className="text-center">
                      <div className="mb-2 text-xs uppercase tracking-[0.18em] text-slate-500">Opponent</div>
                      <div className="flex justify-center">
                        <CardDisplay card={hand.oppCard} hidden size="sm" />
                      </div>
                    </div>

                    <div className="my-6 flex items-center justify-center gap-3">
                      <div className="rounded-full border border-white/10 bg-black/30 px-4 py-2">
                        <span className="text-xs text-slate-500">Pot </span>
                        <span className="font-mono font-bold text-white">{kuhnPot(hand.history)} chips</span>
                      </div>
                      <div className="rounded-full border border-white/10 bg-black/30 px-4 py-2 font-mono text-xs text-emerald-300">
                        {kuhnHistoryLabel(hand.history)}
                      </div>
                    </div>

                    <div className="text-center">
                      <div className="mb-2 text-xs uppercase tracking-[0.18em] text-emerald-300">Your Card</div>
                      <div className="flex justify-center">
                        <CardDisplay card={hand.playerCard} />
                      </div>
                    </div>
                  </div>

                  <div className="grid gap-3 sm:grid-cols-2">
                    {availableActions(hand.history).map(({ idx, label, detail }) => (
                      <button
                        key={idx}
                        type="button"
                        onClick={() => playerAction(idx)}
                        className="group rounded-md border border-white/10 bg-white/[0.04] p-4 text-left transition hover:-translate-y-0.5 hover:border-emerald-300/45 hover:bg-emerald-300/[0.08]"
                      >
                        <div className="flex items-start justify-between gap-3">
                          <div>
                            <div className="font-black text-white group-hover:text-emerald-200">{label}</div>
                            <div className="mt-1 text-xs text-slate-500">{detail}</div>
                          </div>
                          {currentNode && (
                            <div className="text-right font-mono text-xs text-emerald-300">
                              {pct(currentNode.probs[idx] ?? 0)}
                            </div>
                          )}
                        </div>
                        {currentDecision && (
                          <div className="mt-3 flex items-center justify-between border-t border-white/10 pt-3 font-mono text-xs">
                            <span className="text-slate-500">EV {fmt(currentDecision.actionValues[idx] ?? 0, 3)}</span>
                            <span className={(currentDecision.actionDelta[idx] ?? 0) >= 0 ? 'text-emerald-300' : 'text-red-300'}>
                              {currentDecision.actionDelta[idx] >= 0 ? '+' : ''}{fmt(currentDecision.actionDelta[idx] ?? 0, 3)}
                            </span>
                          </div>
                        )}
                      </button>
                    ))}
                  </div>
                </>
              )}
            </div>
          </div>

          <div className="space-y-4">
            {hand?.phase === 'action' && <NashPanel strategy={strategy} hand={hand} />}

            <div className="surface rounded-md p-5">
              <div className="mb-3 flex items-center gap-2">
                <ShieldCheck size={18} className="text-blue-300" />
                <div className="text-sm font-bold text-white">Decision Accounting</div>
              </div>
              <p className="text-sm leading-6 text-slate-400">
                EV is computed over the two possible hidden opponent cards at the current infoset. Future actions use the
                same DCFR average strategy unless you make another decision.
              </p>
            </div>

            <div className="surface rounded-md p-5">
              <div className="mb-3 flex items-center gap-2">
                <CircleDollarSign size={18} className="text-amber-300" />
                <div className="text-sm font-bold text-white">Current Hand EV</div>
              </div>
              <div className={`font-mono text-3xl font-black ${!hand || hand.evDelta >= 0 ? 'text-emerald-300' : 'text-red-300'}`}>
                {hand ? `${hand.evDelta >= 0 ? '+' : ''}${fmt(hand.evDelta, 3)} bb` : '0 bb'}
              </div>
            </div>

            {log.length > 0 && (
              <div className="surface rounded-md p-5">
                <div className="mb-3 flex items-center gap-2">
                  <History size={18} className="text-slate-300" />
                  <div className="text-sm font-bold text-white">Hand Log</div>
                </div>
                <div className="max-h-48 space-y-2 overflow-y-auto">
                  {log.map((entry, i) => (
                    <div key={`${entry}-${i}`} className="rounded-md border border-white/[0.08] bg-black/20 p-2 font-mono text-[11px] leading-5 text-slate-400">
                      {entry}
                    </div>
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
