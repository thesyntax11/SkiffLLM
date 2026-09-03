#pragma once

#include <string>
#include <vector>

#include "skiffllm/config.hpp"
#include "skiffllm/messages.hpp"

namespace skiffllm {

class Session {
   public:
    explicit Session(const Config& config);

    bool load(std::string& error);
    bool save(std::string& error) const;

    void set_system_prompt(const std::string& prompt);
    const std::string& system_prompt() const;
    std::vector<ChatMessage>& messages();
    const std::vector<ChatMessage>& messages() const;
    std::vector<ChatMessage> conversation() const;

    bool empty() const;
    size_t message_count() const;
    const Config& config() const;

   private:
    const Config& config_;
    std::string system_prompt_;
    std::vector<ChatMessage> messages_;
};

}  // namespace skiffllm
