// SPDX-License-Identifier: 0BSD
#include "symbols_check.hpp"

#include "store.hpp"
#include "util.hpp"

#include <cctype>
#include <set>

namespace ghar {
namespace {

// True if `sym` appears as a whole token (not a substring of a longer name).
bool token_present(const std::string& text, const std::string& sym)
{
    size_t pos = 0;
    while ((pos = text.find(sym, pos)) != std::string::npos) {
        const bool left_ok =
            pos == 0 ||
            !(std::isalnum(static_cast<unsigned char>(text[pos - 1])) || text[pos - 1] == '_');
        const size_t end = pos + sym.size();
        const bool right_ok =
            end >= text.size() ||
            !(std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_');
        if (left_ok && right_ok)
            return true;
        pos = end;
    }
    return false;
}

}  // namespace

int cmd_symbols(const SymbolsOpts& opts)
{
    Store store(opts.root);
    CheckResult r;
    r.kind = "symbols";
    r.name = opts.name;

    if (opts.symbols.empty()) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "no symbols given";
        return finish_check(store, r, opts.pretty);
    }

    std::set<std::string> found;
    std::set<std::string> missing(opts.symbols.begin(), opts.symbols.end());

    // Search in binaries via nm
    for (const auto& bin : opts.binaries) {
        if (!path_exists(bin)) {
            r.metrics["missing_binary"] = bin;
            continue;
        }
        auto nm = which("nm");
        if (!nm) {
            r.status = "error";
            r.exit_code = EXIT_TOOL;
            r.detail = "nm not found (binutils)";
            return finish_check(store, r, opts.pretty);
        }
        // Prefer demangled names (-C) so C++ `add` matches; still token-bound.
        auto cr = run_cmd({*nm, "-g", "--defined-only", "-C", bin}, 30);
        if (cr.exit_code != 0)
            cr = run_cmd({*nm, "-C", bin}, 30);
        if (cr.exit_code != 0)
            cr = run_cmd({*nm, "-g", "--defined-only", bin}, 30);
        if (cr.exit_code != 0)
            cr = run_cmd({*nm, bin}, 30);
        for (const auto& sym : opts.symbols) {
            if (token_present(cr.stdout_s, sym)) {
                found.insert(sym);
                missing.erase(sym);
                continue;
            }
            // C++ demangled forms may be "add(int, int)" — accept prefix token + '(' 
            if (token_present(cr.stdout_s, sym + "(")) {
                found.insert(sym);
                missing.erase(sym);
            }
        }
    }

    // Search headers with word-boundary identifier scan
    for (const auto& hdr : opts.headers) {
        if (!is_file(hdr)) {
            r.metrics["missing_header"] = hdr;
            continue;
        }
        const std::string text = read_file(hdr);
        for (const auto& sym : opts.symbols) {
            if (token_present(text, sym)) {
                found.insert(sym);
                missing.erase(sym);
            }
        }
    }

    // If no binaries/headers: try `c++filt`/compiler - nothing; require inputs
    if (opts.binaries.empty() && opts.headers.empty()) {
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = "provide --bin and/or --header to search";
        return finish_check(store, r, opts.pretty);
    }

    r.metrics["requested"] = std::to_string(opts.symbols.size());
    r.metrics["found"] = std::to_string(found.size());
    r.metrics["missing"] = std::to_string(missing.size());
    r.metrics["found_list"] = join(std::vector<std::string>(found.begin(), found.end()), ",");
    r.metrics["missing_list"] =
        join(std::vector<std::string>(missing.begin(), missing.end()), ",");

    if (missing.empty()) {
        r.status = "ok";
        r.exit_code = EXIT_OK;
        r.detail = "all symbols found";
    } else {
        r.status = "fail";
        r.exit_code = EXIT_FAIL;
        r.detail = "missing symbols: " + r.metrics["missing_list"];
    }
    return finish_check(store, r, opts.pretty);
}

}  // namespace ghar
