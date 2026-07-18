// SPDX-License-Identifier: 0BSD
#include "bench_check.hpp"

#include "store.hpp"
#include "util.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ghar {
namespace {

struct Stats {
    double mean = 0;
    double stddev = 0;
    double min_v = 0;
    double max_v = 0;
    double median = 0;
    int n = 0;
    int fails = 0;
};

Stats summarize(std::vector<double> xs)
{
    Stats s;
    s.n = static_cast<int>(xs.size());
    if (xs.empty())
        return s;
    std::sort(xs.begin(), xs.end());
    s.min_v = xs.front();
    s.max_v = xs.back();
    s.mean = std::accumulate(xs.begin(), xs.end(), 0.0) / static_cast<double>(xs.size());
    double var = 0;
    for (double x : xs)
        var += (x - s.mean) * (x - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(xs.size()));
    if (xs.size() % 2 == 1)
        s.median = xs[xs.size() / 2];
    else
        s.median = 0.5 * (xs[xs.size() / 2 - 1] + xs[xs.size() / 2]);
    return s;
}

std::vector<double> time_runs(const BenchOpts& opts, bool baseline, int& fails)
{
    std::vector<double> samples;
    fails = 0;
    const int total = opts.warmup + opts.repeat;
    for (int i = 0; i < total; ++i) {
        CmdResult cr;
        if (baseline) {
            if (!opts.baseline_shell.empty())
                cr = run_shell(opts.baseline_shell, opts.timeout_sec, opts.root);
            else
                cr = run_cmd(opts.baseline_argv, opts.timeout_sec, opts.root);
        } else {
            if (!opts.shell_cmd.empty())
                cr = run_shell(opts.shell_cmd, opts.timeout_sec, opts.root);
            else
                cr = run_cmd(opts.argv, opts.timeout_sec, opts.root);
        }
        if (cr.exit_code != 0 || cr.timed_out) {
            ++fails;
            continue;
        }
        if (i >= opts.warmup)
            samples.push_back(cr.wall_ms);
    }
    return samples;
}

void put_stats(FieldMap& m, const std::string& prefix, const Stats& s)
{
    m[prefix + "n"] = std::to_string(s.n);
    m[prefix + "mean_ms"] = std::to_string(s.mean);
    m[prefix + "std_ms"] = std::to_string(s.stddev);
    m[prefix + "min_ms"] = std::to_string(s.min_v);
    m[prefix + "max_ms"] = std::to_string(s.max_v);
    m[prefix + "median_ms"] = std::to_string(s.median);
    if (s.mean > 0)
        m[prefix + "cv"] = std::to_string(s.stddev / s.mean);  // coefficient of variation
}

}  // namespace

int cmd_bench(const BenchOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "bench";
    r.name = opts.name.empty() ? "bench" : opts.name;

    if (opts.argv.empty() && opts.shell_cmd.empty()) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "no command to benchmark";
        return finish_check(store, r, opts.pretty);
    }
    if (opts.repeat < 1) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "repeat must be >= 1";
        return finish_check(store, r, opts.pretty);
    }

    r.metrics["warmup"] = std::to_string(opts.warmup);
    r.metrics["repeat"] = std::to_string(opts.repeat);
    r.metrics["cmd"] = opts.shell_cmd.empty() ? join(opts.argv, " ") : opts.shell_cmd;

    int fails = 0;
    auto samples = time_runs(opts, false, fails);
    auto st = summarize(samples);
    st.fails = fails;
    put_stats(r.metrics, "", st);
    r.metrics["fails"] = std::to_string(fails);

    const bool has_baseline = !opts.baseline_argv.empty() || !opts.baseline_shell.empty();
    if (has_baseline) {
        r.metrics["baseline_cmd"] =
            opts.baseline_shell.empty() ? join(opts.baseline_argv, " ") : opts.baseline_shell;
        int bfails = 0;
        auto bsamples = time_runs(opts, true, bfails);
        auto bst = summarize(bsamples);
        put_stats(r.metrics, "baseline_", bst);
        r.metrics["baseline_fails"] = std::to_string(bfails);
        if (st.mean > 0 && bst.mean > 0) {
            const double speedup = bst.mean / st.mean;
            r.metrics["speedup"] = std::to_string(speedup);
            r.metrics["speedup_pct"] = std::to_string((speedup - 1.0) * 100.0);
        }
    }

    if (st.n == 0) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "no successful samples";
        return finish_check(store, r, opts.pretty);
    }

    r.status = "ok";
    r.exit_code = EXIT_OK;
    r.detail = "mean_ms=" + r.metrics["mean_ms"] +
               (r.metrics.count("speedup") ? " speedup=" + r.metrics["speedup"] : "");
    return finish_check(store, r, opts.pretty);
}

}  // namespace ghar
