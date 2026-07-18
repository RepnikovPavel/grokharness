// SPDX-License-Identifier: 0BSD
#include "assert_check.hpp"

#include "store.hpp"
#include "util.hpp"

#include <cmath>

namespace ghar {
namespace {

std::string normalize_metric_key(std::string k)
{
    if (starts_with(k, "m."))
        k = k.substr(2);
    return k;
}

bool get_metric(const FieldMap& m, const std::string& key, std::string& out)
{
    const std::string k = normalize_metric_key(key);
    auto it = m.find(k);
    if (it != m.end()) {
        out = it->second;
        return true;
    }
    // try without prefix variants
    it = m.find("out." + k);
    if (it != m.end()) {
        out = it->second;
        return true;
    }
    return false;
}

bool compare_num(double actual, double expected, const std::string& op, double tol, bool relative,
                 std::string& why)
{
    auto rel_ok = [&](double a, double b) {
        const double scale = std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
        return std::fabs(a - b) <= tol * (relative ? scale : 1.0);
    };

    if (op == "eq") {
        const bool ok = rel_ok(actual, expected);
        if (!ok)
            why = "actual!=expected";
        return ok;
    }
    if (op == "ne") {
        const bool ok = !rel_ok(actual, expected);
        if (!ok)
            why = "actual≈expected";
        return ok;
    }
    if (op == "approx") {
        const bool ok = rel_ok(actual, expected);
        if (!ok)
            why = "not within tolerance";
        return ok;
    }
    if (op == "lt") {
        const bool ok = actual < expected;
        if (!ok)
            why = "not <";
        return ok;
    }
    if (op == "le") {
        const bool ok = actual <= expected + (relative ? 0 : 0);
        if (!ok)
            why = "not <=";
        return ok;
    }
    if (op == "gt") {
        const bool ok = actual > expected;
        if (!ok)
            why = "not >";
        return ok;
    }
    if (op == "ge") {
        const bool ok = actual >= expected;
        if (!ok)
            why = "not >=";
        return ok;
    }
    why = "unknown op";
    return false;
}

}  // namespace

int cmd_assert(const AssertOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "assert";
    r.name = opts.name.empty() ? ("assert_" + opts.from + "_" + opts.metric) : opts.name;

    if (opts.from.empty() || opts.metric.empty() || opts.op.empty() || opts.value.empty()) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "need --from --metric --op --value";
        return finish_check(store, r, opts.pretty);
    }

    FieldMap metrics;
    std::string source_kind;
    if (auto c = store.find_claim(opts.from)) {
        metrics = c->metrics;
        source_kind = c->kind;
        r.metrics["source_status"] = c->status;
    } else if (auto res = store.find_result_by_name(opts.from)) {
        metrics = res->metrics;
        source_kind = res->kind;
        r.metrics["source_status"] = res->status;
    } else {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "no claim/result named: " + opts.from;
        return finish_check(store, r, opts.pretty);
    }

    r.metrics["from"] = opts.from;
    r.metrics["source_kind"] = source_kind;
    r.metrics["metric"] = opts.metric;
    r.metrics["op"] = opts.op;
    r.metrics["expected"] = opts.value;
    r.metrics["tol"] = std::to_string(opts.tol);

    std::string actual_s;
    if (!get_metric(metrics, opts.metric, actual_s)) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "metric not found: " + opts.metric;
        // list available keys for agent
        std::vector<std::string> keys;
        for (const auto& kv : metrics)
            keys.push_back(kv.first);
        r.metrics["available"] = join(keys, ",");
        return finish_check(store, r, opts.pretty);
    }
    r.metrics["actual"] = actual_s;

    double actual = 0, expected = 0;
    const bool anum = parse_double(actual_s, actual);
    const bool enum_ = parse_double(opts.value, expected);

    bool pass = false;
    std::string why;
    if (anum && enum_) {
        pass = compare_num(actual, expected, opts.op, opts.tol, opts.relative, why);
        if (anum && enum_ && (opts.op == "eq" || opts.op == "approx" || opts.op == "ne")) {
            r.metrics["abs_err"] = std::to_string(std::fabs(actual - expected));
            const double scale = std::max(1e-12, std::fabs(expected));
            r.metrics["rel_err"] = std::to_string(std::fabs(actual - expected) / scale);
        }
    } else {
        // string compare
        if (opts.op == "eq")
            pass = actual_s == opts.value;
        else if (opts.op == "ne")
            pass = actual_s != opts.value;
        else {
            r.status = "error";
            r.exit_code = EXIT_USAGE;
            r.detail = "non-numeric metric requires op eq|ne";
            return finish_check(store, r, opts.pretty);
        }
        if (!pass)
            why = "string mismatch";
    }

    if (pass) {
        r.status = "ok";
        r.exit_code = EXIT_OK;
        r.detail = opts.metric + "=" + actual_s + " " + opts.op + " " + opts.value;
    } else {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "ASSERT FAIL: " + opts.metric + "=" + actual_s + " not " + opts.op + " " +
                   opts.value + (why.empty() ? "" : " (" + why + ")");
    }
    return finish_check(store, r, opts.pretty);
}

}  // namespace ghar
