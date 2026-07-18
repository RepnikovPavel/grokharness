// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>
#include <vector>

namespace ghar {

struct PythonOpts {
    std::string root = ".";
    std::string name;
    std::string file;
    bool exec_code = false;
    bool check_imports = true;
    int timeout_sec = 30;
    bool pretty = false;
};

struct TorchOpts {
    std::string root = ".";
    std::string name;
    std::string file;
    bool forward = true;
    bool strict_attrs = true;
    std::string device = "cpu";
    std::vector<std::string> require_attrs;
    int timeout_sec = 60;
    bool pretty = false;
};

struct TorchAttrOpts {
    std::string root = ".";
    std::string name;
    std::vector<std::string> attrs;
    bool pretty = false;
};

int cmd_python(const PythonOpts& opts);
int cmd_torch(const TorchOpts& opts);
int cmd_torch_attr(const TorchAttrOpts& opts);

// Resolve path to oracles/py_torch_validate.py
std::string oracle_script_path();

}  // namespace ghar
