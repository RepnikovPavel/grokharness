// SPDX-License-Identifier: 0BSD
#pragma once

#include "tsv.hpp"
#include "util.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ghar {

// Project-local state under <root>/.ghar/
//   results.tsv  — every check run (append-only log)
//   claims.tsv   — named metric snapshots for assert/gate
//   session.tsv  — last gate status

struct CheckResult {
    std::string id;       // unique run id
    std::string ts;       // ISO time
    std::string kind;     // compile|cuda|symbols|import|run|bench|assert|test|doctor
    std::string name;     // optional user-provided name
    std::string status;   // ok|fail|skip|error
    int exit_code = 0;
    FieldMap metrics;     // numeric/string metrics
    std::string detail;   // short human/agent message
};

struct Claim {
    std::string name;
    std::string kind;
    std::string status;
    FieldMap metrics;
    std::string updated_at;
    std::string source_id;  // last result id
};

class Store {
public:
    explicit Store(std::string root);

    const std::string& root() const { return root_; }
    std::string dir() const { return join_path(root_, ".ghar"); }

    bool ensure();
    std::string new_id() const;

    bool append_result(const CheckResult& r);
    bool upsert_claim(const Claim& c);

    std::vector<CheckResult> load_results() const;
    std::vector<Claim> load_claims() const;
    std::optional<Claim> find_claim(const std::string& name) const;
    std::optional<CheckResult> find_result_by_name(const std::string& name) const;

    bool write_session_status(const std::string& status, int fail_count, int ok_count);

private:
    std::string root_;
    std::string results_path() const { return join_path(dir(), "results.tsv"); }
    std::string claims_path() const { return join_path(dir(), "claims.tsv"); }
    std::string session_path() const { return join_path(dir(), "session.tsv"); }
};

// Record a check: emit TSV to stdout, append to store, upsert claim if named.
// Returns EXIT_OK or EXIT_FAIL (or other).
// silent=true: persist only (no stdout) — for nested checks.
int finish_check(Store& store, CheckResult r, bool pretty, bool silent = false);

}  // namespace ghar
