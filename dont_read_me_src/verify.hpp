// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>

namespace ghar {

struct VerifyOpts {
    std::string root = ".";
    bool pretty = false;
    bool skip_gate = false;     // run pipeline only
    bool fail_fast = true;      // override config on_fail=stop
    bool deliver = false;       // after ok pipeline+gate, enforce work session quotas
    std::string only_step;      // if set, run one named step
};

// Aider-style verification loop: lint → build → test (config-driven), then gate.
// Exit 0 only if all steps ok AND gate ok (unless --no-gate).
// On failure prints agent-facing feedback (stderr tail) — like Aider auto-test.
int cmd_verify(const VerifyOpts& opts);

// Print resolved config (for agents/humans).
int cmd_config_show(const std::string& root, bool pretty);

// Scaffold .ghar/config + integration hook files.
int cmd_scaffold(const std::string& root, bool force);

}  // namespace ghar
