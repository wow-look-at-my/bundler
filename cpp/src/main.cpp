// ts0 - Simple TypeScript framework with good defaults
// C++20 Cosmopolitan port

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

// ============================================================================
// JSON Parser (minimal, single-header implementation)
// ============================================================================

namespace json {

struct Value;
using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

struct Value {
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    Value() : data(nullptr) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool b) : data(b) {}
    Value(double d) : data(d) {}
    Value(int i) : data(static_cast<double>(i)) {}
    Value(const std::string& s) : data(s) {}
    Value(std::string&& s) : data(std::move(s)) {}
    Value(const char* s) : data(std::string(s)) {}
    Value(Array arr) : data(std::move(arr)) {}
    Value(Object obj) : data(std::move(obj)) {}

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const { return std::holds_alternative<bool>(data); }
    bool is_number() const { return std::holds_alternative<double>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_array() const { return std::holds_alternative<Array>(data); }
    bool is_object() const { return std::holds_alternative<Object>(data); }

    bool as_bool() const { return std::get<bool>(data); }
    double as_number() const { return std::get<double>(data); }
    const std::string& as_string() const { return std::get<std::string>(data); }
    const Array& as_array() const { return std::get<Array>(data); }
    const Object& as_object() const { return std::get<Object>(data); }
    Object& as_object() { return std::get<Object>(data); }

    const Value& operator[](const std::string& key) const {
        static Value null_val;
        if (!is_object()) return null_val;
        auto& obj = as_object();
        auto it = obj.find(key);
        return it != obj.end() ? it->second : null_val;
    }

    std::optional<std::string> get_string(const std::string& key) const {
        if (!is_object()) return std::nullopt;
        auto& obj = as_object();
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is_string()) {
            return it->second.as_string();
        }
        return std::nullopt;
    }

    std::optional<bool> get_bool(const std::string& key) const {
        if (!is_object()) return std::nullopt;
        auto& obj = as_object();
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is_bool()) {
            return it->second.as_bool();
        }
        return std::nullopt;
    }
};

class Parser {
    std::string_view src;
    size_t pos = 0;

    void skip_ws() {
        while (pos < src.size() && std::isspace(src[pos])) ++pos;
    }

    char peek() { return pos < src.size() ? src[pos] : '\0'; }
    char get() { return pos < src.size() ? src[pos++] : '\0'; }

    std::string parse_string() {
        std::string result;
        get(); // consume opening quote
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < src.size()) {
                ++pos;
                switch (src[pos]) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '\\': result += '\\'; break;
                    case '"': result += '"'; break;
                    default: result += src[pos]; break;
                }
            } else {
                result += src[pos];
            }
            ++pos;
        }
        if (pos < src.size()) ++pos; // consume closing quote
        return result;
    }

    Value parse_number() {
        size_t start = pos;
        if (src[pos] == '-') ++pos;
        while (pos < src.size() && std::isdigit(src[pos])) ++pos;
        if (pos < src.size() && src[pos] == '.') {
            ++pos;
            while (pos < src.size() && std::isdigit(src[pos])) ++pos;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            ++pos;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) ++pos;
            while (pos < src.size() && std::isdigit(src[pos])) ++pos;
        }
        return Value(std::stod(std::string(src.substr(start, pos - start))));
    }

    Value parse_value() {
        skip_ws();
        char c = peek();

        if (c == '"') return Value(parse_string());
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 't') { pos += 4; return Value(true); }
        if (c == 'f') { pos += 5; return Value(false); }
        if (c == 'n') { pos += 4; return Value(nullptr); }
        if (c == '-' || std::isdigit(c)) return parse_number();

        return Value(nullptr);
    }

    Value parse_object() {
        Object obj;
        get(); // consume '{'
        skip_ws();

        while (peek() != '}' && pos < src.size()) {
            skip_ws();
            if (peek() != '"') break;
            std::string key = parse_string();
            skip_ws();
            if (peek() == ':') get();
            skip_ws();
            obj[key] = parse_value();
            skip_ws();
            if (peek() == ',') get();
        }

        if (peek() == '}') get();
        return Value(std::move(obj));
    }

    Value parse_array() {
        Array arr;
        get(); // consume '['
        skip_ws();

        while (peek() != ']' && pos < src.size()) {
            arr.push_back(parse_value());
            skip_ws();
            if (peek() == ',') get();
            skip_ws();
        }

        if (peek() == ']') get();
        return Value(std::move(arr));
    }

public:
    Value parse(std::string_view json) {
        src = json;
        pos = 0;
        return parse_value();
    }
};

std::string stringify(const Value& val, int indent = 0);

std::string stringify_object(const Object& obj, int indent) {
    if (obj.empty()) return "{}";
    std::string result = "{\n";
    std::string pad(indent + 2, ' ');
    bool first = true;
    for (const auto& [k, v] : obj) {
        if (!first) result += ",\n";
        first = false;
        result += pad + "\"" + k + "\": " + stringify(v, indent + 2);
    }
    result += "\n" + std::string(indent, ' ') + "}";
    return result;
}

std::string stringify(const Value& val, int indent) {
    if (val.is_null()) return "null";
    if (val.is_bool()) return val.as_bool() ? "true" : "false";
    if (val.is_number()) {
        double d = val.as_number();
        if (d == static_cast<int>(d)) {
            return std::to_string(static_cast<int>(d));
        }
        return std::to_string(d);
    }
    if (val.is_string()) {
        std::string result = "\"";
        for (char c : val.as_string()) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\t': result += "\\t"; break;
                case '\r': result += "\\r"; break;
                default: result += c; break;
            }
        }
        return result + "\"";
    }
    if (val.is_array()) {
        if (val.as_array().empty()) return "[]";
        std::string result = "[\n";
        std::string pad(indent + 2, ' ');
        bool first = true;
        for (const auto& v : val.as_array()) {
            if (!first) result += ",\n";
            first = false;
            result += pad + stringify(v, indent + 2);
        }
        result += "\n" + std::string(indent, ' ') + "]";
        return result;
    }
    if (val.is_object()) {
        return stringify_object(val.as_object(), indent);
    }
    return "null";
}

Value parse(const std::string& json) {
    Parser p;
    return p.parse(json);
}

} // namespace json

// ============================================================================
// Glob Pattern Matching
// ============================================================================

namespace glob {

bool match(std::string_view pattern, std::string_view str) {
    size_t pi = 0, si = 0;
    size_t star_pi = std::string_view::npos;
    size_t star_si = 0;

    while (si < str.size()) {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == str[si])) {
            ++pi;
            ++si;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            star_pi = pi++;
            star_si = si;
        } else if (star_pi != std::string_view::npos) {
            pi = star_pi + 1;
            si = ++star_si;
        } else {
            return false;
        }
    }

    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

std::vector<fs::path> glob_files(const fs::path& dir, const std::string& pattern) {
    std::vector<fs::path> results;

    // Handle ** for recursive matching
    bool recursive = pattern.find("**") != std::string::npos;

    // Extract the file pattern (last component)
    std::string file_pattern = pattern;
    if (auto pos = pattern.rfind('/'); pos != std::string::npos) {
        file_pattern = pattern.substr(pos + 1);
    }

    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(dir,
                    fs::directory_options::skip_permission_denied)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (match(file_pattern, filename)) {
                        results.push_back(entry.path());
                    }
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (match(file_pattern, filename)) {
                        results.push_back(entry.path());
                    }
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // Ignore permission errors
    }

    return results;
}

} // namespace glob

// ============================================================================
// Configuration System
// ============================================================================

struct Ts0Config {
    std::optional<std::string> entry;
    std::optional<std::string> outfile;
    std::string outdir = "dist";
    std::string target = "node";
    std::string format = "esm";
    bool strict = true;
    bool minify = false;
    bool sourcemap = true;
    std::string test_pattern = "**/*.test.ts";
    json::Object esbuild;
};

std::optional<fs::path> find_config_file(const fs::path& start_dir) {
    fs::path current = fs::absolute(start_dir);

    while (true) {
        fs::path config_path = current / "ts0.json";
        if (fs::exists(config_path)) {
            return config_path;
        }

        if (!current.has_parent_path() || current == current.parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return std::nullopt;
}

std::optional<std::string> auto_detect_entry(const fs::path& base_dir) {
    std::vector<std::string> candidates = {
        "src/main.ts", "src/index.ts", "main.ts", "index.ts"
    };

    for (const auto& candidate : candidates) {
        if (fs::exists(base_dir / candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

Ts0Config load_config(const fs::path& dir) {
    Ts0Config config;

    auto config_path = find_config_file(dir);
    if (config_path) {
        std::ifstream file(*config_path);
        std::stringstream buffer;
        buffer << file.rdbuf();

        json::Value root = json::parse(buffer.str());

        if (auto v = root.get_string("entry")) config.entry = *v;
        if (auto v = root.get_string("outfile")) config.outfile = *v;
        if (auto v = root.get_string("outdir")) config.outdir = *v;
        if (auto v = root.get_string("target")) config.target = *v;
        if (auto v = root.get_string("format")) config.format = *v;
        if (auto v = root.get_bool("strict")) config.strict = *v;
        if (auto v = root.get_bool("minify")) config.minify = *v;
        if (auto v = root.get_bool("sourcemap")) config.sourcemap = *v;

        if (root["test"].is_object()) {
            if (auto v = root["test"].get_string("pattern")) {
                config.test_pattern = *v;
            }
        }

        if (root["esbuild"].is_object()) {
            config.esbuild = root["esbuild"].as_object();
        }
    }

    // Auto-detect entry if not specified
    if (!config.entry) {
        fs::path base = config_path ? config_path->parent_path() : dir;
        config.entry = auto_detect_entry(base);
    }

    return config;
}

// ============================================================================
// Process Execution
// ============================================================================

struct ProcessResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
};

ProcessResult run_command(const std::string& cmd) {
    ProcessResult result;
    result.exit_code = 0;

    // Create temp files for output capture
    std::string stdout_file = "/tmp/ts0_stdout_" + std::to_string(getpid());
    std::string stderr_file = "/tmp/ts0_stderr_" + std::to_string(getpid());

    std::string full_cmd = cmd + " >" + stdout_file + " 2>" + stderr_file;
    result.exit_code = std::system(full_cmd.c_str());
    result.exit_code = WEXITSTATUS(result.exit_code);

    // Read outputs
    if (std::ifstream f(stdout_file); f) {
        std::stringstream ss;
        ss << f.rdbuf();
        result.stdout_output = ss.str();
    }
    if (std::ifstream f(stderr_file); f) {
        std::stringstream ss;
        ss << f.rdbuf();
        result.stderr_output = ss.str();
    }

    // Cleanup
    std::remove(stdout_file.c_str());
    std::remove(stderr_file.c_str());

    return result;
}

int run_interactive(const std::string& cmd) {
    return WEXITSTATUS(std::system(cmd.c_str()));
}

// ============================================================================
// Build Command
// ============================================================================

struct BuildResult {
    bool success;
    std::vector<std::string> output_files;
    std::vector<std::string> errors;
    long duration_ms;
};

struct TypeCheckResult {
    bool success;
    std::vector<std::string> errors;
};

TypeCheckResult typecheck(const Ts0Config& config, const fs::path& base_dir) {
    TypeCheckResult result;
    result.success = true;

    // Generate temporary tsconfig
    json::Object compiler_options;
    compiler_options["target"] = "ESNext";
    compiler_options["module"] = "ESNext";
    compiler_options["moduleResolution"] = "bundler";
    compiler_options["esModuleInterop"] = true;
    compiler_options["strict"] = config.strict;
    compiler_options["skipLibCheck"] = true;
    compiler_options["noEmit"] = true;
    compiler_options["resolveJsonModule"] = true;

    json::Array include;
    include.push_back("src/**/*.ts");
    if (config.entry && config.entry->find("src/") != 0) {
        include.push_back(*config.entry);
    }

    json::Object tsconfig;
    tsconfig["compilerOptions"] = json::Value(std::move(compiler_options));
    tsconfig["include"] = json::Value(std::move(include));

    fs::path tsconfig_path = base_dir / ".ts0-tsconfig.json";
    {
        std::ofstream f(tsconfig_path);
        f << json::stringify(json::Value(tsconfig));
    }

    // Run tsc
    std::string cmd = "npx tsc --project " + tsconfig_path.string();
    auto proc_result = run_command(cmd);

    // Cleanup
    fs::remove(tsconfig_path);

    if (proc_result.exit_code != 0) {
        result.success = false;
        if (!proc_result.stdout_output.empty()) {
            result.errors.push_back(proc_result.stdout_output);
        }
        if (!proc_result.stderr_output.empty()) {
            result.errors.push_back(proc_result.stderr_output);
        }
    }

    return result;
}

BuildResult build(const Ts0Config& config, const fs::path& base_dir, bool watch = false) {
    BuildResult result;
    result.success = true;

    auto start = std::chrono::steady_clock::now();

    if (!config.entry) {
        result.success = false;
        result.errors.push_back("No entry point found. Create src/main.ts or specify 'entry' in ts0.json");
        return result;
    }

    fs::path entry_path = base_dir / *config.entry;
    if (!fs::exists(entry_path)) {
        result.success = false;
        result.errors.push_back("Entry file not found: " + entry_path.string());
        return result;
    }

    // Determine output path
    std::string outpath;
    if (config.outfile) {
        outpath = (base_dir / *config.outfile).string();
    } else {
        fs::path outdir = base_dir / config.outdir;
        fs::create_directories(outdir);
        std::string basename = entry_path.stem().string();
        outpath = (outdir / (basename + ".js")).string();
    }

    // Build esbuild command
    std::ostringstream cmd;
    cmd << "npx esbuild " << entry_path.string();
    cmd << " --bundle";
    cmd << " --platform=" << config.target;
    cmd << " --format=" << config.format;
    cmd << " --target=esnext";

    if (config.outfile) {
        cmd << " --outfile=" << outpath;
    } else {
        cmd << " --outdir=" << (base_dir / config.outdir).string();
    }

    if (config.minify) cmd << " --minify";
    if (config.sourcemap) cmd << " --sourcemap";

    // For node target, don't bundle node_modules
    if (config.target == "node") {
        cmd << " --packages=external";
    }

    if (watch) {
        cmd << " --watch";
    }

    auto proc_result = run_command(cmd.str());

    if (proc_result.exit_code != 0) {
        result.success = false;
        if (!proc_result.stdout_output.empty()) {
            result.errors.push_back(proc_result.stdout_output);
        }
        if (!proc_result.stderr_output.empty()) {
            result.errors.push_back(proc_result.stderr_output);
        }
    } else {
        // Add shebang for CLI applications
        if (config.outfile && config.target == "node") {
            std::string content;
            {
                std::ifstream f(outpath);
                std::stringstream ss;
                ss << f.rdbuf();
                content = ss.str();
            }

            if (content.find("#!") != 0) {
                std::ofstream f(outpath);
                f << "#!/usr/bin/env node\n" << content;
            }

            // Make executable
            fs::permissions(outpath,
                fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                fs::perm_options::add);
        }

        result.output_files.push_back(outpath);
    }

    auto end = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return result;
}

// ============================================================================
// Init Command
// ============================================================================

int cmd_init(bool force) {
    fs::path cwd = fs::current_path();
    fs::path config_path = cwd / "ts0.json";
    fs::path src_dir = cwd / "src";
    fs::path main_path = src_dir / "main.ts";
    fs::path test_path = src_dir / "main.test.ts";

    // Check if files exist
    if (!force) {
        if (fs::exists(config_path)) {
            std::cerr << "ts0.json already exists. Use --force to overwrite.\n";
            return 1;
        }
    }

    // Create ts0.json
    json::Object test_obj;
    test_obj["pattern"] = "**/*.test.ts";

    json::Object config;
    config["entry"] = "src/main.ts";
    config["outdir"] = "dist";
    config["target"] = "node";
    config["format"] = "esm";
    config["strict"] = true;
    config["test"] = json::Value(std::move(test_obj));

    {
        std::ofstream f(config_path);
        f << json::stringify(json::Value(config));
    }
    std::cout << "Created ts0.json\n";

    // Create src directory
    fs::create_directories(src_dir);

    // Create main.ts
    if (!fs::exists(main_path) || force) {
        std::ofstream f(main_path);
        f << R"(// Main entry point
console.log("Hello from ts0!");

export function greet(name: string): string {
    return `Hello, ${name}!`;
}

// Run if executed directly
if (import.meta.url === `file://${process.argv[1]}`) {
    console.log(greet("World"));
}
)";
        std::cout << "Created src/main.ts\n";
    }

    // Create test file
    if (!fs::exists(test_path) || force) {
        std::ofstream f(test_path);
        f << R"(import { test } from "node:test";
import { strict as assert } from "node:assert";
import { greet } from "./main.ts";

test("greet returns greeting message", () => {
    assert.equal(greet("World"), "Hello, World!");
});

test("greet handles different names", () => {
    assert.equal(greet("TypeScript"), "Hello, TypeScript!");
});
)";
        std::cout << "Created src/main.test.ts\n";
    }

    std::cout << "\nProject initialized! Next steps:\n";
    std::cout << "  ts0 build    Build the project\n";
    std::cout << "  ts0 run      Run the application\n";
    std::cout << "  ts0 test     Run tests\n";

    return 0;
}

// ============================================================================
// Build Command Handler
// ============================================================================

int cmd_build(bool watch) {
    fs::path cwd = fs::current_path();
    Ts0Config config = load_config(cwd);

    std::cout << "Type checking...\n";
    auto tc_result = typecheck(config, cwd);

    if (!tc_result.success) {
        std::cerr << "Type errors found:\n";
        for (const auto& err : tc_result.errors) {
            std::cerr << err << "\n";
        }
        return 1;
    }
    std::cout << "Type check passed\n";

    std::cout << "Building...\n";
    auto build_result = build(config, cwd, watch);

    if (!build_result.success) {
        std::cerr << "Build failed:\n";
        for (const auto& err : build_result.errors) {
            std::cerr << err << "\n";
        }
        return 1;
    }

    std::cout << "Build complete in " << build_result.duration_ms << "ms\n";
    for (const auto& file : build_result.output_files) {
        std::cout << "  " << file << "\n";
    }

    return 0;
}

// ============================================================================
// Run Command Handler
// ============================================================================

int cmd_run(bool no_build, const std::string& file, const std::vector<std::string>& args) {
    fs::path cwd = fs::current_path();
    Ts0Config config = load_config(cwd);

    std::string target_file;

    if (!file.empty()) {
        target_file = file;
    } else if (no_build) {
        // Use entry file directly with --experimental-strip-types
        if (!config.entry) {
            std::cerr << "No entry point found\n";
            return 1;
        }
        target_file = *config.entry;
    } else {
        // Build first, then run output
        std::cout << "Building...\n";
        auto tc_result = typecheck(config, cwd);
        if (!tc_result.success) {
            std::cerr << "Type errors found:\n";
            for (const auto& err : tc_result.errors) {
                std::cerr << err << "\n";
            }
            return 1;
        }

        auto build_result = build(config, cwd);
        if (!build_result.success) {
            std::cerr << "Build failed:\n";
            for (const auto& err : build_result.errors) {
                std::cerr << err << "\n";
            }
            return 1;
        }

        if (build_result.output_files.empty()) {
            std::cerr << "No output files generated\n";
            return 1;
        }

        target_file = build_result.output_files[0];
    }

    // Build command
    std::ostringstream cmd;

    if (target_file.ends_with(".ts")) {
        cmd << "node --experimental-strip-types " << target_file;
    } else {
        cmd << "node " << target_file;
    }

    for (const auto& arg : args) {
        cmd << " " << arg;
    }

    return run_interactive(cmd.str());
}

// ============================================================================
// Test Command Handler
// ============================================================================

int cmd_test(const std::string& pattern, bool watch) {
    fs::path cwd = fs::current_path();
    Ts0Config config = load_config(cwd);

    std::string test_pattern = pattern.empty() ? config.test_pattern : pattern;

    // Find test files
    auto test_files = glob::glob_files(cwd, test_pattern);

    if (test_files.empty()) {
        std::cerr << "No test files found matching: " << test_pattern << "\n";
        return 1;
    }

    std::cout << "Found " << test_files.size() << " test file(s)\n";

    // Build command for node test runner
    std::ostringstream cmd;
    cmd << "node --experimental-strip-types --test";

    if (watch) {
        cmd << " --watch";
    }

    for (const auto& file : test_files) {
        cmd << " " << file.string();
    }

    return run_interactive(cmd.str());
}

// ============================================================================
// CLI
// ============================================================================

void print_help() {
    std::cout << R"(ts0 - Simple TypeScript framework with good defaults

Usage: ts0 <command> [options]

Commands:
  init          Initialize a new ts0 project
  build         Build the project
  run [file]    Run the application
  test          Run tests

Options:
  -h, --help    Show this help message
  -w, --watch   Watch mode (build, test)
  --no-build    Skip build step (run)
  --force       Overwrite existing files (init)

Examples:
  ts0 init              Create a new project
  ts0 build             Build the project
  ts0 build --watch     Build and watch for changes
  ts0 run               Build and run
  ts0 run --no-build    Run without building (uses --experimental-strip-types)
  ts0 test              Run all tests
  ts0 test --watch      Run tests in watch mode
)";
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) {
        print_help();
        return 0;
    }

    // Parse flags
    bool watch = false;
    bool no_build = false;
    bool force = false;
    bool help = false;
    std::string command;
    std::vector<std::string> positional;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == "-h" || arg == "--help") {
            help = true;
        } else if (arg == "-w" || arg == "--watch") {
            watch = true;
        } else if (arg == "--no-build") {
            no_build = true;
        } else if (arg == "--force") {
            force = true;
        } else if (arg[0] != '-') {
            if (command.empty()) {
                command = arg;
            } else {
                positional.push_back(arg);
            }
        }
    }

    if (help || command == "help") {
        print_help();
        return 0;
    }

    // Dispatch commands
    if (command == "init") {
        return cmd_init(force);
    } else if (command == "build") {
        return cmd_build(watch);
    } else if (command == "run") {
        std::string file = positional.empty() ? "" : positional[0];
        std::vector<std::string> run_args(positional.begin() + (file.empty() ? 0 : 1), positional.end());
        return cmd_run(no_build, file, run_args);
    } else if (command == "test") {
        std::string pattern = positional.empty() ? "" : positional[0];
        return cmd_test(pattern, watch);
    } else {
        std::cerr << "Unknown command: " << command << "\n";
        std::cerr << "Run 'ts0 --help' for usage\n";
        return 1;
    }
}
