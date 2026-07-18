// SPDX-License-Identifier: 0BSD
#pragma once

#include <map>
#include <string>
#include <vector>

namespace ghar {

// Generic row of named metrics / fields (string values).
using FieldMap = std::map<std::string, std::string>;

struct TsvTable {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
};

std::string fields_to_tsv_line(const std::vector<std::string>& cols);
std::vector<std::string> parse_tsv_line(const std::string& line);
TsvTable read_tsv_file(const std::string& path);
bool write_tsv_file(const std::string& path, const TsvTable& table);
bool append_tsv_row(const std::string& path, const std::vector<std::string>& header,
                    const std::vector<std::string>& row);

// Emit one machine-readable result block to stdout (kind + key=value TSV rows).
void emit_row(const std::string& kind, const FieldMap& fields);
void emit_rows(const std::string& kind, const std::vector<std::string>& header,
               const std::vector<FieldMap>& rows);

}  // namespace ghar
