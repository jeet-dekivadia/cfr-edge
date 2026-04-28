'use client';
import Link from 'next/link';
import { motion } from 'framer-motion';
import PokerTableScene from '@/components/PokerTableScene';

const FEATURES = [
  {
    icon: '∇',
    title: 'Counterfactual Regret Minimization',
    desc: 'CFR / CFR+ / DCFR with full regret tracking, exploitability measurement, and average-strategy convergence guarantee.',
    href: '/algorithm',
    color: 'from-emerald-500/10 to-emerald-500/5 border-emerald-500/20',
    accent: 'text-emerald-400',
  },
  {
    icon: '⇌',
    title: 'Nash Equilibrium Solver',
    desc: 'Exact Nash strategies for Kuhn Poker (12 infosets) and Leduc Hold\'em (288 infosets), verified by exploitability < 10⁻³.',
    href: '/solve',
    color: 'from-blue-500/10 to-blue-500/5 border-blue-500/20',
    accent: 'text-blue-400',
  },
  {
    icon: '♠',
    title: 'Texas Hold\'em MCCFR',
    desc: 'External-sampling MCCFR on HUNL with 169-bucket preflop abstraction and EHS² postflop bucketing. 33K+ infosets.',
    href: '/holdem',
    color: 'from-amber-500/10 to-amber-500/5 border-amber-500/20',
    accent: 'text-amber-400',
  },
  {
    icon: '▶',
    title: 'Play vs Nash',
    desc: 'Make decisions and see the GTO response. Every action shows EV delta vs Nash — measure how exploitable your play is.',
    href: '/play',
    color: 'from-purple-500/10 to-purple-500/5 border-purple-500/20',
    accent: 'text-purple-400',
  },
];

const STATS = [
  { value: '< 10⁻³', label: 'Exploitability', sub: 'after 10K iters (Kuhn)' },
  { value: '0.605',  label: 'Convergence rate α', sub: 'ε ~ C · T⁻ᵅ  (DCFR)' },
  { value: '4×',     label: 'SIMD throughput', sub: 'AVX2 doubles/cycle' },
  { value: '33K+',   label: 'Hold\'em infosets', sub: 'MCCFR external sampling' },
];

export default function Home() {
  return (
    <div className="min-h-screen bg-gray-950 text-white overflow-x-hidden">

      {/* ---- Hero ---- */}
      <section className="relative h-[92vh] flex flex-col items-center justify-center overflow-hidden">
        {/* 3D table background */}
        <div className="absolute inset-0 opacity-60">
          <PokerTableScene />
        </div>

        {/* Gradient overlay */}
        <div className="absolute inset-0 bg-gradient-to-b from-transparent via-gray-950/30 to-gray-950 pointer-events-none" />

        {/* Hero text */}
        <motion.div
          initial={{ opacity: 0, y: 40 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.9, ease: 'easeOut' }}
          className="relative z-10 text-center px-4 max-w-4xl"
        >
          <div className="inline-block mb-4 px-3 py-1 rounded-full border border-emerald-500/30 bg-emerald-500/10 text-emerald-400 text-xs font-mono tracking-widest uppercase">
            Quantitative Game Theory
          </div>

          <h1 className="text-6xl sm:text-7xl font-black tracking-tight mb-6 leading-none">
            CFR<span className="text-emerald-400">Edge</span>
            <br />
            <span className="text-3xl sm:text-4xl font-light text-gray-300">
              Poker Nash Equilibrium Solver
            </span>
          </h1>

          <p className="text-lg text-gray-400 max-w-2xl mx-auto mb-10 leading-relaxed">
            A full-stack CFR / CFR+ / DCFR engine solving Kuhn, Leduc, and Texas Hold&apos;em
            to provably near-optimal strategies. AVX2-accelerated C++ back-end,
            live convergence visualisation.
          </p>

          <div className="flex flex-wrap gap-4 justify-center">
            <Link
              href="/solve"
              className="px-8 py-3 rounded-lg bg-emerald-500 hover:bg-emerald-400 text-black font-bold transition-all text-sm shadow-lg shadow-emerald-500/20 hover:scale-105"
            >
              Watch it Solve →
            </Link>
            <Link
              href="/play"
              className="px-8 py-3 rounded-lg border border-gray-600 hover:border-gray-400 text-gray-300 hover:text-white font-medium transition-all text-sm hover:scale-105"
            >
              Play vs Nash
            </Link>
          </div>
        </motion.div>
      </section>

      {/* ---- Stats bar ---- */}
      <section className="border-y border-gray-800 bg-gray-900/50">
        <div className="max-w-5xl mx-auto px-4 py-8 grid grid-cols-2 sm:grid-cols-4 gap-8">
          {STATS.map((s, i) => (
            <motion.div
              key={i}
              initial={{ opacity: 0, y: 20 }}
              whileInView={{ opacity: 1, y: 0 }}
              transition={{ delay: i * 0.1 }}
              className="text-center"
            >
              <div className="text-3xl font-black text-emerald-400 font-mono">{s.value}</div>
              <div className="text-sm font-semibold text-white mt-1">{s.label}</div>
              <div className="text-xs text-gray-500 mt-0.5">{s.sub}</div>
            </motion.div>
          ))}
        </div>
      </section>

      {/* ---- Feature grid ---- */}
      <section className="max-w-6xl mx-auto px-4 py-24">
        <motion.h2
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          className="text-center text-3xl font-bold text-white mb-16"
        >
          Every layer is exploitability-driven
        </motion.h2>

        <div className="grid sm:grid-cols-2 gap-6">
          {FEATURES.map((f, i) => (
            <motion.div
              key={i}
              initial={{ opacity: 0, y: 30 }}
              whileInView={{ opacity: 1, y: 0 }}
              transition={{ delay: i * 0.1 }}
            >
              <Link
                href={f.href}
                className={`group block p-6 rounded-2xl border bg-gradient-to-br ${f.color} 
                  hover:scale-[1.02] transition-all duration-300`}
              >
                <div className={`text-4xl font-black mb-3 ${f.accent} font-mono`}>{f.icon}</div>
                <h3 className="text-lg font-bold text-white mb-2 group-hover:text-emerald-100 transition-colors">
                  {f.title}
                </h3>
                <p className="text-sm text-gray-400 leading-relaxed">{f.desc}</p>
                <div className={`mt-4 text-xs font-mono ${f.accent} opacity-0 group-hover:opacity-100 transition-opacity`}>
                  Explore →
                </div>
              </Link>
            </motion.div>
          ))}
        </div>
      </section>

      {/* ---- CFR explainer ---- */}
      <section className="max-w-4xl mx-auto px-4 pb-24">
        <div className="rounded-2xl border border-gray-800 bg-gray-900/60 p-8">
          <h2 className="text-2xl font-bold mb-6 text-white">How CFR finds Nash equilibrium</h2>
          <div className="grid sm:grid-cols-3 gap-6">
            {[
              {
                step: '01',
                title: 'Initialize',
                desc: 'All infosets start with uniform random strategies. Exploitability is high — any opponent can profit.',
              },
              {
                step: '02',
                title: 'Accumulate Regret',
                desc: 'For each decision, compute the counterfactual value of every action. Actions better than average accumulate positive regret.',
              },
              {
                step: '03',
                title: 'Regret Matching',
                desc: 'Strategy is proportional to positive regrets. Over T iterations, the time-average strategy converges: ε(T) ~ O(T⁻¹/²).',
              },
            ].map((s) => (
              <div key={s.step} className="flex gap-4">
                <div className="text-4xl font-black text-emerald-900 font-mono shrink-0">{s.step}</div>
                <div>
                  <div className="font-semibold text-white mb-1">{s.title}</div>
                  <div className="text-sm text-gray-400">{s.desc}</div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </section>
    </div>
  );
}
