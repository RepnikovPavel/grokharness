// SPDX-License-Identifier: 0BSD
#pragma once

#include <string>

namespace ghar {

int cmd_gate(const std::string& root, bool pretty);
int cmd_report(const std::string& root, bool pretty, bool claims_only);
int cmd_init(const std::string& root, bool with_scaffold = true);
int cmd_reset(const std::string& root, bool keep_results);

}  // namespace ghar
