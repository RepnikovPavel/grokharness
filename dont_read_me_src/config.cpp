// SPDX-License-Identifier: 0BSD
#include "config.hpp"

#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <map>

namespace ghar {
namespace {

std::string strip_comment(const std::string& line)
{
    // allow # in commands if escaped? keep simple: # only at start or after space before comment
    bool in_quote = false;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '"' || line[i] == '\'')
            in_quote = !in_quote;
        if (!in_quote && line[i] == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1]))))
            return trim(line.substr(0, i));
    }
    return trim(line);
}

bool parse_kv(const std::string& line, std::string& key, std::string& val)
{
    const auto eq = line.find('=');
    if (eq == std::string::npos)
        return false;
    key = trim(line.substr(0, eq));
    val = trim(line.substr(eq + 1));
    return !key.empty();
}

ProjectConfig defaults_from_layout(const std::string& root)
{
    ProjectConfig c;
    c.timeout_sec = 600;
    c.on_fail = "stop";

    // Aider / industry default: lint → build → test (execution as oracle).
    // Prefer tests/run_all.sh over full ctest so `ghar verify` does not recurse into itself.
    if (path_exists(join_path(root, "CMakeLists.txt"))) {
        c.steps.push_back({"build", "cmake --build build -j\"$(nproc 2>/dev/null || echo 2)\""});
        if (path_exists(join_path(root, "dont_read_me_src/tests/run_all.sh"))) {
            c.steps.push_back({"test", "bash dont_read_me_src/tests/run_all.sh"});
        } else {
            c.steps.push_back(
                {"test", "ctest --test-dir build -L integration --output-on-failure"});
        }
    } else if (path_exists(join_path(root, "package.json"))) {
        c.steps.push_back({"lint", "npm run lint --if-present"});
        c.steps.push_back({"test", "npm test --if-present"});
    } else if (path_exists(join_path(root, "pyproject.toml")) ||
               path_exists(join_path(root, "pytest.ini")) || path_exists(join_path(root, "tests"))) {
        c.steps.push_back({"test", "pytest -q --tb=short"});
    } else if (path_exists(join_path(root, "Makefile"))) {
        c.steps.push_back({"build", "make -j\"$(nproc 2>/dev/null || echo 2)\""});
        c.steps.push_back({"test", "make test"});
    } else {
        c.steps.push_back({"test", "true  # configure test_cmd in .ghar/config"});
    }
    return c;
}

}  // namespace

std::string default_config_text()
{
    return
        "# ghar.conf — project oracles (execution as truth; NO LLM-as-judge)\n"
        "# Best practices: Aider (--lint-cmd/--test-cmd), Claude Code Stop hooks.\n"
        "#\n"
        "# Pipeline: lint → build → test (then optional domain steps).\n"
        "# Each step must exit 0. First failure stops (on_fail=stop) → FEEDBACK to agent.\n"
        "#\n"
        "# Do NOT set test_cmd=ghar verify (recursion).\n"
        "# Prefer project scripts over ctest labels that include ghar verify itself.\n"
        "\n"
        "timeout_sec=600\n"
        "on_fail=stop\n"
        "\n"
        "lint_cmd=\n"
        "build_cmd=cmake --build build -j$(nproc)\n"
        "test_cmd=bash dont_read_me_src/tests/run_all.sh\n"
        "\n"
        "# Anti-5-minute-quit (ghar work start defaults)\n"
        "min_work_minutes=60\n"
        "min_verify_ok=2\n"
        "min_heartbeats=6\n"
        "heartbeat_max_gap_sec=900\n"
        "enforce_work_on_verify=false\n"
        "\n"
        "# Extra domain oracles (optional)\n"
        "# step.cuda=./build/ghar cuda --device --name gpu\n"
        "\n"
        "# verify_order=lint,build,test\n";
}

bool write_default_config(const std::string& root, bool force)
{
    const std::string dir = join_path(root, ".ghar");
    if (!mkdir_p(dir))
        return false;
    const std::string path = join_path(dir, "config");
    if (path_exists(path) && !force)
        return true;
    return write_file(path, default_config_text());
}

ProjectConfig load_project_config(const std::string& root)
{
    // Prefer committed ghar.conf (repo oracle) over local .ghar/config.
    std::vector<std::string> candidates = {join_path(root, "ghar.conf"), join_path(root, ".ghar/config")};

    std::string text;
    std::string used;
    for (const auto& p : candidates) {
        if (is_file(p)) {
            text = read_file(p);
            used = p;
            break;
        }
    }

    if (text.empty()) {
        auto d = defaults_from_layout(root);
        d.path = "";
        d.loaded = false;
        return d;
    }

    ProjectConfig c;
    c.path = used;
    c.loaded = true;

    std::map<std::string, std::string> aliases;  // lint/build/test + step.*
    std::string order;

    for (const auto& raw : split(text, '\n')) {
        auto line = strip_comment(raw);
        if (line.empty())
            continue;
        std::string key, val;
        if (!parse_kv(line, key, val))
            continue;
        if (key == "timeout_sec") {
            int t = 0;
            if (parse_int(val, t) && t > 0)
                c.timeout_sec = t;
        } else if (key == "on_fail") {
            c.on_fail = val;
        } else if (key == "verify_order") {
            order = val;
        } else if (key == "lint_cmd" || key == "lint") {
            aliases["lint"] = val;
        } else if (key == "build_cmd" || key == "build") {
            aliases["build"] = val;
        } else if (key == "test_cmd" || key == "test") {
            aliases["test"] = val;
        } else if (starts_with(key, "step.")) {
            aliases[key.substr(5)] = val;
        }
    }

    auto push_if = [&](const std::string& name) {
        auto it = aliases.find(name);
        if (it == aliases.end())
            return;
        if (trim(it->second).empty())
            return;
        c.steps.push_back({name, it->second});
    };

    if (!order.empty()) {
        for (const auto& part : split(order, ',')) {
            auto n = trim(part);
            if (!n.empty())
                push_if(n);
        }
        // append any remaining step.* not in order
        for (const auto& kv : aliases) {
            bool seen = false;
            for (const auto& s : c.steps)
                if (s.name == kv.first)
                    seen = true;
            if (!seen && !trim(kv.second).empty())
                c.steps.push_back({kv.first, kv.second});
        }
    } else {
        // Aider default order
        push_if("lint");
        push_if("build");
        push_if("test");
        for (const auto& kv : aliases) {
            if (kv.first == "lint" || kv.first == "build" || kv.first == "test")
                continue;
            if (!trim(kv.second).empty())
                c.steps.push_back({kv.first, kv.second});
        }
    }

    if (c.steps.empty()) {
        // file existed but empty cmds — fall back
        auto d = defaults_from_layout(root);
        d.path = used;
        d.loaded = true;
        d.timeout_sec = c.timeout_sec;
        d.on_fail = c.on_fail;
        return d;
    }
    return c;
}

}  // namespace ghar
