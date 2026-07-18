// SPDX-License-Identifier: 0BSD
#include "run_check.hpp"

#include "store.hpp"
#include "util.hpp"

namespace ghar {

int cmd_run(const RunOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "run";
    r.name = opts.name;

    if (opts.argv.empty() && opts.shell_cmd.empty()) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "no command";
        return finish_check(store, r, opts.pretty);
    }

    CmdResult cr;
    if (!opts.shell_cmd.empty()) {
        r.metrics["cmd"] = opts.shell_cmd;
        cr = run_shell(opts.shell_cmd, opts.timeout_sec, opts.root);
    } else {
        r.metrics["cmd"] = join(opts.argv, " ");
        cr = run_cmd(opts.argv, opts.timeout_sec, opts.root);
    }

    r.metrics["wall_ms"] = std::to_string(cr.wall_ms);
    r.metrics["proc_exit"] = std::to_string(cr.exit_code);
    r.metrics["stdout_bytes"] = std::to_string(cr.stdout_s.size());
    r.metrics["stderr_bytes"] = std::to_string(cr.stderr_s.size());
    r.metrics["stdout_fp"] = sha256_hex(cr.stdout_s);
    r.metrics["stderr_fp"] = sha256_hex(cr.stderr_s);
    r.metrics["timed_out"] = cr.timed_out ? "1" : "0";
    r.metrics["expect_exit"] = std::to_string(opts.expect_exit);

    // Extract first float-like KEY=VALUE lines from stdout for metric chaining
    for (const auto& line : split(cr.stdout_s, '\n')) {
        auto L = trim(line);
        auto eq = L.find('=');
        if (eq == std::string::npos || eq == 0)
            continue;
        std::string key = L.substr(0, eq);
        std::string val = L.substr(eq + 1);
        bool key_ok = true;
        for (char c : key) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.')) {
                key_ok = false;
                break;
            }
        }
        if (!key_ok || key.size() > 64)
            continue;
        double d;
        if (parse_double(trim(val), d))
            r.metrics["out." + key] = trim(val);
    }

    if (cr.timed_out) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "timed out";
    } else if (cr.exit_code != opts.expect_exit) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "exit " + std::to_string(cr.exit_code) + " != expect " +
                   std::to_string(opts.expect_exit);
    } else {
        r.status = "ok";
        r.exit_code = EXIT_OK;
        r.detail = "run ok";
    }
    return finish_check(store, r, opts.pretty);
}

}  // namespace ghar
