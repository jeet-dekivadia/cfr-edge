import Link from 'next/link';

const SNIPPETS = [
  {
    title: 'Information nodes',
    file: 'include/infoset.h',
    note: 'Every decision point stores cumulative regret and the accumulated average strategy. The final policy shown in the UI comes from `strategy_sum`, not the last instantaneous strategy.',
    code: `struct InfoNode {
    double regret_sum[MAX_ACTIONS];
    double strategy_sum[MAX_ACTIONS];
    int num_actions;

    void get_strategy(double* out) const {
        double norm = 0.0;
        for (int a = 0; a < num_actions; a++) {
            out[a] = std::max(regret_sum[a], 0.0);
            norm += out[a];
        }
        if (norm > 0.0) {
            for (int a = 0; a < num_actions; a++) out[a] /= norm;
        } else {
            double u = 1.0 / num_actions;
            for (int a = 0; a < num_actions; a++) out[a] = u;
        }
    }
};`,
  },
  {
    title: 'Kuhn traversal',
    file: 'src/kuhn.cpp',
    note: 'The traversal builds an infoset key from the acting player’s private card and public betting history, then updates regret using the opponent reach probability.',
    code: `int player = acting_player(history);
int card = (player == 0) ? p0_card : p1_card;
std::string key = std::string(1, card_name(card)) + ":" + history;

auto it = nodes.find(key);
if (it == nodes.end()) {
    it = nodes.emplace(key, InfoNode(NUM_ACTIONS)).first;
}
InfoNode& node = it->second;
double strategy[NUM_ACTIONS];
node.get_strategy(strategy);

double cf_reach = (player == 0) ? p1_reach : p0_reach;
double sign = (player == 0) ? 1.0 : -1.0;

for (int a = 0; a < NUM_ACTIONS; a++) {
    node.regret_sum[a] += cf_reach * sign * (util[a] - node_util);
}`,
  },
  {
    title: 'Average strategy weighting',
    file: 'src/kuhn.cpp',
    note: 'CFR, CFR+, and DCFR share the same traversal shape, but differ in how much each iteration contributes to the exported average strategy.',
    code: `double weight;
if      (mode == Mode::CFR_PLUS) weight = (double)iteration;
else if (mode == Mode::DCFR)     weight = (double)iteration * (double)iteration;
else                             weight = 1.0;

for (int a = 0; a < NUM_ACTIONS; a++) {
    node.strategy_sum[a] += weight * my_reach * strategy[a];
}`,
  },
  {
    title: 'Best response consistency',
    file: 'src/kuhn.cpp',
    note: 'Exploitability needs an infoset-level best response. The best-response player cannot secretly choose different actions for hidden deals that look identical.',
    code: `std::unordered_map<int, std::vector<DealState>> by_card;
for (const auto& s : states) {
    int card = (br_player == 0) ? s.p0_card : s.p1_card;
    by_card[card].push_back(s);
}

for (auto& [card, group] : by_card) {
    double best = -1e18;
    for (int a = 0; a < NUM_ACTIONS; a++)
        best = std::max(best, br_traverse(group, history + ac[a], br_player, nodes));
    total += best;
}`,
  },
  {
    title: 'Structure-of-arrays storage',
    file: 'include/soa_store.h',
    note: 'For batch operations, nodes are grouped by action count and stored action-major. That layout lets regret matching run across many nodes at once.',
    code: `struct Group {
    int count = 0;
    std::vector<std::string> keys;
    std::vector<std::vector<double>> regrets;     // [action][node]
    std::vector<std::vector<double>> strat_sums;  // [action][node]
    std::vector<std::vector<double>> strategy;    // [action][node]
};`,
  },
  {
    title: 'SIMD regret matching',
    file: 'include/simd_utils.h',
    note: 'The AVX2 path processes four double-precision regrets at a time. The scalar tail keeps the implementation portable.',
    code: `__m256d r0  = _mm256_loadu_pd(regret0 + i);
__m256d r1  = _mm256_loadu_pd(regret1 + i);
__m256d p0  = _mm256_max_pd(r0, zero);
__m256d p1  = _mm256_max_pd(r1, zero);
__m256d sum = _mm256_add_pd(p0, p1);
__m256d inv = _mm256_div_pd(one, _mm256_max_pd(sum, tiny));

_mm256_storeu_pd(strat0 + i, _mm256_mul_pd(p0, inv));
_mm256_storeu_pd(strat1 + i, _mm256_mul_pd(p1, inv));`,
  },
  {
    title: "Hold'em bucket mapping",
    file: 'src/holdem.cpp',
    note: 'The Hold’em demo reduces 1,326 starting hands to 169 canonical preflop buckets: pairs, suited hands, and offsuit hands.',
    code: `if (pair) {
    return 12 - r0; // AA=0, KK=1, ..., 22=12
}
int idx = 0;
for (int h = 12; h > high; h--) idx += h;
idx += (high - 1 - low);

return suited ? 13 + idx : 91 + idx;`,
  },
  {
    title: 'Static JSON export',
    file: 'include/json_output.h',
    note: 'The frontend is intentionally static. The C++ exporter writes convergence curves, snapshots, final strategies, regrets, and metadata into JSON bundles.',
    code: `root["game"] = cfg.game;
root["algorithm"] = cfg.algorithm;
root["iterations"] = cfg.total_iterations;
root["convergence"] = conv_arr;
root["strategy_snapshots"] = snaps;
root["final_strategy"] = infomap_to_json(final_nodes, cfg.action_labels);`,
  },
];

export default function CppPage() {
  return (
    <div className="min-h-screen bg-[#050806] px-5 py-24 text-slate-100">
      <main className="mx-auto max-w-5xl">
        <header className="border-b border-emerald-900/70 pb-10">
          <p className="font-mono text-xs uppercase tracking-[0.24em] text-emerald-500">C++ internals</p>
          <h1 className="mt-5 font-display text-6xl font-semibold leading-none text-emerald-50 sm:text-7xl">
            Critical C++ pieces
          </h1>
          <p className="mt-6 max-w-2xl text-lg leading-8 text-slate-400">
            These are the parts of CFR-Edge that made the project feel like a real solver instead of a visualization demo.
          </p>
          <div className="mt-7 flex flex-wrap gap-2">
            <Link href="/solve" className="rounded-full border border-emerald-900 px-4 py-2 text-sm text-slate-300 hover:border-emerald-300 hover:text-emerald-200">
              Solver
            </Link>
            <Link href="/strategy" className="rounded-full border border-emerald-900 px-4 py-2 text-sm text-slate-300 hover:border-emerald-300 hover:text-emerald-200">
              Strategy
            </Link>
            <Link href="/" className="rounded-full border border-emerald-900 px-4 py-2 text-sm text-slate-300 hover:border-emerald-300 hover:text-emerald-200">
              Article
            </Link>
          </div>
        </header>

        <div className="mt-12 grid gap-8">
          {SNIPPETS.map((snippet, index) => (
            <section key={snippet.title} className="border-t border-emerald-900/70 pt-8">
              <div className="grid gap-6 lg:grid-cols-[250px_minmax(0,1fr)]">
                <div>
                  <p className="font-mono text-xs uppercase tracking-[0.22em] text-emerald-600">
                    {String(index + 1).padStart(2, '0')} / {snippet.file}
                  </p>
                  <h2 className="mt-3 font-display text-3xl font-semibold text-emerald-50">{snippet.title}</h2>
                  <p className="mt-3 text-sm leading-6 text-slate-500">{snippet.note}</p>
                </div>
                <pre className="overflow-x-auto border border-emerald-900/80 bg-[#07110a] p-4 text-sm leading-6 text-emerald-100">
                  <code>{snippet.code}</code>
                </pre>
              </div>
            </section>
          ))}
        </div>
      </main>
    </div>
  );
}
