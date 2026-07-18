// SPDX-License-Identifier: 0BSD
#include "import_check.hpp"

#include "store.hpp"
#include "util.hpp"

#include <cctype>

namespace ghar {

int cmd_import(const ImportOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "import";
    r.name = opts.name;

    if (opts.modules.empty()) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "no modules";
        return finish_check(store, r, opts.pretty);
    }

    auto py = which(opts.python);
    if (!py) {
        r.status = "error";
        r.exit_code = EXIT_TOOL;
        r.detail = "python not found: " + opts.python;
        return finish_check(store, r, opts.pretty);
    }

    int ok = 0, fail = 0;
    std::vector<std::string> missing;
    std::vector<std::string> present;
    double total_ms = 0;

    for (const auto& mod : opts.modules) {
        // Reject anything that is not a dotted identifier (prevents -c injection).
        bool valid = !mod.empty();
        for (size_t i = 0; i < mod.size() && valid; ++i) {
            const unsigned char ch = static_cast<unsigned char>(mod[i]);
            if (std::isalnum(ch) || ch == '_' || ch == '.')
                continue;
            valid = false;
        }
        if (valid && (mod.front() == '.' || mod.back() == '.' || mod.find("..") != std::string::npos))
            valid = false;
        if (!valid) {
            ++fail;
            missing.push_back(mod);
            r.metrics["err." + mod] = "invalid module name";
            continue;
        }
        // Pass module name via argv env-safe: python -c with sys.argv
        const std::string code =
            "import importlib,sys\n"
            "mod=sys.argv[1]\n"
            "m=importlib.import_module(mod)\n"
            "v=getattr(m,'__version__',getattr(m,'VERSION',''))\n"
            "print('OK',getattr(m,'__file__',''),v)\n";
        auto cr = run_cmd({*py, "-c", code, mod}, 60);
        total_ms += cr.wall_ms;
        if (cr.exit_code == 0 && starts_with(trim(cr.stdout_s), "OK")) {
            ++ok;
            present.push_back(mod);
            auto parts = split_ws(trim(cr.stdout_s));
            if (parts.size() >= 3)
                r.metrics["ver." + mod] = parts.back();
            if (parts.size() >= 2)
                r.metrics["path." + mod] = parts[1];
        } else {
            ++fail;
            missing.push_back(mod);
            auto err = trim(cr.stderr_s);
            auto nl = err.find('\n');
            if (nl != std::string::npos)
                err = err.substr(0, nl);
            r.metrics["err." + mod] = err;
        }
    }

    r.metrics["requested"] = std::to_string(opts.modules.size());
    r.metrics["ok"] = std::to_string(ok);
    r.metrics["fail"] = std::to_string(fail);
    r.metrics["wall_ms"] = std::to_string(total_ms);
    r.metrics["missing_list"] = join(missing, ",");
    r.metrics["ok_list"] = join(present, ",");

    if (fail == 0) {
        r.status = "ok";
        r.exit_code = EXIT_OK;
        r.detail = "all imports ok";
    } else {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "missing imports: " + join(missing, ",");
    }
    return finish_check(store, r, opts.pretty);
}

}  // namespace ghar
