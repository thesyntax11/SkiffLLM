#include "skifflm/engine.hpp"

#include <llama.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <thread>

namespace skifflm {
namespace {

double now_ms() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

uint32_t resolve_seed(uint32_t seed) {
    return seed;
}

}

LlmEngine::LlmEngine(const Config& config) : config_(config) {}

LlmEngine::~LlmEngine() {
    close();
}

bool LlmEngine::load(std::string& error) {
    close();
    error.clear();
    error_.clear();

    std::error_code ec;
    if (!std::filesystem::exists(config_.model_path, ec) ||
        !std::filesystem::is_regular_file(config_.model_path, ec)) {
        error = "model file does not exist: " + config_.model_path.string();
        return false;
    }

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = config_.n_gpu_layers;

    if (config_.use_mmap && config_.use_mlock) {
        model_params.load_mode = LLAMA_LOAD_MODE_MMAP_MLOCK;
    } else if (config_.use_mmap) {
        model_params.load_mode = LLAMA_LOAD_MODE_MMAP;
    } else if (config_.use_mlock) {
        model_params.load_mode = LLAMA_LOAD_MODE_MLOCK;
    } else {
        model_params.load_mode = LLAMA_LOAD_MODE_NONE;
    }

    model_ = llama_model_load_from_file(config_.model_path.c_str(), model_params);
    if (model_ == nullptr) {
        error = "failed to load model from " + config_.model_path.string();
        return false;
    }

    vocab_ = llama_model_get_vocab(model_);

    int threads = config_.n_threads;
    if (threads <= 0) {
        const auto available = std::thread::hardware_concurrency();
        threads = available == 0 ? 4 : static_cast<int>(available);
    }

    llama_context_params context_params = llama_context_default_params();
    context_params.n_ctx = static_cast<uint32_t>(config_.context_size);
    context_params.n_batch = static_cast<uint32_t>(std::min(config_.batch_size, config_.context_size));
    context_params.n_ubatch = std::min(context_params.n_batch, 512u);
    context_params.n_seq_max = 1;
    context_params.n_threads = threads;
    context_params.n_threads_batch = threads;
    context_params.embeddings = false;
    context_params.no_perf = true;

    if (config_.n_gpu_layers != 0) {
        context_params.offload_kqv = true;
    }

    ctx_ = llama_init_from_model(model_, context_params);
    if (ctx_ == nullptr) {
        error = "failed to create the inference context";
        close();
        return false;
    }

    memory_ = llama_get_memory(ctx_);
    if (memory_ == nullptr) {
        error = "failed to acquire context memory";
        close();
        return false;
    }

    char buffer[512] = {0};
    llama_model_desc(model_, buffer, sizeof(buffer));
    info_.description = buffer;

    const char* ftype_name = llama_ftype_name(llama_model_ftype(model_));
    info_.file_type = ftype_name == nullptr ? "unknown" : ftype_name;

    info_.size_bytes = llama_model_size(model_);
    info_.n_params = llama_model_n_params(model_);
    info_.n_ctx_train = llama_model_n_ctx_train(model_);
    info_.n_vocab = llama_vocab_n_tokens(vocab_);

    const char* chat_template = llama_model_chat_template(model_, nullptr);
    info_.chat_template = chat_template == nullptr ? "" : chat_template;

    return true;
}

void LlmEngine::close() {
    if (sampler_ != nullptr) {
        llama_sampler_free(sampler_);
        sampler_ = nullptr;
    }
    if (ctx_ != nullptr) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_ != nullptr) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    vocab_ = nullptr;
    memory_ = nullptr;
}

const ModelInfo& LlmEngine::info() const {
    return info_;
}

std::vector<llama_token> LlmEngine::encode(const std::string& text,
                                           std::string& error) const {
    if (vocab_ == nullptr) {
        error = "model vocabulary is unavailable";
        return {};
    }

    int32_t required = llama_tokenize(vocab_,
                                      text.c_str(),
                                      static_cast<int32_t>(text.size()),
                                      nullptr,
                                      0,
                                      true,
                                      true);
    if (required < 0) {
        required = -required;
    }
    if (required <= 0) {
        error = "failed to tokenize input";
        return {};
    }

    std::vector<llama_token> tokens(static_cast<size_t>(required));
    const int32_t written = llama_tokenize(vocab_,
                                           text.c_str(),
                                           static_cast<int32_t>(text.size()),
                                           tokens.data(),
                                           required,
                                           true,
                                           true);
    if (written < 0) {
        tokens.resize(static_cast<size_t>(-written));
        return tokens;
    }

    tokens.resize(static_cast<size_t>(written));
    return tokens;
}

std::string LlmEngine::token_to_piece(llama_token token) const {
    if (vocab_ == nullptr || token < 0) {
        return {};
    }
    char buffer[512] = {0};
    const int32_t length = llama_token_to_piece(vocab_, token, buffer, sizeof(buffer), 0, false);
    if (length <= 0) {
        return {};
    }
    return std::string(buffer, static_cast<size_t>(length));
}

std::string LlmEngine::build_prompt(const std::vector<ChatMessage>& messages,
                                    std::string& error) const {
    if (messages.empty()) {
        error = "no messages provided";
        return {};
    }

    std::vector<const char*> roles;
    std::vector<const char*> contents;
    roles.reserve(messages.size());
    contents.reserve(messages.size());
    for (const auto& message : messages) {
        roles.push_back(message.role.c_str());
        contents.push_back(message.content.c_str());
    }

    std::vector<llama_chat_message> raw(messages.size());
    for (size_t i = 0; i < messages.size(); ++i) {
        raw[i] = {roles[i], contents[i]};
    }

    std::string tmpl = info_.chat_template.empty() ? "chatml" : info_.chat_template;
    int32_t required = llama_chat_apply_template(tmpl.c_str(),
                                                 raw.data(),
                                                 raw.size(),
                                                 true,
                                                 nullptr,
                                                 0);
    if (required <= 0) {
        tmpl = "chatml";
        required = llama_chat_apply_template(tmpl.c_str(),
                                             raw.data(),
                                             raw.size(),
                                             true,
                                             nullptr,
                                             0);
    }

    if (required <= 0) {
        error = "chat template is not supported by the loaded model";
        return {};
    }

    std::string prompt(static_cast<size_t>(required) + 1, '\0');
    const int32_t written = llama_chat_apply_template(tmpl.c_str(),
                                                      raw.data(),
                                                      raw.size(),
                                                      true,
                                                      &prompt[0],
                                                      static_cast<int32_t>(prompt.size()));
    if (written <= 0) {
        error = "failed to apply the chat template";
        return {};
    }

    prompt.resize(static_cast<size_t>(written));
    return prompt;
}

bool LlmEngine::build_sampler(const GenerationOptions& options, std::string& error) {
    if (vocab_ == nullptr) {
        error = "model vocabulary is unavailable";
        return false;
    }

    if (sampler_ != nullptr) {
        llama_sampler_free(sampler_);
        sampler_ = nullptr;
    }

    llama_sampler_chain_params params = llama_sampler_chain_default_params();
    sampler_ = llama_sampler_chain_init(params);
    if (sampler_ == nullptr) {
        error = "failed to initialize the sampler chain";
        return false;
    }

    if (options.top_k > 0) {
        llama_sampler_chain_add(sampler_, llama_sampler_init_top_k(options.top_k));
    }
    if (options.top_p > 0.0f && options.top_p <= 1.0f) {
        llama_sampler_chain_add(sampler_, llama_sampler_init_top_p(options.top_p, 1));
    }
    if (options.repeat_penalty > 0.0f && options.repeat_penalty != 1.0f) {
        llama_sampler_chain_add(sampler_,
                                llama_sampler_init_penalties(llama_vocab_n_tokens(vocab_),
                                                             options.repeat_last_n,
                                                             options.repeat_penalty,
                                                             0.0f,
                                                             0.0f));
    }
    if (options.temperature > 0.0f) {
        llama_sampler_chain_add(sampler_, llama_sampler_init_temp(options.temperature));
    }
    llama_sampler_chain_add(sampler_, llama_sampler_init_dist(resolve_seed(options.seed)));

    return true;
}

bool LlmEngine::decode(const std::vector<llama_token>& tokens, std::string& error) {
    if (ctx_ == nullptr) {
        error = "inference context is not loaded";
        return false;
    }
    if (tokens.empty()) {
        return true;
    }

    llama_token* mutable_tokens = const_cast<llama_token*>(tokens.data());
    llama_batch batch = llama_batch_get_one(mutable_tokens, static_cast<int32_t>(tokens.size()));
    const int32_t result = llama_decode(ctx_, batch);
    if (result == 1) {
        error = "context window is full; increase --ctx or shorten the conversation";
        return false;
    }
    if (result == 2) {
        error = "generation was aborted";
        return false;
    }
    if (result != 0) {
        error = "llama_decode failed with code " + std::to_string(result);
        return false;
    }
    return true;
}

bool LlmEngine::generate(const std::vector<ChatMessage>& messages,
                         const GenerationOptions& options,
                         GenerationResult& result,
                         const std::function<bool()>& should_stop,
                         std::string& error) {
    error.clear();
    result = GenerationResult{};

    if (model_ == nullptr || ctx_ == nullptr || vocab_ == nullptr) {
        error = "the model is not loaded";
        return false;
    }

    const std::string prompt = build_prompt(messages, error);
    if (prompt.empty()) {
        return false;
    }

    std::vector<llama_token> prompt_tokens = encode(prompt, error);
    if (prompt_tokens.empty()) {
        return false;
    }

    const uint32_t context_size = llama_n_ctx(ctx_);
    if (prompt_tokens.size() >= context_size) {
        error = "prompt exceeds the context window; increase --ctx or shorten the conversation";
        return false;
    }

    if (!build_sampler(options, error)) {
        return false;
    }

    llama_memory_clear(memory_, true);
    llama_sampler_reset(sampler_);

    const double prompt_start = now_ms();
    const uint32_t batch_size = llama_n_batch(ctx_);
    const size_t step = batch_size == 0 ? prompt_tokens.size() : static_cast<size_t>(batch_size);
    size_t offset = 0;
    while (offset < prompt_tokens.size()) {
        const size_t count = std::min(step, prompt_tokens.size() - offset);
        const std::vector<llama_token> chunk(prompt_tokens.begin() + offset,
                                             prompt_tokens.begin() + offset + count);
        if (!decode(chunk, error)) {
            return false;
        }
        offset += count;
    }
    const double prompt_end = now_ms();

    result.prompt_tokens = static_cast<int32_t>(prompt_tokens.size());
    result.prompt_ms = prompt_end - prompt_start;

    const double generation_start = now_ms();
    bool stopped = false;
    int generated = 0;

    while (generated < options.n_predict) {
        if (should_stop && should_stop()) {
            stopped = true;
            break;
        }

        const llama_token token = llama_sampler_sample(sampler_, ctx_, -1);
        if (token < 0) {
            error = "sampler returned an invalid token";
            return false;
        }

        if (llama_vocab_is_eog(vocab_, token)) {
            break;
        }

        std::vector<llama_token> next = {token};
        if (!decode(next, error)) {
            return false;
        }

        const std::string piece = token_to_piece(token);
        if (!piece.empty()) {
            result.text += piece;
            if (options.token_callback) {
                options.token_callback(piece);
            }
        }

        generated += 1;
    }

    const double generation_end = now_ms();
    result.generated_tokens = generated;
    result.generation_ms = generation_end - generation_start;
    result.stopped = stopped;

    if (result.generation_ms > 0.0 && generated > 0) {
        result.tokens_per_second = static_cast<double>(generated) / (result.generation_ms / 1000.0);
    }

    return true;
}

}
