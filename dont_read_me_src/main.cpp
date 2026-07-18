// SPDX-License-Identifier: 0BSD
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assert_check.hpp"
#include "bench_check.hpp"
#include "compile_check.hpp"
#include "cuda_check.hpp"
#include "doctor.hpp"
#include "gate.hpp"
#include "import_check.hpp"
#include "python_check.hpp"
#include "run_check.hpp"
#include "symbols_check.hpp"
#include "test_check.hpp"
#include "util.hpp"
#include "verify.hpp"
#include "work.hpp"

namespace {

constexpr const char* kVersion = "ghar 0.4.0";

void print_help()
{
    std::fputs(
        "ghar — grounding harness: anti-hallucination + anti-early-stop for coding agents\n"
        "\n"
        "Best practices: Aider lint/test, Claude Stop hooks, long work sessions.\n"
        "  • Execution is truth — NEVER LLM-as-judge\n"
        "  • Do NOT quit after 5 minutes on deep tasks — use work sessions\n"
        "  • After edits: ghar verify; before user: ghar work done (quotas met)\n"
        "\n"
        "Long autonomous work (solves 5-minute agent quits):\n"
        "  ghar work start --minutes 120 --goal 'concrete goal'\n"
        "  # loop: edit → ghar work heartbeat --note '...' → ghar verify\n"
        "  ghar work status              # remain_min / verify_ok / heartbeats\n"
        "  ghar work done                # exit 0 only if time+verify+heartbeat quotas met\n"
        "\n"
        "Short delivery (no active work session):\n"
        "  ghar verify                   # lint→build→test→gate\n"
        "\n"
        "Usage:\n"
        "  ghar work start|heartbeat|status|done|abandon\n"
        "  ghar scaffold [--force]         config + Claude/Aider integrations\n"
        "  ghar config | verify | init | doctor | gate | reset | report\n"
        "  ghar compile|cuda|symbols|import|run|bench|assert|test\n"
        "  ghar python|torch|torch-attr     Python / PyTorch validators\n"
        "\n"
        "python options:\n"
        "  --file PATH       .py source to validate (AST + optional imports/exec)\n"
        "  --exec            execute module after syntax/import checks\n"
        "  --no-imports      skip importlib probe of top-level imports\n"
        "  --timeout N       exec timeout seconds (default 30)\n"
        "\n"
        "torch options:\n"
        "  --file PATH       model/op module (model/Model/build_model/run_op/main)\n"
        "  --forward         run forward/op (default on; --no-forward for static only)\n"
        "  --strict-attrs    resolve torch.* attrs against installed torch (default on)\n"
        "  --require-attr A  require attr path (repeatable), e.g. torch.nn.Linear\n"
        "  --device cpu|cuda\n"
        "  --timeout N\n"
        "\n"
        "torch-attr: ghar torch-attr torch.nn.Linear torch.matmul ...\n"
        "\n"
        "work start options:\n"
        "  --minutes N       min wall time before done (default: config min_work_minutes)\n"
        "  --goal TEXT       what you must finish\n"
        "  --min-verify N    successful verify count required\n"
        "  --min-heartbeats N\n"
        "  --force           restart session\n"
        "\n"
        "verify options:\n"
        "  --no-gate  --step NAME  --continue  --deliver\n"
        "\n"
        "Exit: 0 ok | 1 usage | 2 tool | 3 IO | 4 verify/work block\n"
        "Config: ghar.conf  Env: GHAR_MIN_WORK_MINUTES GHAR_ROOT\n"
        "State: .ghar/{work,claims,results,session}.tsv\n"
        "Oracle: dont_read_me_src/oracles/py_torch_validate.py (pure program checks)\n",
        stdout);
}

struct GlobalOpts {
    std::string root = ".";
    bool pretty = false;
    std::string name;
};

bool take_flag(std::vector<std::string>& args, const std::string& flag)
{
    for (auto it = args.begin(); it != args.end(); ++it) {
        if (*it == flag) {
            args.erase(it);
            return true;
        }
    }
    return false;
}

std::string take_opt(std::vector<std::string>& args, const std::string& flag)
{
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == flag && i + 1 < args.size()) {
            std::string v = args[i + 1];
            args.erase(args.begin() + static_cast<long>(i),
                       args.begin() + static_cast<long>(i) + 2);
            return v;
        }
        if (ghar::starts_with(args[i], flag + "=")) {
            std::string v = args[i].substr(flag.size() + 1);
            args.erase(args.begin() + static_cast<long>(i));
            return v;
        }
    }
    return "";
}

std::vector<std::string> take_multi(std::vector<std::string>& args, const std::string& flag)
{
    std::vector<std::string> out;
    for (;;) {
        std::string v = take_opt(args, flag);
        if (v.empty())
            break;
        out.push_back(v);
    }
    return out;
}

// Split args at `--` into before/after
void split_dashdash(std::vector<std::string>& args, std::vector<std::string>& after)
{
    after.clear();
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--") {
            after.assign(args.begin() + static_cast<long>(i) + 1, args.end());
            args.resize(i);
            return;
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        print_help();
        return ghar::EXIT_USAGE;
    }

    std::string cmd = argv[1];
    if (cmd == "-h" || cmd == "--help") {
        print_help();
        return ghar::EXIT_OK;
    }
    if (cmd == "--version") {
        std::puts(kVersion);
        return ghar::EXIT_OK;
    }

    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i)
        args.emplace_back(argv[i]);

    GlobalOpts g;
    if (take_flag(args, "--format"))
        g.pretty = true;
    std::string root = take_opt(args, "--root");
    if (root.empty())
        root = take_opt(args, "-C");
    if (!root.empty())
        g.root = root;
    g.name = take_opt(args, "--name");

    // trailing path for simple commands: ghar gate [path]
    auto maybe_path = [&]() {
        if (args.size() == 1 && (args[0] == "." || ghar::is_dir(args[0]) || args[0] == "..")) {
            g.root = args[0];
            args.clear();
        }
    };

    if (cmd == "init") {
        maybe_path();
        return ghar::cmd_init(g.root, true);
    }
    if (cmd == "scaffold") {
        const bool force = take_flag(args, "--force");
        maybe_path();
        return ghar::cmd_scaffold(g.root, force);
    }
    if (cmd == "config") {
        maybe_path();
        return ghar::cmd_config_show(g.root, g.pretty);
    }
    if (cmd == "work") {
        std::string sub = args.empty() ? "" : args.front();
        if (!args.empty())
            args.erase(args.begin());
        if (sub == "start") {
            ghar::WorkStartOpts o;
            o.root = g.root;
            o.pretty = g.pretty;
            o.force = take_flag(args, "--force");
            o.goal = take_opt(args, "--goal");
            std::string m = take_opt(args, "--minutes");
            if (!m.empty()) {
                int mv = 0;
                if (ghar::parse_int(m, mv))
                    o.min_minutes = mv;  // allows 0
            }
            std::string mv = take_opt(args, "--min-verify");
            if (!mv.empty())
                ghar::parse_int(mv, o.min_verify_ok);
            std::string mh = take_opt(args, "--min-heartbeats");
            if (!mh.empty())
                ghar::parse_int(mh, o.min_heartbeats);
            // remaining words as goal if --goal not set
            if (o.goal.empty() && !args.empty())
                o.goal = ghar::join(args, " ");
            return ghar::cmd_work_start(o);
        }
        ghar::WorkOpts o;
        o.root = g.root;
        o.pretty = g.pretty;
        o.force = take_flag(args, "--force");
        o.note = take_opt(args, "--note");
        if (o.note.empty() && !args.empty())
            o.note = ghar::join(args, " ");
        if (sub == "heartbeat" || sub == "hb")
            return ghar::cmd_work_heartbeat(o);
        if (sub == "status" || sub == "st")
            return ghar::cmd_work_status(o);
        if (sub == "done")
            return ghar::cmd_work_done(o);
        if (sub == "abandon")
            return ghar::cmd_work_abandon(o);
        std::fprintf(stderr,
                     "ghar work: need start|heartbeat|status|done|abandon\n"
                     "  ghar work start --minutes 120 --goal '...'\n");
        return ghar::EXIT_USAGE;
    }
    if (cmd == "verify") {
        ghar::VerifyOpts o;
        o.root = g.root;
        o.pretty = g.pretty;
        o.skip_gate = take_flag(args, "--no-gate");
        o.fail_fast = !take_flag(args, "--continue");
        o.deliver = take_flag(args, "--deliver");
        o.only_step = take_opt(args, "--step");
        maybe_path();
        o.root = g.root;
        return ghar::cmd_verify(o);
    }
    if (cmd == "doctor") {
        maybe_path();
        return ghar::cmd_doctor(g.root, g.pretty);
    }
    if (cmd == "gate") {
        maybe_path();
        return ghar::cmd_gate(g.root, g.pretty);
    }
    if (cmd == "reset") {
        const bool keep = take_flag(args, "--keep-results");
        maybe_path();
        return ghar::cmd_reset(g.root, keep);
    }
    if (cmd == "report") {
        maybe_path();
        return ghar::cmd_report(g.root, g.pretty, false);
    }
    if (cmd == "claims") {
        maybe_path();
        return ghar::cmd_report(g.root, g.pretty, true);
    }

    if (cmd == "compile") {
        ghar::CompileOpts o;
        o.root = g.root;
        o.name = g.name;
        o.pretty = g.pretty;
        o.compiler = take_opt(args, "--compiler");
        if (o.compiler.empty())
            o.compiler = "auto";
        std::string stdv = take_opt(args, "--std");
        if (!stdv.empty())
            o.standard = stdv;
        o.output = take_opt(args, "-o");
        if (take_flag(args, "-c"))
            o.link = false;
        o.flags = take_multi(args, "--flag");
        // also accept -I -L -l style left in args starting with -
        std::vector<std::string> sources;
        for (const auto& a : args) {
            if (a == "--")
                continue;
            if (!a.empty() && a[0] == '-' && a != "-")
                o.flags.push_back(a);
            else
                sources.push_back(a);
        }
        o.sources = sources;
        if (o.name.empty() && !sources.empty())
            o.name = "compile_" + ghar::basename(sources[0]);
        return ghar::cmd_compile(o);
    }

    if (cmd == "cuda") {
        ghar::CudaOpts o;
        o.root = g.root;
        o.name = g.name;
        o.pretty = g.pretty;
        if (take_flag(args, "--device") || take_flag(args, "--device-only"))
            o.device_only = true;
        o.flags = take_multi(args, "--flag");
        for (const auto& a : args) {
            if (!a.empty() && a[0] == '-')
                o.flags.push_back(a);
            else
                o.sources.push_back(a);
        }
        return ghar::cmd_cuda(o);
    }

    if (cmd == "symbols") {
        ghar::SymbolsOpts o;
        o.root = g.root;
        o.name = g.name.empty() ? "symbols" : g.name;
        o.pretty = g.pretty;
        o.binaries = take_multi(args, "--bin");
        o.headers = take_multi(args, "--header");
        for (const auto& a : args) {
            if (!a.empty() && a[0] != '-')
                o.symbols.push_back(a);
        }
        return ghar::cmd_symbols(o);
    }

    if (cmd == "import") {
        ghar::ImportOpts o;
        o.root = g.root;
        o.name = g.name.empty() ? "import" : g.name;
        o.pretty = g.pretty;
        std::string py = take_opt(args, "--python");
        if (!py.empty())
            o.python = py;
        for (const auto& a : args) {
            if (!a.empty() && a[0] != '-')
                o.modules.push_back(a);
        }
        return ghar::cmd_import(o);
    }

    if (cmd == "run") {
        ghar::RunOpts o;
        o.root = g.root;
        o.name = g.name.empty() ? "run" : g.name;
        o.pretty = g.pretty;
        std::string exp = take_opt(args, "--expect-exit");
        if (!exp.empty())
            ghar::parse_int(exp, o.expect_exit);
        std::string to = take_opt(args, "--timeout");
        if (!to.empty())
            ghar::parse_int(to, o.timeout_sec);
        o.shell_cmd = take_opt(args, "--sh");
        std::vector<std::string> after;
        split_dashdash(args, after);
        if (!after.empty())
            o.argv = after;
        else {
            for (const auto& a : args) {
                if (!a.empty() && a[0] != '-')
                    o.argv.push_back(a);
            }
        }
        return ghar::cmd_run(o);
    }

    if (cmd == "bench") {
        ghar::BenchOpts o;
        o.root = g.root;
        o.name = g.name.empty() ? "bench" : g.name;
        o.pretty = g.pretty;
        std::string rep = take_opt(args, "--repeat");
        if (!rep.empty()) {
            unsigned u = 0;
            ghar::parse_uint(rep, u);
            o.repeat = static_cast<int>(u);
        }
        std::string warm = take_opt(args, "--warmup");
        if (!warm.empty()) {
            unsigned u = 0;
            ghar::parse_uint(warm, u);
            o.warmup = static_cast<int>(u);
        }
        std::string to = take_opt(args, "--timeout");
        if (!to.empty())
            ghar::parse_int(to, o.timeout_sec);
        o.baseline_shell = take_opt(args, "--baseline-sh");
        o.shell_cmd = take_opt(args, "--sh");

        // --baseline -- cmd...  : everything after first -- is primary, need another approach
        // Support: ghar bench --name x --baseline-sh 'cmd' --sh 'cmd'
        // and: ghar bench --name x --repeat 10 -- ./a.out
        std::vector<std::string> after;
        split_dashdash(args, after);
        // Check for --baseline flag consuming remaining as baseline? Keep simple:
        // if --baseline present, next non-option tokens until end are baseline if no --
        if (take_flag(args, "--baseline")) {
            // remaining args are baseline; primary must be --sh
            for (const auto& a : args)
                o.baseline_argv.push_back(a);
            args.clear();
        }
        if (!after.empty())
            o.argv = after;
        else {
            for (const auto& a : args) {
                if (!a.empty())
                    o.argv.push_back(a);
            }
        }
        return ghar::cmd_bench(o);
    }

    if (cmd == "assert") {
        ghar::AssertOpts o;
        o.root = g.root;
        o.name = g.name;
        o.pretty = g.pretty;
        o.from = take_opt(args, "--from");
        o.metric = take_opt(args, "--metric");
        o.op = take_opt(args, "--op");
        o.value = take_opt(args, "--value");
        std::string tol = take_opt(args, "--tol");
        if (!tol.empty())
            ghar::parse_double(tol, o.tol);
        if (take_flag(args, "--relative"))
            o.relative = true;
        return ghar::cmd_assert(o);
    }

    if (cmd == "test") {
        ghar::TestOpts o;
        o.root = g.root;
        o.name = g.name;
        o.pretty = g.pretty;
        o.runner = take_opt(args, "--runner");
        std::string to = take_opt(args, "--timeout");
        if (!to.empty())
            ghar::parse_int(to, o.timeout_sec);
        maybe_path();
        o.root = g.root;
        for (const auto& a : args)
            o.extra_args.push_back(a);
        return ghar::cmd_test(o);
    }

    if (cmd == "python") {
        ghar::PythonOpts o;
        o.root = g.root;
        o.name = g.name.empty() ? "python" : g.name;
        o.pretty = g.pretty;
        o.file = take_opt(args, "--file");
        o.exec_code = take_flag(args, "--exec");
        o.check_imports = !take_flag(args, "--no-imports");
        std::string to = take_opt(args, "--timeout");
        if (!to.empty())
            ghar::parse_int(to, o.timeout_sec);
        // bare path: ghar python path/to/file.py
        for (const auto& a : args) {
            if (!a.empty() && a[0] != '-' && o.file.empty())
                o.file = a;
        }
        if (o.name == "python" && !o.file.empty())
            o.name = "python_" + ghar::basename(o.file);
        return ghar::cmd_python(o);
    }

    if (cmd == "torch") {
        ghar::TorchOpts o;
        o.root = g.root;
        o.name = g.name.empty() ? "torch" : g.name;
        o.pretty = g.pretty;
        o.file = take_opt(args, "--file");
        if (take_flag(args, "--no-forward"))
            o.forward = false;
        else if (take_flag(args, "--forward"))
            o.forward = true;
        if (take_flag(args, "--no-strict-attrs"))
            o.strict_attrs = false;
        else if (take_flag(args, "--strict-attrs"))
            o.strict_attrs = true;
        o.require_attrs = take_multi(args, "--require-attr");
        std::string dev = take_opt(args, "--device");
        if (!dev.empty())
            o.device = dev;
        std::string to = take_opt(args, "--timeout");
        if (!to.empty())
            ghar::parse_int(to, o.timeout_sec);
        for (const auto& a : args) {
            if (!a.empty() && a[0] != '-' && o.file.empty())
                o.file = a;
        }
        if (o.name == "torch" && !o.file.empty())
            o.name = "torch_" + ghar::basename(o.file);
        return ghar::cmd_torch(o);
    }

    if (cmd == "torch-attr") {
        ghar::TorchAttrOpts o;
        o.root = g.root;
        o.name = g.name.empty() ? "torch_attr" : g.name;
        o.pretty = g.pretty;
        for (const auto& a : args) {
            if (!a.empty() && a[0] != '-')
                o.attrs.push_back(a);
        }
        return ghar::cmd_torch_attr(o);
    }

    std::fprintf(stderr, "ghar: unknown command '%s' (try --help)\n", cmd.c_str());
    return ghar::EXIT_USAGE;
}
