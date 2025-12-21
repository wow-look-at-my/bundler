#include "process.hpp"
#include <cstdlib>
#include <cstring>
#include <array>
#include <memory>
#include <sstream>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

namespace fs = std::filesystem;

namespace ts0 {

namespace {

// Resolve command, checking node_modules/.bin first
std::string resolve_command(const std::string& command, const std::optional<fs::path>& working_dir) {
    // If it's already an absolute path, use it directly
    if (command.starts_with("/")) {
        return command;
    }

    // Check node_modules/.bin in the working directory
    fs::path base = working_dir.value_or(fs::current_path());
    fs::path node_bin = base / "node_modules" / ".bin" / command;
    if (fs::exists(node_bin) && access(node_bin.c_str(), X_OK) == 0) {
        return node_bin.string();
    }

    // Walk up directory tree looking for node_modules/.bin
    fs::path current = base;
    while (current.has_parent_path() && current != current.parent_path()) {
        node_bin = current / "node_modules" / ".bin" / command;
        if (fs::exists(node_bin) && access(node_bin.c_str(), X_OK) == 0) {
            return node_bin.string();
        }
        current = current.parent_path();
    }

    // Fall back to PATH lookup (execvp will handle this)
    return command;
}

} // anonymous namespace

ProcessResult Process::run(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::optional<fs::path>& working_dir,
    bool capture_output
) {
    ProcessResult result{0, "", ""};

    // Resolve command (check node_modules/.bin first)
    std::string resolved_cmd = resolve_command(command, working_dir);

    // Build the command line
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(resolved_cmd.c_str()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    if (capture_output) {
        if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
            result.exit_code = -1;
            result.stderr_output = "Failed to create pipes";
            return result;
        }
    }

    pid_t pid = fork();

    if (pid == -1) {
        result.exit_code = -1;
        result.stderr_output = "Failed to fork process";
        if (capture_output) {
            close(stdout_pipe[0]); close(stdout_pipe[1]);
            close(stderr_pipe[0]); close(stderr_pipe[1]);
        }
        return result;
    }

    if (pid == 0) {
        // Child process
        if (working_dir.has_value()) {
            if (chdir(working_dir->c_str()) != 0) {
                _exit(127);
            }
        }

        if (capture_output) {
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stdout_pipe[1]);
            close(stderr_pipe[1]);
        }

        execvp(resolved_cmd.c_str(), argv.data());
        _exit(127);
    }

    // Parent process
    if (capture_output) {
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Read output
        std::array<char, 4096> buffer;
        ssize_t bytes_read;

        while ((bytes_read = read(stdout_pipe[0], buffer.data(), buffer.size())) > 0) {
            result.stdout_output.append(buffer.data(), bytes_read);
        }
        close(stdout_pipe[0]);

        while ((bytes_read = read(stderr_pipe[0], buffer.data(), buffer.size())) > 0) {
            result.stderr_output.append(buffer.data(), bytes_read);
        }
        close(stderr_pipe[0]);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    } else {
        result.exit_code = -1;
    }

    return result;
}

int Process::spawn(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::optional<fs::path>& working_dir
) {
    // Resolve command (check node_modules/.bin first)
    std::string resolved_cmd = resolve_command(command, working_dir);

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(resolved_cmd.c_str()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();

    if (pid == -1) {
        return -1;
    }

    if (pid == 0) {
        // Child process
        if (working_dir.has_value()) {
            if (chdir(working_dir->c_str()) != 0) {
                _exit(127);
            }
        }

        execvp(resolved_cmd.c_str(), argv.data());
        _exit(127);
    }

    // Parent process - wait for child
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return -1;
}

std::optional<std::string> Process::which(const std::string& command) {
    // Check if it's an absolute path
    if (fs::exists(command)) {
        return command;
    }

    // Search in PATH
    const char* path_env = std::getenv("PATH");
    if (!path_env) {
        return std::nullopt;
    }

    std::string path_str(path_env);
    std::istringstream path_stream(path_str);
    std::string dir;

    while (std::getline(path_stream, dir, ':')) {
        fs::path candidate = fs::path(dir) / command;
        if (fs::exists(candidate) && access(candidate.c_str(), X_OK) == 0) {
            return candidate.string();
        }
    }

    return std::nullopt;
}

} // namespace ts0
