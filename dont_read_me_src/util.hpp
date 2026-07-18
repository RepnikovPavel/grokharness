// SPDX-License-Identifier: 0BSD
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ghar {

struct CmdResult {
    int exit_code = -1;
    std::string stdout_s;
    std::string stderr_s;
    double wall_ms = 0.0;
    bool timed_out = false;
};

struct PathStat {
    bool valid = false;
    bool is_file = false;
    bool is_dir = false;
    std::uint64_t size = 0;
};

// Shell / process
CmdResult run_cmd(const std::vector<std::string>& argv,
                  int timeout_sec = 0,
                  const std::string& cwd = "");
CmdResult run_shell(const std::string& cmd, int timeout_sec = 0, const std::string& cwd = "");
std::optional<std::string> which(const std::string& name);
bool command_exists(const std::string& name);

// FS
PathStat path_stat(const std::string& path);
bool path_exists(const std::string& path);
bool is_dir(const std::string& path);
bool is_file(const std::string& path);
bool mkdir_p(const std::string& path);
std::string abs_path(const std::string& path);
std::string join_path(const std::string& a, const std::string& b);
std::string parent_dir(const std::string& path);
std::string basename(const std::string& path);
std::string extension(const std::string& path);
std::string read_file(const std::string& path);
bool write_file(const std::string& path, const std::string& data);
bool append_file(const std::string& path, const std::string& data);

// String helpers
std::string trim(std::string_view s);
std::vector<std::string> split_ws(std::string_view s);
std::vector<std::string> split(std::string_view s, char delim);
std::string join(const std::vector<std::string>& parts, std::string_view sep);
bool starts_with(std::string_view s, std::string_view p);
bool ends_with(std::string_view s, std::string_view p);
std::string to_lower(std::string s);
std::string escape_tsv(std::string_view s);
std::string now_iso8601();
std::string sha256_hex(std::string_view data);  // lightweight FNV-1a64 as hex if no openssl

// Parsing
bool parse_double(std::string_view s, double& out);
bool parse_int(std::string_view s, int& out);
bool parse_uint(std::string_view s, unsigned& out);

// Timing
class Timer {
public:
    Timer() : start_(std::chrono::steady_clock::now()) {}
    double ms() const
    {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

// Env
std::string getenv_or(const char* key, const std::string& fallback = "");

// Exit codes (agent-facing contract)
// 0 = all checks passed
// 1 = usage / bad args
// 2 = required external tool missing
// 3 = internal/IO error
// 4 = verification failed (hallucination / claim false)
constexpr int EXIT_OK = 0;
constexpr int EXIT_USAGE = 1;
constexpr int EXIT_TOOL = 2;
constexpr int EXIT_IO = 3;
constexpr int EXIT_FAIL = 4;

}  // namespace ghar
