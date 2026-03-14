#include "cfr.h"
#include "kuhn.h"
#include "leduc.h"
#include "abstraction.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <iomanip>

using namespace cfr;

static void write_csv(const std::string& path,
                      const std::vector<std::pair<int,double>>& curve,
                      const std::string& label) {
    std::ofstream out(path);
    out << "iteration,exploitability,label\n";
    out << std::scientific << std::setprecision(8);
    for (auto& [it, ex] : curve)
        out << it << "," << ex << "," << label << "\n";
    out.close();
    std::cout << "  wrote " << path << " (" << curve.size() << " points)\n";
}

static void append_csv(std::ofstream& out,
                       const std::vector<std::pair<int,double>>& curve,
                       const std::string& label) {
    out << std::scientific << std::setprecision(8);
    for (auto& [it, ex] : curve)
        out << it << "," << ex << "," << label << "\n";
}

static void run_kuhn_experiments() {
    std::cout << "\n========== KUHN POKER ==========\n";

    // CFR vs CFR+
    std::ofstream combined("results/kuhn_convergence.csv");
    combined << "iteration,exploitability,variant\n";

    {
        TrainConfig cfg;
        cfg.game = Game::KUHN;
        cfg.mode = Mode::CFR;
        cfg.iterations = 10000;
        cfg.eval_every = 50;

        std::cout << "\n[Kuhn] Vanilla CFR, " << cfg.iterations << " iters...\n";
        auto res = run_training(cfg);
        std::cout << "  done in " << std::fixed << std::setprecision(3)
                  << res.elapsed_seconds << "s, "
                  << res.num_infosets << " infosets, "
                  << "final expl=" << std::scientific << res.final_exploitability << "\n";
        append_csv(combined, res.exploit_curve, "CFR");

        // print strategy
        InfoMap nodes;
        kuhn::train(nodes, cfg.iterations, Mode::CFR, cfg.eval_every);
        kuhn::print_strategy(nodes);
    }

    {
        TrainConfig cfg;
        cfg.game = Game::KUHN;
        cfg.mode = Mode::CFR_PLUS;
        cfg.iterations = 10000;
        cfg.eval_every = 50;

        std::cout << "\n[Kuhn] CFR+, " << cfg.iterations << " iters...\n";
        auto res = run_training(cfg);
        std::cout << "  done in " << std::fixed << std::setprecision(3)
                  << res.elapsed_seconds << "s, "
                  << "final expl=" << std::scientific << res.final_exploitability << "\n";
        append_csv(combined, res.exploit_curve, "CFR+");
    }

    // SoA path comparison
    {
        TrainConfig cfg;
        cfg.game = Game::KUHN;
        cfg.mode = Mode::CFR;
        cfg.iterations = 10000;
        cfg.eval_every = 50;
        cfg.use_soa = true;

        std::cout << "\n[Kuhn] CFR (SoA/SIMD), " << cfg.iterations << " iters...\n";
        auto res = run_training(cfg);
        std::cout << "  done in " << std::fixed << std::setprecision(3)
                  << res.elapsed_seconds << "s, "
                  << "final expl=" << std::scientific << res.final_exploitability << "\n";
        append_csv(combined, res.exploit_curve, "CFR_SoA");
    }

    combined.close();
    std::cout << "\n  -> results/kuhn_convergence.csv\n";
}

static void run_leduc_experiments() {
    std::cout << "\n========== LEDUC HOLD'EM ==========\n";

    std::ofstream combined("results/leduc_convergence.csv");
    combined << "iteration,exploitability,variant\n";

    {
        TrainConfig cfg;
        cfg.game = Game::LEDUC;
        cfg.mode = Mode::CFR;
        cfg.iterations = 5000;
        cfg.eval_every = 100;

        std::cout << "\n[Leduc] Vanilla CFR, " << cfg.iterations << " iters...\n";
        auto res = run_training(cfg);
        std::cout << "  done in " << std::fixed << std::setprecision(3)
                  << res.elapsed_seconds << "s, "
                  << res.num_infosets << " infosets, "
                  << "final expl=" << std::scientific << res.final_exploitability << "\n";
        append_csv(combined, res.exploit_curve, "CFR");
    }

    {
        TrainConfig cfg;
        cfg.game = Game::LEDUC;
        cfg.mode = Mode::CFR_PLUS;
        cfg.iterations = 5000;
        cfg.eval_every = 100;

        std::cout << "\n[Leduc] CFR+, " << cfg.iterations << " iters...\n";
        auto res = run_training(cfg);
        std::cout << "  done in " << std::fixed << std::setprecision(3)
                  << res.elapsed_seconds << "s, "
                  << "final expl=" << std::scientific << res.final_exploitability << "\n";
        append_csv(combined, res.exploit_curve, "CFR+");
    }

    combined.close();
    std::cout << "\n  -> results/leduc_convergence.csv\n";
}

static void run_abstraction_experiments() {
    std::cout << "\n========== ABSTRACTION EXPERIMENTS ==========\n";

    std::ofstream combined("results/abstraction_comparison.csv");
    combined << "iteration,exploitability,scheme\n";

    // Full game baseline
    {
        abstraction::AbstractConfig cfg;
        cfg.buckets = abstraction::BucketScheme::NONE;
        cfg.actions = abstraction::ActionScheme::FULL;
        cfg.cfr_mode = Mode::CFR_PLUS;
        cfg.iterations = 3000;
        cfg.eval_every = 100;

        std::cout << "\n[Abstraction] Full game (no abstraction)...\n";
        auto res = abstraction::run_abstraction_experiment(cfg);
        std::cout << "  abstract infosets: " << res.num_abstract_infosets
                  << ", original: " << res.num_original_infosets
                  << ", final expl=" << std::scientific << res.final_exploit << "\n";
        append_csv(combined, res.exploit_curve, "full");
    }

    // Strength bucketing
    {
        abstraction::AbstractConfig cfg;
        cfg.buckets = abstraction::BucketScheme::STRENGTH;
        cfg.actions = abstraction::ActionScheme::FULL;
        cfg.cfr_mode = Mode::CFR_PLUS;
        cfg.iterations = 3000;
        cfg.eval_every = 100;

        std::cout << "\n[Abstraction] Strength buckets (pair/high/low)...\n";
        auto res = abstraction::run_abstraction_experiment(cfg);
        std::cout << "  abstract infosets: " << res.num_abstract_infosets
                  << ", original: " << res.num_original_infosets
                  << ", final expl=" << std::scientific << res.final_exploit << "\n";
        append_csv(combined, res.exploit_curve, "strength_buckets");
    }

    // No-raise action abstraction
    {
        abstraction::AbstractConfig cfg;
        cfg.buckets = abstraction::BucketScheme::NONE;
        cfg.actions = abstraction::ActionScheme::NO_RAISE;
        cfg.cfr_mode = Mode::CFR_PLUS;
        cfg.iterations = 3000;
        cfg.eval_every = 100;

        std::cout << "\n[Abstraction] No-raise action abstraction...\n";
        auto res = abstraction::run_abstraction_experiment(cfg);
        std::cout << "  abstract infosets: " << res.num_abstract_infosets
                  << ", original: " << res.num_original_infosets
                  << ", final expl=" << std::scientific << res.final_exploit << "\n";
        append_csv(combined, res.exploit_curve, "no_raise");
    }

    // Both: strength buckets + no-raise
    {
        abstraction::AbstractConfig cfg;
        cfg.buckets = abstraction::BucketScheme::STRENGTH;
        cfg.actions = abstraction::ActionScheme::NO_RAISE;
        cfg.cfr_mode = Mode::CFR_PLUS;
        cfg.iterations = 3000;
        cfg.eval_every = 100;

        std::cout << "\n[Abstraction] Strength buckets + no-raise...\n";
        auto res = abstraction::run_abstraction_experiment(cfg);
        std::cout << "  abstract infosets: " << res.num_abstract_infosets
                  << ", original: " << res.num_original_infosets
                  << ", final expl=" << std::scientific << res.final_exploit << "\n";
        append_csv(combined, res.exploit_curve, "strength+no_raise");
    }

    combined.close();
    std::cout << "\n  -> results/abstraction_comparison.csv\n";
}

int main(int argc, char** argv) {
    std::cout << "cfr-edge: Poker CFR/CFR+ Solver\n";
    std::cout << "================================\n";

    bool do_kuhn = true, do_leduc = true, do_abstract = true;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--kuhn-only") == 0) {
            do_leduc = false; do_abstract = false;
        } else if (std::strcmp(argv[i], "--leduc-only") == 0) {
            do_kuhn = false; do_abstract = false;
        } else if (std::strcmp(argv[i], "--no-abstract") == 0) {
            do_abstract = false;
        }
    }

    if (do_kuhn) run_kuhn_experiments();
    if (do_leduc) run_leduc_experiments();
    if (do_abstract) run_abstraction_experiments();

    std::cout << "\nAll done.\n";
    return 0;
}
