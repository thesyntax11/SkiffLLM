#include <jni.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "skiffllm/config.hpp"
#include "skiffllm/engine.hpp"
#include "skiffllm/skills.hpp"
#include "skiffllm/tools.hpp"

namespace {

struct NativeState {
    skiffllm::Config config;
    std::unique_ptr<skiffllm::SkiffEngine> engine;
    std::atomic<bool> stop_requested{false};
    std::string last_error;
};

NativeState* state_from(jlong handle) {
    return reinterpret_cast<NativeState*>(handle);
}

std::string to_string(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

std::string to_string(JNIEnv* env, jobject value) {
    if (value == nullptr) {
        return {};
    }
    auto string_value = static_cast<jstring>(value);
    return to_string(env, string_value);
}

jstring to_jstring(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
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
                    char buffer[8] = {};
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

std::filesystem::path storage_root(std::string root) {
    if (root.empty()) {
        return std::filesystem::temp_directory_path() / "skiffllm";
    }
    return std::filesystem::path(root);
}

std::map<std::string, std::string> parse_args_json(const std::string& input) {
    std::map<std::string, std::string> args;
    size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && (input[i] == ' ' || input[i] == '{' || input[i] == ',')) {
            ++i;
        }
        if (i >= input.size()) {
            break;
        }
        if (input[i] != '"') {
            ++i;
            continue;
        }
        ++i;
        std::string key;
        while (i < input.size() && input[i] != '"') {
            key += input[i++];
        }
        ++i;
        while (i < input.size() && (input[i] == ' ' || input[i] == ':')) {
            ++i;
        }
        if (i >= input.size()) {
            break;
        }
        std::string value;
        if (input[i] == '"') {
            ++i;
            while (i < input.size() && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < input.size()) {
                    ++i;
                }
                value += input[i++];
            }
            ++i;
        } else {
            while (i < input.size() && input[i] != ',' && input[i] != '}') {
                value += input[i++];
            }
        }
        if (!key.empty()) {
            args[key] = value;
        }
    }
    return args;
}

}

extern "C" JNIEXPORT jlong JNICALL Java_com_skiffllm_app_SkiffNative_create(
    JNIEnv* env, jobject, jint context_size, jint threads, jint gpu_layers, jstring chat_template,
    jstring storage_root_value) {
    auto state = std::make_unique<NativeState>();
    state->config = skiffllm::default_config();
    state->config.model_path.clear();
    state->config.context_size = static_cast<int>(context_size);
    state->config.n_threads = static_cast<int>(threads);
    state->config.n_gpu_layers = static_cast<int>(gpu_layers);
    state->config.chat_template = to_string(env, chat_template);
    const auto root = storage_root(to_string(env, storage_root_value));
    state->config.history_path = root / "history.skif";
    state->config.memory_path = root / "memories.txt";
    return reinterpret_cast<jlong>(state.release());
}

extern "C" JNIEXPORT void JNICALL Java_com_skiffllm_app_SkiffNative_destroy(JNIEnv*, jobject,
                                                                            jlong handle) {
    delete state_from(handle);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_skiffllm_app_SkiffNative_load(JNIEnv* env, jobject,
                                                                             jlong handle,
                                                                             jstring model_path) {
    NativeState* state = state_from(handle);
    if (state == nullptr) {
        return JNI_FALSE;
    }
    state->config.model_path = to_string(env, model_path);
    state->stop_requested = false;
    state->last_error.clear();
    auto engine = std::make_unique<skiffllm::SkiffEngine>(state->config);
    std::string error;
    if (!engine->load(error)) {
        state->last_error = error;
        return JNI_FALSE;
    }
    state->engine = std::move(engine);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_skiffllm_app_SkiffNative_loadError(JNIEnv* env,
                                                                                 jobject,
                                                                                 jlong handle) {
    NativeState* state = state_from(handle);
    if (state == nullptr) {
        return to_jstring(env, "engine is not initialized");
    }
    if (state->engine != nullptr) {
        return to_jstring(env, "model is loaded");
    }
    if (!state->last_error.empty()) {
        return to_jstring(env, state->last_error);
    }
    return to_jstring(env, "model is not loaded");
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_skiffllm_app_SkiffNative_warmup(JNIEnv*, jobject,
                                                                               jlong handle) {
    NativeState* state = state_from(handle);
    if (state == nullptr || state->engine == nullptr) {
        return JNI_FALSE;
    }
    std::string error;
    if (!state->engine->warmup(error)) {
        state->last_error = error;
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_skiffllm_app_SkiffNative_infoJson(JNIEnv* env,
                                                                                jobject,
                                                                                jlong handle) {
    NativeState* state = state_from(handle);
    if (state == nullptr || state->engine == nullptr) {
        return nullptr;
    }
    const auto& info = state->engine->info();
    std::ostringstream out;
    out << "{";
    out << "\"model\":\"" << state->config.model_path.string() << "\",";
    out << "\"description\":\"" << info.description << "\",";
    out << "\"file_type\":\"" << info.file_type << "\",";
    out << "\"params\":" << info.n_params << ",";
    out << "\"size_bytes\":" << info.size_bytes << ",";
    out << "\"context_train\":" << info.n_ctx_train << ",";
    out << "\"vocab_size\":" << info.n_vocab;
    out << "}";
    return to_jstring(env, out.str());
}

extern "C" JNIEXPORT void JNICALL Java_com_skiffllm_app_SkiffNative_stop(JNIEnv*, jobject,
                                                                         jlong handle) {
    NativeState* state = state_from(handle);
    if (state != nullptr) {
        state->stop_requested = true;
    }
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_skiffllm_app_SkiffNative_generate(
    JNIEnv* env, jobject, jlong handle, jobjectArray roles, jobjectArray contents,
    jfloat temperature, jfloat top_p, jint top_k, jfloat min_p, jfloat typical_p,
    jfloat repeat_penalty, jint repeat_last_n, jint max_tokens, jlong seed,
    jobjectArray stop_sequences, jobject callback) {
    NativeState* state = state_from(handle);

    jclass callback_class = env->GetObjectClass(callback);
    if (callback_class == nullptr) {
        return JNI_FALSE;
    }
    jmethodID on_token = env->GetMethodID(callback_class, "onToken", "(Ljava/lang/String;)V");
    jmethodID on_done = env->GetMethodID(callback_class, "onDone", "(IIDDDZ)V");
    jmethodID on_error = env->GetMethodID(callback_class, "onError", "(Ljava/lang/String;)V");
    if (on_token == nullptr || on_done == nullptr || on_error == nullptr) {
        env->DeleteLocalRef(callback_class);
        return JNI_FALSE;
    }

    if (state == nullptr || state->engine == nullptr || roles == nullptr || contents == nullptr) {
        const char* message = state == nullptr           ? "engine is not initialized"
                              : state->engine == nullptr ? "model is not loaded"
                                                         : "invalid generation arguments";
        jstring text = env->NewStringUTF(message);
        if (text != nullptr) {
            env->CallVoidMethod(callback, on_error, text);
            env->DeleteLocalRef(text);
        }
        env->DeleteLocalRef(callback_class);
        return JNI_FALSE;
    }

    const jsize count = env->GetArrayLength(roles);
    if (count != env->GetArrayLength(contents)) {
        jstring text = env->NewStringUTF("invalid generation arguments");
        if (text != nullptr) {
            env->CallVoidMethod(callback, on_error, text);
            env->DeleteLocalRef(text);
        }
        env->DeleteLocalRef(callback_class);
        return JNI_FALSE;
    }

    std::vector<skiffllm::ChatMessage> messages;
    messages.reserve(static_cast<size_t>(count));
    for (jsize i = 0; i < count; ++i) {
        jobject role = env->GetObjectArrayElement(roles, i);
        jobject content = env->GetObjectArrayElement(contents, i);
        messages.push_back({to_string(env, role), to_string(env, content)});
        env->DeleteLocalRef(role);
        env->DeleteLocalRef(content);
    }

    std::vector<std::string> stop_sequence_values;
    if (stop_sequences != nullptr) {
        const jsize stop_count = env->GetArrayLength(stop_sequences);
        stop_sequence_values.reserve(static_cast<size_t>(stop_count));
        for (jsize i = 0; i < stop_count; ++i) {
            jobject value = env->GetObjectArrayElement(stop_sequences, i);
            stop_sequence_values.push_back(to_string(env, value));
            env->DeleteLocalRef(value);
        }
    }

    skiffllm::GenerationOptions options;
    options.temperature = static_cast<float>(temperature);
    options.top_p = static_cast<float>(top_p);
    options.top_k = static_cast<int>(top_k);
    options.min_p = static_cast<float>(min_p);
    options.typical_p = static_cast<float>(typical_p);
    options.repeat_penalty = static_cast<float>(repeat_penalty);
    options.repeat_last_n = static_cast<int>(repeat_last_n);
    options.n_predict = static_cast<int>(max_tokens);
    options.seed = static_cast<uint32_t>(seed);
    options.stop_sequences = std::move(stop_sequence_values);
    options.auto_trim = true;
    options.token_callback = [env, callback, on_token](const std::string& part) {
        jstring piece = env->NewStringUTF(part.c_str());
        if (piece != nullptr) {
            env->CallVoidMethod(callback, on_token, piece);
            env->DeleteLocalRef(piece);
        }
    };

    state->stop_requested = false;
    skiffllm::GenerationResult result;
    std::string error;
    const bool ok = state->engine->generate(
        messages, options, result, [state]() { return state->stop_requested.load(); }, error);
    if (!ok) {
        jstring message = env->NewStringUTF(error.c_str());
        if (message != nullptr) {
            env->CallVoidMethod(callback, on_error, message);
            env->DeleteLocalRef(message);
        }
        env->DeleteLocalRef(callback_class);
        return JNI_FALSE;
    }

    skiffllm::record_generation(state->config, result.prompt_tokens, result.generated_tokens,
                                result.prompt_ms, result.generation_ms, result.tokens_per_second);
    env->CallVoidMethod(callback, on_done, static_cast<jint>(result.prompt_tokens),
                        static_cast<jint>(result.generated_tokens),
                        static_cast<jdouble>(result.prompt_ms),
                        static_cast<jdouble>(result.generation_ms),
                        static_cast<jdouble>(result.tokens_per_second),
                        static_cast<jboolean>(result.stopped));
    env->DeleteLocalRef(callback_class);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_skiffllm_app_SkiffNative_skillCatalog(JNIEnv* env,
                                                                                    jobject,
                                                                                    jlong handle) {
    NativeState* state = state_from(handle);
    std::ostringstream out;
    out << "[";
    const auto catalog = skiffllm::skill_catalog();
    bool first = true;
    for (const auto& name : catalog) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << "{\"name\":" << json_escape(name);
        out << ",\"description\":" << json_escape(skiffllm::skill_description(name));
        out << ",\"example\":" << json_escape(skiffllm::skill_example(name));
        out << "}";
    }
    out << "]";
    return to_jstring(env, (state == nullptr ? std::string("[]") : out.str()));
}

extern "C" JNIEXPORT jstring JNICALL Java_com_skiffllm_app_SkiffNative_executeSkill(
    JNIEnv* env, jobject, jlong handle, jstring name_value, jstring args_json) {
    NativeState* state = state_from(handle);
    if (state == nullptr) {
        return to_jstring(env, "{\"ok\":false,\"error\":\"engine is not initialized\"}");
    }
    skiffllm::SkillRequest request;
    request.name = to_string(env, name_value);
    request.args = parse_args_json(to_string(env, args_json));
    std::string error;
    const std::string result = skiffllm::execute_skill(state->config, request, error);
    std::ostringstream out;
    out << "{\"ok\":" << (error.empty() ? "true" : "false");
    out << ",\"result\":" << json_escape(result);
    out << ",\"error\":" << json_escape(error);
    out << "}";
    return to_jstring(env, out.str());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_skiffllm_app_SkiffNative_memoryLoad(JNIEnv* env,
                                                                                  jobject,
                                                                                  jlong handle) {
    NativeState* state = state_from(handle);
    if (state == nullptr) {
        return to_jstring(env, "");
    }
    return to_jstring(env, skiffllm::load_memories(state->config));
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_skiffllm_app_SkiffNative_memoryAppend(
    JNIEnv* env, jobject, jlong handle, jstring text_value) {
    NativeState* state = state_from(handle);
    if (state == nullptr) {
        return JNI_FALSE;
    }
    std::string error;
    return skiffllm::append_memory(state->config, to_string(env, text_value), error) ? JNI_TRUE
                                                                                     : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_skiffllm_app_SkiffNative_memoryClear(JNIEnv*,
                                                                                    jobject,
                                                                                    jlong handle) {
    NativeState* state = state_from(handle);
    if (state == nullptr) {
        return JNI_FALSE;
    }
    std::string error;
    return skiffllm::clear_memories(state->config, error) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_skiffllm_app_SkiffNative_usageStats(JNIEnv* env,
                                                                                  jobject,
                                                                                  jlong handle) {
    NativeState* state = state_from(handle);
    if (state == nullptr) {
        return to_jstring(env, "{\"ok\":false}");
    }
    skiffllm::UsageStats stats;
    std::string error;
    const bool ok = skiffllm::load_usage_stats(state->config, stats, error);
    std::ostringstream out;
    out << "{\"ok\":" << (ok ? "true" : "false");
    out << ",\"sessions\":" << stats.sessions;
    out << ",\"messages\":" << stats.messages;
    out << ",\"prompt_tokens\":" << stats.prompt_tokens;
    out << ",\"generated_tokens\":" << stats.generated_tokens;
    out << ",\"total_prompt_ms\":" << stats.total_prompt_ms;
    out << ",\"total_generation_ms\":" << stats.total_generation_ms;
    out << ",\"error\":" << json_escape(error);
    out << "}";
    return to_jstring(env, out.str());
}
