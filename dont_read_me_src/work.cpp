// SPDX-License-Identifier: 0BSD
#include "work.hpp"

#include "config.hpp"
#include "output.hpp"
#include "store.hpp"
#include "tsv.hpp"
#include "util.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>

namespace ghar {
namespace {

using KV = std::map<std::string, std::string>;

std::string work_path(const std::string& root)
{
    return join_path(join_path(root, ".ghar"), "work.tsv");
}

std::int64_t now_unix()
{
    return static_cast<std::int64_t>(std::time(nullptr));
}

KV load_kv(const std::string& path)
{
    KV m;
    if (!is_file(path))
        return m;
    auto table = read_tsv_file(path);
    // either header key value rows, or key=value style single column mishap
    if (table.header.size() >= 2) {
        for (const auto& row : table.rows) {
            if (row.size() >= 2)
                m[row[0]] = row[1];
        }
    }
    return m;
}

bool save_kv(const std::string& path, const KV& m)
{
    TsvTable t;
    t.header = {"key", "value"};
    for (const auto& kv : m)
        t.rows.push_back({kv.first, kv.second});
    return write_tsv_file(path, t);
}

int get_i(const KV& m, const std::string& k, int def)
{
    auto it = m.find(k);
    if (it == m.end())
        return def;
    int v = def;
    if (!parse_int(it->second, v))
        return def;
    return v;
}

std::int64_t get_i64(const KV& m, const std::string& k, std::int64_t def)
{
    auto it = m.find(k);
    if (it == m.end() || it->second.empty())
        return def;
    char* end = nullptr;
    const long long v = std::strtoll(it->second.c_str(), &end, 10);
    if (!end || end == it->second.c_str() || *end != '\0')
        return def;
    return static_cast<std::int64_t>(v);
}

std::string get_s(const KV& m, const std::string& k, const std::string& def = "")
{
    auto it = m.find(k);
    return it == m.end() ? def : it->second;
}

void emit_work(const KV& m, bool pretty, const std::string& kind = "work")
{
    const auto now = now_unix();
    const auto started = get_i64(m, "started_at_unix", 0);
    const double elapsed_min =
        started > 0 ? static_cast<double>(now - started) / 60.0 : 0.0;
    const int min_m = get_i(m, "min_minutes", 0);
    const double remain = static_cast<double>(min_m) - elapsed_min;

    if (pretty) {
        std::printf("work status: %s\n", get_s(m, "status", "none").c_str());
        std::printf("  goal:              %s\n", get_s(m, "goal", "-").c_str());
        std::printf("  elapsed_min:       %.2f\n", elapsed_min);
        std::printf("  min_minutes:       %d  (remain %.2f)\n", min_m, remain > 0 ? remain : 0.0);
        std::printf("  verify_ok:         %d / %d\n", get_i(m, "verify_ok_count", 0),
                    get_i(m, "min_verify_ok", 0));
        std::printf("  heartbeats:        %d / %d\n", get_i(m, "heartbeat_count", 0),
                    get_i(m, "min_heartbeats", 0));
        std::printf("  last_heartbeat:    %s\n", get_s(m, "last_heartbeat_iso", "-").c_str());
        std::printf("  started:           %s\n", get_s(m, "started_at_iso", "-").c_str());
        return;
    }
    FieldMap f;
    f["status"] = get_s(m, "status", "none");
    f["goal"] = get_s(m, "goal");
    f["elapsed_min"] = std::to_string(elapsed_min);
    f["min_minutes"] = std::to_string(min_m);
    f["remain_min"] = std::to_string(remain > 0 ? remain : 0.0);
    f["verify_ok"] = std::to_string(get_i(m, "verify_ok_count", 0));
    f["min_verify_ok"] = std::to_string(get_i(m, "min_verify_ok", 0));
    f["heartbeats"] = std::to_string(get_i(m, "heartbeat_count", 0));
    f["min_heartbeats"] = std::to_string(get_i(m, "min_heartbeats", 0));
    f["started_at"] = get_s(m, "started_at_iso");
    emit_row(kind, f);
}

struct QuotaResult {
    bool ok = false;
    std::string reason;
    FieldMap metrics;
};

QuotaResult check_quotas(const KV& m, const WorkConfig& wc)
{
    QuotaResult q;
    if (get_s(m, "status") != "active") {
        q.ok = true;
        q.reason = "no_active_session";
        return q;
    }
    const auto now = now_unix();
    const auto started = get_i64(m, "started_at_unix", 0);
    const double elapsed_min =
        started > 0 ? static_cast<double>(now - started) / 60.0 : 0.0;
    const int min_m = get_i(m, "min_minutes", wc.min_work_minutes);
    const int min_v = get_i(m, "min_verify_ok", wc.min_verify_ok);
    const int min_h = get_i(m, "min_heartbeats", wc.min_heartbeats);
    const int v = get_i(m, "verify_ok_count", 0);
    const int h = get_i(m, "heartbeat_count", 0);
    const auto last_hb = get_i64(m, "last_heartbeat_unix", started);

    q.metrics["elapsed_min"] = std::to_string(elapsed_min);
    q.metrics["min_minutes"] = std::to_string(min_m);
    q.metrics["verify_ok"] = std::to_string(v);
    q.metrics["min_verify_ok"] = std::to_string(min_v);
    q.metrics["heartbeats"] = std::to_string(h);
    q.metrics["min_heartbeats"] = std::to_string(min_h);

    if (elapsed_min + 1e-9 < static_cast<double>(min_m)) {
        q.ok = false;
        q.reason = "early_stop: elapsed_min=" + std::to_string(elapsed_min) + " < min_minutes=" +
                   std::to_string(min_m) +
                   " — continue working; ghar work heartbeat; do not deliver yet";
        return q;
    }
    if (v < min_v) {
        q.ok = false;
        q.reason = "need_more_verify: verify_ok=" + std::to_string(v) + " < min_verify_ok=" +
                   std::to_string(min_v) + " — run ghar verify until quota";
        return q;
    }
    if (h < min_h) {
        q.ok = false;
        q.reason = "need_more_heartbeats: heartbeats=" + std::to_string(h) +
                   " < min_heartbeats=" + std::to_string(min_h) +
                   " — call ghar work heartbeat during the session";
        return q;
    }
    if (wc.heartbeat_max_gap_sec > 0 && last_hb > 0 &&
        (now - last_hb) > wc.heartbeat_max_gap_sec) {
        q.ok = false;
        q.reason = "stale_heartbeat: last heartbeat too old — run ghar work heartbeat then done";
        return q;
    }
    q.ok = true;
    q.reason = "quotas_met";
    return q;
}

}  // namespace

WorkConfig load_work_config(const std::string& root)
{
    WorkConfig w;
    // Reuse project config file keys
    const std::vector<std::string> paths = {join_path(root, "ghar.conf"),
                                            join_path(root, ".ghar/config")};
    for (const auto& p : paths) {
        if (!is_file(p))
            continue;
        for (const auto& raw : split(read_file(p), '\n')) {
            auto line = trim(raw);
            if (line.empty() || line[0] == '#')
                continue;
            auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            auto key = trim(line.substr(0, eq));
            auto val = trim(line.substr(eq + 1));
            int iv = 0;
            if (key == "min_work_minutes" && parse_int(val, iv))
                w.min_work_minutes = iv;
            else if (key == "min_verify_ok" && parse_int(val, iv))
                w.min_verify_ok = iv;
            else if (key == "min_heartbeats" && parse_int(val, iv))
                w.min_heartbeats = iv;
            else if (key == "heartbeat_max_gap_sec" && parse_int(val, iv))
                w.heartbeat_max_gap_sec = iv;
            else if (key == "enforce_work_on_verify")
                w.enforce_on_verify = (val == "1" || val == "true" || val == "yes");
        }
        break;  // first existing file wins (ghar.conf preferred)
    }
    // Env overrides for campaign mode
    const std::string em = getenv_or("GHAR_MIN_WORK_MINUTES", "");
    if (!em.empty()) {
        int iv = 0;
        if (parse_int(em, iv) && iv >= 0)
            w.min_work_minutes = iv;
    }
    return w;
}

void work_note_verify_ok(const std::string& root)
{
    Store store(root);
    store.ensure();
    auto m = load_kv(work_path(root));
    if (get_s(m, "status") != "active")
        return;
    int v = get_i(m, "verify_ok_count", 0) + 1;
    m["verify_ok_count"] = std::to_string(v);
    m["last_verify_ok_iso"] = now_iso8601();
    m["last_verify_ok_unix"] = std::to_string(now_unix());
    save_kv(work_path(root), m);
}

int work_check_delivery(const std::string& root, bool pretty, std::string& reason_out)
{
    auto m = load_kv(work_path(root));
    if (get_s(m, "status") != "active") {
        reason_out = "no_active_session";
        return EXIT_OK;
    }
    auto q = check_quotas(m, load_work_config(root));
    reason_out = q.reason;
    if (!q.ok) {
        std::fprintf(stderr,
                     "\n======== ghar work EARLY-STOP BLOCK ========\n"
                     "%s\n"
                     "Continue autonomous work. Commands:\n"
                     "  ghar work status\n"
                     "  ghar work heartbeat --note '...'\n"
                     "  ghar verify\n"
                     "  ghar work done     # only when quotas met\n"
                     "======== do NOT deliver to user yet ========\n\n",
                     q.reason.c_str());
        if (pretty)
            emit_work(m, true);
        else {
            FieldMap f = q.metrics;
            f["status"] = "blocked";
            f["reason"] = q.reason;
            emit_row("work_block", f);
        }
        return EXIT_FAIL;
    }
    return EXIT_OK;
}

int cmd_work_start(const WorkStartOpts& opts)
{
    Store store(opts.root);
    store.ensure();
    auto wc = load_work_config(opts.root);
    auto path = work_path(opts.root);
    auto existing = load_kv(path);
    if (get_s(existing, "status") == "active" && !opts.force) {
        std::fprintf(stderr, "ghar work: session already active (use --force to restart)\n");
        emit_work(existing, opts.pretty);
        return EXIT_USAGE;
    }

    // min_minutes: CLI --minutes sets value including 0 (zero wait for tests/CI).
    // Sentinel: WorkStartOpts.min_minutes < 0 means "use config" (default -1 after we set it).
    int min_m = opts.min_minutes >= 0 ? opts.min_minutes : wc.min_work_minutes;
    int min_v = opts.min_verify_ok >= 0 ? opts.min_verify_ok : wc.min_verify_ok;
    int min_h = opts.min_heartbeats >= 0 ? opts.min_heartbeats : wc.min_heartbeats;
    if (opts.min_heartbeats < 0 && min_m > 0) {
        // ~1 heartbeat per 10 minutes, at least config default
        const int scaled = std::max(3, min_m / 10);
        min_h = std::max(wc.min_heartbeats, scaled);
    }

    // Env GHAR_MIN_WORK_MINUTES already applied in load_work_config when min_minutes==0 path

    KV m;
    const auto now = now_unix();
    m["status"] = "active";
    m["started_at_unix"] = std::to_string(now);
    m["started_at_iso"] = now_iso8601();
    m["min_minutes"] = std::to_string(min_m);
    m["min_verify_ok"] = std::to_string(min_v);
    m["min_heartbeats"] = std::to_string(min_h);
    m["verify_ok_count"] = "0";
    m["heartbeat_count"] = "0";
    m["last_heartbeat_unix"] = std::to_string(now);
    m["last_heartbeat_iso"] = now_iso8601();
    m["goal"] = opts.goal.empty() ? "(unspecified — set a concrete goal)" : opts.goal;
    m["heartbeat_max_gap_sec"] = std::to_string(wc.heartbeat_max_gap_sec);
    save_kv(path, m);

    CheckResult r;
    r.kind = "work";
    r.name = "start";
    r.status = "ok";
    r.detail = "work session started min_minutes=" + std::to_string(min_m);
    r.metrics["min_minutes"] = std::to_string(min_m);
    r.metrics["min_verify_ok"] = std::to_string(min_v);
    r.metrics["min_heartbeats"] = std::to_string(min_h);
    r.metrics["goal"] = m["goal"];
    finish_check(store, r, opts.pretty);

    if (opts.pretty)
        emit_work(m, true);
    else
        emit_work(m, false, "work_start");

    std::fprintf(stderr,
                 "\n======== ghar work session ACTIVE ========\n"
                 "min_minutes=%d  min_verify_ok=%d  min_heartbeats=%d\n"
                 "goal: %s\n"
                 "You MUST NOT deliver to the user until: ghar work done → exit 0\n"
                 "Loop: work → heartbeat → verify → … → work done\n"
                 "==========================================\n\n",
                 min_m, min_v, min_h, m["goal"].c_str());
    return EXIT_OK;
}

int cmd_work_heartbeat(const WorkOpts& opts)
{
    Store store(opts.root);
    store.ensure();
    auto path = work_path(opts.root);
    auto m = load_kv(path);
    if (get_s(m, "status") != "active") {
        std::fprintf(stderr, "ghar work heartbeat: no active session (ghar work start first)\n");
        return EXIT_USAGE;
    }
    const auto now = now_unix();
    int h = get_i(m, "heartbeat_count", 0) + 1;
    m["heartbeat_count"] = std::to_string(h);
    m["last_heartbeat_unix"] = std::to_string(now);
    m["last_heartbeat_iso"] = now_iso8601();
    if (!opts.note.empty())
        m["last_note"] = opts.note;
    save_kv(path, m);

    CheckResult r;
    r.kind = "work";
    r.name = "heartbeat";
    r.status = "ok";
    r.detail = opts.note.empty() ? ("heartbeat #" + std::to_string(h)) : opts.note;
    r.metrics["heartbeat_count"] = std::to_string(h);
    finish_check(store, r, opts.pretty);
    emit_work(m, opts.pretty, "work_heartbeat");
    return EXIT_OK;
}

int cmd_work_status(const WorkOpts& opts)
{
    auto m = load_kv(work_path(opts.root));
    if (m.empty()) {
        if (opts.pretty)
            std::puts("work status: none (ghar work start --minutes N --goal '...')");
        else {
            FieldMap f;
            f["status"] = "none";
            emit_row("work", f);
        }
        return EXIT_OK;
    }
    emit_work(m, opts.pretty);
    auto q = check_quotas(m, load_work_config(opts.root));
    if (opts.pretty)
        std::printf("  delivery_ready:    %s (%s)\n", q.ok ? "yes" : "NO", q.reason.c_str());
    else {
        FieldMap f;
        f["delivery_ready"] = q.ok ? "1" : "0";
        f["reason"] = q.reason;
        emit_row("work_delivery", f);
    }
    return q.ok ? EXIT_OK : EXIT_FAIL;
}

int cmd_work_done(const WorkOpts& opts)
{
    Store store(opts.root);
    store.ensure();
    auto path = work_path(opts.root);
    auto m = load_kv(path);
    if (get_s(m, "status") != "active") {
        std::fprintf(stderr, "ghar work done: no active session\n");
        return EXIT_USAGE;
    }

    std::string reason;
    if (!opts.force) {
        const int rc = work_check_delivery(opts.root, opts.pretty, reason);
        if (rc != EXIT_OK) {
            CheckResult r;
            r.kind = "work";
            r.name = "done";
            r.status = "fail";
            r.exit_code = EXIT_FAIL;
            r.detail = reason;
            return finish_check(store, r, opts.pretty);
        }
    } else {
        reason = "forced";
    }

    m["status"] = "done";
    m["ended_at_iso"] = now_iso8601();
    m["ended_at_unix"] = std::to_string(now_unix());
    m["end_reason"] = reason;
    save_kv(path, m);

    CheckResult r;
    r.kind = "work";
    r.name = "done";
    r.status = "ok";
    r.detail = "work session complete — delivery allowed";
    r.metrics["end_reason"] = reason;
    finish_check(store, r, opts.pretty);
    emit_work(m, opts.pretty, "work_done");
    return EXIT_OK;
}

int cmd_work_abandon(const WorkOpts& opts)
{
    Store store(opts.root);
    store.ensure();
    auto path = work_path(opts.root);
    auto m = load_kv(path);
    if (get_s(m, "status") != "active") {
        std::fprintf(stderr, "ghar work abandon: no active session\n");
        return EXIT_USAGE;
    }
    m["status"] = "abandoned";
    m["ended_at_iso"] = now_iso8601();
    m["ended_at_unix"] = std::to_string(now_unix());
    m["end_reason"] = opts.note.empty() ? "abandoned" : opts.note;
    save_kv(path, m);

    CheckResult r;
    r.kind = "work";
    r.name = "abandon";
    r.status = "fail";
    r.exit_code = EXIT_FAIL;
    r.detail = "session abandoned — NOT a successful delivery";
    finish_check(store, r, opts.pretty);
    // abandon always exit 4 unless force (explicit failure signal)
    return opts.force ? EXIT_OK : EXIT_FAIL;
}

}  // namespace ghar
