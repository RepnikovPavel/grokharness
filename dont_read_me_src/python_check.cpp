// SPDX-License-Identifier: 0BSD
#include "python_check.hpp"

#include "store.hpp"
#include "util.hpp"

#include <unistd.h>

namespace ghar {
namespace {

std::string exe_dir()
{
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return ".";
    buf[n] = '\0';
    return parent_dir(std::string(buf));
}

// Parse oracle stdout: status=ok\tkey=val\t...
void parse_oracle_line(const std::string& line, std::string& status, FieldMap& metrics)
{
    status = "fail";
    for (const auto& part : split(line, '\t')) {
        auto eq = part.find('=');
        if (eq == std::string::npos)
            continue;
        auto k = part.substr(0, eq);
        auto v = part.substr(eq + 1);
        if (k == "status")
            status = v;
        else if (k == "detail")
            continue;
        else
            metrics[k] = v;
    }
    // detail
    for (const auto& part : split(line, '\t')) {
        if (starts_with(part, "detail="))
            metrics["detail"] = part.substr(7);
    }
}

int run_oracle(const std::vector<std::string>& argv, Store& store, CheckResult& r, bool pretty)
{
    auto py = which("python3");
    if (!py) {
        r.status = "error";
        r.exit_code = EXIT_TOOL;
        r.detail = "python3 not found";
        return finish_check(store, r, pretty);
    }
    std::vector<std::string> cmd = {*py};
    cmd.insert(cmd.end(), argv.begin(), argv.end());
    r.metrics["oracle"] = join(cmd, " ");
    auto cr = run_cmd(cmd, 120);
    r.metrics["wall_ms"] = std::to_string(cr.wall_ms);
    r.metrics["proc_exit"] = std::to_string(cr.exit_code);

    // last non-empty stdout line is metrics
    std::string last;
    for (const auto& L : split(cr.stdout_s, '\n')) {
        if (!trim(L).empty())
            last = trim(L);
    }
    std::string st;
    FieldMap om;
    parse_oracle_line(last, st, om);
    for (const auto& kv : om) {
        if (kv.first == "detail")
            continue;
        r.metrics[kv.first] = kv.second;
    }
    if (om.count("detail"))
        r.detail = om["detail"];

    if (cr.timed_out) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "oracle timeout";
        return finish_check(store, r, pretty);
    }
    if (st == "ok" && cr.exit_code == 0) {
        r.status = "ok";
        r.exit_code = EXIT_OK;
        if (r.detail.empty())
            r.detail = "ok";
        return finish_check(store, r, pretty);
    }
    if (cr.exit_code == EXIT_TOOL || st == "error") {
        r.status = "error";
        r.exit_code = EXIT_TOOL;
        if (r.detail.empty())
            r.detail = trim(cr.stderr_s).empty() ? "oracle tool error" : trim(cr.stderr_s).substr(0, 200);
        return finish_check(store, r, pretty);
    }
    r.status = "fail";
    r.exit_code = EXIT_FAIL;
    if (r.detail.empty()) {
        auto err = trim(cr.stderr_s);
        r.detail = err.empty() ? ("oracle fail exit " + std::to_string(cr.exit_code)) : err.substr(0, 200);
    }
    return finish_check(store, r, pretty);
}

}  // namespace

std::string oracle_script_path()
{
    // Search common locations relative to binary and cwd
    std::vector<std::string> cands;
    const std::string env = getenv_or("GHAR_ROOT", "");
    if (!env.empty()) {
        cands.push_back(join_path(env, "dont_read_me_src/oracles/py_torch_validate.py"));
        cands.push_back(join_path(env, "oracles/py_torch_validate.py"));
    }
    cands.push_back(join_path(exe_dir(), "../dont_read_me_src/oracles/py_torch_validate.py"));
    cands.push_back(join_path(exe_dir(), "../../dont_read_me_src/oracles/py_torch_validate.py"));
    cands.push_back(join_path(exe_dir(), "../oracles/py_torch_validate.py"));
    cands.push_back(join_path(exe_dir(), "oracles/py_torch_validate.py"));
    cands.push_back("dont_read_me_src/oracles/py_torch_validate.py");
    cands.push_back("oracles/py_torch_validate.py");

    for (const auto& p : cands) {
        if (is_file(p))
            return abs_path(p);
    }
    return "oracles/py_torch_validate.py";
}

int cmd_python(const PythonOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "python";
    r.name = opts.name.empty() ? "python" : opts.name;
    if (opts.file.empty() || !is_file(opts.file)) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "python file not found";
        r.metrics["failure_class"] = "file_missing";
        return finish_check(store, r, opts.pretty);
    }
    const std::string script = oracle_script_path();
    if (!is_file(script)) {
        r.status = "error";
        r.exit_code = EXIT_TOOL;
        r.detail = "oracle script missing: " + script;
        return finish_check(store, r, opts.pretty);
    }
    std::vector<std::string> argv = {script, "python", "--file", abs_path(opts.file),
                                     "--timeout", std::to_string(opts.timeout_sec)};
    if (opts.check_imports)
        argv.push_back("--check-imports");
    if (opts.exec_code)
        argv.push_back("--exec");
    return run_oracle(argv, store, r, opts.pretty);
}

int cmd_torch(const TorchOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "torch";
    r.name = opts.name.empty() ? "torch" : opts.name;
    if (opts.file.empty() || !is_file(opts.file)) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "torch file not found";
        r.metrics["failure_class"] = "file_missing";
        return finish_check(store, r, opts.pretty);
    }
    const std::string script = oracle_script_path();
    if (!is_file(script)) {
        r.status = "error";
        r.exit_code = EXIT_TOOL;
        r.detail = "oracle script missing: " + script;
        return finish_check(store, r, opts.pretty);
    }
    std::vector<std::string> argv = {script, "torch", "--file", abs_path(opts.file),
                                     "--device", opts.device,
                                     "--timeout", std::to_string(opts.timeout_sec)};
    if (opts.forward)
        argv.push_back("--forward");
    if (opts.strict_attrs)
        argv.push_back("--strict-attrs");
    for (const auto& a : opts.require_attrs) {
        argv.push_back("--require-attr");
        argv.push_back(a);
    }
    return run_oracle(argv, store, r, opts.pretty);
}

int cmd_torch_attr(const TorchAttrOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "torch";
    r.name = opts.name.empty() ? "torch_attr" : opts.name;
    if (opts.attrs.empty()) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "no attrs";
        return finish_check(store, r, opts.pretty);
    }
    const std::string script = oracle_script_path();
    std::vector<std::string> argv = {script, "torch-attr"};
    for (const auto& a : opts.attrs)
        argv.push_back(a);
    return run_oracle(argv, store, r, opts.pretty);
}

}  // namespace ghar
