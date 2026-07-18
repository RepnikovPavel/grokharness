// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

struct BenchOpts {
    std::string root = ".";
    std::string name;
    std::vector<std::string> argv;
    std::string shell_cmd;
    std::vector<std::string> baseline_argv;
    std::string baseline_shell;
    int warmup = 1;
    int repeat = 10;
    int timeout_sec = 300;
    bool pretty = false;
};

int cmd_bench(const BenchOpts& opts);

}  // namespace ghar
