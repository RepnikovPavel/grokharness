// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

struct SymbolsOpts {
    std::string root = ".";
    std::string name;
    std::vector<std::string> symbols;
    std::vector<std::string> binaries;  // .so / .a / ELF to nm
    std::vector<std::string> headers;   // grep headers for declarations
    bool pretty = false;
};

int cmd_symbols(const SymbolsOpts& opts);

}  // namespace ghar
