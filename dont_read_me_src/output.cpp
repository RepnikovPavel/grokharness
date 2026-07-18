// SPDX-License-Identifier: 0BSD
#include "output.hpp"

#include "tsv.hpp"
#include "util.hpp"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace ghar {
namespace {

std::string metrics_flat(const FieldMap& m)
{
    std::vector<std::string> parts;
    for (const auto& kv : m)
        parts.push_back(kv.first + "=" + kv.second);
    return join(parts, " ");
}

}  // namespace

void print_table(const std::vector<std::string>& header,
                 const std::vector<std::vector<std::string>>& rows)
{
    if (header.empty())
        return;
    std::vector<size_t> widths(header.size(), 0);
    for (size_t i = 0; i < header.size(); ++i)
        widths[i] = header[i].size();
    for (const auto& row : rows) {
        for (size_t i = 0; i < header.size() && i < row.size(); ++i)
            widths[i] = std::max(widths[i], row[i].size());
    }

    auto line = [&]() {
        std::fputs("+", stdout);
        for (size_t w : widths) {
            for (size_t i = 0; i < w + 2; ++i)
                std::fputc('-', stdout);
            std::fputs("+", stdout);
        }
        std::fputc('\n', stdout);
    };

    auto row_out = [&](const std::vector<std::string>& cols) {
        std::fputs("|", stdout);
        for (size_t i = 0; i < header.size(); ++i) {
            const std::string& c = i < cols.size() ? cols[i] : "";
            std::fprintf(stdout, " %-*s |", static_cast<int>(widths[i]), c.c_str());
        }
        std::fputc('\n', stdout);
    };

    line();
    row_out(header);
    line();
    for (const auto& r : rows)
        row_out(r);
    line();
}

void print_check_result(const CheckResult& r, bool pretty)
{
    if (pretty) {
        std::vector<std::string> header = {"field", "value"};
        std::vector<std::vector<std::string>> rows = {
            {"id", r.id},
            {"kind", r.kind},
            {"name", r.name.empty() ? "-" : r.name},
            {"status", r.status},
            {"exit_code", std::to_string(r.exit_code)},
            {"detail", r.detail},
        };
        for (const auto& kv : r.metrics)
            rows.push_back({"metric." + kv.first, kv.second});
        print_table(header, rows);
        return;
    }

    FieldMap f;
    f["id"] = r.id;
    f["ts"] = r.ts;
    f["kind"] = r.kind;
    f["name"] = r.name;
    f["status"] = r.status;
    f["exit_code"] = std::to_string(r.exit_code);
    f["detail"] = r.detail;
    for (const auto& kv : r.metrics)
        f["m." + kv.first] = kv.second;
    emit_row("result", f);
}

void print_claims(const std::vector<Claim>& claims, bool pretty)
{
    if (pretty) {
        std::vector<std::string> header = {"name", "kind", "status", "metrics", "updated_at"};
        std::vector<std::vector<std::string>> rows;
        for (const auto& c : claims)
            rows.push_back({c.name, c.kind, c.status, metrics_flat(c.metrics), c.updated_at});
        print_table(header, rows);
        return;
    }
    for (const auto& c : claims) {
        FieldMap f;
        f["name"] = c.name;
        f["kind"] = c.kind;
        f["status"] = c.status;
        f["updated_at"] = c.updated_at;
        f["source_id"] = c.source_id;
        for (const auto& kv : c.metrics)
            f["m." + kv.first] = kv.second;
        emit_row("claim", f);
    }
}

void print_results(const std::vector<CheckResult>& results, bool pretty)
{
    if (pretty) {
        std::vector<std::string> header = {"ts", "kind", "name", "status", "detail"};
        std::vector<std::vector<std::string>> rows;
        for (const auto& r : results)
            rows.push_back({r.ts, r.kind, r.name.empty() ? "-" : r.name, r.status, r.detail});
        print_table(header, rows);
        return;
    }
    for (const auto& r : results)
        print_check_result(r, false);
}

void print_gate(const std::vector<Claim>& claims, int ok, int fail, bool pretty)
{
    const std::string status = fail == 0 ? "ok" : "fail";
    if (pretty) {
        std::printf("gate status=%s  ok=%d  fail=%d\n", status.c_str(), ok, fail);
        print_claims(claims, true);
        return;
    }
    FieldMap f;
    f["status"] = status;
    f["ok"] = std::to_string(ok);
    f["fail"] = std::to_string(fail);
    f["claims"] = std::to_string(claims.size());
    emit_row("gate", f);
    for (const auto& c : claims) {
        FieldMap cf;
        cf["name"] = c.name;
        cf["kind"] = c.kind;
        cf["status"] = c.status;
        emit_row("gate_claim", cf);
    }
}

void print_doctor(const std::vector<FieldMap>& rows, bool pretty)
{
    if (pretty) {
        std::vector<std::string> header = {"tool", "status", "path_or_detail", "version"};
        std::vector<std::vector<std::string>> table;
        for (const auto& r : rows) {
            auto get = [&](const char* k) {
                auto it = r.find(k);
                return it == r.end() ? "" : it->second;
            };
            table.push_back({get("tool"), get("status"), get("path"), get("version")});
        }
        print_table(header, table);
        return;
    }
    for (const auto& r : rows)
        emit_row("doctor", r);
}

}  // namespace ghar
