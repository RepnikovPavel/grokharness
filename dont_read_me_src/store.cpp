// SPDX-License-Identifier: 0BSD
#include "store.hpp"

#include "output.hpp"

#include <chrono>
#include <cstdio>
#include <random>
#include <sstream>
#include <unistd.h>

namespace ghar {
namespace {

// Escape so metrics blob survives `;` and `=` inside values (e.g. feedback text).
std::string metric_escape(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\')
            out += "\\\\";
        else if (c == ';')
            out += "\\;";
        else if (c == '=')
            out += "\\=";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c == '\t')
            out += "\\t";
        else
            out += c;
    }
    return out;
}

std::string metric_unescape(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[i + 1];
            if (n == '\\' || n == ';' || n == '=') {
                out += n;
                ++i;
                continue;
            }
            if (n == 'n') {
                out += '\n';
                ++i;
                continue;
            }
            if (n == 'r') {
                out += '\r';
                ++i;
                continue;
            }
            if (n == 't') {
                out += '\t';
                ++i;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

std::string metrics_encode(const FieldMap& m)
{
    std::vector<std::string> parts;
    parts.reserve(m.size());
    for (const auto& kv : m)
        parts.push_back(metric_escape(kv.first) + "=" + metric_escape(kv.second));
    return join(parts, ";");
}

// True if s[i] is unescaped (even number of backslashes immediately before it).
bool unescaped_at(const std::string& s, size_t i)
{
    size_t nbs = 0;
    size_t j = i;
    while (j > 0 && s[j - 1] == '\\') {
        ++nbs;
        --j;
    }
    return (nbs % 2) == 0;
}

FieldMap metrics_decode(const std::string& s)
{
    FieldMap m;
    if (s.empty())
        return m;
    std::string cur;
    auto flush = [&]() {
        if (cur.empty())
            return;
        size_t eq = std::string::npos;
        for (size_t i = 0; i < cur.size(); ++i) {
            if (cur[i] == '=' && unescaped_at(cur, i)) {
                eq = i;
                break;
            }
        }
        if (eq == std::string::npos)
            m[metric_unescape(cur)] = "";
        else
            m[metric_unescape(cur.substr(0, eq))] = metric_unescape(cur.substr(eq + 1));
        cur.clear();
    };
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ';' && unescaped_at(s, i)) {
            flush();
            continue;
        }
        cur += s[i];
    }
    flush();
    return m;
}

std::vector<std::string> result_header()
{
    return {"id", "ts", "kind", "name", "status", "exit_code", "metrics", "detail"};
}

std::vector<std::string> claim_header()
{
    return {"name", "kind", "status", "metrics", "updated_at", "source_id"};
}

}  // namespace

Store::Store(std::string root) : root_(std::move(root))
{
    if (root_.empty())
        root_ = ".";
}

bool Store::ensure()
{
    return mkdir_p(dir());
}

std::string Store::new_id() const
{
    static thread_local std::mt19937_64 rng{
        static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) ^
        (static_cast<std::uint64_t>(::getpid()) << 32)};
    std::uniform_int_distribution<std::uint64_t> dist;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s-%08llx", now_iso8601().c_str(),
                  static_cast<unsigned long long>(dist(rng) & 0xffffffffull));
    std::string s(buf);
    for (char& c : s) {
        if (c == ':')
            c = '-';
    }
    return s;
}

bool Store::append_result(const CheckResult& r)
{
    if (!ensure())
        return false;
    std::vector<std::string> row = {r.id,
                                    r.ts,
                                    r.kind,
                                    r.name,
                                    r.status,
                                    std::to_string(r.exit_code),
                                    metrics_encode(r.metrics),
                                    r.detail};
    return append_tsv_row(results_path(), result_header(), row);
}

bool Store::upsert_claim(const Claim& c)
{
    if (!ensure())
        return false;
    auto claims = load_claims();
    bool found = false;
    for (auto& x : claims) {
        if (x.name == c.name) {
            x = c;
            found = true;
            break;
        }
    }
    if (!found)
        claims.push_back(c);

    TsvTable t;
    t.header = claim_header();
    for (const auto& x : claims) {
        t.rows.push_back({x.name, x.kind, x.status, metrics_encode(x.metrics), x.updated_at,
                          x.source_id});
    }
    return write_tsv_file(claims_path(), t);
}

std::vector<CheckResult> Store::load_results() const
{
    std::vector<CheckResult> out;
    const auto table = read_tsv_file(results_path());
    for (const auto& row : table.rows) {
        if (row.size() < 8)
            continue;
        CheckResult r;
        r.id = row[0];
        r.ts = row[1];
        r.kind = row[2];
        r.name = row[3];
        r.status = row[4];
        parse_int(row[5], r.exit_code);
        r.metrics = metrics_decode(row[6]);
        r.detail = row[7];
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<Claim> Store::load_claims() const
{
    std::vector<Claim> out;
    const auto table = read_tsv_file(claims_path());
    for (const auto& row : table.rows) {
        if (row.size() < 6)
            continue;
        Claim c;
        c.name = row[0];
        c.kind = row[1];
        c.status = row[2];
        c.metrics = metrics_decode(row[3]);
        c.updated_at = row[4];
        c.source_id = row[5];
        out.push_back(std::move(c));
    }
    return out;
}

std::optional<Claim> Store::find_claim(const std::string& name) const
{
    for (const auto& c : load_claims()) {
        if (c.name == name)
            return c;
    }
    return std::nullopt;
}

std::optional<CheckResult> Store::find_result_by_name(const std::string& name) const
{
    // most recent with matching name
    std::optional<CheckResult> best;
    for (const auto& r : load_results()) {
        if (r.name == name)
            best = r;
    }
    return best;
}

bool Store::write_session_status(const std::string& status, int fail_count, int ok_count)
{
    if (!ensure())
        return false;
    TsvTable t;
    t.header = {"ts", "status", "ok", "fail"};
    t.rows.push_back({now_iso8601(), status, std::to_string(ok_count), std::to_string(fail_count)});
    return write_tsv_file(session_path(), t);
}

int finish_check(Store& store, CheckResult r, bool pretty, bool silent)
{
    if (r.id.empty())
        r.id = store.new_id();
    if (r.ts.empty())
        r.ts = now_iso8601();

    store.append_result(r);
    if (!r.name.empty()) {
        Claim c;
        c.name = r.name;
        c.kind = r.kind;
        c.status = r.status;
        c.metrics = r.metrics;
        c.updated_at = r.ts;
        c.source_id = r.id;
        store.upsert_claim(c);
    }

    if (!silent)
        print_check_result(r, pretty);

    // Agent fix-loop: always surface actionable FEEDBACK on stderr for non-ok
    // claims (import/compile/assert/python/torch/…), not only `ghar verify`.
    // TSV stays on stdout; agents that only tail stderr still get a signal.
    if (!silent && r.status != "ok" && r.status != "skip") {
        const char* label = r.name.empty() ? r.kind.c_str() : r.name.c_str();
        std::fprintf(stderr, "\n======== ghar FEEDBACK (%s/%s) ========\n",
                     r.kind.c_str(), label);
        if (!r.detail.empty())
            std::fprintf(stderr, "%s\n", r.detail.c_str());
        auto it = r.metrics.find("feedback");
        if (it != r.metrics.end() && !it->second.empty())
            std::fprintf(stderr, "%s\n", it->second.c_str());
        std::fprintf(stderr,
                     "======== exit %d — fix, then re-run the same command or: ghar verify ========\n\n",
                     r.exit_code == EXIT_OK ? EXIT_FAIL : r.exit_code);
    }

    if (r.status == "ok" || r.status == "skip")
        return EXIT_OK;
    if (r.status == "error") {
        // Preserve tool/usage codes; default error → IO
        if (r.exit_code == EXIT_TOOL)
            return EXIT_TOOL;
        if (r.exit_code == EXIT_USAGE)
            return EXIT_USAGE;
        if (r.exit_code == EXIT_IO || r.exit_code == EXIT_FAIL || r.exit_code == EXIT_OK)
            return r.exit_code == EXIT_OK ? EXIT_IO : r.exit_code;
        return EXIT_IO;
    }
    return EXIT_FAIL;
}

}  // namespace ghar
