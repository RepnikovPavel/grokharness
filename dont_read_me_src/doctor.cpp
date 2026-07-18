// SPDX-License-Identifier: 0BSD
#include "doctor.hpp"

#include "output.hpp"
#include "store.hpp"
#include "util.hpp"

#include <vector>

namespace ghar {
namespace {

FieldMap probe_tool(const std::string& tool, const std::vector<std::string>& version_argv)
{
    FieldMap f;
    f["tool"] = tool;
    auto path = which(tool);
    // also try common cuda path
    if (!path && tool == "nvcc") {
        if (path_exists("/usr/local/cuda/bin/nvcc"))
            path = "/usr/local/cuda/bin/nvcc";
    }
    if (!path) {
        f["status"] = "missing";
        f["path"] = "";
        f["version"] = "";
        return f;
    }
    f["status"] = "ok";
    f["path"] = *path;
    if (!version_argv.empty()) {
        std::vector<std::string> argv = version_argv;
        argv[0] = *path;
        auto r = run_cmd(argv, 10);
        std::string ver = trim(r.stdout_s.empty() ? r.stderr_s : r.stdout_s);
        // first line only
        auto nl = ver.find('\n');
        if (nl != std::string::npos)
            ver = ver.substr(0, nl);
        if (ver.size() > 120)
            ver = ver.substr(0, 117) + "...";
        f["version"] = ver;
    } else {
        f["version"] = "";
    }
    return f;
}

}  // namespace

int cmd_doctor(const std::string& root, bool pretty)
{
    Store store(root);
    std::vector<FieldMap> rows;

    rows.push_back(probe_tool("g++", {"g++", "--version"}));
    rows.push_back(probe_tool("clang++", {"clang++", "--version"}));
    rows.push_back(probe_tool("cmake", {"cmake", "--version"}));
    rows.push_back(probe_tool("make", {"make", "--version"}));
    rows.push_back(probe_tool("ninja", {"ninja", "--version"}));
    rows.push_back(probe_tool("nvcc", {"nvcc", "--version"}));
    rows.push_back(probe_tool("nvidia-smi", {"nvidia-smi", "--query-gpu=name,driver_version",
                                            "--format=csv,noheader"}));
    rows.push_back(probe_tool("python3", {"python3", "--version"}));
    rows.push_back(probe_tool("pytest", {"pytest", "--version"}));
    rows.push_back(probe_tool("ctest", {"ctest", "--version"}));
    rows.push_back(probe_tool("nm", {"nm", "--version"}));
    rows.push_back(probe_tool("objdump", {"objdump", "--version"}));
    rows.push_back(probe_tool("trtexec", {"trtexec", "--help"}));  // TensorRT

    // PyTorch probe (oracle dependency for ghar torch)
    {
        FieldMap f;
        f["tool"] = "torch";
        auto py = which("python3");
        if (!py) {
            f["status"] = "missing";
            f["path"] = "";
            f["version"] = "python3 missing";
        } else {
            auto r = run_cmd({*py, "-c",
                              "import torch; print(torch.__version__); "
                              "print('cuda='+str(torch.cuda.is_available()))"},
                             30);
            if (r.exit_code == 0) {
                f["status"] = "ok";
                f["path"] = *py;
                auto ver = trim(r.stdout_s);
                auto nl = ver.find('\n');
                if (nl != std::string::npos)
                    ver = ver.substr(0, nl) + " " + trim(ver.substr(nl + 1));
                if (ver.size() > 120)
                    ver = ver.substr(0, 117) + "...";
                f["version"] = ver;
            } else {
                f["status"] = "missing";
                f["path"] = *py;
                f["version"] = "import torch failed";
            }
        }
        rows.push_back(f);
    }

    // GPU memory snapshot if nvidia-smi present
    if (command_exists("nvidia-smi") || path_exists("/usr/bin/nvidia-smi")) {
        auto r = run_cmd({"nvidia-smi",
                          "--query-gpu=index,name,memory.total,memory.free,utilization.gpu",
                          "--format=csv,noheader,nounits"},
                         10);
        if (r.exit_code == 0) {
            for (const auto& line : split(r.stdout_s, '\n')) {
                auto L = trim(line);
                if (L.empty())
                    continue;
                auto parts = split(L, ',');
                FieldMap g;
                g["tool"] = "gpu";
                g["status"] = "ok";
                if (parts.size() >= 5) {
                    g["path"] = trim(parts[1]) + " idx=" + trim(parts[0]);
                    g["version"] = "mem_total_mib=" + trim(parts[2]) +
                                   " mem_free_mib=" + trim(parts[3]) +
                                   " util_pct=" + trim(parts[4]);
                } else {
                    g["path"] = L;
                    g["version"] = "";
                }
                rows.push_back(g);
            }
        }
    }

    print_doctor(rows, pretty);

    CheckResult cr;
    cr.kind = "doctor";
    cr.name = "env";
    cr.status = "ok";
    cr.detail = "environment probe";
    int missing = 0;
    for (const auto& row : rows) {
        auto it = row.find("status");
        if (it != row.end() && it->second == "missing")
            ++missing;
    }
    cr.metrics["tools_probed"] = std::to_string(rows.size());
    cr.metrics["tools_missing"] = std::to_string(missing);
    // pretty already printed the table; machine mode already printed doctor rows
    return finish_check(store, cr, false, /*silent=*/true);
}

}  // namespace ghar
