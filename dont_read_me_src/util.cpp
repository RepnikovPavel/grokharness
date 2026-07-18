// SPDX-License-Identifier: 0BSD
#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ghar {
namespace {

std::string read_fd_all(int fd)
{
    std::string out;
    char buf[4096];
    for (;;) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (n == 0)
            break;
        out.append(buf, static_cast<size_t>(n));
    }
    return out;
}

}  // namespace

CmdResult run_cmd(const std::vector<std::string>& argv, int timeout_sec, const std::string& cwd)
{
    CmdResult r;
    if (argv.empty()) {
        r.exit_code = 127;
        r.stderr_s = "empty argv";
        return r;
    }

    int out_pipe[2];
    int err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        r.exit_code = 127;
        r.stderr_s = "pipe failed";
        return r;
    }

    Timer timer;
    const pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);
        r.exit_code = 127;
        r.stderr_s = "fork failed";
        return r;
    }

    if (pid == 0) {
        // Own process group so timeout can kill shell + descendants.
        setpgid(0, 0);
        if (!cwd.empty()) {
            if (chdir(cwd.c_str()) != 0)
                _exit(127);
        }
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);

        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto& a : argv)
            cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }

    // Parent also moves child into its group (race-safe with child setpgid).
    setpgid(pid, pid);

    close(out_pipe[1]);
    close(err_pipe[1]);

    // Non-blocking reads with optional timeout via waitpid loop
    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

    int status = 0;
    bool done = false;
    while (!done) {
        char buf[4096];
        for (;;) {
            const ssize_t n = ::read(out_pipe[0], buf, sizeof(buf));
            if (n > 0)
                r.stdout_s.append(buf, static_cast<size_t>(n));
            else
                break;
        }
        for (;;) {
            const ssize_t n = ::read(err_pipe[0], buf, sizeof(buf));
            if (n > 0)
                r.stderr_s.append(buf, static_cast<size_t>(n));
            else
                break;
        }

        const pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            done = true;
        } else if (w < 0) {
            done = true;
            status = -1;
        } else {
            if (timeout_sec > 0 && timer.ms() > static_cast<double>(timeout_sec) * 1000.0) {
                // Kill whole process group (sh -c children included).
                kill(-pid, SIGKILL);
                waitpid(pid, &status, 0);
                r.timed_out = true;
                done = true;
            } else {
                usleep(2000);
            }
        }
    }

    // Drain remaining
    r.stdout_s += read_fd_all(out_pipe[0]);
    r.stderr_s += read_fd_all(err_pipe[0]);
    close(out_pipe[0]);
    close(err_pipe[0]);

    r.wall_ms = timer.ms();
    if (r.timed_out) {
        r.exit_code = 124;
    } else if (WIFEXITED(status)) {
        r.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        r.exit_code = 128 + WTERMSIG(status);
    } else {
        r.exit_code = -1;
    }
    return r;
}

CmdResult run_shell(const std::string& cmd, int timeout_sec, const std::string& cwd)
{
    return run_cmd({"/bin/sh", "-c", cmd}, timeout_sec, cwd);
}

std::optional<std::string> which(const std::string& name)
{
    if (name.find('/') != std::string::npos) {
        if (access(name.c_str(), X_OK) == 0)
            return name;
        return std::nullopt;
    }
    const char* path = std::getenv("PATH");
    if (!path)
        return std::nullopt;
    std::string p = path;
    size_t start = 0;
    while (start <= p.size()) {
        size_t end = p.find(':', start);
        if (end == std::string::npos)
            end = p.size();
        std::string dir = p.substr(start, end - start);
        if (!dir.empty()) {
            std::string cand = dir + "/" + name;
            if (access(cand.c_str(), X_OK) == 0)
                return cand;
        }
        if (end == p.size())
            break;
        start = end + 1;
    }
    return std::nullopt;
}

bool command_exists(const std::string& name)
{
    return which(name).has_value();
}

PathStat path_stat(const std::string& path)
{
    PathStat s;
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0)
        return s;
    s.valid = true;
    s.is_file = S_ISREG(st.st_mode);
    s.is_dir = S_ISDIR(st.st_mode);
    s.size = static_cast<std::uint64_t>(st.st_size);
    return s;
}

bool path_exists(const std::string& path)
{
    return path_stat(path).valid;
}

bool is_dir(const std::string& path)
{
    return path_stat(path).is_dir;
}

bool is_file(const std::string& path)
{
    return path_stat(path).is_file;
}

bool mkdir_p(const std::string& path)
{
    if (path.empty() || path == ".")
        return true;
    if (is_dir(path))
        return true;
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        if (path[i] == '/' || i + 1 == path.size()) {
            if (cur.empty() || cur == "/")
                continue;
            // strip trailing slash for mkdir except root
            std::string d = cur;
            if (d.size() > 1 && d.back() == '/')
                d.pop_back();
            if (mkdir(d.c_str(), 0755) != 0 && errno != EEXIST)
                return false;
        }
    }
    return is_dir(path);
}

std::string abs_path(const std::string& path)
{
    char* r = realpath(path.c_str(), nullptr);
    if (!r)
        return path;
    std::string out(r);
    free(r);
    return out;
}

std::string join_path(const std::string& a, const std::string& b)
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

std::string parent_dir(const std::string& path)
{
    const auto pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return ".";
    if (pos == 0)
        return "/";
    return path.substr(0, pos);
}

std::string basename(const std::string& path)
{
    const auto pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return path;
    return path.substr(pos + 1);
}

std::string extension(const std::string& path)
{
    const std::string base = basename(path);
    const auto pos = base.find_last_of('.');
    if (pos == std::string::npos)
        return "";
    return base.substr(pos);
}

std::string read_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool write_file(const std::string& path, const std::string& data)
{
    const std::string dir = parent_dir(path);
    if (!mkdir_p(dir))
        return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

bool append_file(const std::string& path, const std::string& data)
{
    const std::string dir = parent_dir(path);
    if (!mkdir_p(dir))
        return false;
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out)
        return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

std::string trim(std::string_view s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
        ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
        --e;
    return std::string(s.substr(b, e - b));
}

std::vector<std::string> split_ws(std::string_view s)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
            ++i;
        if (i >= s.size())
            break;
        size_t j = i;
        while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j])))
            ++j;
        out.emplace_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

std::vector<std::string> split(std::string_view s, char delim)
{
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            out.emplace_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& parts, std::string_view sep)
{
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i)
            out.append(sep);
        out += parts[i];
    }
    return out;
}

bool starts_with(std::string_view s, std::string_view p)
{
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

bool ends_with(std::string_view s, std::string_view p)
{
    return s.size() >= p.size() && s.substr(s.size() - p.size()) == p;
}

std::string to_lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string escape_tsv(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\t')
            out += "\\t";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c == '\\')
            out += "\\\\";
        else
            out += c;
    }
    return out;
}

std::string now_iso8601()
{
    std::time_t t = std::time(nullptr);
    std::tm tm {};
    gmtime_r(&t, &tm);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

// FNV-1a 64-bit as hex — not cryptographic; used as content fingerprint for agents
std::string sha256_hex(std::string_view data)
{
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ull;
    }
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

bool parse_double(std::string_view s, double& out)
{
    if (s.empty())
        return false;
    char* end = nullptr;
    std::string tmp(s);
    out = std::strtod(tmp.c_str(), &end);
    return end && end != tmp.c_str() && *end == '\0' && std::isfinite(out);
}

bool parse_int(std::string_view s, int& out)
{
    if (s.empty())
        return false;
    char* end = nullptr;
    std::string tmp(s);
    errno = 0;
    long v = std::strtol(tmp.c_str(), &end, 10);
    if (!end || end == tmp.c_str() || *end != '\0')
        return false;
    if (errno == ERANGE || v > static_cast<long>(INT_MAX) || v < static_cast<long>(INT_MIN))
        return false;
    out = static_cast<int>(v);
    return true;
}

bool parse_uint(std::string_view s, unsigned& out)
{
    int v = 0;
    if (!parse_int(s, v) || v < 0)
        return false;
    out = static_cast<unsigned>(v);
    return true;
}

std::string getenv_or(const char* key, const std::string& fallback)
{
    const char* v = std::getenv(key);
    if (!v || !*v)
        return fallback;
    return v;
}

}  // namespace ghar
