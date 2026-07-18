// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

struct TestOpts {
    std::string root = ".";
    std::string name;
    std::string runner;  // auto|pytest|ctest|sh
    std::vector<std::string> extra_args;
    int timeout_sec = 600;
    bool pretty = false;
};

int cmd_test(const TestOpts& opts);

}  // namespace ghar
