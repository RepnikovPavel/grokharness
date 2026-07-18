// SPDX-License-Identifier: 0BSD
#include "compile_check.hpp"

#include "store.hpp"
#include "util.hpp"

#include <cstdio>

namespace ghar {
namespace {

std::string pick_compiler(const std::string& want, const std::string& source)
{
    if (want == "nvcc" || ends_with(to_lower(source), ".cu") ||
        ends_with(to_lower(source), ".cuh")) {
        if (auto p = which("nvcc"))
            return *p;
        if (path_exists("/usr/local/cuda/bin/nvcc"))
            return "/usr/local/cuda/bin/nvcc";
        return "nvcc";
    }
    if (want == "clang++") {
        if (auto p = which("clang++"))
            return *p;
        return "clang++";
    }
    if (want == "g++" || want == "auto" || want.empty()) {
        if (auto p = which("g++"))
            return *p;
        if (auto p = which("clang++"))
            return *p;
        return "g++";
    }
    return want;
}

int count_lines_matching(const std::string& text, const char* needle)
{
    int n = 0;
    for (const auto& line : split(text, '\n')) {
        if (line.find(needle) != std::string::npos)
            ++n;
    }
    return n;
}

}  // namespace

int cmd_compile(const CompileOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "compile";
    r.name = opts.name;

    if (opts.sources.empty()) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "no sources";
        return finish_check(store, r, opts.pretty, opts.silent);
    }

    for (const auto& s : opts.sources) {
        if (!is_file(s)) {
            r.status = "fail";
            r.exit_code = EXIT_FAIL;
            r.detail = "source not found: " + s;
            r.metrics["missing_source"] = s;
            return finish_check(store, r, opts.pretty, opts.silent);
        }
    }

    const std::string compiler = pick_compiler(opts.compiler, opts.sources[0]);
    if (!command_exists(compiler) && !path_exists(compiler)) {
        r.status = "error";
        r.exit_code = EXIT_TOOL;
        r.detail = "compiler not found: " + compiler;
        return finish_check(store, r, opts.pretty, opts.silent);
    }

    std::string out = opts.output;
    if (out.empty()) {
        const std::string base = basename(opts.sources[0]);
        const auto dot = base.find_last_of('.');
        const std::string stem = dot == std::string::npos ? base : base.substr(0, dot);
        out = join_path(store.dir(), "bin_" + (opts.name.empty() ? stem : opts.name));
        store.ensure();
    }

    std::vector<std::string> argv;
    argv.push_back(compiler);

    const bool is_nvcc = basename(compiler) == "nvcc" || compiler.find("nvcc") != std::string::npos;
    if (is_nvcc) {
        argv.push_back("-std=c++17");
    } else {
        argv.push_back("-std=" + opts.standard);
        argv.push_back("-Wall");
    }
    if (!opts.link)
        argv.push_back("-c");
    for (const auto& f : opts.flags)
        argv.push_back(f);
    argv.push_back("-o");
    argv.push_back(out);
    for (const auto& s : opts.sources)
        argv.push_back(s);

    r.metrics["compiler"] = compiler;
    r.metrics["output"] = out;
    r.metrics["sources"] = std::to_string(opts.sources.size());
    r.metrics["cmd"] = join(argv, " ");

    auto cr = run_cmd(argv, opts.timeout_sec);
    r.metrics["wall_ms"] = std::to_string(cr.wall_ms);
    r.metrics["compiler_exit"] = std::to_string(cr.exit_code);
    r.metrics["stderr_bytes"] = std::to_string(cr.stderr_s.size());
    r.metrics["error_lines"] =
        std::to_string(count_lines_matching(cr.stderr_s, "error:") +
                       count_lines_matching(cr.stderr_s, "error "));
    r.metrics["warning_lines"] = std::to_string(count_lines_matching(cr.stderr_s, "warning:"));

    if (cr.timed_out) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "compile timed out";
        return finish_check(store, r, opts.pretty, opts.silent);
    }

    if (cr.exit_code != 0) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        // keep last non-empty stderr lines in detail
        auto lines = split(cr.stderr_s, '\n');
        std::string tail;
        int kept = 0;
        for (int i = static_cast<int>(lines.size()) - 1; i >= 0 && kept < 3; --i) {
            auto L = trim(lines[static_cast<size_t>(i)]);
            if (L.empty())
                continue;
            if (!tail.empty())
                tail = L + " | " + tail;
            else
                tail = L;
            ++kept;
        }
        r.detail = "compile failed: " + tail;
        r.metrics["stderr_fp"] = sha256_hex(cr.stderr_s);
        return finish_check(store, r, opts.pretty, opts.silent);
    }

    auto st = path_stat(out);
    r.metrics["binary_bytes"] = st.valid ? std::to_string(st.size) : "0";
    r.metrics["binary_exists"] = st.valid ? "1" : "0";
    r.status = "ok";
    r.exit_code = EXIT_OK;
    r.detail = "compile ok → " + out;
    return finish_check(store, r, opts.pretty, opts.silent);
}

}  // namespace ghar
