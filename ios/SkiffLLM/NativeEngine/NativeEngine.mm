#import "NativeEngine.h"

#import <dispatch/dispatch.h>

#include <llama.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

double now_ms() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

uint32_t resolve_seed(uint32_t seed) {
    if (seed == 0xFFFFFFFFu) {
        return static_cast<uint32_t>(std::rand());
    }
    return seed;
}

std::string ns_to_utf8(NSString *value) {
    return value == nil ? std::string() : std::string(value.UTF8String ? value.UTF8String : "");
}

NSString *utf8_to_ns(const std::string &value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

}

@implementation LlamaGenerationResult
@end

@implementation NativeEngine {
    llama_model *_model;
    llama_context *_ctx;
    const llama_vocab *_vocab;
    llama_sampler *_sampler;
    std::atomic<bool> _stopping;
    std::string _chatTemplate;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            llama_backend_init();
        });
        _model = nullptr;
        _ctx = nullptr;
        _vocab = nullptr;
        _sampler = nullptr;
        _stopping.store(false);
        _chatTemplate.clear();
    }
    return self;
}

- (void)close {
    if (_sampler) {
        llama_sampler_free(_sampler);
        _sampler = nullptr;
    }
    if (_ctx) {
        llama_free(_ctx);
        _ctx = nullptr;
    }
    if (_model) {
        llama_model_free(_model);
        _model = nullptr;
    }
    _vocab = nullptr;
    _chatTemplate.clear();
}

- (void)dealloc {
    [self close];
}

- (int)contextCapacity {
    return _ctx ? (int)llama_n_ctx(_ctx) : 0;
}

- (NSString *)activeBackends {
    std::string out;
    bool first = true;
    auto add = [&](const char *name) {
        if (!name || !*name) return;
        if (!first) out += ", ";
        first = false;
        out += name;
    };
    add("CPU");
#ifdef GGML_USE_METAL
    add("Metal");
#endif
#ifdef GGML_USE_BLAS
    add("BLAS");
#endif
    return utf8_to_ns(out.empty() ? "CPU" : out);
}

- (NSString *)modelDescription {
    if (!_model) return @"";
    char buffer[4096] = {0};
    int32_t written = llama_model_desc(_model, buffer, sizeof(buffer));
    std::string desc = written > 0 ? std::string(buffer, (size_t)written) : "";
    uint64_t params = llama_model_n_params(_model);
    int32_t ctx = llama_model_n_ctx_train(_model);
    std::string out = desc;
    if (!out.empty() && out.back() == '\n') out.pop_back();
    std::ostringstream info;
    if (params >= 1000000000ULL) {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "%.1fB params", (double)params / 1000000000.0);
        info << buf;
    } else if (params > 0) {
        info << (params / 1000000ULL) << "M params";
    }
    if (ctx > 0) info << " · ctx " << ctx;
    if (!out.empty() && !info.str().empty()) {
        out += " · ";
    }
    out += info.str();
    return [NSString stringWithUTF8String:out.c_str()];
}

- (BOOL)loadModelAtPath:(NSString *)path
            contextSize:(int)contextSize
               threads:(int)threads
             gpuLayers:(int)gpuLayers
           chatTemplate:(NSString *)chatTemplate
                 error:(NSError **)error {
    [self close];
    _stopping.store(false);

    std::string modelPath = ns_to_utf8(path);
    if (modelPath.empty()) {
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:1
                                     userInfo:@{NSLocalizedDescriptionKey: @"Empty model path"}];
        }
        return NO;
    }

    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = gpuLayers;
    mp.load_mode = LLAMA_LOAD_MODE_MMAP;

    _model = llama_model_load_from_file(modelPath.c_str(), mp);
    if (!_model) {
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:2
                                     userInfo:@{NSLocalizedDescriptionKey: @"Failed to load GGUF model"}];
        }
        return NO;
    }

    _vocab = llama_model_get_vocab(_model);

    int nThreads = threads > 0 ? threads : (int)std::max(1u, std::thread::hardware_concurrency());

    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = (uint32_t)std::max(64, contextSize);
    cp.n_batch = (uint32_t)std::max(1, std::min(512, (int)cp.n_ctx));
    cp.n_ubatch = std::min(cp.n_batch, (uint32_t)512);
    cp.n_seq_max = 1;
    cp.n_threads = nThreads;
    cp.n_threads_batch = nThreads;
    cp.embeddings = false;
    cp.no_perf = true;
    cp.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_AUTO;
    cp.offload_kqv = true;

    _ctx = llama_init_from_model(_model, cp);
    if (!_ctx) {
        [self close];
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:3
                                     userInfo:@{NSLocalizedDescriptionKey: @"Failed to create inference context"}];
        }
        return NO;
    }

    // Use the template chosen by the caller, or fall back to the embedded/model default.
    _chatTemplate = ns_to_utf8(chatTemplate);
    return YES;
}

- (BOOL)buildSamplerWithTemperature:(float)temperature
                                topP:(float)topP
                                topK:(int)topK
                                minP:(float)minP
                             typicalP:(float)typicalP
                       repeatPenalty:(float)repeatPenalty
                       repeatLastN:(int)repeatLastN
                            seed:(uint32_t)seed
                            error:(NSError **)error {
    if (_sampler) {
        llama_sampler_free(_sampler);
        _sampler = nullptr;
    }
    if (!_vocab) {
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:4
                                     userInfo:@{NSLocalizedDescriptionKey: @"Vocabulary unavailable"}];
        }
        return NO;
    }

    llama_sampler_chain_params cp = llama_sampler_chain_default_params();
    _sampler = llama_sampler_chain_init(cp);
    if (!_sampler) {
        return NO;
    }

    if (topK > 0) {
        llama_sampler_chain_add(_sampler, llama_sampler_init_top_k(topK));
    }
    if (topP > 0.0f && topP <= 1.0f) {
        llama_sampler_chain_add(_sampler, llama_sampler_init_top_p(topP, 1));
    }
    if (minP > 0.0f && minP <= 1.0f) {
        llama_sampler_chain_add(_sampler, llama_sampler_init_min_p(minP, 1));
    }
    if (typicalP > 0.0f && typicalP <= 1.0f) {
        llama_sampler_chain_add(_sampler, llama_sampler_init_typical(typicalP, 1));
    }
    if (repeatPenalty > 0.0f && repeatPenalty != 1.0f) {
        llama_sampler_chain_add(_sampler,
                                llama_sampler_init_penalties(llama_vocab_n_tokens(_vocab),
                                                             repeatLastN,
                                                             repeatPenalty,
                                                             0.0f,
                                                             0.0f));
    }
    if (temperature > 0.0f) {
        llama_sampler_chain_add(_sampler, llama_sampler_init_temp(temperature));
        llama_sampler_chain_add(_sampler, llama_sampler_init_dist(resolve_seed(seed)));
    } else {
        llama_sampler_chain_add(_sampler, llama_sampler_init_greedy());
    }
    return YES;
}

- (std::vector<llama_token>)encode:(NSString *)text
                             error:(NSError **)error {
    std::vector<llama_token> tokens;
    if (!_vocab) {
        return tokens;
    }
    std::string content = ns_to_utf8(text);
    int32_t required = llama_tokenize(_vocab, content.c_str(), (int32_t)content.size(),
                                      nullptr, 0, true, true);
    if (required < 0) required = -required;
    if (required <= 0) return tokens;
    tokens.resize((size_t)required);
    int32_t written = llama_tokenize(_vocab, content.c_str(), (int32_t)content.size(),
                                     tokens.data(), required, true, true);
    if (written < 0) {
        tokens.resize((size_t)(-written));
        return tokens;
    }
    tokens.resize((size_t)written);
    return tokens;
}

- (BOOL)decode:(const std::vector<llama_token> &)tokens
         error:(NSError **)error {
    if (!_ctx) {
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:5
                                     userInfo:@{NSLocalizedDescriptionKey: @"No inference context"}];
        }
        return NO;
    }
    if (tokens.empty()) return YES;
    llama_token *ptr = const_cast<llama_token *>(tokens.data());
    llama_batch batch = llama_batch_get_one(ptr, (int32_t)tokens.size());
    if (llama_decode(_ctx, batch) != 0) {
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:6
                                     userInfo:@{NSLocalizedDescriptionKey: @"llama_decode failed"}];
        }
        return NO;
    }
    return YES;
}

- (BOOL)warmup:(NSError **)error {
    if (!_model || !_vocab || !_ctx) {
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:11
                                     userInfo:@{NSLocalizedDescriptionKey: @"No model loaded"}];
        }
        return NO;
    }
    NSError *local = nil;
    std::vector<llama_token> tokens = [self encode:@"Hello" error:&local];
    if (local) tokens.clear();
    if (tokens.empty()) {
        if (error) {
            *error = local ?: [NSError errorWithDomain:@"SkiffLLM" code:12
                                              userInfo:@{NSLocalizedDescriptionKey: @"Warm-up prompt failed"}];
        }
        return NO;
    }
    if (![self decode:tokens error:&local]) {
        if (error) *error = local;
        return NO;
    }
    if (llama_memory_t mem = llama_get_memory(_ctx)) {
        llama_memory_clear(mem, true);
    }
    return YES;
}

- (std::string)buildPromptForMessages:(NSArray<NSDictionary<NSString *, NSString *> *> *)messages
                                error:(NSError **)error {
    if (messages.count == 0) {
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:7
                                     userInfo:@{NSLocalizedDescriptionKey: @"No messages"}];
        }
        return {};
    }

    std::string tmplStr = _chatTemplate;
    if (tmplStr.empty()) {
        const char *tmpl = llama_model_chat_template(_model, nullptr);
        tmplStr = tmpl ? tmpl : "";
    }
    if (tmplStr.empty()) tmplStr = "chatml";

    std::vector<const char *> roles;
    std::vector<const char *> contents;
    std::vector<std::string> roleStorage;
    std::vector<std::string> contentStorage;
    roleStorage.reserve(messages.count);
    contentStorage.reserve(messages.count);
    for (NSDictionary<NSString *, NSString *> *item in messages) {
        NSString *role = item[@"role"] ?: @"user";
        NSString *content = item[@"content"] ?: @"";
        roleStorage.push_back(ns_to_utf8(role));
        contentStorage.push_back(ns_to_utf8(content));
    }
    for (size_t i = 0; i < roleStorage.size(); ++i) {
        roles.push_back(roleStorage[i].c_str());
        contents.push_back(contentStorage[i].c_str());
    }

    std::vector<llama_chat_message> raw(messages.count);
    for (size_t i = 0; i < roles.size(); ++i) {
        raw[i] = {roles[i], contents[i]};
    }

    int32_t required = llama_chat_apply_template(tmplStr.c_str(), raw.data(), raw.size(), true,
                                                 nullptr, 0);
    if (required <= 0) {
        required = llama_chat_apply_template("chatml", raw.data(), raw.size(), true, nullptr, 0);
        tmplStr = "chatml";
    }
    if (required <= 0) {
        if (error) {
            *error = [NSError errorWithDomain:@"SkiffLLM" code:8
                                     userInfo:@{NSLocalizedDescriptionKey: @"Chat template failed"}];
        }
        return {};
    }
    std::string prompt((size_t)required + 1, '\0');
    int32_t written = llama_chat_apply_template(tmplStr.c_str(), raw.data(), raw.size(), true,
                                                &prompt[0], (int32_t)prompt.size());
    if (written <= 0) return {};
    prompt.resize((size_t)written);
    return prompt;
}

- (void)deliverCompletionResult:(LlamaGenerationResult *_Nullable)result
                          error:(NSError *_Nullable)error
                    completion:(void (^_Nullable)(LlamaGenerationResult *_Nullable result,
                                                  NSError *_Nullable error))completion {
    if (!completion) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        completion(result, error);
    });
}

- (void)generateMessages:(NSArray<NSDictionary<NSString *, NSString *> *> *)messages
              temperature:(float)temperature
                    topP:(float)topP
                    topK:(int)topK
                    minP:(float)minP
                 typicalP:(float)typicalP
           repeatPenalty:(float)repeatPenalty
           repeatLastN:(int)repeatLastN
              maxTokens:(int)maxTokens
                    seed:(uint32_t)seed
           stopSequences:(NSArray<NSString *> *)stopSequences
           tokenCallback:(void (^_Nullable)(NSString *_Nullable token))tokenCallback
             completion:(void (^_Nullable)(LlamaGenerationResult *_Nullable result,
                                           NSError *_Nullable error))completion {
    _stopping.store(false);
    if (!_model || !_vocab || !_ctx) {
        NSError *e = [NSError errorWithDomain:@"SkiffLLM" code:10
                                     userInfo:@{NSLocalizedDescriptionKey: @"No model loaded"}];
        [self deliverCompletionResult:nil error:e completion:completion];
        return;
    }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *localError = nil;
        uint32_t capacity = _ctx ? llama_n_ctx(_ctx) : 0;
        uint32_t generationReserve = capacity > 128 ? 64 : 1;
        uint32_t maxPrompt = capacity > generationReserve ? capacity - generationReserve : capacity;

        NSMutableArray *workingMessages = [messages mutableCopy];
        std::vector<llama_token> prompt;
        std::string promptText;

        while (true) {
            promptText = [self buildPromptForMessages:workingMessages error:&localError];
            if (localError) {
                [self deliverCompletionResult:nil error:localError completion:completion];
                return;
            }
            prompt = [self encode:utf8_to_ns(promptText) error:&localError];
            if (localError) {
                [self deliverCompletionResult:nil error:localError completion:completion];
                return;
            }
            if ((uint32_t)prompt.size() <= maxPrompt || workingMessages.count <= 2) {
                break;
            }
            // Auto-trim: drop the oldest non-system message and rebuild.
            NSUInteger removeIndex = [[workingMessages.firstObject objectForKey:@"role"] isEqual:@"system"] ? 1 : 0;
            if (removeIndex >= workingMessages.count) break;
            [workingMessages removeObjectAtIndex:removeIndex];
        }

        if ((uint32_t)prompt.size() > maxPrompt) {
            NSError *e = [NSError errorWithDomain:@"SkiffLLM" code:9
                                         userInfo:@{NSLocalizedDescriptionKey:
                                             @"Prompt still exceeds the context window after trimming; increase Context in Settings or shorten the conversation."}];
            [self deliverCompletionResult:nil error:e completion:completion];
            return;
        }

        if (![self buildSamplerWithTemperature:temperature
                                         topP:topP
                                         topK:topK
                                         minP:minP
                                      typicalP:typicalP
                                repeatPenalty:repeatPenalty
                                repeatLastN:repeatLastN
                                     seed:seed
                                     error:&localError]) {
            [self deliverCompletionResult:nil error:localError completion:completion];
            return;
        }

        if (llama_memory_t mem = llama_get_memory(_ctx)) {
            llama_memory_clear(mem, true);
        }
        llama_sampler_reset(_sampler);

        const double promptStart = now_ms();
        uint32_t nBatch = _ctx ? llama_n_batch(_ctx) : 0;
        if (nBatch == 0) nBatch = (uint32_t)prompt.size();
        size_t offset = 0;
        while (offset < prompt.size()) {
            size_t count = std::min((size_t)nBatch, prompt.size() - offset);
            std::vector<llama_token> chunk(prompt.begin() + offset, prompt.begin() + offset + count);
            if (![self decode:chunk error:&localError]) {
                [self deliverCompletionResult:nil error:localError completion:completion];
                return;
            }
            offset += count;
        }
        const double promptEnd = now_ms();

        int32_t promptTokens = (int32_t)prompt.size();
        int32_t target = std::min(maxTokens > 0 ? maxTokens : 512, (int)maxPrompt - (int)prompt.size());
        if (target < 1) target = 0;

        std::string output;
        bool stopped = false;
        int generated = 0;
        const double generationStart = now_ms();

        while (generated < target) {
            if (_stopping.load()) {
                stopped = true;
                break;
            }
            llama_token token = llama_sampler_sample(_sampler, _ctx, -1);
            if (token < 0) break;
            if (llama_vocab_is_eog(_vocab, token)) break;

            std::vector<llama_token> next = {token};
            if (![self decode:next error:&localError]) {
                [self deliverCompletionResult:nil error:localError completion:completion];
                return;
            }
            char buf[512] = {0};
            int32_t len = llama_token_to_piece(_vocab, token, buf, sizeof(buf), 0, false);
            if (len > 0) {
                std::string piece(buf, (size_t)len);
                output += piece;
                if (tokenCallback) {
                    tokenCallback(utf8_to_ns(piece));
                }
            }
            for (NSString *rawStop in stopSequences) {
                std::string stop = ns_to_utf8(rawStop);
                if (stop.empty() || output.size() < stop.size()) continue;
                if (output.compare(output.size() - stop.size(), stop.size(), stop) == 0) {
                    output.resize(output.size() - stop.size());
                    stopped = true;
                    break;
                }
            }
            if (stopped) break;
            ++generated;
        }
        const double generationEnd = now_ms();

        LlamaGenerationResult *result = [[LlamaGenerationResult alloc] init];
        result.text = utf8_to_ns(output);
        result.promptTokens = promptTokens;
        result.generatedTokens = generated;
        result.promptMs = promptEnd - promptStart;
        result.generationMs = generationEnd - generationStart;
        result.tokensPerSecond = generated > 0 && result.generationMs > 0
            ? (double)generated / (result.generationMs / 1000.0)
            : 0.0;
        result.stopped = stopped;

        if (completion) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(result, nil);
            });
        }
    });
}

- (void)stop {
    _stopping.store(true);
}

@end
