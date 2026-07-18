// SPDX-License-Identifier: 0BSD
#include "test_check.hpp"

#include "store.hpp"
#include "util.hpp"

#include <regex>

namespace ghar {
namespace {

std::string detect_runner(const std::string& root)
{
    if (path_exists(join_path(root, "pytest.ini")) || path_exists(join_path(root, "pyproject.toml")) ||
        path_exists(join_path(root, "tests")) || path_exists(join_path(root, "test"))) {
        if (which("pytest"))
            return "pytest";
    }
    if (path_exists(join_path(root, "CTestTestfile.cmake")) ||
        path_exists(join_path(root, "build/CTestTestfile.cmake"))) {
        if (which("ctest"))
            return "ctest";
    }
    if (path_exists(join_path(root, "CMakeLists.txt")) && which("ctest"))
        return "ctest";
    if (which("pytest"))
        return "pytest";
    return "";
}

void parse_pytest(const std::string& out, FieldMap& m)
{
    // "===== 3 passed, 1 failed, 2 skipped in 1.23s ====="
    std::regex re(R"((\d+)\s+passed)");
    std::regex re_f(R"((\d+)\s+failed)");
    std::regex re_s(R"((\d+)\s+skipped)");
    std::regex re_t(R"(in\s+([0-9.]+)s)");
    std::smatch match;
    if (std::regex_search(out, match, re))
        m["passed"] = match[1];
    if (std::regex_search(out, match, re_f))
        m["failed"] = match[1];
    if (std::regex_search(out, match, re_s))
        m["skipped"] = match[1];
    if (std::regex_search(out, match, re_t))
        m["pytest_time_s"] = match[1];
}

void parse_ctest(const std::string& out, FieldMap& m)
{
    // "tests passed, 2 tests failed out of 10"
    std::regex re_pass(R"((\d+)\s+tests?\s+passed)");
    std::regex re_fail(R"((\d+)\s+tests?\s+failed)");
    std::regex re_total(R"(out of\s+(\d+))");
    std::smatch match;
    if (std::regex_search(out, match, re_pass))
        m["passed"] = match[1];
    if (std::regex_search(out, match, re_fail))
        m["failed"] = match[1];
    if (std::regex_search(out, match, re_total))
        m["total"] = match[1];
}

}  // namespace

int cmd_test(const TestOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "test";
    r.name = opts.name.empty() ? "test" : opts.name;

    std::string runner = opts.runner;
    if (runner.empty() || runner == "auto")
        runner = detect_runner(opts.root);
    if (runner.empty()) {
        r.status = "error";
        r.exit_code = EXIT_TOOL;
        r.detail = "no test runner detected (pytest/ctest)";
        return finish_check(store, r, opts.pretty);
    }
    r.metrics["runner"] = runner;

    CmdResult cr;
    if (runner == "pytest") {
        auto py = which("pytest");
        if (!py) {
            r.status = "error";
            r.exit_code = EXIT_TOOL;
            r.detail = "pytest not found";
            return finish_check(store, r, opts.pretty);
        }
        std::vector<std::string> argv = {*py, "-q", "--tb=no"};
        for (const auto& a : opts.extra_args)
            argv.push_back(a);
        r.metrics["cmd"] = join(argv, " ");
        cr = run_cmd(argv, opts.timeout_sec, opts.root);
        parse_pytest(cr.stdout_s + "\n" + cr.stderr_s, r.metrics);
    } else if (runner == "ctest") {
        auto ct = which("ctest");
        if (!ct) {
            r.status = "error";
            r.exit_code = EXIT_TOOL;
            r.detail = "ctest not found";
            return finish_check(store, r, opts.pretty);
        }
        std::string cwd = opts.root;
        if (path_exists(join_path(opts.root, "build/CTestTestfile.cmake")))
            cwd = join_path(opts.root, "build");
        std::vector<std::string> argv = {*ct, "--output-on-failure"};
        for (const auto& a : opts.extra_args)
            argv.push_back(a);
        r.metrics["cmd"] = join(argv, " ");
        r.metrics["cwd"] = cwd;
        cr = run_cmd(argv, opts.timeout_sec, cwd);
        parse_ctest(cr.stdout_s + "\n" + cr.stderr_s, r.metrics);
    } else {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "unknown runner: " + runner;
        return finish_check(store, r, opts.pretty);
    }

    r.metrics["wall_ms"] = std::to_string(cr.wall_ms);
    r.metrics["proc_exit"] = std::to_string(cr.exit_code);
    r.metrics["timed_out"] = cr.timed_out ? "1" : "0";

    if (!r.metrics.count("passed"))
        r.metrics["passed"] = cr.exit_code == 0 ? "?" : "0";
    if (!r.metrics.count("failed"))
        r.metrics["failed"] = cr.exit_code == 0 ? "0" : "?";

    if (cr.timed_out) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "tests timed out";
    } else if (cr.exit_code != 0) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "tests failed (exit " + std::to_string(cr.exit_code) + ")";
    } else {
        r.status = "ok";
        r.exit_code = EXIT_OK;
        r.detail = "tests passed";
    }
    return finish_check(store, r, opts.pretty);
}

}  // namespace ghar
