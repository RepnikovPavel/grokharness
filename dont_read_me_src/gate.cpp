// SPDX-License-Identifier: 0BSD
#include "gate.hpp"

#include "config.hpp"
#include "output.hpp"
#include "store.hpp"
#include "tsv.hpp"
#include "util.hpp"

namespace ghar {

int cmd_init(const std::string& root, bool with_scaffold)
{
    Store store(root);
    if (!store.ensure())
        return EXIT_IO;
    // touch empty tables
    if (!path_exists(join_path(store.dir(), "results.tsv"))) {
        write_file(join_path(store.dir(), "results.tsv"),
                   "id\tts\tkind\tname\tstatus\texit_code\tmetrics\tdetail\n");
    }
    if (!path_exists(join_path(store.dir(), "claims.tsv"))) {
        write_file(join_path(store.dir(), "claims.tsv"),
                   "name\tkind\tstatus\tmetrics\tupdated_at\tsource_id\n");
    }
    CheckResult r;
    r.kind = "init";
    r.name = "session";
    r.status = "ok";
    r.detail = "initialized " + store.dir();
    r.metrics["dir"] = store.dir();
    const int rc = finish_check(store, r, false);
    if (rc != EXIT_OK)
        return rc;
    // Best practice: project oracles config exists from day one (Aider-style).
    if (with_scaffold) {
        // Avoid circular call into verify::cmd_scaffold — write config only here;
        // full integrations via `ghar scaffold`.
        write_default_config(root, false);
        r.metrics["config"] = join_path(root, ".ghar/config");
    }
    return EXIT_OK;
}

namespace {

// Bookkeeping kinds do not satisfy the delivery gate by themselves.
bool is_substantive_kind(const std::string& kind)
{
    // lint/build/verify = Aider-style pipeline steps from `ghar verify`
    return kind == "compile" || kind == "cuda" || kind == "symbols" || kind == "import" ||
           kind == "run" || kind == "bench" || kind == "assert" || kind == "test" ||
           kind == "python" || kind == "torch" ||
           kind == "lint" || kind == "build" || kind == "verify";
}

}  // namespace

int cmd_gate(const std::string& root, bool pretty)
{
    Store store(root);
    auto claims = store.load_claims();
    int ok = 0, fail = 0, substantive = 0;
    std::vector<Claim> counted;
    for (const auto& c : claims) {
        if (!is_substantive_kind(c.kind))
            continue;
        counted.push_back(c);
        ++substantive;
        if (c.status == "ok" || c.status == "skip")
            ++ok;
        else
            ++fail;
    }
    store.write_session_status(fail == 0 && substantive > 0 ? "ok" : "fail", fail, ok);
    print_gate(counted, ok, fail, pretty);

    if (substantive == 0) {
        FieldMap f;
        f["status"] = "fail";
        f["reason"] = "no_substantive_claims";
        f["hint"] = "run compile|cuda|import|python|torch|run|bench|assert|test|symbols first";
        if (!pretty)
            emit_row("gate", f);
        else
            std::fputs(
                "gate: no substantive claims — run compile/cuda/import/python/torch/bench/… first\n",
                stdout);
        std::fprintf(stderr,
                     "\n======== ghar FEEDBACK (gate) ========\n"
                     "no substantive claims yet — run compile|import|python|torch|test|… first\n"
                     "======== exit 4 — do not deliver ========\n\n");
        return EXIT_FAIL;
    }
    if (fail != 0) {
        // List failing claims so agents know what to fix or ghar reset.
        std::fprintf(stderr,
                     "\n======== ghar FEEDBACK (gate) ========\n"
                     "gate FAIL: %d failing claim(s) (ok=%d). Failed names:\n",
                     fail, ok);
        int shown = 0;
        for (const auto& c : counted) {
            if (c.status == "ok" || c.status == "skip")
                continue;
            std::fprintf(stderr, "  - %s (%s) status=%s\n", c.name.c_str(), c.kind.c_str(),
                         c.status.c_str());
            if (++shown >= 20) {
                std::fprintf(stderr, "  … (%d more; ghar report --claims)\n", fail - shown);
                break;
            }
        }
        std::fprintf(stderr,
                     "Fix those claims, or clear intentional fails with: ghar reset\n"
                     "======== exit 4 — do not deliver ========\n\n");
        return EXIT_FAIL;
    }
    return EXIT_OK;
}

int cmd_reset(const std::string& root, bool keep_results)
{
    Store store(root);
    if (!store.ensure())
        return EXIT_IO;
    write_file(join_path(store.dir(), "claims.tsv"),
               "name\tkind\tstatus\tmetrics\tupdated_at\tsource_id\n");
    write_file(join_path(store.dir(), "session.tsv"), "ts\tstatus\tok\tfail\n");
    if (!keep_results) {
        write_file(join_path(store.dir(), "results.tsv"),
                   "id\tts\tkind\tname\tstatus\texit_code\tmetrics\tdetail\n");
    }
    CheckResult r;
    r.kind = "reset";
    r.name = "session";
    r.status = "ok";
    r.detail = keep_results ? "claims cleared (results kept)" : "claims+results cleared";
    return finish_check(store, r, false);
}

int cmd_report(const std::string& root, bool pretty, bool claims_only)
{
    Store store(root);
    if (claims_only) {
        print_claims(store.load_claims(), pretty);
        return EXIT_OK;
    }
    if (pretty) {
        std::fputs("== claims ==\n", stdout);
        print_claims(store.load_claims(), true);
        std::fputs("\n== recent results ==\n", stdout);
        auto res = store.load_results();
        // last 20
        if (res.size() > 20)
            res.erase(res.begin(), res.end() - 20);
        print_results(res, true);
    } else {
        print_claims(store.load_claims(), false);
        for (const auto& r : store.load_results())
            print_check_result(r, false);
    }
    return EXIT_OK;
}

}  // namespace ghar
