#include "cfr.h"
#include "kuhn.h"
#include "leduc.h"
#include "abstraction.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <iomanip>
#include <filesystem>
#include <chrono>

using namespace cfr;

// Kuhn poker Nash value (known analytically): -1/18
static constexpr double KUHN_NASH_EXPLOITABILITY = 0.0;  // exploitability at Nash = 0
static constexpr double KUHN_NASH_VALUE_P0       = -1.0 / 18.0;  // EV for P0 at Nash

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

// ---- pretty-print a single result line ----

static void print_result(const std::string& tag, const TrainResult& r) {
    std::cout << "  [" << tag << "]"
              << "  time=" << std::fixed << std::setprecision(2) << r.elapsed_seconds << "s"
              << "  infosets=" << r.num_infosets
              << "  final_expl=" << std::scientific << std::setprecision(4)
              << r.final_exploitability << "\n";
}

// ---- Kuhn experiments ----

static void run_kuhn_experiments() {
    std::cout << "\n========== KUHN POKER ==========\n";
    std::cout << "  Known Nash: P0 EV = " << std::fixed << std::setprecision(6)
              << KUHN_NASH_VALUE_P0 << ",  exploitability at Nash = 0\n";

    std::ofstream combined("results/kuhn_convergence.csv");
    write_csv_header(combined);

    // --- Vanilla CFR ---
    {
        TrainConfig cfg;
        cfg.game       = Game::KUHN;
        cfg.mode       = Mode::CFR;
        cfg.iterations = 10000;
        cfg.eval_every = 100;
        std::cout << "\n[Kuhn] Vanilla CFR  (" << cfg.iterations << " iters)\n";
        auto res = run_training(cfg);
        print_result("CFR", res);
        append_csv(combined, res.exploit_curve, "CFR");
    }

    // --- CFR+ ---
    {
        TrainConfig cfg;
        cfg.game       = Game::KUHN;
        cfg.mode       = Mode::CFR_PLUS;
        cfg.iterations = 10000;
        cfg.eval_every = 100;
        std::cout << "\n[Kuhn] CFR+         (" << cfg.iterations << " iters)\n";
        auto res = run_training(cfg);
        print_result("CFR+", res);
        append_csv(combined, res.exploit_curve, "CFR+");
    }

    // --- DCFR ---
    {
        TrainConfig cfg;
        cfg.game       = Game::KUHN;
        cfg.mode       = Mode::DCFR;
        cfg.iterations = 10000;
        cfg.eval_every = 100;
        std::cout << "\n[Kuhn] DCFR         (" << cfg.iterations << " iters)\n";
        auto res = run_training(cfg);
        print_result("DCFR", res);
        append_csv(combined, res.exploit_curve, "DCFR");
    }

    // --- SoA/SIMD (CFR) ---
    {
        TrainConfig cfg;
        cfg.game       = Game::KUHN;
        cfg.mode       = Mode::CFR;
        cfg.iterations = 10000;
        cfg.eval_every = 100;
        cfg.use_soa    = true;
        std::cout << "\n[Kuhn] CFR SoA/SIMD (" << cfg.iterations << " iters)\n";
        auto res = run_training(cfg);
        print_result("CFR_SoA", res);
        append_csv(combined, res.exploit_curve, "CFR_SoA");
    }

    combined.close();
    std::cout << "\n  -> results/kuhn_convergence.csv\n";

    // Print converged strategy (DCFR gives the cleanest result)
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

    // --- Vanilla CFR ---
    {
        TrainConfig cfg;
        cfg.game       = Game::LEDUC;
        cfg.mode       = Mode::CFR;
        cfg.iterations = 10000;
        cfg.eval_every = 200;
        std::cout << "\n[Leduc] Vanilla CFR  (" << cfg.iterations << " iters)\n";
        auto res = run_training(cfg);
        print_result("CFR", res);
        append_csv(combined, res.exploit_curve, "CFR");
    }

    // --- CFR+ ---
    {
        TrainConfig cfg;
        cfg.game       = Game::LEDUC;
        cfg.mode       = Mode::CFR_PLUS;
        cfg.iterations = 10000;
        cfg.eval_every = 200;
        std::cout << "\n[Leduc] CFR+         (" << cfg.iterations << " iters)\n";
        auto res = run_training(cfg);
        print_result("CFR+", res);
        append_csv(combined, res.exploit_curve, "CFR+");
    }

    // --- DCFR ---
    {
        TrainConfig cfg;
        cfg.game       = Game::LEDUC;
        cfg.mode       = Mode::DCFR;
        cfg.iterations = 10000;
        cfg.eval_every = 200;
        std::cout << "\n[Leduc] DCFR         (" << cfg.iterations << " iters)\n";
        auto res = run_training(cfg);
        print_result("DCFR", res);
        append_csv(combined, res.exploit_curve, "DCFR");
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
        std::cout << "  abstract infosets=" << res.num_abstract_infosets
                  << "  full infosets="     << res.num_original_infosets
                  << "  final_expl=" << std::scientific << std::setprecision(4)
                  << res.final_exploit << "\n";

        append_csv(combined, res.exploit_curve, label);
    };

    run("full",             abstraction::BucketScheme::NONE,     abstraction::ActionScheme::FULL);
    run("strength_buckets", abstraction::BucketScheme::STRENGTH,  abstraction::ActionScheme::FULL);
    run("no_raise",         abstraction::BucketScheme::NONE,     abstraction::ActionScheme::NO_RAISE);
    run("strength+no_raise",abstraction::BucketScheme::STRENGTH,  abstraction::ActionScheme::NO_RAISE);

    combined.close();
    std::cout << "\n  -> results/abstraction_comparison.csv\n";
}

// ---- main ----

int main(int argc, char** argv) {
    std::cout << "cfr-edge: Poker CFR/CFR+/DCFR Solver\n";
    std::cout << "=====================================\n";
    std::cout << "Algorithms: Vanilla CFR | CFR+ | DCFR (alpha=1.5, gamma=2)\n";

    std::filesystem::create_directories("results");

    bool do_kuhn = true, do_leduc = true, do_abstract = true;

    for (int i = 1; i < argc; i++) {
        if      (std::strcmp(argv[i], "--kuhn-only")  == 0) { do_leduc = false; do_abstract = false; }
        else if (std::strcmp(argv[i], "--leduc-only") == 0) { do_kuhn  = false; do_abstract = false; }
        else if (std::strcmp(argv[i], "--no-abstract")== 0) { do_abstract = false; }
    }

    if (do_kuhn)     run_kuhn_experiments();
    if (do_leduc)    run_leduc_experiments();
    if (do_abstract) run_abstraction_experiments();

    std::cout << "\nAll done. Run: python scripts/plot_convergence.py\n";
    return 0;
}
