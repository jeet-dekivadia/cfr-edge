#include "cfr.h"
#include "kuhn.h"
#include "leduc.h"
#include "holdem.h"
#include "abstraction.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <iomanip>
#include <filesystem>
#include <chrono>

using namespace cfr;

// Known Kuhn Nash: P0 EV = -1/18, exploitability at Nash = 0.
static constexpr double KUHN_NASH_VALUE_P0 = -1.0 / 18.0;

// ---- CSV helpers ----

static void write_csv_header(std::ofstream& out) {
    out << "iteration,exploitability,variant\n";
}

static void append_csv(std::ofstream& out,
                        const std::vector<std::pair<int,double>>& curve,
                        const std::string& label) {
    out << std::scientific << std::setprecision(8);
    for (const auto& [it, ex] : curve)
        out << it << "," << ex << "," << label << "\n";
}

static void print_result_line(const std::string& tag, const TrainResult& r) {
    std::cout << "  [" << tag << "]"
              << "  time="      << std::fixed    << std::setprecision(2) << r.elapsed_seconds << "s"
              << "  infosets="  << r.num_infosets
              << "  final_expl=" << std::scientific << std::setprecision(4)
              << r.final_exploitability
              << "  rate(α)="   << std::fixed << std::setprecision(3)
              << r.convergence_rate << "\n";
}

// ---- Kuhn experiments ----

static void run_kuhn_experiments() {
    std::cout << "\n========== KUHN POKER ==========\n";
    std::cout << "  Nash: P0 EV = " << std::fixed << std::setprecision(6)
              << KUHN_NASH_VALUE_P0 << ",  exploitability = 0 at Nash\n";

    std::ofstream combined("results/kuhn_convergence.csv");
    write_csv_header(combined);

    for (auto [mode, label] : std::vector<std::pair<Mode,std::string>>{
            {Mode::CFR,      "CFR"},
            {Mode::CFR_PLUS, "CFR+"},
            {Mode::DCFR,     "DCFR"}}) {
        TrainConfig cfg;
        cfg.game       = Game::KUHN;
        cfg.mode       = mode;
        cfg.iterations = 10000;
        cfg.eval_every = 100;
        std::cout << "\n[Kuhn] " << label << "  (10000 iters)\n";
        auto res = run_training(cfg);
        res.convergence_rate = estimate_convergence_rate(res.exploit_curve);
        print_result_line(label, res);
        append_csv(combined, res.exploit_curve, label);
    }

    // SoA/SIMD path
    {
        TrainConfig cfg;
        cfg.game       = Game::KUHN;
        cfg.mode       = Mode::CFR;
        cfg.iterations = 10000;
        cfg.eval_every = 100;
        cfg.use_soa    = true;
        std::cout << "\n[Kuhn] CFR (SoA/SIMD)  (10000 iters)\n";
        auto res = run_training(cfg);
        res.convergence_rate = estimate_convergence_rate(res.exploit_curve);
        print_result_line("CFR_SoA", res);
        append_csv(combined, res.exploit_curve, "CFR_SoA");
    }

    combined.close();
    std::cout << "\n  -> results/kuhn_convergence.csv\n";

    // Print converged strategy
    {
        InfoMap nodes;
        kuhn::train(nodes, 20000, Mode::DCFR, 20000);
        kuhn::print_strategy(nodes);
    }
}

// ---- Leduc experiments ----

static void run_leduc_experiments() {
    std::cout << "\n========== LEDUC HOLD'EM ==========\n";

    std::ofstream combined("results/leduc_convergence.csv");
    write_csv_header(combined);

    for (auto [mode, label] : std::vector<std::pair<Mode,std::string>>{
            {Mode::CFR,      "CFR"},
            {Mode::CFR_PLUS, "CFR+"},
            {Mode::DCFR,     "DCFR"}}) {
        TrainConfig cfg;
        cfg.game       = Game::LEDUC;
        cfg.mode       = mode;
        cfg.iterations = 10000;
        cfg.eval_every = 200;
        std::cout << "\n[Leduc] " << label << "  (10000 iters)\n";
        auto res = run_training(cfg);
        res.convergence_rate = estimate_convergence_rate(res.exploit_curve);
        print_result_line(label, res);
        append_csv(combined, res.exploit_curve, label);
    }

    combined.close();
    std::cout << "\n  -> results/leduc_convergence.csv\n";
}

// ---- Abstraction experiments ----

static void run_abstraction_experiments() {
    std::cout << "\n========== ABSTRACTION EXPERIMENTS ==========\n";
    std::cout << "  (exploitability measured within the abstract game)\n";

    std::ofstream combined("results/abstraction_comparison.csv");
    combined << "iteration,exploitability,scheme\n";

    auto run = [&](const char* label,
                    abstraction::BucketScheme bs,
                    abstraction::ActionScheme as) {
        abstraction::AbstractConfig cfg;
        cfg.buckets    = bs;
        cfg.actions    = as;
        cfg.cfr_mode   = Mode::DCFR;
        cfg.iterations = 5000;
        cfg.eval_every = 100;

        std::cout << "\n[Abstraction] " << label << "\n";
        auto res = abstraction::run_abstraction_experiment(cfg);
        std::cout << "  abstract_infosets=" << res.num_abstract_infosets
                  << "  full_infosets=" << res.num_original_infosets
                  << "  final_expl=" << std::scientific << std::setprecision(4)
                  << res.final_exploit << "\n";
        append_csv(combined, res.exploit_curve, label);
    };

    run("full",              abstraction::BucketScheme::NONE,     abstraction::ActionScheme::FULL);
    run("strength_buckets",  abstraction::BucketScheme::STRENGTH,  abstraction::ActionScheme::FULL);
    run("no_raise",          abstraction::BucketScheme::NONE,     abstraction::ActionScheme::NO_RAISE);
    run("strength+no_raise", abstraction::BucketScheme::STRENGTH,  abstraction::ActionScheme::NO_RAISE);

    combined.close();
    std::cout << "\n  -> results/abstraction_comparison.csv\n";
}

// ---- Texas Hold'em experiments ----

static void run_holdem_experiments() {
    std::cout << "\n========== TEXAS HOLD'EM (MCCFR) ==========\n";

    InfoMap nodes;
    std::cout << "  Running DCFR MCCFR (3000 iters, eval every 500)...\n";
    auto t0 = std::chrono::high_resolution_clock::now();
    auto curve = holdem::train_holdem(nodes, 3000, Mode::DCFR, 500);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "  infosets=" << nodes.size()
              << "  time=" << std::fixed << std::setprecision(2) << elapsed << "s\n";

    // Write convergence CSV
    std::ofstream out("results/holdem_convergence.csv");
    out << "iteration,exploitability,variant\n";
    out << std::scientific << std::setprecision(8);
    for (const auto& [it, ex] : curve)
        out << it << "," << ex << ",MCCFR_DCFR\n";
    out.close();
    std::cout << "  -> results/holdem_convergence.csv\n";

    holdem::print_holdem_strategy(nodes);
}

// ---- Main ----

int main(int argc, char** argv) {
    std::cout << "cfr-edge: Poker CFR/CFR+/DCFR Solver v2\n";
    std::cout << "=========================================\n";
    std::cout << "Algorithms : Vanilla CFR | CFR+ | DCFR (α=1.5, β=-∞, γ=2)\n";
    std::cout << "SIMD       : ";
#if HAS_AVX2
    std::cout << "AVX2 (4 doubles/cycle)\n";
#elif HAS_SSE2
    std::cout << "SSE2 (2 doubles/cycle)\n";
#else
    std::cout << "scalar\n";
#endif

    std::filesystem::create_directories("results");

    bool do_kuhn = true, do_leduc = true, do_abstract = true, do_holdem = true;
    for (int i = 1; i < argc; i++) {
        if      (!std::strcmp(argv[i], "--kuhn-only"))  { do_leduc = do_abstract = do_holdem = false; }
        else if (!std::strcmp(argv[i], "--leduc-only")) { do_kuhn  = do_abstract = do_holdem = false; }
        else if (!std::strcmp(argv[i], "--holdem-only")){ do_kuhn  = do_leduc = do_abstract = false; }
        else if (!std::strcmp(argv[i], "--no-abstract")) do_abstract = false;
        else if (!std::strcmp(argv[i], "--no-holdem"))   do_holdem   = false;
    }

    if (do_kuhn)     run_kuhn_experiments();
    if (do_leduc)    run_leduc_experiments();
    if (do_abstract) run_abstraction_experiments();
    if (do_holdem)   run_holdem_experiments();

    std::cout << "\nAll done. Convergence CSVs written to results/.\n";
    return 0;
}
