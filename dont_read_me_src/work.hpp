// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>

namespace ghar {

// Long-running agent work session — solves "agent quits after 5 minutes".
//
// Industry parallel: Claude Stop hooks + campaign timers + "don't stop until
// oracles + quota met". Wall-clock alone is weak; we combine:
//   • min_work_minutes   (default from config / --minutes)
//   • min_verify_ok      (successful ghar verify count in this session)
//   • min_heartbeats     (agent must ping work heartbeat while working)
//
// Delivery path:
//   ghar work start --minutes 120 --goal "..."
//   … long autonomous work …
//   ghar work heartbeat   # periodically
//   ghar verify           # each fix cycle
//   ghar work done        # exit 0 only if quotas met (or ghar verify --deliver)

struct WorkStartOpts {
    std::string root = ".";
    int min_minutes = -1;      // -1 → config; 0 = no wall-clock wait; >0 = minutes
    int min_verify_ok = -1;    // -1 → config / default 2
    int min_heartbeats = -1;   // -1 → config / default max(3, min_minutes/10)
    std::string goal;
    bool pretty = false;
    bool force = false;  // restart if active
};

struct WorkOpts {
    std::string root = ".";
    bool pretty = false;
    std::string note;
    bool force = false;  // for done/abandon
};

int cmd_work_start(const WorkStartOpts& opts);
int cmd_work_heartbeat(const WorkOpts& opts);
int cmd_work_status(const WorkOpts& opts);
int cmd_work_done(const WorkOpts& opts);      // delivery: quotas + optional last verify
int cmd_work_abandon(const WorkOpts& opts);   // explicit give-up (still exit 4 unless --force)

// Called from verify when --deliver or when work session requires it.
// Returns EXIT_OK if no active session or quotas satisfied; EXIT_FAIL + FEEDBACK otherwise.
int work_check_delivery(const std::string& root, bool pretty, std::string& reason_out);

// Increment verify_ok counter (call after successful pipeline).
void work_note_verify_ok(const std::string& root);

// Config defaults for work quotas.
struct WorkConfig {
    int min_work_minutes = 60;  // anti-5-minute-quit default for deep tasks
    int min_verify_ok = 2;
    int min_heartbeats = 6;
    int heartbeat_max_gap_sec = 900;  // 15 min without heartbeat → stale (warn/block done)
    bool enforce_on_verify = false;   // if true, plain `ghar verify` also checks work quotas
};

WorkConfig load_work_config(const std::string& root);

}  // namespace ghar
