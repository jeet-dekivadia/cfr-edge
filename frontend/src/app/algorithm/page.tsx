'use client';
import { useEffect, useState, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import type { GameStrategy, StrategyMap } from '@/lib/types';
import { loadStrategy } from '@/lib/data';
import { fmt, pct } from '@/lib/utils';

// CFR step descriptions
const CFR_STEPS = [
  {
    id: 'init',
    title: '1. Initialize',
    code: 'regret_sum[a] = 0 ∀ a\nstrategy_sum[a] = 0 ∀ a\nσ(I,a) = 1/|A(I)|',
    desc: 'All infosets start with uniform random strategies. No regret has been accumulated.',
    color: 'border-blue-500/40 bg-blue-500/5',
  },
  {
    id: 'traverse',
    title: '2. Traverse Game Tree',
    code: 'v(σ, I) = Σ_a σ(I,a) · v(σ, I·a)\n\ncfr(I,a) = v(σ_{-I→a}, I) − v(σ, I)',
    desc: 'Recursively compute counterfactual values. For each infoset, compute the value of each action minus the node value.',
    color: 'border-amber-500/40 bg-amber-500/5',
  },
  {
    id: 'regret',
    title: '3. Accumulate Regret',
    code: 'R^T(I,a) = Σ_{t=1}^T π^σ_{-i}(I) · cfr^t(I,a)\n\nR^{T,+}(I,a) = max(R^T(I,a), 0)',
    desc: 'The regret measures how much better action a would have been. Positive regret means we should play a more.',
    color: 'border-emerald-500/40 bg-emerald-500/5',
  },
  {
    id: 'match',
    title: '4. Regret Matching',
    code: 'σ^{T+1}(I,a) = R^{T,+}(I,a) / Σ_b R^{T,+}(I,b)\n\nIf Σ R^+ = 0: uniform',
    desc: 'The new strategy is proportional to positive regrets. Actions we regret not playing get higher probability.',
    color: 'border-purple-500/40 bg-purple-500/5',
  },
  {
    id: 'average',
    title: '5. Time-Average Strategy',
    code: '\\bar{σ}^T(I,a) = Σ_{t=1}^T w_t · π^σ_i^t(I) · σ^t(I,a)\n\nCFR: w=1  CFR+: w=t  DCFR: w=t²',
    desc: 'The Nash equilibrium approximation is the time-average, not the final, strategy. Convergence: ε(T) ≤ Δ·√|I| / √T.',
    color: 'border-red-500/40 bg-red-500/5',
  },
];

// Animate regret accumulation for a specific infoset
function RegretWaterfall({ data }: { data: { action: string; regret: number; prob: number }[] }) {
  const maxRegret = Math.max(...data.map(d => Math.abs(d.regret)), 0.01);

  return (
    <div className="space-y-3">
      {data.map((d, i) => (
        <div key={d.action}>
          <div className="flex justify-between text-xs mb-1">
            <span className="text-gray-400 font-mono">{d.action}</span>
            <div className="flex gap-4">
              <span className="text-amber-400 font-mono">regret: {fmt(d.regret, 4)}</span>
              <span className="text-emerald-400 font-mono">σ: {pct(d.prob)}</span>
            </div>
          </div>
          <div className="h-6 bg-gray-900 rounded overflow-hidden relative">
            {/* Regret bar */}
            <motion.div
              initial={{ width: 0 }}
              animate={{ width: `${Math.abs(d.regret) / maxRegret * 100}%` }}
              transition={{ duration: 0.8, delay: i * 0.1, ease: 'easeOut' }}
              className={`absolute top-0 bottom-0 left-0 rounded ${d.regret > 0 ? 'bg-amber-500/50' : 'bg-red-500/30'}`}
            />
            {/* Probability bar */}
            <motion.div
              initial={{ width: 0 }}
              animate={{ width: `${d.prob * 100}%` }}
              transition={{ duration: 0.8, delay: i * 0.1 + 0.2, ease: 'easeOut' }}
              className="absolute top-1 bottom-1 left-0 rounded bg-emerald-500/70"
            />
          </div>
        </div>
      ))}
    </div>
  );
}

// Pseudocode display
function PseudoCode({ code }: { code: string }) {
  return (
    <pre className="bg-gray-950 rounded-lg p-4 text-xs font-mono text-emerald-300 overflow-x-auto
                   border border-gray-800 leading-relaxed">
      {code}
    </pre>
  );
}

export default function AlgorithmPage() {
  const [strategy, setStrategy] = useState<GameStrategy | null>(null);
  const [activeStep, setActiveStep] = useState(0);
  const [selectedInfoset, setSelectedInfoset] = useState('K:');

  useEffect(() => {
    loadStrategy('kuhn_dcfr.json')
      .then(s => { setStrategy(s); })
      .catch(console.error);
  }, []);

  const allInfosets = strategy ? Object.keys(strategy.final_strategy).sort() : [];

  const infosetData = strategy && selectedInfoset
    ? (() => {
        const node = strategy.final_strategy[selectedInfoset];
        if (!node) return [];
        return node.actions.map((a, i) => ({
          action: a,
          regret: node.regrets[i],
          prob: node.probs[i],
        }));
      })()
    : [];

  return (
    <div className="min-h-screen bg-gray-950 text-white p-6">
      <div className="max-w-6xl mx-auto">

        <motion.div initial={{ opacity: 0, y: -20 }} animate={{ opacity: 1, y: 0 }} className="mb-8">
          <h1 className="text-4xl font-black text-white mb-2">
            How CFR Works
          </h1>
          <p className="text-gray-400">
            Walk through the algorithm step-by-step. Select an infoset to see live regret accumulation.
          </p>
        </motion.div>

        <div className="grid lg:grid-cols-5 gap-6">

          {/* Left: Step-by-step */}
          <div className="lg:col-span-3 space-y-3">
            {CFR_STEPS.map((step, idx) => (
              <motion.div
                key={step.id}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: idx * 0.1 }}
                className={`rounded-xl border p-5 cursor-pointer transition-all ${step.color}
                  ${activeStep === idx ? 'ring-1 ring-white/10 scale-[1.01]' : 'hover:scale-[1.005]'}`}
                onClick={() => setActiveStep(idx)}
              >
                <div className="flex items-start justify-between mb-3">
                  <h3 className="font-bold text-white">{step.title}</h3>
                  {activeStep === idx && (
                    <span className="text-[10px] bg-white/10 px-2 py-0.5 rounded-full text-gray-300 font-mono">
                      active
                    </span>
                  )}
                </div>

                <AnimatePresence>
                  {activeStep === idx && (
                    <motion.div
                      initial={{ opacity: 0, height: 0 }}
                      animate={{ opacity: 1, height: 'auto' }}
                      exit={{ opacity: 0, height: 0 }}
                    >
                      <PseudoCode code={step.code} />
                      <p className="mt-3 text-sm text-gray-300 leading-relaxed">{step.desc}</p>
                    </motion.div>
                  )}
                </AnimatePresence>

                {activeStep !== idx && (
                  <p className="text-xs text-gray-500 truncate">{step.desc}</p>
                )}
              </motion.div>
            ))}

            {/* Convergence theorem */}
            <div className="rounded-xl border border-gray-700 bg-gray-900/40 p-5 mt-2">
              <h3 className="font-bold text-white mb-3">Convergence Guarantee</h3>
              <PseudoCode code={`ε(T) ≤ Δ · √|I| / √T                 (Vanilla CFR)
ε(T) ≤ O(T^(-1))                    (CFR+ and DCFR, empirically)

where:
  Δ   = max possible payoff difference
  |I| = number of information sets
  T   = training iterations

DCFR achieves empirical α ≈ 0.6 in ε ~ C·T^(-α)`} />
              <p className="text-xs text-gray-400 mt-3 leading-relaxed">
                The time-average strategy profile is guaranteed to converge to an
                ε-Nash equilibrium. DCFR achieves this ~100× faster than vanilla CFR
                through aggressive discounting of early (noisy) regret estimates.
              </p>
            </div>
          </div>

          {/* Right: Live regret viz */}
          <div className="lg:col-span-2 space-y-4">
            <div className="bg-gray-900/60 rounded-2xl border border-gray-800 p-5">
              <h2 className="font-bold text-white mb-4">Live Regret Waterfall</h2>

              {/* Infoset selector */}
              <div className="mb-4">
                <label className="text-xs text-gray-500 block mb-2 uppercase tracking-widest">
                  Select Infoset (Kuhn Poker)
                </label>
                <div className="flex flex-wrap gap-2">
                  {allInfosets.map(key => (
                    <button
                      key={key}
                      onClick={() => setSelectedInfoset(key)}
                      className={`px-2.5 py-1 rounded text-xs font-mono transition-all ${
                        selectedInfoset === key
                          ? 'bg-emerald-500/20 text-emerald-400 ring-1 ring-emerald-500/40'
                          : 'bg-gray-800 text-gray-400 hover:text-white'
                      }`}
                    >
                      {key}
                    </button>
                  ))}
                </div>
              </div>

              {infosetData.length > 0 ? (
                <>
                  <RegretWaterfall data={infosetData} />
                  <div className="mt-4 text-xs text-gray-500 leading-relaxed">
                    <span className="inline-block w-3 h-3 rounded bg-amber-500/50 mr-1 align-middle" />
                    Positive regret (action was better than average)
                    <br />
                    <span className="inline-block w-3 h-3 rounded bg-emerald-500/70 mr-1 mt-1 align-middle" />
                    Current strategy probability (σ)
                  </div>
                </>
              ) : (
                <div className="text-gray-600 text-sm">Loading...</div>
              )}
            </div>

            {/* Algorithm comparison */}
            <div className="bg-gray-900/60 rounded-2xl border border-gray-800 p-5">
              <h3 className="font-bold text-white mb-3">Algorithm Comparison</h3>
              <div className="space-y-3 text-xs">
                {[
                  { algo: 'CFR', rate: '0.532', desc: 'Uniform weighting. Proved O(1/√T).', color: '#4e79a7' },
                  { algo: 'CFR+', rate: '0.496', desc: 'Linear weighting + regret floor. O(1/T) in practice.', color: '#f28e2b' },
                  { algo: 'DCFR', rate: '0.605', desc: 'Discounted α=1.5, quadratic weighting. Best empirically.', color: '#e15759' },
                ].map(a => (
                  <div key={a.algo} className="flex items-start gap-3">
                    <div className="w-2 h-2 rounded-full mt-1 shrink-0" style={{ backgroundColor: a.color }} />
                    <div>
                      <span className="font-bold text-white">{a.algo}</span>
                      <span className="text-gray-500 ml-2">α = {a.rate}</span>
                      <p className="text-gray-500 mt-0.5">{a.desc}</p>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
