// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

// Project verification config (Aider-style oracles).
// Search order:
//   1) <root>/.ghar/config
//   2) <root>/ghar.conf
//
// Format (line-oriented, no TOML dep):
//   # comment
//   lint_cmd=...
//   build_cmd=...
//   test_cmd=...
//   step.<name>=shell command
//   verify_order=lint,build,test,cuda   (optional; default lint,build,test)
//   timeout_sec=600
//   on_fail=stop|continue

struct VerifyStep {
    std::string name;  // lint|build|test|cuda|custom
    std::string cmd;   // shell command, run in project root
};

struct ProjectConfig {
    std::string path;  // which file was loaded (empty if defaults only)
    std::vector<VerifyStep> steps;
    int timeout_sec = 600;
    std::string on_fail = "stop";  // stop after first failure (Aider-like feedback)
    bool loaded = false;
};

// Load config; if missing, synthesize sensible defaults from project layout.
ProjectConfig load_project_config(const std::string& root);

// Write a starter config into .ghar/config (does not overwrite unless force).
bool write_default_config(const std::string& root, bool force = false);

// Human/agent description of default content.
std::string default_config_text();

}  // namespace ghar
