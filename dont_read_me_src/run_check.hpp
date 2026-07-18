// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

struct RunOpts {
    std::string root = ".";
    std::string name;
    std::vector<std::string> argv;
    std::string shell_cmd;  // if set, run via sh -c
    int timeout_sec = 300;
    int expect_exit = 0;
    bool pretty = false;
};

int cmd_run(const RunOpts& opts);

}  // namespace ghar
