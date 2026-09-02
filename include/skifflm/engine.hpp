#pragma once

#include <llama.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "skifflm/config.hpp"
#include "skifflm/messages.hpp"

namespace skifflm {

struct ModelInfo {
    std::string description;
    std::string file_type;
    std::string chat_template;
    uint64_t size_bytes = 0;
    uint64_t n_params = 0;
    int32_t n_ctx_train = 0;
    int32_t n_vocab = 0;
};

struct GenerationOptions {
    int n_predict = 512;
    float temperature = 0.7f;
    float top_p = 0.95f;
    float min_p = 0.0f;
    float typical_p = 0.0f;
    int top_k = 40;
    float repeat_penalty = 1.1f;
    int repeat_last_n = 64;
    uint32_t seed = 0xFFFFFFFFu;
    bool auto_trim = true;
    int reserve_ctx = 0;
    int n_keep = 0;
    std::vector<std::string> stop_sequences;
    std::function<void(const std::string&)> token_callback;
};

struct GenerationResult {
    std::string text;
    int32_t prompt_tokens = 0;
    int32_t generated_tokens = 0;
    double prompt_ms = 0.0;
    double generation_ms = 0.0;
    double tokens_per_second = 0.0;
    bool stopped = false;
};

class LlmEngine {
   public:
    explicit LlmEngine(const Config& config);
    ~LlmEngine();

    LlmEngine(const LlmEngine&) = delete;
    LlmEngine& operator=(const LlmEngine&) = delete;

    bool load(std::string& error);
    void close();

    const ModelInfo& info() const;
    uint32_t context_capacity() const;
    // Human-readable list of the backends linked into this build
    // (CPU, CUDA, Metal, Vulkan, OpenCL, BLAS...). Empty when unavailable.
    std::string active_backends() const;
    bool tokenize(const std::string& text, std::vector<int32_t>& tokens, std::string& error) const;
    bool generate(const std::vector<ChatMessage>& messages, const GenerationOptions& options,
                  GenerationResult& result, const std::function<bool()>& should_stop,
                  std::string& error);
    bool warmup(std::string& error);

   private:
    std::vector<llama_token> encode(const std::string& text, std::string& error) const;
    std::string token_to_piece(llama_token token) const;
    std::string build_prompt(const std::vector<ChatMessage>& messages, std::string& error) const;
    bool build_sampler(const GenerationOptions& options, std::string& error);
    bool decode(const std::vector<llama_token>& tokens, std::string& error);

    const Config& config_;
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    const llama_vocab* vocab_ = nullptr;
    llama_sampler* sampler_ = nullptr;
    llama_memory_t memory_ = nullptr;
    ModelInfo info_;
    std::string error_;
};

}  // namespace skifflm
