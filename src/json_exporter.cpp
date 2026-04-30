// json_exporter.cpp
// Standalone tool that runs all CFR experiments and writes strategy JSON bundles
// into the directory consumed by the Next.js web app.
//
// Usage:
//   ./json_exporter --out ../web/public/strategies/
//
// Outputs:
//   kuhn_cfr.json, kuhn_cfr_plus.json, kuhn_dcfr.json
//   leduc_cfr.json, leduc_cfr_plus.json, leduc_dcfr.json
//   holdem_mccfr.json
//   meta.json   (index of all files + game summaries)

#include "cfr.h"
#include "kuhn.h"
#include "leduc.h"
#include "holdem.h"
#include "abstraction.h"
#include "json_output.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <set>

using namespace cfr;
using json = nlohmann::json;

// ---- Helpers ----

static void write_json(const std::string& path, const json& j) {
    std::ofstream f(path);
    f << j.dump(2);
    std::cout << "  wrote " << path << "\n";
}

static std::string mode_str(Mode m) {
    if (m == Mode::CFR)      return "CFR";
    if (m == Mode::CFR_PLUS) return "CFR+";
    return "DCFR";
}

// ---- Training helpers that also capture snapshots ----

struct SnapshotTrainResult {
    std::vector<std::pair<int,double>> curve;
    std::vector<SnapshotEntry> snapshots;
    InfoMap final_nodes;
    size_t  num_infosets;
    double  elapsed;
};

// Snapshot iterations: capture strategy at these checkpoints.
static const std::vector<int> KUHN_SNAP_ITERS   = {50, 200, 500, 1000, 2000, 5000, 10000};
static const std::vector<int> LEDUC_SNAP_ITERS   = {100, 500, 1000, 2000, 5000, 10000};
static const std::vector<int> HOLDEM_SNAP_ITERS  = {200, 500, 1000, 2000, 5000};

// Run Kuhn training and capture snapshots at key iterations.
static SnapshotTrainResult run_kuhn_snapshotted(Mode mode, int iterations, int eval_every) {
    SnapshotTrainResult res;
    constexpr int deals[][2] = {{0,1},{0,2},{1,0},{1,2},{2,0},{2,1}};
    auto t0 = std::chrono::high_resolution_clock::now();

    InfoMap nodes;
    const std::set<int> snap_set(KUHN_SNAP_ITERS.begin(), KUHN_SNAP_ITERS.end());

    for (int t = 1; t <= iterations; t++) {
        if (mode == Mode::DCFR) {
            double alpha = 1.5;
            double pt    = std::pow((double)t, alpha);
            double factor = pt / (pt + 1.0);
            for (auto& [key, nd] : nodes)
                for (int a = 0; a < nd.num_actions; a++)
                    nd.regret_sum[a] = std::max(nd.regret_sum[a], 0.0) * factor;
        }
        for (const auto& d : deals)
            kuhn::cfr_traverse(nodes, d[0], d[1], "", 1.0, 1.0, mode, t);
        if (mode == Mode::CFR_PLUS || mode == Mode::DCFR)
            for (auto& [key, nd] : nodes) nd.floor_regrets();

        if (t == 1 || t % eval_every == 0 || t == iterations)
            res.curve.push_back({t, kuhn::exploitability(nodes)});

        if (snap_set.count(t))
            res.snapshots.push_back({t, nodes});
    }

    res.final_nodes   = nodes;
    res.num_infosets  = nodes.size();
    auto t1 = std::chrono::high_resolution_clock::now();
    res.elapsed = std::chrono::duration<double>(t1 - t0).count();
    return res;
}

// Run Leduc training and capture snapshots.
static SnapshotTrainResult run_leduc_snapshotted(Mode mode, int iterations, int eval_every) {
    SnapshotTrainResult res;
    auto deals = leduc::all_deals();
    auto t0 = std::chrono::high_resolution_clock::now();

    InfoMap nodes;
    const std::set<int> snap_set(LEDUC_SNAP_ITERS.begin(), LEDUC_SNAP_ITERS.end());

    for (int t = 1; t <= iterations; t++) {
        if (mode == Mode::DCFR) {
            double alpha  = 1.5;
            double pt     = std::pow((double)t, alpha);
            double factor = pt / (pt + 1.0);
            for (auto& [key, nd] : nodes)
                for (int a = 0; a < nd.num_actions; a++)
                    nd.regret_sum[a] = std::max(nd.regret_sum[a], 0.0) * factor;
        }
        for (const auto& deal : deals) {
            int chips[2] = {1, 1};
            leduc::cfr_traverse(nodes, deal.cards, 0, "", 0, chips,
                                 deal.prob, deal.prob, mode, t);
        }
        if (mode == Mode::CFR_PLUS || mode == Mode::DCFR)
            for (auto& [key, nd] : nodes) nd.floor_regrets();

        if (t == 1 || t % eval_every == 0 || t == iterations)
            res.curve.push_back({t, leduc::exploitability(nodes)});

        if (snap_set.count(t))
            res.snapshots.push_back({t, nodes});
    }

    res.final_nodes  = nodes;
    res.num_infosets = nodes.size();
    auto t1 = std::chrono::high_resolution_clock::now();
    res.elapsed = std::chrono::duration<double>(t1 - t0).count();
    return res;
}

// ---- Main ----

int main(int argc, char** argv) {
    std::string out_dir = "./strategies";
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out_dir = argv[++i];
    }

    std::filesystem::create_directories(out_dir);
    std::cout << "cfr-edge json exporter\n";
    std::cout << "output: " << out_dir << "\n\n";

    const std::vector<std::string> kuhn_actions  = {"pass", "bet"};
    const std::vector<std::string> leduc_actions  = {"fold", "check/call", "bet/raise"};
    const std::vector<std::string> holdem_actions = {"fold","check/call","bet33","bet75","bet100","allin"};

    json meta;
    meta["games"] = json::array();

    // ============================================================
    // KUHN POKER
    // ============================================================
    std::cout << "=== Kuhn Poker ===\n";
    json kuhn_entry;
    kuhn_entry["game"]  = "kuhn";
    kuhn_entry["files"] = json::array();

    for (Mode mode : {Mode::CFR, Mode::CFR_PLUS, Mode::DCFR}) {
        std::string algo = mode_str(mode);
        std::cout << "  Running " << algo << " (10000 iters)...\n";

        auto res = run_kuhn_snapshotted(mode, 10000, 100);

        double rate = estimate_convergence_rate(res.curve);
        std::cout << "    infosets=" << res.num_infosets
                  << "  final_expl=" << std::scientific << std::setprecision(4)
                  << res.curve.back().second
                  << "  rate=" << std::fixed << std::setprecision(3) << rate
                  << "  time=" << res.elapsed << "s\n";

        JsonOutputConfig cfg;
        cfg.game             = "kuhn";
        cfg.algorithm        = algo;
        cfg.total_iterations = 10000;
        cfg.convergence_rate = rate;
        cfg.action_labels    = kuhn_actions;
        cfg.snapshot_iters   = KUHN_SNAP_ITERS;

        std::string fname = "kuhn_" + std::string(mode == Mode::CFR ? "cfr" :
                                                   mode == Mode::CFR_PLUS ? "cfr_plus" : "dcfr")
                          + ".json";
        json out = build_json_output(cfg, res.curve, res.snapshots, res.final_nodes);
        out["num_infosets"]      = res.num_infosets;
        out["elapsed_seconds"]   = std::round(res.elapsed * 100.0) / 100.0;
        out["final_exploitability"] = res.curve.back().second;
        write_json(out_dir + "/" + fname, out);

        json fentry;
        fentry["file"]      = fname;
        fentry["algorithm"] = algo;
        fentry["final_exploitability"] = res.curve.back().second;
        kuhn_entry["files"].push_back(fentry);
    }
    kuhn_entry["description"] = "3-card poker with 12 information sets. Unique analytically known Nash equilibrium.";
    kuhn_entry["nash_exploitability"] = 0.0;
    meta["games"].push_back(kuhn_entry);

    // ============================================================
    // LEDUC HOLD'EM
    // ============================================================
    std::cout << "\n=== Leduc Hold'em ===\n";
    json leduc_entry;
    leduc_entry["game"]  = "leduc";
    leduc_entry["files"] = json::array();

    for (Mode mode : {Mode::CFR, Mode::CFR_PLUS, Mode::DCFR}) {
        std::string algo = mode_str(mode);
        std::cout << "  Running " << algo << " (10000 iters)...\n";

        auto res = run_leduc_snapshotted(mode, 10000, 200);

        double rate = estimate_convergence_rate(res.curve);
        std::cout << "    infosets=" << res.num_infosets
                  << "  final_expl=" << std::scientific << std::setprecision(4)
                  << res.curve.back().second
                  << "  rate=" << std::fixed << std::setprecision(3) << rate
                  << "  time=" << res.elapsed << "s\n";

        JsonOutputConfig cfg;
        cfg.game             = "leduc";
        cfg.algorithm        = algo;
        cfg.total_iterations = 10000;
        cfg.convergence_rate = rate;
        cfg.action_labels    = leduc_actions;
        cfg.snapshot_iters   = LEDUC_SNAP_ITERS;

        std::string fname = "leduc_" + std::string(mode == Mode::CFR ? "cfr" :
                                                    mode == Mode::CFR_PLUS ? "cfr_plus" : "dcfr")
                          + ".json";
        json out = build_json_output(cfg, res.curve, res.snapshots, res.final_nodes);
        out["num_infosets"]         = res.num_infosets;
        out["elapsed_seconds"]      = std::round(res.elapsed * 100.0) / 100.0;
        out["final_exploitability"] = res.curve.back().second;
        write_json(out_dir + "/" + fname, out);

        json fentry;
        fentry["file"]      = fname;
        fentry["algorithm"] = algo;
        fentry["final_exploitability"] = res.curve.back().second;
        leduc_entry["files"].push_back(fentry);
    }
    leduc_entry["description"] = "Simplified Hold'em with 2 betting rounds and a public card. 936 information sets.";
    meta["games"].push_back(leduc_entry);

    // ============================================================
    // TEXAS HOLD'EM (MCCFR)
    // ============================================================
    std::cout << "\n=== Texas Hold'em (MCCFR) ===\n";
    {
        InfoMap nodes;
        std::cout << "  Running DCFR MCCFR (1000 iters)...\n";
        auto t0 = std::chrono::high_resolution_clock::now();
        auto curve = holdem::train_holdem(nodes, 1000, Mode::DCFR, 200);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();

        double rate = estimate_convergence_rate(curve);
        std::cout << "  infosets=" << nodes.size()
                  << "  time=" << elapsed << "s\n";

        // Build snapshot entries at the sampled iterations
        // For holdem, we only have exploitability at eval_every points,
        // so snapshots are the same as convergence checkpoints.
        std::vector<SnapshotEntry> snapshots;
        // We can't easily replay holdem snapshots without storing intermediate
        // InfoMaps (expensive). Use the final nodes as the only snapshot.
        snapshots.push_back({5000, nodes});

        JsonOutputConfig cfg;
        cfg.game             = "holdem";
        cfg.algorithm        = "DCFR";
        cfg.total_iterations = 5000;
        cfg.convergence_rate = rate;
        cfg.action_labels    = holdem_actions;
        cfg.snapshot_iters   = {5000};

        json out = build_json_output(cfg, curve, snapshots, nodes);
        out["total_iterations"]     = 1000;
        out["num_infosets"]         = nodes.size();
        out["elapsed_seconds"]      = std::round(elapsed * 100.0) / 100.0;
        out["final_exploitability"] = curve.empty() ? 0.0 : curve.back().second;

        // Add preflop range summary (useful for the frontend range chart)
        json preflop_summary = json::object();
        for (const auto& [key, nd] : nodes) {
            if (key.substr(0, 2) == "PF") {
                double s[6];
                nd.get_average_strategy(s);
                json actions_obj;
                for (int a = 0; a < nd.num_actions; a++) {
                    static const char* names[] = {"fold","check/call","bet33","bet75","bet100","allin"};
                    actions_obj[names[a]] = std::round(s[a] * 10000.0) / 10000.0;
                }
                preflop_summary[key] = actions_obj;
            }
        }
        out["preflop_summary"] = preflop_summary;
        write_json(out_dir + "/holdem_dcfr.json", out);

        json holdem_entry;
        holdem_entry["game"]        = "holdem";
        holdem_entry["description"] = "Heads-up No-Limit Texas Hold'em. 169 preflop + EHS2 postflop abstraction, MCCFR external sampling.";
        holdem_entry["files"]       = json::array();
        json fentry;
        fentry["file"]      = "holdem_dcfr.json";
        fentry["algorithm"] = "DCFR";
        fentry["final_exploitability"] = curve.empty() ? 0.0 : curve.back().second;
        holdem_entry["files"].push_back(fentry);
        meta["games"].push_back(holdem_entry);
    }

    // ============================================================
    // meta.json — index for the frontend
    // ============================================================
    meta["generated_at"] = "2026-04-30";
    meta["version"]      = "2.0";
    write_json(out_dir + "/meta.json", meta);

    std::cout << "\nAll done. Strategies written to: " << out_dir << "\n";
    return 0;
}
