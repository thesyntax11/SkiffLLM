#include "skifflm/terminal.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#ifdef SKIFFLLM_HAVE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

namespace skifflm {
namespace {

bool is_terminal(FILE* stream) {
#ifdef _WIN32
    return _isatty(_fileno(stream)) != 0;
#else
    return isatty(fileno(stream)) != 0;
#endif
}

bool is_alnum(unsigned char ch) {
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9');
}

bool is_balanced(const std::string& text) {
    bool single = false;
    bool double_quote = false;
    int round = 0;
    int square = 0;
    int curly = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '\\' && i + 1 < text.size()) {
            i += 1;
            continue;
        }
        if (single) {
            if (ch == '\'') {
                single = false;
            }
            continue;
        }
        if (double_quote) {
            if (ch == '"') {
                double_quote = false;
            }
            continue;
        }
        if (ch == '\'') {
            const bool previous_is_word = i > 0 && is_alnum(static_cast<unsigned char>(text[i - 1]));
            if (!previous_is_word) {
                single = true;
            }
            continue;
        }
        if (ch == '"') {
            double_quote = !double_quote;
            continue;
        }
        if (ch == '(') {
            round += 1;
        } else if (ch == ')') {
            round -= 1;
        } else if (ch == '[') {
            square += 1;
        } else if (ch == ']') {
            square -= 1;
        } else if (ch == '{') {
            curly += 1;
        } else if (ch == '}') {
            curly -= 1;
        }
    }

    return !single && !double_quote && round <= 0 && square <= 0 && curly <= 0;
}

}

Terminal::Terminal(const Config& config) : color_(config.color) {
    if (!config.history_path.empty()) {
        readline_history_path_ =
            (config.history_path.parent_path() / "readline_history").string();
    }
    if (!config.color) {
        return;
    }
    const char* no_color = std::getenv("NO_COLOR");
    const char* term = std::getenv("TERM");
    if (no_color != nullptr && *no_color != '\0') {
        color_ = false;
    }
    if (term != nullptr && std::string(term) == "dumb") {
        color_ = false;
    }
    if (!is_terminal(stdout)) {
        color_ = false;
    }
#ifdef SKIFFLLM_HAVE_READLINE
    if (!readline_history_path_.empty()) {
        read_history(readline_history_path_.c_str());
    }
#endif
}

Terminal::~Terminal() {
#ifdef SKIFFLLM_HAVE_READLINE
    if (!readline_history_path_.empty()) {
        const std::filesystem::path path(readline_history_path_);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        write_history(readline_history_path_.c_str());
    }
#endif
}

bool Terminal::color_enabled() const {
    return color_;
}

bool Terminal::live_output() const {
    return is_terminal(stdout);
}

void Terminal::use_color(bool enabled) {
    color_ = enabled;
}

std::string Terminal::paint(const std::string& text, Color color) const {
    if (!color_) {
        return text;
    }
    const char* code = "\033[0m";
    switch (color) {
        case Color::Bold:    code = "\033[1m"; break;
        case Color::Dim:     code = "\033[2m"; break;
        case Color::Red:     code = "\033[31m"; break;
        case Color::Green:   code = "\033[32m"; break;
        case Color::Yellow:  code = "\033[33m"; break;
        case Color::Blue:    code = "\033[34m"; break;
        case Color::Magenta: code = "\033[35m"; break;
        case Color::Cyan:    code = "\033[36m"; break;
        case Color::White:   code = "\033[37m"; break;
        case Color::Reset:   code = "\033[0m"; break;
    }
    return std::string(code) + text + "\033[0m";
}

void Terminal::write(const std::string& text, Color color) const {
    std::cout << paint(text, color);
}

void Terminal::write_raw(const std::string& text) const {
    std::cout << text;
}

void Terminal::write_live(const std::string& text, std::size_t tokens) const {
    if (!is_terminal(stdout)) {
        write_raw(text);
        return;
    }
    if (live_column_) {
        std::cout << "\r\033[K";
    }
    live_column_ = true;
    std::cout << text;
    if (tokens > 0) {
        std::cout << "  " << paint(std::to_string(tokens) + " tok", Color::Dim);
    }
    std::cout.flush();
}

void Terminal::write_raw_line(const std::string& text) const {
    if (live_column_) {
        std::cout << "\r\033[K";
        live_column_ = false;
    }
    std::cout << text;
    if (text.empty() || text.back() != '\n') {
        std::cout << "\n";
    }
    std::cout.flush();
}

void Terminal::finish_live() const {
    if (live_column_) {
        std::cout << "\r\033[K";
        live_column_ = false;
    }
    std::cout.flush();
}

void Terminal::info(const std::string& text) const {
    write(text + "\n", Color::Blue);
}

void Terminal::success(const std::string& text) const {
    write(text + "\n", Color::Green);
}

void Terminal::warning(const std::string& text) const {
    write(text + "\n", Color::Yellow);
}

void Terminal::error(const std::string& text) const {
    write(text + "\n", Color::Red);
}

void Terminal::highlight(const std::string& text) const {
    write(text + "\n", Color::Magenta);
}

bool Terminal::read_prompt(const std::string& prompt, std::string& output) {
    std::string line;
#ifdef SKIFFLLM_HAVE_READLINE
    if (is_terminal(stdin)) {
        char* input = readline(prompt.c_str());
        if (input == nullptr) {
            return false;
        }
        line = input;
        std::free(input);
        if (!line.empty()) {
            add_history(line.c_str());
        }
    } else
#endif
    {
        write(prompt, Color::Cyan);
        std::cout.flush();
        if (!std::getline(std::cin, line)) {
            return false;
        }
    }

    if (!line.empty() && line.front() == '/') {
        output = line;
        return true;
    }

    std::string result = line;
    while (true) {
        if (!result.empty() && result.back() == '\\') {
            result.pop_back();
            write("... ", Color::Dim);
            std::cout.flush();
            std::string next;
            if (!std::getline(std::cin, next)) {
                break;
            }
            result += next;
            continue;
        }
        if (!result.empty() && !is_balanced(result)) {
            write("... ", Color::Dim);
            std::cout.flush();
            std::string next;
            if (!std::getline(std::cin, next)) {
                break;
            }
            result += "\n" + next;
            continue;
        }
        break;
    }

    output = result;
    return true;
}

void Terminal::print_banner(const std::string& version,
                            const ModelInfo& info,
                            const Config& config) {
    write("SkiffLLM ", Color::Bold);
    write(version + "\n", Color::Green);
    write("Offline local assistant powered by llama.cpp\n", Color::Dim);
    if (!info.description.empty()) {
        write("Model: ", Color::Cyan);
        write(info.description + "\n");
    }
    if (info.n_params > 0) {
        write("Parameters: ", Color::Cyan);
        write(to_human_count(info.n_params) + "\n");
    }
    if (info.size_bytes > 0) {
        write("Model size: ", Color::Cyan);
        write(to_human_bytes(info.size_bytes) + "\n");
    }
    if (info.n_ctx_train > 0) {
        write("Training context: ", Color::Cyan);
        write(std::to_string(info.n_ctx_train) + "\n");
    }
    write("Context: ", Color::Cyan);
    write(std::to_string(config.context_size));
    write(" | Threads: ", Color::Cyan);
    write(config.n_threads == 0 ? "auto" : std::to_string(config.n_threads));
    write(" | GPU layers: ", Color::Cyan);
    write(std::to_string(config.n_gpu_layers) + "\n");
    write("\nType /help for commands. Press Ctrl+C or type /exit to quit.\n\n");
}

void Terminal::print_stats(const GenerationResult& result, const std::string& label) const {
    write(label + ": ", Color::Blue);
    write(std::to_string(result.prompt_tokens) + " prompt tokens, ");
    write(std::to_string(result.generated_tokens) + " generated tokens, ");
    char buffer[160];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%.2f s total, %.2f tok/s",
                  (result.prompt_ms + result.generation_ms) / 1000.0,
                  result.tokens_per_second);
    write(buffer + std::string("\n"), Color::Dim);
    if (result.stopped) {
        write("Generation stopped by user.\n", Color::Yellow);
    }
}

void Terminal::print_help() const {
    write("Commands:\n", Color::Cyan);
    write("  /help                 Show this help\n");
    write("  /info                 Show model and session information\n");
    write("  /history              Show the current conversation\n");
    write("  /export <path>        Export the conversation as Markdown\n");
    write("  /clear                Clear the conversation history\n");
    write("  /reset                Clear history and restore the default system prompt\n");
    write("  /system <text>        Set or show the system prompt\n");
    write("  /model <path>         Reload the model from a GGUF file\n");
    write("  /file <path>          Attach a file for upcoming messages\n");
    write("  /clear-attach         Remove all attached files\n");
    write("  /settings             Show the current sampling settings\n");
    write("  /tokenize <text>      Show the token count for the text\n");
    write("  /profile <name>       Use balanced, fast, creative, code or precise\n");
    write("  /stop <text>          Add a stop sequence; /stop shows current stops\n");
    write("  /temp <value>         Change sampling temperature\n");
    write("  /top-p <value>        Change nucleus sampling threshold\n");
    write("  /top-k <value>        Change top-k sampling value\n");
    write("  /min-p <value>        Change minimum probability filter\n");
    write("  /typical <value>      Change locally typical sampling value\n");
    write("  /n <value>            Change maximum generated tokens\n");
    write("  /ctx <value>          Change the context size\n");
    write("  /save                 Save the session history now\n");
    write("  /exit or /quit        Exit SkiffLLM\n");
    write("  Anything else         Send the text to the local model\n");
}

void Terminal::print_history(const std::vector<ChatMessage>& messages) const {
    if (messages.empty()) {
        warning("No conversation history.");
        return;
    }
    for (const auto& message : messages) {
        const std::string role = message.role == "system" ? "System"
                                : message.role == "user" ? "You"
                                : message.role == "assistant" ? "Assistant"
                                : message.role;
        write(role + ": ", Color::Cyan);
        write(message.content + "\n");
    }
}

}
