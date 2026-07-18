// SPDX-License-Identifier: 0BSD
#include "tsv.hpp"

#include "util.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace ghar {

std::string fields_to_tsv_line(const std::vector<std::string>& cols)
{
    std::vector<std::string> esc;
    esc.reserve(cols.size());
    for (const auto& c : cols)
        esc.push_back(escape_tsv(c));
    return join(esc, "\t");
}

std::vector<std::string> parse_tsv_line(const std::string& line)
{
    // Reverse of escape_tsv for our limited escapes
    std::vector<std::string> cols;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\t') {
            cols.push_back(cur);
            cur.clear();
            continue;
        }
        if (line[i] == '\\' && i + 1 < line.size()) {
            const char n = line[i + 1];
            if (n == 't') {
                cur += '\t';
                ++i;
                continue;
            }
            if (n == 'n') {
                cur += '\n';
                ++i;
                continue;
            }
            if (n == 'r') {
                cur += '\r';
                ++i;
                continue;
            }
            if (n == '\\') {
                cur += '\\';
                ++i;
                continue;
            }
        }
        cur += line[i];
    }
    cols.push_back(cur);
    return cols;
}

TsvTable read_tsv_file(const std::string& path)
{
    TsvTable t;
    std::ifstream in(path);
    if (!in)
        return t;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        auto cols = parse_tsv_line(line);
        if (first) {
            t.header = std::move(cols);
            first = false;
        } else {
            t.rows.push_back(std::move(cols));
        }
    }
    return t;
}

bool write_tsv_file(const std::string& path, const TsvTable& table)
{
    std::ostringstream ss;
    ss << fields_to_tsv_line(table.header) << '\n';
    for (const auto& row : table.rows)
        ss << fields_to_tsv_line(row) << '\n';
    return write_file(path, ss.str());
}

bool append_tsv_row(const std::string& path, const std::vector<std::string>& header,
                    const std::vector<std::string>& row)
{
    // Create file with header if missing OR empty OR has no non-empty lines.
    bool need_header = !path_exists(path);
    if (!need_header) {
        const std::string existing = read_file(path);
        bool any = false;
        for (char c : existing) {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                any = true;
                break;
            }
        }
        if (!any)
            need_header = true;
    }
    if (need_header) {
        TsvTable t;
        t.header = header;
        t.rows.push_back(row);
        return write_tsv_file(path, t);
    }
    return append_file(path, fields_to_tsv_line(row) + "\n");
}

void emit_row(const std::string& kind, const FieldMap& fields)
{
    std::fputs("kind\t", stdout);
    std::fputs(kind.c_str(), stdout);
    for (const auto& kv : fields) {
        std::fputc('\t', stdout);
        std::fputs(escape_tsv(kv.first).c_str(), stdout);
        std::fputc('=', stdout);
        std::fputs(escape_tsv(kv.second).c_str(), stdout);
    }
    std::fputc('\n', stdout);
}

void emit_rows(const std::string& kind, const std::vector<std::string>& header,
               const std::vector<FieldMap>& rows)
{
    // Header line for table-ish consumers
    FieldMap h;
    h["columns"] = join(header, ",");
    emit_row(kind + "_header", h);
    for (const auto& row : rows)
        emit_row(kind, row);
}

}  // namespace ghar
