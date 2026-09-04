#include "skiffllm/engine.hpp"

#include <llama.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <random>
#include <sstream>
#include <thread>

namespace skiffllm {
namespace {

double now_ms() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

uint32_t resolve_seed(uint32_t seed) {
    if (seed == 0xFFFFFFFFu) {
        std::random_device rd;
        const uint32_t value = rd();
        return value == 0xFFFFFFFFu ? 0x9E3779B9u : value;
    }
    return seed;
}

}

SkiffEngine::SkiffEngine(const Config& config) : config_(config) {}

SkiffEngine::~SkiffEngine() {
    close();
}

bool SkiffEngine::load(std::string& error) {
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

    if (config_.numa) {
        static bool numa_initialized = false;
        if (!numa_initialized) {
            llama_numa_init(GGML_NUMA_STRATEGY_DISTRIBUTE);
            numa_initialized = true;
        }
    }

    model_ = llama_model_load_from_file(config_.model_path.string().c_str(), model_params);
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
    context_params.n_batch =
        static_cast<uint32_t>(std::min(config_.batch_size, config_.context_size));
    const uint32_t ubatch = config_.n_ubatch > 0 ? static_cast<uint32_t>(config_.n_ubatch)
                                                 : std::min(context_params.n_batch, 512u);
    context_params.n_ubatch = std::min(ubatch, context_params.n_batch);
    context_params.n_seq_max = 1;
    context_params.n_threads = threads;
    context_params.n_threads_batch = threads;
    context_params.embeddings = false;
    context_params.no_perf = true;
    context_params.flash_attn_type =
        config_.flash_attn ? LLAMA_FLASH_ATTN_TYPE_ENABLED : LLAMA_FLASH_ATTN_TYPE_DISABLED;
    context_params.offload_kqv = config_.offload_kqv;

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
    if (!config_.chat_template.empty()) {
        info_.chat_template = config_.chat_template;
    }

    return true;
}

void SkiffEngine::close() {
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

const ModelInfo& SkiffEngine::info() const {
    return info_;
}

uint32_t SkiffEngine::context_capacity() const {
    return ctx_ == nullptr ? 0 : llama_n_ctx(ctx_);
}

std::string SkiffEngine::active_backends() const {
    std::ostringstream out;
    bool first = true;
    const auto add = [&](const char* name) {
        if (name == nullptr || name[0] == '\0') {
            return;
        }
        if (!first) {
            out << ", ";
        }
        first = false;
        out << name;
    };
    add("CPU");
#ifdef GGML_USE_CUDA
    add("CUDA");
#endif
#ifdef GGML_USE_CUBLAS
    add("CUDA");
#endif
#ifdef GGML_USE_METAL
    add("Metal");
#endif
#ifdef GGML_USE_VULKAN
    add("Vulkan");
#endif
#ifdef GGML_USE_OPENCL
    add("OpenCL");
#endif
#ifdef GGML_USE_BLAS
    add("BLAS");
#endif
#ifdef GGML_USE_RPC
    add("RPC");
#endif
    return out.str();
}

bool SkiffEngine::tokenize(const std::string& text, std::vector<int32_t>& tokens,
                           std::string& error) const {
    const std::vector<llama_token> encoded = encode(text, error);
    if (encoded.empty()) {
        return false;
    }
    tokens.clear();
    tokens.reserve(encoded.size());
    for (const llama_token token : encoded) {
        tokens.push_back(static_cast<int32_t>(token));
    }
    return true;
}

std::vector<llama_token> SkiffEngine::encode(const std::string& text, std::string& error) const {
    if (vocab_ == nullptr) {
        error = "model vocabulary is unavailable";
        return {};
    }

    int32_t required = llama_tokenize(vocab_, text.c_str(), static_cast<int32_t>(text.size()),
                                      nullptr, 0, true, true);
    if (required < 0) {
        required = -required;
    }
    if (required <= 0) {
        error = "failed to tokenize input";
        return {};
    }

    std::vector<llama_token> tokens(static_cast<size_t>(required));
    const int32_t written = llama_tokenize(vocab_, text.c_str(), static_cast<int32_t>(text.size()),
                                           tokens.data(), required, true, true);
    if (written < 0) {
        tokens.resize(static_cast<size_t>(-written));
        return tokens;
    }

    tokens.resize(static_cast<size_t>(written));
    return tokens;
}

std::string SkiffEngine::token_to_piece(llama_token token) const {
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

std::string SkiffEngine::build_prompt(const std::vector<ChatMessage>& messages,
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
    int32_t required =
        llama_chat_apply_template(tmpl.c_str(), raw.data(), raw.size(), true, nullptr, 0);
    if (required <= 0) {
        tmpl = "chatml";
        required =
            llama_chat_apply_template(tmpl.c_str(), raw.data(), raw.size(), true, nullptr, 0);
    }

    if (required <= 0) {
        error = "chat template is not supported by the loaded model";
        return {};
    }

    std::string prompt(static_cast<size_t>(required) + 1, '\0');
    const int32_t written =
        llama_chat_apply_template(tmpl.c_str(), raw.data(), raw.size(), true, &prompt[0],
                                  static_cast<int32_t>(prompt.size()));
    if (written <= 0) {
        error = "failed to apply the chat template";
        return {};
    }

    prompt.resize(static_cast<size_t>(written));
    return prompt;
}

bool SkiffEngine::build_sampler(const GenerationOptions& options, std::string& error) {
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
    if (options.min_p > 0.0f && options.min_p <= 1.0f) {
        llama_sampler_chain_add(sampler_, llama_sampler_init_min_p(options.min_p, 1));
    }
    if (options.typical_p > 0.0f && options.typical_p <= 1.0f) {
        llama_sampler_chain_add(sampler_, llama_sampler_init_typical(options.typical_p, 1));
    }
    if (options.repeat_penalty > 0.0f && options.repeat_penalty != 1.0f) {
        llama_sampler_chain_add(sampler_,
                                llama_sampler_init_penalties(llama_vocab_n_tokens(vocab_),
                                                             options.repeat_last_n,
                                                             options.repeat_penalty, 0.0f, 0.0f));
    }
    if (options.temperature > 0.0f) {
        llama_sampler_chain_add(sampler_, llama_sampler_init_temp(options.temperature));
        llama_sampler_chain_add(sampler_, llama_sampler_init_dist(resolve_seed(options.seed)));
    } else {
        llama_sampler_chain_add(sampler_, llama_sampler_init_greedy());
    }

    return true;
}

bool SkiffEngine::decode(const std::vector<llama_token>& tokens, std::string& error) {
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

bool SkiffEngine::generate(const std::vector<ChatMessage>& messages,
                           const GenerationOptions& options, GenerationResult& result,
                           const std::function<bool()>& should_stop, std::string& error) {
    error.clear();
    result = GenerationResult{};

    if (model_ == nullptr || ctx_ == nullptr || vocab_ == nullptr) {
        error = "the model is not loaded";
        return false;
    }

    std::vector<ChatMessage> active_messages = messages;
    std::string prompt;
    std::vector<llama_token> prompt_tokens;

    auto encode_active = [&]() -> bool {
        const std::string candidate = build_prompt(active_messages, error);
        if (candidate.empty()) {
            return false;
        }
        std::vector<llama_token> tokens = encode(candidate, error);
        if (tokens.empty()) {
            return false;
        }
        prompt = std::move(candidate);
        prompt_tokens = std::move(tokens);
        return true;
    };

    if (!encode_active()) {
        return false;
    }

    const uint32_t context_size = llama_n_ctx(ctx_);
    uint32_t max_prompt_tokens = context_size;
    if (options.reserve_ctx > 0) {
        const uint32_t reserve = static_cast<uint32_t>(options.reserve_ctx);
        max_prompt_tokens = context_size > reserve ? context_size - reserve : 1;
    }

    if (prompt_tokens.size() > max_prompt_tokens) {
        if (!options.auto_trim || active_messages.size() <= 1) {
            error =
                "prompt exceeds the context window; increase --ctx, raise --reserve-ctx or shorten "
                "the conversation";
            return false;
        }
        const size_t minimum_messages =
            options.n_keep > 0
                ? static_cast<size_t>(options.n_keep * 2 +
                                      (active_messages.front().role == "system" ? 1 : 0))
                : 2;
        while (prompt_tokens.size() > max_prompt_tokens &&
               active_messages.size() > minimum_messages) {
            const size_t remove_index = active_messages[0].role == "system" ? 1 : 0;
            active_messages.erase(active_messages.begin() +
                                  static_cast<std::ptrdiff_t>(remove_index));
            if (!encode_active()) {
                return false;
            }
        }
        if (prompt_tokens.size() > max_prompt_tokens) {
            error =
                "prompt still exceeds the context window after trimming; increase --ctx or shorten "
                "the conversation";
            return false;
        }
    }

    const uint32_t generation_capacity = context_size - static_cast<uint32_t>(prompt_tokens.size());
    const int generation_target =
        std::min(options.n_predict, static_cast<int>(generation_capacity));
    if (generation_target < 1) {
        error = "no room for generation; increase --ctx or shorten the conversation";
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

    while (generated < generation_target) {
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

        bool matched_stop = false;
        for (const auto& stop : options.stop_sequences) {
            if (stop.empty() || result.text.size() < stop.size()) {
                continue;
            }
            const size_t start = result.text.size() - stop.size();
            if (result.text.compare(start, stop.size(), stop) == 0) {
                result.text.erase(start);
                matched_stop = true;
                break;
            }
        }
        if (matched_stop) {
            break;
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

bool SkiffEngine::warmup(std::string& error) {
    if (model_ == nullptr || ctx_ == nullptr || vocab_ == nullptr) {
        error = "the model is not loaded";
        return false;
    }

    std::vector<ChatMessage> ask;
    ask.push_back({"user", "Warmup."});

    GenerationOptions warm;
    warm.n_predict = 1;
    warm.temperature = 0.0f;
    warm.top_p = 1.0f;
    warm.top_k = 1;
    warm.min_p = 0.0f;
    warm.typical_p = 0.0f;
    warm.repeat_penalty = 1.0f;
    warm.repeat_last_n = 0;
    warm.seed = 0xFFFFFFFFu;
    warm.auto_trim = false;
    warm.reserve_ctx = 0;
    warm.token_callback = nullptr;
    warm.stop_sequences.clear();

    GenerationResult result;
    return generate(ask, warm, result, []() { return false; }, error);
}

}
