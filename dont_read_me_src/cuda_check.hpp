// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

struct CudaOpts {
    std::string root = ".";
    std::string name;
    std::vector<std::string> sources;  // .cu files to compile
    std::vector<std::string> flags;
    bool device_only = false;  // only probe GPU
    int timeout_sec = 180;
    bool pretty = false;
};

int cmd_cuda(const CudaOpts& opts);

}  // namespace ghar
