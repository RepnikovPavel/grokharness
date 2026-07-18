// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>

namespace ghar {

struct AssertOpts {
    std::string root = ".";
    std::string name;          // name of this assert claim
    std::string from;          // claim/result name to read metric from
    std::string metric;        // metric key (e.g. mean_ms, speedup, m.out.loss)
    std::string op;            // eq|ne|lt|le|gt|ge|approx
    std::string value;         // expected
    double tol = 1e-6;         // for approx / relative
    bool relative = false;     // tol is relative for approx
    bool pretty = false;
};

int cmd_assert(const AssertOpts& opts);

}  // namespace ghar
