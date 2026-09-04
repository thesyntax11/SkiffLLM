#include "skiffllm/server.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "skiffllm/http_auth.hpp"
#include "skiffllm/skills.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET skiffllm_socket_t;
const skiffllm_socket_t invalid_socket = INVALID_SOCKET;
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int skiffllm_socket_t;
const skiffllm_socket_t invalid_socket = -1;
#endif

namespace skiffllm {
namespace {

std::string socket_error() {
#ifdef _WIN32
    const int code = WSAGetLastError();
    std::ostringstream out;
    out << "socket error " << code;
    return out.str();
#else
    return std::string(std::strerror(errno));
#endif
}

void close_socket(skiffllm_socket_t socket_fd) {
#ifdef _WIN32
    closesocket(socket_fd);
#else
    close(socket_fd);
#endif
}

bool set_socket_timeout(skiffllm_socket_t socket_fd, int seconds, std::string& error) {
#ifdef _WIN32
    const DWORD timeout = static_cast<DWORD>(seconds) * 1000;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                   sizeof(timeout)) != 0) {
        error = "cannot set receive timeout: " + socket_error();
        return false;
    }
    if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout),
                   sizeof(timeout)) != 0) {
        error = "cannot set send timeout: " + socket_error();
        return false;
    }
#else
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        error = "cannot set receive timeout: " + socket_error();
        return false;
    }
    if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        error = "cannot set send timeout: " + socket_error();
        return false;
    }
#endif
    return true;
}

bool send_all(skiffllm_socket_t socket_fd, const std::string& data, std::string& error) {
    size_t offset = 0;
    while (offset < data.size()) {
        const int length = static_cast<int>(data.size() - offset);
        const int sent = send(socket_fd, data.data() + offset, length, 0);
        if (sent <= 0) {
            error = "failed to send response: " + socket_error();
            return false;
        }
        offset += static_cast<size_t>(sent);
    }
    return true;
}

std::string status_reason(int status) {
    switch (status) {
        case 200:
            return "OK";
        case 204:
            return "No Content";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 500:
            return "Internal Server Error";
        default:
            return "Error";
    }
}

bool send_response(skiffllm_socket_t socket_fd, int status, const std::string& content_type,
                   const std::string& body, std::string& error) {
    std::ostringstream headers;
    headers << "HTTP/1.1 " << status << " " << status_reason(status) << "\r\n";
    headers << "Content-Type: " << content_type << "\r\n";
    headers << "Content-Length: " << body.size() << "\r\n";
    headers << "Cache-Control: no-store\r\n";
    headers << "X-Content-Type-Options: nosniff\r\n";
    headers << "Access-Control-Allow-Origin: *\r\n";
    headers << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    headers << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    headers << "Access-Control-Max-Age: 86400\r\n";
    headers << "Connection: close\r\n\r\n";
    return send_all(socket_fd, headers.str() + body, error);
}

bool send_stream_headers(skiffllm_socket_t socket_fd, std::string& error) {
    std::ostringstream headers;
    headers << "HTTP/1.1 200 OK\r\n";
    headers << "Content-Type: text/event-stream\r\n";
    headers << "Cache-Control: no-store\r\n";
    headers << "X-Accel-Buffering: no\r\n";
    headers << "X-Content-Type-Options: nosniff\r\n";
    headers << "Access-Control-Allow-Origin: *\r\n";
    headers << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    headers << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    headers << "Access-Control-Max-Age: 86400\r\n";
    headers << "Connection: close\r\n\r\n";
    return send_all(socket_fd, headers.str(), error);
}

bool create_listener(const std::string& host, int port, skiffllm_socket_t& listener,
                     std::string& error) {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        error = "WSAStartup failed";
        return false;
    }
#endif

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char port_text[16] = {0};
    std::snprintf(port_text, sizeof(port_text), "%d", port);
    struct addrinfo* addresses = nullptr;
    const int lookup =
        getaddrinfo(host.empty() ? nullptr : host.c_str(), port_text, &hints, &addresses);
    if (lookup != 0) {
#ifdef _WIN32
        error = "address lookup failed: " + std::string(gai_strerror(lookup));
        WSACleanup();
#else
        error = "address lookup failed: " + std::string(gai_strerror(lookup));
#endif
        return false;
    }

    skiffllm_socket_t candidate = invalid_socket;
    for (struct addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == invalid_socket) {
            continue;
        }
        const int reuse = 1;
        setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                   sizeof(reuse));
#ifdef _WIN32
        const int bind_result =
            bind(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen));
#else
        const int bind_result = bind(candidate, address->ai_addr, address->ai_addrlen);
#endif
        if (bind_result == 0 && listen(candidate, 16) == 0) {
            break;
        }
        close_socket(candidate);
        candidate = invalid_socket;
    }

    freeaddrinfo(addresses);
    if (candidate == invalid_socket) {
#ifdef _WIN32
        error = "cannot bind to " + host + ":" + port_text + ": " + socket_error();
        WSACleanup();
#else
        error = "cannot bind to " + host + ":" + port_text + ": " + socket_error();
#endif
        return false;
    }

    listener = candidate;
    return true;
}

struct HttpRequest {
    std::string method;
    std::string target;
    std::string body;
    std::map<std::string, std::string> headers;
};

bool read_http_request(skiffllm_socket_t socket_fd, HttpRequest& request, std::string& error) {
    std::string data;
    char buffer[4096] = {0};
    while (data.size() < 65536) {
        const int received = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (received == 0) {
            error = "client closed before completing the request";
            return false;
        }
        if (received < 0) {
#ifdef _WIN32
            const int code = WSAGetLastError();
            if (code == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            error = "request read failed: " + socket_error();
            return false;
        }
        data.append(buffer, static_cast<size_t>(received));
        if (data.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }
    if (data.size() >= 65536) {
        error = "request headers are too large";
        return false;
    }

    const size_t header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        error = "incomplete request headers";
        return false;
    }
    const size_t header_size = header_end + 4;

    const size_t first_break = data.find("\r\n");
    if (first_break == std::string::npos) {
        error = "invalid request line";
        return false;
    }

    {
        std::istringstream first_line(data.substr(0, first_break));
        first_line >> request.method >> request.target;
    }
    if (request.method.empty() || request.target.empty()) {
        error = "invalid request line";
        return false;
    }

    long content_length = 0;
    size_t position = first_break + 2;
    while (position < header_size) {
        const size_t next_break = data.find("\r\n", position);
        if (next_break == std::string::npos || next_break > header_size) {
            break;
        }
        const std::string line = data.substr(position, next_break - position);
        position = next_break + 2;
        const size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string name = lower(trim(line.substr(0, colon)));
        const std::string value = trim(line.substr(colon + 1));
        request.headers[name] = value;
        if (name == "content-length") {
            char* end = nullptr;
            content_length = std::strtol(value.c_str(), &end, 10);
            if (end == value.c_str()) {
                content_length = -1;
            }
        }
    }

    if (content_length < 0 || content_length > static_cast<long>(64 * 1024 * 1024)) {
        error = "invalid content length";
        return false;
    }

    std::string body = data.substr(header_size);
    while (body.size() < static_cast<size_t>(content_length)) {
        char buffer[8192] = {0};
        const int received =
            recv(socket_fd, buffer,
                 static_cast<int>(
                     std::min<size_t>(sizeof(buffer),
                                      static_cast<size_t>(content_length) - body.size())),
                 0);
        if (received <= 0) {
            error = "request body read failed: " + socket_error();
            return false;
        }
        body.append(buffer, static_cast<size_t>(received));
    }

    body.resize(static_cast<size_t>(content_length));
    request.body = body;
    return true;
}

struct Json {
    enum class Type { Null, Boolean, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;
};

const Json* json_find(const Json& value, const std::string& key) {
    if (value.type != Json::Type::Object) {
        return nullptr;
    }
    for (const auto& entry : value.object) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

bool json_bool(const Json& value, bool default_value) {
    if (value.type != Json::Type::Boolean) {
        return default_value;
    }
    return value.boolean;
}

double json_number(const Json& value, double default_value) {
    if (value.type != Json::Type::Number) {
        return default_value;
    }
    return value.number;
}

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    char buffer[8] = {0};
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                    out << buffer;
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    out << '"';
    return out.str();
}

class JsonParser {
   public:
    explicit JsonParser(const std::string& input) : text_(input) {}

    bool parse(Json& output, std::string& error) {
        skip_whitespace();
        if (!parse_value(output)) {
            error = error_;
            return false;
        }
        skip_whitespace();
        if (index_ != text_.size()) {
            error = "trailing characters after JSON value";
            return false;
        }
        return true;
    }

   private:
    bool parse_value(Json& output) {
        skip_whitespace();
        if (index_ >= text_.size()) {
            error_ = "unexpected end of JSON";
            return false;
        }
        const char ch = text_[index_];
        if (ch == '"') {
            output.type = Json::Type::String;
            return parse_string(output.string);
        }
        if (ch == '{') {
            output.type = Json::Type::Object;
            return parse_object(output.object);
        }
        if (ch == '[') {
            output.type = Json::Type::Array;
            return parse_array(output.array);
        }
        if (ch == 't' || ch == 'f') {
            output.type = Json::Type::Boolean;
            return parse_boolean(output.boolean);
        }
        if (ch == 'n') {
            return parse_null(output);
        }
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            output.type = Json::Type::Number;
            return parse_number(output.number);
        }
        error_ = std::string("unexpected character in JSON: ") + ch;
        return false;
    }

    bool parse_string(std::string& output) {
        if (text_[index_] != '"') {
            error_ = "expected string";
            return false;
        }
        index_ += 1;
        output.clear();
        while (index_ < text_.size()) {
            const char ch = text_[index_];
            index_ += 1;
            if (ch == '"') {
                return true;
            }
            if (ch != '\\') {
                output.push_back(ch);
                continue;
            }
            if (index_ >= text_.size()) {
                error_ = "unterminated escape in JSON string";
                return false;
            }
            const char escape = text_[index_];
            index_ += 1;
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
                case 'u': {
                    uint32_t codepoint = 0;
                    if (!read_hex4(codepoint)) {
                        return false;
                    }
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF && index_ + 1 < text_.size() &&
                        text_[index_] == '\\' && text_[index_ + 1] == 'u') {
                        index_ += 2;
                        uint32_t low = 0;
                        if (read_hex4(low)) {
                            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                        }
                    } else if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        codepoint = 0xFFFD;
                    }
                    append_utf8(output, codepoint);
                    break;
                }
                default:
                    error_ = "invalid escape in JSON string";
                    return false;
            }
        }
        error_ = "unterminated JSON string";
        return false;
    }

    bool read_hex4(uint32_t& value) {
        value = 0;
        if (index_ + 4 > text_.size()) {
            error_ = "truncated unicode escape";
            return false;
        }
        for (int i = 0; i < 4; ++i) {
            const char ch = text_[index_];
            index_ += 1;
            value <<= 4;
            if (ch >= '0' && ch <= '9') {
                value += static_cast<uint32_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                value += static_cast<uint32_t>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                value += static_cast<uint32_t>(ch - 'A' + 10);
            } else {
                error_ = "invalid unicode escape";
                return false;
            }
        }
        return true;
    }

    static void append_utf8(std::string& output, uint32_t codepoint) {
        if (codepoint <= 0x7F) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0x10FFFF) {
            output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            append_utf8(output, 0xFFFD);
        }
    }

    bool parse_object(std::vector<std::pair<std::string, Json>>& output) {
        index_ += 1;
        output.clear();
        skip_whitespace();
        if (index_ < text_.size() && text_[index_] == '}') {
            index_ += 1;
            return true;
        }
        while (index_ < text_.size()) {
            skip_whitespace();
            if (text_[index_] != '"') {
                error_ = "expected object key";
                return false;
            }
            std::string key;
            if (!parse_string(key)) {
                return false;
            }
            skip_whitespace();
            if (index_ >= text_.size() || text_[index_] != ':') {
                error_ = "expected colon after object key";
                return false;
            }
            index_ += 1;
            Json value;
            if (!parse_value(value)) {
                return false;
            }
            output.emplace_back(std::move(key), std::move(value));
            skip_whitespace();
            if (index_ < text_.size() && text_[index_] == ',') {
                index_ += 1;
                continue;
            }
            if (index_ < text_.size() && text_[index_] == '}') {
                index_ += 1;
                return true;
            }
            error_ = "expected comma or closing brace in object";
            return false;
        }
        error_ = "unterminated JSON object";
        return false;
    }

    bool parse_array(std::vector<Json>& output) {
        index_ += 1;
        output.clear();
        skip_whitespace();
        if (index_ < text_.size() && text_[index_] == ']') {
            index_ += 1;
            return true;
        }
        while (index_ < text_.size()) {
            Json value;
            if (!parse_value(value)) {
                return false;
            }
            output.push_back(std::move(value));
            skip_whitespace();
            if (index_ < text_.size() && text_[index_] == ',') {
                index_ += 1;
                continue;
            }
            if (index_ < text_.size() && text_[index_] == ']') {
                index_ += 1;
                return true;
            }
            error_ = "expected comma or closing bracket in array";
            return false;
        }
        error_ = "unterminated JSON array";
        return false;
    }

    bool parse_boolean(bool& output) {
        if (text_.compare(index_, 4, "true") == 0) {
            index_ += 4;
            output = true;
            return true;
        }
        if (text_.compare(index_, 5, "false") == 0) {
            index_ += 5;
            output = false;
            return true;
        }
        error_ = "invalid boolean";
        return false;
    }

    bool parse_null(Json& output) {
        if (text_.compare(index_, 4, "null") == 0) {
            index_ += 4;
            output.type = Json::Type::Null;
            return true;
        }
        error_ = "invalid null";
        return false;
    }

    bool parse_number(double& output) {
        const size_t start = index_;
        if (text_[index_] == '-') {
            index_ += 1;
        }
        while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
            index_ += 1;
        }
        if (index_ < text_.size() && text_[index_] == '.') {
            index_ += 1;
            while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
                index_ += 1;
            }
        }
        if (index_ < text_.size() && (text_[index_] == 'e' || text_[index_] == 'E')) {
            index_ += 1;
            if (index_ < text_.size() && (text_[index_] == '+' || text_[index_] == '-')) {
                index_ += 1;
            }
            while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
                index_ += 1;
            }
        }
        if (start == index_) {
            error_ = "invalid number";
            return false;
        }
        const std::string token = text_.substr(start, index_ - start);
        char* end = nullptr;
        const double value = std::strtod(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0') {
            error_ = "invalid number";
            return false;
        }
        output = value;
        return true;
    }

    void skip_whitespace() {
        while (index_ < text_.size()) {
            const char ch = text_[index_];
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                index_ += 1;
            } else {
                break;
            }
        }
    }

    const std::string& text_;
    size_t index_ = 0;
    std::string error_;
};

std::string model_name(const Config& config) {
    const std::string path = config.model_path.string();
    const size_t slash = path.find_last_of("/\\");
    const std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
    return file.empty() ? "skiffllm" : file;
}

bool request_messages(const Json& body, std::vector<ChatMessage>& messages, std::string& error) {
    const Json* raw = json_find(body, "messages");
    if (raw == nullptr || raw->type != Json::Type::Array) {
        error = "\"messages\" must be an array";
        return false;
    }
    messages.clear();
    for (const Json& item : raw->array) {
        if (item.type != Json::Type::Object) {
            error = "message entries must be objects";
            return false;
        }
        const Json* role = json_find(item, "role");
        const Json* content = json_find(item, "content");
        if (role == nullptr || role->type != Json::Type::String || content == nullptr ||
            content->type != Json::Type::String) {
            error = "message entries need string \"role\" and \"content\"";
            return false;
        }
        if (!role->string.empty()) {
            messages.push_back({role->string, content->string});
        }
    }
    if (messages.empty()) {
        error = "at least one message is required";
        return false;
    }
    return true;
}

void send_chat_error(skiffllm_socket_t socket_fd, bool stream_started, const std::string& message,
                     const std::string& request_id) {
    (void)request_id;
    std::string error;
    if (stream_started) {
        std::ostringstream body;
        body << "data: {\"error\":{\"message\":" << json_escape(message)
             << ",\"type\":\"generation_error\",\"param\":null,\"code\":null}}\n\n";
        body << "data: [DONE]\n\n";
        send_all(socket_fd, body.str(), error);
    } else {
        std::ostringstream body;
        body << "{\"error\":{\"message\":" << json_escape(message)
             << ",\"type\":\"generation_error\",\"param\":null,\"code\":null}}";
        send_response(socket_fd, 500, "application/json", body.str(), error);
    }
}

void handle_chat_completions(skiffllm_socket_t socket_fd, const Config& config, SkiffEngine& engine,
                             const GenerationOptions& base_options, const HttpRequest& request,
                             const std::function<bool()>& interrupted,
                             const std::string& request_id) {
    Json body;
    std::string parse_error;
    JsonParser parser(request.body);
    if (!parser.parse(body, parse_error)) {
        send_chat_error(socket_fd, false, "invalid JSON body: " + parse_error, request_id);
        return;
    }
    if (body.type != Json::Type::Object) {
        send_chat_error(socket_fd, false, "request body must be a JSON object", request_id);
        return;
    }

    std::vector<ChatMessage> messages;
    if (!request_messages(body, messages, parse_error)) {
        send_chat_error(socket_fd, false, parse_error, request_id);
        return;
    }

    GenerationOptions options = base_options;
    if (const Json* value = json_find(body, "temperature")) {
        const double number = json_number(*value, options.temperature);
        if (number >= 0.0) {
            options.temperature = static_cast<float>(number);
        }
    }
    if (const Json* value = json_find(body, "top_p")) {
        const double number = json_number(*value, options.top_p);
        if (number > 0.0 && number <= 1.0) {
            options.top_p = static_cast<float>(number);
        }
    }
    if (const Json* value = json_find(body, "max_tokens")) {
        const double number = json_number(*value, -1.0);
        if (number >= 1.0) {
            options.n_predict = static_cast<int>(number);
        }
    }
    if (const Json* value = json_find(body, "seed")) {
        const double number = json_number(*value, -1.0);
        if (number >= 0.0) {
            options.seed = static_cast<uint32_t>(number);
        }
    }
    if (const Json* value = json_find(body, "stop")) {
        options.stop_sequences.clear();
        if (value->type == Json::Type::String) {
            options.stop_sequences.push_back(value->string);
        } else if (value->type == Json::Type::Array) {
            for (const Json& item : value->array) {
                if (item.type == Json::Type::String) {
                    options.stop_sequences.push_back(item.string);
                }
            }
        }
    }

    const Json* stream_value = json_find(body, "stream");
    const bool stream = json_bool(stream_value == nullptr ? Json{} : *stream_value, false);
    const std::string model = config.model_path.empty() ? "skiffllm" : model_name(config);
    const long created = static_cast<long>(std::time(nullptr));

    std::shared_ptr<bool> write_ok = std::make_shared<bool>(true);
    std::string send_error;

    if (stream) {
        std::string error;
        if (!send_stream_headers(socket_fd, error)) {
            return;
        }
        options.token_callback = [socket_fd, write_ok, &send_error, &request_id, &model,
                                  created](const std::string& part) {
            std::ostringstream chunk;
            chunk << "data: {";
            chunk << "\"id\":" << json_escape(request_id) << ",";
            chunk << "\"object\":\"chat.completion.chunk\",";
            chunk << "\"created\":" << created << ",";
            chunk << "\"model\":" << json_escape(model) << ",";
            chunk << "\"choices\":[{\"index\":0,\"delta\":{\"content\":";
            chunk << json_escape(part);
            chunk << "},\"finish_reason\":null}]}\n\n";
        };

        GenerationResult result;
        std::string generation_error;
        const bool ok = engine.generate(
            messages, options, result,
            [interrupted, write_ok]() { return interrupted() || !*write_ok; }, generation_error);
        if (!ok) {
            send_chat_error(socket_fd, true, generation_error, request_id);
            return;
        }

        std::ostringstream finish_event;
        finish_event << "data: {";
        finish_event << "\"id\":" << json_escape(request_id) << ",";
        finish_event << "\"object\":\"chat.completion.chunk\",";
        finish_event << "\"created\":" << created << ",";
        finish_event << "\"model\":" << json_escape(model) << ",";
        finish_event << "\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n";
        finish_event << "data: [DONE]\n\n";
        send_all(socket_fd, finish_event.str(), send_error);
        return;
    }

    options.token_callback = nullptr;
    GenerationResult result;
    std::string error;
    std::vector<ChatMessage> conversation = messages;
    bool ok = false;
    for (int round = 0; round < 8; ++round) {
        GenerationResult round_result;
        ok = engine.generate(conversation, options, round_result, interrupted, error);
        if (!ok) {
            break;
        }
        result = round_result;
        std::vector<SkillRequest> requests;
        std::string skill_error;
        if (!parse_skill_requests(result.text, requests, skill_error) || requests.empty()) {
            break;
        }
        conversation.push_back({"assistant", result.text});
        bool executed = false;
        for (const SkillRequest& request : requests) {
            std::string result_text;
            std::string execute_error;
            result_text = execute_skill(config, request, execute_error);
            conversation.push_back(
                {"user", "Skill result for " + request.name + ":\n" +
                             (execute_error.empty() ? result_text : "Error: " + execute_error)});
            executed = true;
        }
        if (!executed) {
            break;
        }
    }
    if (!ok) {
        send_chat_error(socket_fd, false, error, request_id);
        return;
    }
    result.text = strip_skill_markers(result.text);

    std::ostringstream out;
    out << "{\n";
    out << "  \"id\":" << json_escape(request_id) << ",\n";
    out << "  \"object\":\"chat.completion\",\n";
    out << "  \"created\":" << created << ",\n";
    out << "  \"model\":" << json_escape(model) << ",\n";
    out << "  \"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":";
    out << json_escape(result.text);
    out << "},\"finish_reason\":\"stop\"}],\n";
    out << "  \"usage\":{\"prompt_tokens\":" << result.prompt_tokens
        << ",\"completion_tokens\":" << result.generated_tokens
        << ",\"total_tokens\":" << (result.prompt_tokens + result.generated_tokens) << "}\n";
    out << "}\n";
    send_response(socket_fd, 200, "application/json", out.str(), error);
}

void handle_request(skiffllm_socket_t socket_fd, const Config& config, SkiffEngine& engine,
                    const GenerationOptions& options, const HttpRequest& request,
                    const std::function<bool()>& interrupted, std::mutex& generation_mutex,
                    const std::string& request_id) {
    std::string error;
    if (request.method == "OPTIONS") {
        send_response(socket_fd, 204, "text/plain", "", error);
        return;
    }
    if (request.method == "GET" && request.target == "/health") {
        std::ostringstream out;
        out << "{\"status\":\"ok\",\"model\":\"" << json_escape(model_name(config)) << "\"}\n";
        send_response(socket_fd, 200, "application/json", out.str(), error);
        return;
    }
    if (request.method == "GET" && request.target == "/version") {
        std::ostringstream out;
        out << "{\"name\":\"skiffllm\",\"version\":\"";
#ifdef SKIFFLLM_VERSION
        out << SKIFFLLM_VERSION;
#else
        out << "unknown";
#endif
        out << "\",\"backend\":\"llama.cpp\"}\n";
        send_response(socket_fd, 200, "application/json", out.str(), error);
        return;
    }
    if (request.target == "/v1/models" || request.target == "/v1/chat/completions" ||
        request.target == "/v1/skills" || request.target == "/v1/skills/execute") {
        const auto authorization = request.headers.find("authorization");
        const bool authorized =
            config.api_key.empty() || (authorization != request.headers.end() &&
                                       bearer_token_matches(authorization->second, config.api_key));
        if (!authorized) {
            send_response(socket_fd, 401, "application/json",
                          "{\"error\":\"missing or invalid API key\"}\n", error);
            return;
        }
    }
    if (request.method == "GET" && request.target == "/v1/models") {
        std::ostringstream out;
        out << "{\"object\":\"list\",\"data\":[{\"id\":";
        out << json_escape(model_name(config))
            << ",\"object\":\"model\",\"created\":" << std::time(nullptr);
        out << ",\"owned_by\":\"skiffllm\"}]}\n";
        send_response(socket_fd, 200, "application/json", out.str(), error);
        return;
    }
    if (request.method == "GET" && request.target == "/v1/skills") {
        std::ostringstream out;
        out << "{\"object\":\"list\",\"data\":[";
        const auto catalog = skill_catalog();
        for (size_t i = 0; i < catalog.size(); ++i) {
            if (i != 0) {
                out << ",";
            }
            out << "{\"name\":" << json_escape(catalog[i])
                << ",\"description\":" << json_escape(skill_description(catalog[i]))
                << ",\"example\":" << json_escape(skill_example(catalog[i])) << "}";
        }
        out << "]}\n";
        send_response(socket_fd, 200, "application/json", out.str(), error);
        return;
    }
    if (request.method == "POST" && request.target == "/v1/skills/execute") {
        Json body;
        std::string parse_error;
        JsonParser parser(request.body);
        if (!parser.parse(body, parse_error)) {
            send_response(socket_fd, 400, "application/json",
                          "{\"error\":\"" + json_escape("invalid JSON body") + "\"}\n", error);
            return;
        }
        const Json* name = json_find(body, "name");
        if (name == nullptr || name->type != Json::Type::String || name->string.empty()) {
            send_response(socket_fd, 400, "application/json", "{\"error\":\"name is required\"}\n",
                          error);
            return;
        }
        SkillRequest request;
        request.name = name->string;
        if (const Json* args = json_find(body, "args")) {
            if (args->type == Json::Type::Object) {
                for (const auto& entry : args->object) {
                    if (entry.second.type == Json::Type::String) {
                        request.args[entry.first] = entry.second.string;
                    } else if (entry.second.type == Json::Type::Number) {
                        request.args[entry.first] = std::to_string(entry.second.number);
                    } else if (entry.second.type == Json::Type::Boolean) {
                        request.args[entry.first] = entry.second.boolean ? "true" : "false";
                    }
                }
            }
        }
        std::string skill_error;
        const std::string result = execute_skill(config, request, skill_error);
        std::ostringstream out;
        out << "{\"ok\":" << (skill_error.empty() ? "true" : "false")
            << ",\"result\":" << json_escape(result) << ",\"error\":" << json_escape(skill_error)
            << "}\n";
        send_response(socket_fd, 200, "application/json", out.str(), error);
        return;
    }
    if (request.method == "GET" && request.target == "/") {
        send_response(socket_fd, 200, "text/plain",
                      "SkiffLLM local inference server\n"
                      "Endpoints: GET /health, GET /version, GET /v1/models, "
                      "GET /v1/skills, POST /v1/skills/execute, "
                      "POST /v1/chat/completions\n",
                      error);
        return;
    }
    if (request.method == "POST" && request.target == "/v1/chat/completions") {
        std::lock_guard<std::mutex> lock(generation_mutex);
        handle_chat_completions(socket_fd, config, engine, options, request, interrupted,
                                request_id);
        return;
    }
    send_response(socket_fd, 404, "application/json", "{\"error\":\"not found\"}\n", error);
}

std::string make_request_id() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream out;
    out << "chatcmpl-" << now;
    return out.str();
}

}

int run_server(Config& config, SkiffEngine& engine, Terminal& terminal,
               const GenerationOptions& options, const std::function<bool()>& interrupted) {
    skiffllm_socket_t listener = invalid_socket;
    std::string error;
    if (!create_listener(config.server_host, config.server_port, listener, error)) {
        terminal.error(error);
        return 1;
    }

    terminal.success("Serving local API on http://" + config.server_host + ":" +
                     std::to_string(config.server_port) +
                     " (Ctrl+C to stop; generation is serialized per model)");
    if (!config.api_key.empty()) {
        terminal.info(
            "/v1/* requires Authorization: Bearer <key>; public endpoints are /health, /version "
            "and /.");
    }

    std::mutex generation_mutex;
    struct ClientWorker {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> done;
    };
    std::vector<ClientWorker> workers;

    while (!interrupted()) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(listener, &read_set);
        struct timeval wait_time;
        wait_time.tv_sec = 1;
        wait_time.tv_usec = 0;
#ifdef _WIN32
        const int ready = select(0, &read_set, nullptr, nullptr, &wait_time);
#else
        const int ready = select(listener + 1, &read_set, nullptr, nullptr, &wait_time);
#endif
        if (ready < 0) {
#ifdef _WIN32
            const int code = WSAGetLastError();
            if (code == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            if (interrupted()) {
                break;
            }
            terminal.warning("Select failed: " + socket_error());
            continue;
        }
        if (ready == 0) {
            continue;
        }

        struct sockaddr_storage address;
#ifdef _WIN32
        int address_length = sizeof(address);
#else
        socklen_t address_length = sizeof(address);
#endif
        skiffllm_socket_t client =
            accept(listener, reinterpret_cast<struct sockaddr*>(&address), &address_length);
        if (client == invalid_socket) {
#ifdef _WIN32
            const int code = WSAGetLastError();
            if (code == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            if (interrupted()) {
                break;
            }
            terminal.warning("Accept failed: " + socket_error());
            continue;
        }

        set_socket_timeout(client, 30, error);
        auto done = std::make_shared<std::atomic<bool>>(false);
        workers.push_back(
            ClientWorker{std::thread([client, &config, &engine, &options, &interrupted,
                                      &generation_mutex, done]() {
                             std::string thread_error;
                             HttpRequest request;
                             if (!read_http_request(client, request, thread_error)) {
                                 send_response(client, 400, "application/json",
                                               "{\"error\":\"bad request\"}\n", thread_error);
                             } else {
                                 handle_request(client, config, engine, options, request,
                                                interrupted, generation_mutex, make_request_id());
                             }
                             close_socket(client);
                             done->store(true, std::memory_order_release);
                         }),
                         done});

        for (auto it = workers.begin(); it != workers.end();) {
            if (it->done->load(std::memory_order_acquire) && it->thread.joinable()) {
                it->thread.join();
                it = workers.erase(it);
            } else {
                ++it;
            }
        }
        if (workers.size() > 64) {
            terminal.warning("Too many concurrent clients; generation queue is busy.");
        }
    }

    for (auto& worker : workers) {
        if (worker.thread.joinable()) {
            worker.thread.join();
        }
    }

    close_socket(listener);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

}
