#include "skiffllm/skills.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "skiffllm/tools.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <fcntl.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace skiffllm {
namespace {

constexpr size_t kMaxReadBytes = 256 * 1024;
constexpr size_t kMaxFetchBytes = 1024 * 1024;
constexpr size_t kMaxListEntries = 512;
constexpr size_t kMaxSearchHits = 256;
constexpr size_t kMaxCommandBytes = 64 * 1024;

std::string trimmed(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' ||
                                    value[start] == '\n' || value[start] == '\r')) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                           value[end - 1] == '\n' || value[end - 1] == '\r')) {
        --end;
    }
    return value.substr(start, end - start);
}

bool begins_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;
};

const JsonValue* json_find(const JsonValue& value, const std::string& key) {
    if (value.type != JsonValue::Type::Object) {
        return nullptr;
    }
    for (const auto& entry : value.object) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

class JsonParser {
   public:
    explicit JsonParser(const std::string& input) : text_(input), index_(0) {}

    bool parse(JsonValue& output, std::string& error) {
        skip_space();
        if (!parse_value(output)) {
            error = error_;
            return false;
        }
        skip_space();
        if (index_ != text_.size()) {
            error = "trailing data";
            return false;
        }
        return true;
    }

   private:
    bool parse_value(JsonValue& output) {
        skip_space();
        if (index_ >= text_.size()) {
            error_ = "unexpected end";
            return false;
        }
        const char ch = text_[index_];
        if (ch == '"') {
            output.type = JsonValue::Type::String;
            return parse_string(output.string);
        }
        if (ch == '{') {
            output.type = JsonValue::Type::Object;
            return parse_object(output.object);
        }
        if (ch == '[') {
            output.type = JsonValue::Type::Array;
            return parse_array(output.array);
        }
        if (ch == 't') {
            if (text_.compare(index_, 4, "true") != 0) {
                error_ = "invalid literal";
                return false;
            }
            index_ += 4;
            output.type = JsonValue::Type::Bool;
            output.boolean = true;
            return true;
        }
        if (ch == 'f') {
            if (text_.compare(index_, 5, "false") != 0) {
                error_ = "invalid literal";
                return false;
            }
            index_ += 5;
            output.type = JsonValue::Type::Bool;
            output.boolean = false;
            return true;
        }
        if (ch == 'n') {
            if (text_.compare(index_, 4, "null") != 0) {
                error_ = "invalid literal";
                return false;
            }
            index_ += 4;
            output.type = JsonValue::Type::Null;
            return true;
        }
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            output.type = JsonValue::Type::Number;
            return parse_number(output.number);
        }
        error_ = "unexpected character";
        return false;
    }

    bool parse_string(std::string& output) {
        ++index_;
        output.clear();
        while (index_ < text_.size()) {
            const char ch = text_[index_++];
            if (ch == '"') {
                return true;
            }
            if (ch != '\\') {
                output.push_back(ch);
                continue;
            }
            if (index_ >= text_.size()) {
                error_ = "bad escape";
                return false;
            }
            const char escape = text_[index_++];
            switch (escape) {
                case '"':
                    output.push_back('"');
                    break;
                case '\\':
                    output.push_back('\\');
                    break;
                case '/':
                    output.push_back('/');
                    break;
                case 'b':
                    output.push_back('\b');
                    break;
                case 'f':
                    output.push_back('\f');
                    break;
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                default:
                    error_ = "bad escape";
                    return false;
            }
        }
        error_ = "unterminated string";
        return false;
    }

    bool parse_number(double& output) {
        const size_t start = index_;
        if (text_[index_] == '-') {
            ++index_;
        }
        if (index_ < text_.size() && text_[index_] == '0') {
            ++index_;
        } else {
            while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
                ++index_;
            }
        }
        if (index_ < text_.size() && text_[index_] == '.') {
            ++index_;
            while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
                ++index_;
            }
        }
        if (index_ < text_.size() && (text_[index_] == 'e' || text_[index_] == 'E')) {
            ++index_;
            if (index_ < text_.size() && (text_[index_] == '+' || text_[index_] == '-')) {
                ++index_;
            }
            while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
                ++index_;
            }
        }
        if (index_ == start) {
            error_ = "bad number";
            return false;
        }
        output = std::strtod(text_.substr(start, index_ - start).c_str(), nullptr);
        return true;
    }

    bool parse_object(std::vector<std::pair<std::string, JsonValue>>& output) {
        ++index_;
        skip_space();
        if (index_ < text_.size() && text_[index_] == '}') {
            ++index_;
            return true;
        }
        while (index_ < text_.size()) {
            skip_space();
            if (index_ >= text_.size() || text_[index_] != '"') {
                error_ = "expected key";
                return false;
            }
            std::string key;
            if (!parse_string(key)) {
                return false;
            }
            skip_space();
            if (index_ >= text_.size() || text_[index_] != ':') {
                error_ = "expected colon";
                return false;
            }
            ++index_;
            JsonValue value;
            if (!parse_value(value)) {
                return false;
            }
            output.emplace_back(std::move(key), std::move(value));
            skip_space();
            if (index_ < text_.size() && text_[index_] == ',') {
                ++index_;
                continue;
            }
            if (index_ < text_.size() && text_[index_] == '}') {
                ++index_;
                return true;
            }
            error_ = "expected comma";
            return false;
        }
        error_ = "unterminated object";
        return false;
    }

    bool parse_array(std::vector<JsonValue>& output) {
        ++index_;
        skip_space();
        if (index_ < text_.size() && text_[index_] == ']') {
            ++index_;
            return true;
        }
        while (index_ < text_.size()) {
            JsonValue value;
            if (!parse_value(value)) {
                return false;
            }
            output.push_back(std::move(value));
            skip_space();
            if (index_ < text_.size() && text_[index_] == ',') {
                ++index_;
                continue;
            }
            if (index_ < text_.size() && text_[index_] == ']') {
                ++index_;
                return true;
            }
            error_ = "expected comma";
            return false;
        }
        error_ = "unterminated array";
        return false;
    }

    void skip_space() {
        while (index_ < text_.size()) {
            const char ch = text_[index_];
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                break;
            }
            ++index_;
        }
    }

    const std::string& text_;
    size_t index_;
    std::string error_;
};

bool is_hidden(const std::filesystem::path& path) {
    std::string name = path.filename().string();
    if (name == ".git" || name == ".hg" || name == ".svn" || name == "build" ||
        name == "node_modules" || name == "vendor" || name == "_deps" || name == ".gradle" ||
        name == "dist" || name == "out" || name == "target" || name == ".venv" || name == "venv" ||
        name == "__pycache__" || name == ".next" || name == ".cache" || name == ".pytest_cache" ||
        name == ".idea" || name == ".vscode") {
        return true;
    }
    return name.size() > 1 && name[0] == '.';
}

std::string read_file_text(const std::filesystem::path& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        error = "cannot open file";
        return {};
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string value = buffer.str();
    if (value.size() > kMaxReadBytes) {
        value.resize(kMaxReadBytes);
        value += "\n[truncated]";
    }
    return value;
}

bool write_file_text(const std::filesystem::path& path, const std::string& content,
                     std::string& error) {
    std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = "cannot create directory: " + ec.message();
            return false;
        }
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        error = "cannot write file";
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

std::string list_directory(const std::filesystem::path& path, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
        error = "directory does not exist";
        return {};
    }
    std::ostringstream out;
    size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (ec) {
            break;
        }
        if (count >= kMaxListEntries) {
            out << "[truncated]\n";
            break;
        }
        ++count;
        if (entry.is_directory()) {
            out << "[dir] ";
        } else {
            out << "[file] ";
        }
        out << entry.path().filename().string() << "\n";
    }
    return out.str();
}

std::string search_files(const std::filesystem::path& root, const std::string& query,
                         std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        error = "directory does not exist";
        return {};
    }
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::ostringstream out;
    size_t hits = 0;
    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        if (ec) {
            break;
        }
        const auto& entry = *it;
        if (entry.is_directory()) {
            if (is_hidden(entry.path())) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (is_hidden(entry.path())) {
            continue;
        }
        std::string name = entry.path().string();
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lower.find(lower_query) == std::string::npos) {
            continue;
        }
        out << name << "\n";
        ++hits;
        if (hits >= kMaxSearchHits) {
            out << "[truncated]\n";
            break;
        }
    }
    return out.str();
}

std::string run_command(const std::string& command, std::string& error) {
    if (command.size() > 4096) {
        error = "command too long";
        return {};
    }
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "rb");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        error = "cannot start command";
        return {};
    }
    std::ostringstream out;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        if (out.tellp() >= static_cast<std::streampos>(kMaxCommandBytes)) {
            out << "\n[truncated]";
            break;
        }
        out << buffer;
    }
#ifdef _WIN32
    const int status = _pclose(pipe);
#else
    const int status = pclose(pipe);
#endif
    if (status != 0 && out.str().empty()) {
        error = "command failed with status " + std::to_string(status);
        return {};
    }
    return out.str();
}

std::string current_time() {
    std::time_t now = std::time(nullptr);
    char buffer[64] = {};
    struct tm local {};
#ifdef _WIN32
    localtime_s(&local, &now);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %Z", &local);
#else
    localtime_r(&now, &local);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %Z", &local);
#endif
    return buffer;
}

#ifdef _WIN32
int close_socket(SOCKET socket_fd) {
    return closesocket(socket_fd);
}
using Socket = SOCKET;
const Socket invalid_socket_value = INVALID_SOCKET;
#else
int close_socket(int socket_fd) {
    return close(socket_fd);
}
using Socket = int;
const Socket invalid_socket_value = -1;
#endif

std::string socket_error_text() {
#ifdef _WIN32
    return "socket error " + std::to_string(WSAGetLastError());
#else
    return std::string(std::strerror(errno));
#endif
}

std::string fetch_http(const std::string& url, std::string& error) {
    if (!begins_with(url, "http://")) {
        error = "only http:// URLs are supported";
        return {};
    }
    std::string rest = url.substr(7);
    const size_t slash = rest.find('/');
    std::string host = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
    if (host.empty()) {
        error = "invalid URL";
        return {};
    }
    std::string port_text = "80";
    const size_t colon = host.rfind(':');
    if (colon != std::string::npos) {
        port_text = host.substr(colon + 1);
        host = host.substr(0, colon);
    }
    if (host.empty()) {
        error = "invalid URL";
        return {};
    }
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        error = "WSAStartup failed";
        return {};
    }
#endif
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* addresses = nullptr;
    const int lookup = getaddrinfo(host.c_str(), port_text.c_str(), &hints, &addresses);
    if (lookup != 0) {
        error = "address lookup failed";
#ifdef _WIN32
        WSACleanup();
#endif
        return {};
    }
    Socket socket_fd = invalid_socket_value;
    for (struct addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        socket_fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket_fd == invalid_socket_value) {
            continue;
        }
#ifdef _WIN32
        if (connect(socket_fd, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0) {
            break;
        }
#else
        if (connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
#endif
        close_socket(socket_fd);
        socket_fd = invalid_socket_value;
    }
    freeaddrinfo(addresses);
    if (socket_fd == invalid_socket_value) {
        error = "cannot connect: " + socket_error_text();
#ifdef _WIN32
        WSACleanup();
#endif
        return {};
    }
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << ":" << port_text << "\r\n";
    request << "User-Agent: SkiffLLM/1.9\r\n";
    request << "Accept: */*\r\n";
    request << "Connection: close\r\n\r\n";
    const std::string request_text = request.str();
    size_t sent = 0;
    bool send_ok = true;
    while (sent < request_text.size()) {
#ifdef _WIN32
        const int count = send(socket_fd, request_text.data() + sent,
                               static_cast<int>(request_text.size() - sent), 0);
#else
        const ssize_t count =
            send(socket_fd, request_text.data() + sent, request_text.size() - sent, 0);
#endif
        if (count <= 0) {
            send_ok = false;
            break;
        }
        sent += static_cast<size_t>(count);
    }
    if (!send_ok) {
        error = "cannot send request: " + socket_error_text();
        close_socket(socket_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return {};
    }
    std::string raw;
    char buffer[8192];
    while (true) {
#ifdef _WIN32
        const int count = recv(socket_fd, buffer, sizeof(buffer), 0);
#else
        const ssize_t count = recv(socket_fd, buffer, sizeof(buffer), 0);
#endif
        if (count <= 0) {
            break;
        }
        raw.append(buffer, static_cast<size_t>(count));
        if (raw.size() > kMaxFetchBytes) {
            break;
        }
    }
    close_socket(socket_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    const size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        error = "invalid HTTP response";
        return {};
    }
    std::string headers = raw.substr(0, header_end);
    std::string body = raw.substr(header_end + 4);
    std::string status_line = headers.substr(0, headers.find("\r\n"));
    if (status_line.find(" 200 ") == std::string::npos) {
        error = "HTTP request failed: " + status_line;
        return {};
    }
    if (body.size() > kMaxFetchBytes) {
        body.resize(kMaxFetchBytes);
        body += "\n[truncated]";
    }
    return body;
}

std::string memory_load(const Config& cfg) {
    std::string memory = load_memories(cfg);
    return memory.empty() ? "No saved memories." : memory;
}

}

bool parse_skill_requests(const std::string& text, std::vector<SkillRequest>& requests,
                          std::string& error) {
    requests.clear();
    std::istringstream lines(text);
    std::string line;
    const std::string open = "{{skill:";
    const std::string close = "}}";
    while (std::getline(lines, line)) {
        const std::string value = trimmed(line);
        if (!begins_with(value, open) || !ends_with(value, close)) {
            continue;
        }
        const std::string inner =
            value.substr(open.size(), value.size() - open.size() - close.size());
        JsonValue root;
        JsonParser parser(inner);
        std::string parse_error;
        if (!parser.parse(root, parse_error) || root.type != JsonValue::Type::Object) {
            error = "bad skill invocation: " + parse_error;
            return false;
        }
        const JsonValue* name = json_find(root, "name");
        if (name == nullptr || name->type != JsonValue::Type::String || name->string.empty()) {
            error = "skill invocation missing name";
            return false;
        }
        SkillRequest request;
        request.name = name->string;
        if (const JsonValue* args = json_find(root, "args")) {
            if (args->type == JsonValue::Type::Object) {
                for (const auto& entry : args->object) {
                    request.args[entry.first] =
                        entry.second.type == JsonValue::Type::String
                            ? entry.second.string
                            : (entry.second.type == JsonValue::Type::Number
                                   ? std::to_string(entry.second.number)
                                   : (entry.second.type == JsonValue::Type::Bool
                                          ? (entry.second.boolean ? "true" : "false")
                                          : ""));
                }
            }
        }
        requests.push_back(std::move(request));
    }
    return true;
}

std::string strip_skill_markers(const std::string& text) {
    std::istringstream lines(text);
    std::string line;
    std::ostringstream out;
    const std::string open = "{{skill:";
    const std::string close = "}}";
    while (std::getline(lines, line)) {
        const std::string value = trimmed(line);
        if (begins_with(value, open) && ends_with(value, close)) {
            continue;
        }
        out << line << "\n";
    }
    return trimmed(out.str());
}

std::string skill_instructions(const std::vector<std::string>& enabled) {
    if (enabled.empty()) {
        return "";
    }
    std::ostringstream out;
    out << "\nYou have access to the following skills. When you need one, emit a single exact "
           "line in this form and nothing else on that line:\n"
           "{{skill:{\"name\":\"read_file\",\"args\":{\"path\":\"<path>\"}}}}\n"
           "If you need multiple skills, emit one such line per skill, each on its own line. A "
           "tool result will be added as a new user message before you answer.\n"
           "Available skills:\n";
    for (const auto& name : enabled) {
        out << "- " << name << "\n";
    }
    out << "\nSkills are executed automatically when enabled. Use file and command skills "
           "carefully because they can change files on this machine. Only use skills from the "
           "list above.\n";
    return out.str();
}

bool skill_available(const std::string& name, const std::vector<std::string>& enabled) {
    return std::find(enabled.begin(), enabled.end(), name) != enabled.end();
}

std::vector<std::string> skill_catalog() {
    return {"read_file", "write_file",   "list_files",  "search_files",  "run_command",
            "fetch_url", "current_time", "memory_save", "memory_recall", "memory_clear"};
}

std::string skill_description(const std::string& name) {
    if (name == "read_file") {
        return "Read a text file from disk.";
    }
    if (name == "write_file") {
        return "Write text content to a file.";
    }
    if (name == "list_files") {
        return "List files and directories under a path.";
    }
    if (name == "search_files") {
        return "Search a directory tree for matching file paths.";
    }
    if (name == "run_command") {
        return "Run a shell command and capture its output.";
    }
    if (name == "fetch_url") {
        return "Fetch an http:// URL over the network.";
    }
    if (name == "current_time") {
        return "Return the current local date and time.";
    }
    if (name == "memory_save") {
        return "Save a persistent user fact to memory.";
    }
    if (name == "memory_recall") {
        return "Recall all saved persistent user facts.";
    }
    if (name == "memory_clear") {
        return "Clear all saved persistent user facts.";
    }
    return "";
}

std::string skill_example(const std::string& name) {
    if (name == "read_file") {
        return "{{skill:{\"name\":\"read_file\",\"args\":{\"path\":\"<path>\"}}}}";
    }
    if (name == "write_file") {
        return "{{skill:{\"name\":\"write_file\",\"args\":{\"path\":\"<path>\",\"content\":\"<text>"
               "\"}}}}";
    }
    if (name == "list_files") {
        return "{{skill:{\"name\":\"list_files\",\"args\":{\"path\":\"<path>\"}}}}";
    }
    if (name == "search_files") {
        return "{{skill:{\"name\":\"search_files\",\"args\":{\"path\":\"<path>\",\"query\":\"<"
               "query>\"}}}}";
    }
    if (name == "run_command") {
        return "{{skill:{\"name\":\"run_command\",\"args\":{\"command\":\"<command>\"}}}}";
    }
    if (name == "fetch_url") {
        return "{{skill:{\"name\":\"fetch_url\",\"args\":{\"url\":\"<url>\"}}}}";
    }
    if (name == "current_time") {
        return "{{skill:{\"name\":\"current_time\",\"args\":{}}}}";
    }
    if (name == "memory_save") {
        return "{{skill:{\"name\":\"memory_save\",\"args\":{\"text\":\"<fact>\"}}}}";
    }
    if (name == "memory_recall") {
        return "{{skill:{\"name\":\"memory_recall\",\"args\":{}}}}";
    }
    if (name == "memory_clear") {
        return "{{skill:{\"name\":\"memory_clear\",\"args\":{}}}}";
    }
    return "";
}

std::string execute_skill(const Config& cfg, const SkillRequest& request, std::string& error) {
    error.clear();
    if (request.name == "read_file") {
        const auto it = request.args.find("path");
        if (it == request.args.end() || it->second.empty()) {
            error = "read_file requires path";
            return {};
        }
        return read_file_text(expand_path(it->second), error);
    }
    if (request.name == "write_file") {
        const auto ipath = request.args.find("path");
        const auto icontent = request.args.find("content");
        if (ipath == request.args.end() || ipath->second.empty() ||
            icontent == request.args.end()) {
            error = "write_file requires path and content";
            return {};
        }
        if (!write_file_text(expand_path(ipath->second), icontent->second, error)) {
            return {};
        }
        return "File written: " + expand_path(ipath->second).string();
    }
    if (request.name == "list_files") {
        const auto it = request.args.find("path");
        const std::string path = it == request.args.end() ? cfg.model_dir.string() : it->second;
        return list_directory(expand_path(path), error);
    }
    if (request.name == "search_files") {
        const auto ipath = request.args.find("path");
        const auto iquery = request.args.find("query");
        if (iquery == request.args.end() || iquery->second.empty()) {
            error = "search_files requires query";
            return {};
        }
        const std::string path =
            ipath == request.args.end() ? cfg.model_dir.string() : ipath->second;
        return search_files(expand_path(path), iquery->second, error);
    }
    if (request.name == "run_command") {
        const auto it = request.args.find("command");
        if (it == request.args.end() || it->second.empty()) {
            error = "run_command requires command";
            return {};
        }
        return run_command(it->second, error);
    }
    if (request.name == "fetch_url") {
        const auto it = request.args.find("url");
        if (it == request.args.end() || it->second.empty()) {
            error = "fetch_url requires url";
            return {};
        }
        return fetch_http(it->second, error);
    }
    if (request.name == "current_time") {
        return current_time();
    }
    if (request.name == "memory_save") {
        const auto it = request.args.find("text");
        if (it == request.args.end() || it->second.empty()) {
            error = "memory_save requires text";
            return {};
        }
        std::string memory_error;
        if (!append_memory(cfg, it->second, memory_error)) {
            error = memory_error;
            return {};
        }
        return "Saved to memory.";
    }
    if (request.name == "memory_recall") {
        return memory_load(cfg);
    }
    if (request.name == "memory_clear") {
        std::string memory_error;
        if (!clear_memories(cfg, memory_error)) {
            error = memory_error;
            return {};
        }
        return "Memory cleared.";
    }
    error = "unknown skill: " + request.name;
    return {};
}

}
