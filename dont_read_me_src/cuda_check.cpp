// SPDX-License-Identifier: 0BSD
#include "cuda_check.hpp"

#include "compile_check.hpp"
#include "store.hpp"
#include "util.hpp"

namespace ghar {
namespace {

void add_device_metrics(FieldMap& m)
{
    auto smi = which("nvidia-smi");
    if (!smi && path_exists("/usr/bin/nvidia-smi"))
        smi = "/usr/bin/nvidia-smi";
    if (!smi) {
        m["gpu_present"] = "0";
        return;
    }
    m["gpu_present"] = "1";
    auto r = run_cmd({*smi,
                      "--query-gpu=count,name,compute_cap,memory.total,memory.free,driver_version",
                      "--format=csv,noheader,nounits"},
                     10);
    if (r.exit_code != 0) {
        m["gpu_query"] = "fail";
        return;
    }
    auto lines = split(r.stdout_s, '\n');
    int n = 0;
    for (const auto& line : lines) {
        if (trim(line).empty())
            continue;
        ++n;
        if (n == 1) {
            auto p = split(line, ',');
            // count is first field of first line for this query? Actually each line is one GPU
            // with fields: name may include commas rarely — use simple split
            // query was: count is wrong — use per-GPU lines without count
        }
    }
    // re-query properly per GPU
    auto r2 = run_cmd({*smi,
                       "--query-gpu=index,name,compute_cap,memory.total,memory.free,driver_version",
                       "--format=csv,noheader,nounits"},
                      10);
    int gpus = 0;
    double free_sum = 0, total_sum = 0;
    std::string names;
    std::string caps;
    std::string driver;
    for (const auto& line : split(r2.stdout_s, '\n')) {
        auto L = trim(line);
        if (L.empty())
            continue;
        auto p = split(L, ',');
        if (p.size() < 6)
            continue;
        ++gpus;
        if (!names.empty())
            names += ";";
        names += trim(p[1]);
        if (!caps.empty())
            caps += ";";
        caps += trim(p[2]);
        double tot = 0, free = 0;
        parse_double(trim(p[3]), tot);
        parse_double(trim(p[4]), free);
        total_sum += tot;
        free_sum += free;
        driver = trim(p[5]);
    }
    m["gpu_count"] = std::to_string(gpus);
    m["gpu_name"] = names;
    m["compute_cap"] = caps;
    m["mem_total_mib"] = std::to_string(total_sum);
    m["mem_free_mib"] = std::to_string(free_sum);
    m["driver"] = driver;
}

std::string find_nvcc()
{
    if (auto p = which("nvcc"))
        return *p;
    if (path_exists("/usr/local/cuda/bin/nvcc"))
        return "/usr/local/cuda/bin/nvcc";
    return "";
}

}  // namespace

int cmd_cuda(const CudaOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "cuda";
    r.name = opts.name.empty() ? "cuda" : opts.name;

    add_device_metrics(r.metrics);

    const std::string nvcc = find_nvcc();
    r.metrics["nvcc"] = nvcc.empty() ? "missing" : nvcc;

    if (opts.device_only || opts.sources.empty()) {
        if (r.metrics["gpu_present"] == "1") {
            r.status = "ok";
            r.detail = "GPU probe ok: " + r.metrics["gpu_name"];
            r.exit_code = EXIT_OK;
        } else {
            r.status = "fail";
            r.detail = "no GPU / nvidia-smi failed";
            r.exit_code = EXIT_FAIL;
        }
        return finish_check(store, r, opts.pretty);
    }

    if (nvcc.empty()) {
        r.status = "error";
        r.exit_code = EXIT_TOOL;
        r.detail = "nvcc not found";
        return finish_check(store, r, opts.pretty);
    }

    // compile each .cu (or all together)
    CompileOpts co;
    co.root = opts.root;
    co.name = r.name + "_build";
    co.compiler = "nvcc";
    co.sources = opts.sources;
    co.flags = opts.flags;
    co.timeout_sec = opts.timeout_sec;
    co.pretty = false;
    co.silent = true;  // parent emits one result
    const int rc = cmd_compile(co);

    // pull claim metrics from store
    if (auto claim = store.find_claim(co.name)) {
        for (const auto& kv : claim->metrics)
            r.metrics["compile." + kv.first] = kv.second;
        r.metrics["compile_status"] = claim->status;
    }

    if (rc == EXIT_OK && r.metrics["gpu_present"] == "1") {
        r.status = "ok";
        r.exit_code = EXIT_OK;
        r.detail = "nvcc compile + GPU probe ok";
    } else if (rc != EXIT_OK) {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "nvcc compile failed";
    } else {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "compile ok but no GPU";
    }
    return finish_check(store, r, opts.pretty);
}

}  // namespace ghar
