// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

struct CompileOpts {
    std::string root = ".";
    std::string name;
    std::string compiler;  // g++|clang++|nvcc|auto
    std::string standard = "c++17";
    std::vector<std::string> sources;
    std::vector<std::string> flags;
    std::string output;  // optional binary path
    bool link = true;
    int timeout_sec = 120;
    bool pretty = false;
    bool silent = false;  // nested use: persist only
};

int cmd_compile(const CompileOpts& opts);

}  // namespace ghar
