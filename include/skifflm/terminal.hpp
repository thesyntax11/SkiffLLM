#pragma once

#include "skifflm/config.hpp"
#include "skifflm/engine.hpp"

#include <string>
#include <vector>

namespace skifflm {

enum class Color {
    Reset,
    Bold,
    Dim,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
};

class Terminal {
public:
    explicit Terminal(const Config& config);

    bool color_enabled() const;
    void use_color(bool enabled);

    std::string paint(const std::string& text, Color color) const;
    void write(const std::string& text, Color color = Color::Reset) const;
    void write_raw(const std::string& text) const;
    void info(const std::string& text) const;
    void success(const std::string& text) const;
    void warning(const std::string& text) const;
    void error(const std::string& text) const;
    void highlight(const std::string& text) const;

    bool read_prompt(const std::string& prompt, std::string& output);
    void print_banner(const std::string& version, const ModelInfo& info, const Config& config);
    void print_stats(const GenerationResult& result, const std::string& label) const;
    void print_help() const;
    void print_history(const std::vector<ChatMessage>& messages) const;

private:
    bool color_;
};

}
