// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

struct ImportOpts {
    std::string root = ".";
    std::string name;
    std::vector<std::string> modules;  // python modules or "pkg.mod"
    std::string python = "python3";
    bool pretty = false;
};

int cmd_import(const ImportOpts& opts);

}  // namespace ghar
