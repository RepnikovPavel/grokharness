// SPDX-License-Identifier: 0BSD
#pragma once

#include "store.hpp"

#include <string>
#include <vector>

namespace ghar {

void print_check_result(const CheckResult& r, bool pretty);
void print_claims(const std::vector<Claim>& claims, bool pretty);
void print_results(const std::vector<CheckResult>& results, bool pretty);
void print_gate(const std::vector<Claim>& claims, int ok, int fail, bool pretty);
void print_doctor(const std::vector<FieldMap>& rows, bool pretty);

// Pretty psql-style table
void print_table(const std::vector<std::string>& header,
                 const std::vector<std::vector<std::string>>& rows);

}  // namespace ghar
