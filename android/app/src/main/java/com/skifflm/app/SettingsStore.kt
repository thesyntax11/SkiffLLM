package com.skifflm.app

import android.content.Context

class SettingsStore(private val appContext: Context) {

    private val prefs = appContext.getSharedPreferences("skifflm_settings", Context.MODE_PRIVATE)

    fun loadLoadParams(defaultThreads: Int): LoadParams = LoadParams(
        contextSize = prefs.getInt(KEY_CONTEXT, 2048),
        threads = prefs.getInt(KEY_THREADS, defaultThreads),
        gpuLayers = prefs.getInt(KEY_GPU_LAYERS, 0),
        chatTemplate = prefs.getString(KEY_CHAT_TEMPLATE, "").orEmpty()
    )

    fun saveLoadParams(params: LoadParams) {
        prefs.edit()
            .putInt(KEY_CONTEXT, params.contextSize)
            .putInt(KEY_THREADS, params.threads)
            .putInt(KEY_GPU_LAYERS, params.gpuLayers)
            .putString(KEY_CHAT_TEMPLATE, params.chatTemplate)
            .apply()
    }

    fun loadSampling(): SamplingParams = SamplingParams(
        temperature = prefs.getFloat(KEY_TEMPERATURE, 0.7f),
        topP = prefs.getFloat(KEY_TOP_P, 0.95f),
        topK = prefs.getInt(KEY_TOP_K, 40),
        minP = prefs.getFloat(KEY_MIN_P, 0.0f),
        typicalP = prefs.getFloat(KEY_TYPICAL_P, 0.0f),
        repeatPenalty = prefs.getFloat(KEY_REPEAT_PENALTY, 1.10f),
        repeatLastN = prefs.getInt(KEY_REPEAT_LAST_N, 64),
        maxTokens = prefs.getInt(KEY_MAX_TOKENS, 512),
        seed = prefs.getLong(KEY_SEED, 0xFFFFFFFFL)
    )

    fun saveSampling(params: SamplingParams) {
        prefs.edit()
            .putFloat(KEY_TEMPERATURE, params.temperature)
            .putFloat(KEY_TOP_P, params.topP)
            .putInt(KEY_TOP_K, params.topK)
            .putFloat(KEY_MIN_P, params.minP)
            .putFloat(KEY_TYPICAL_P, params.typicalP)
            .putFloat(KEY_REPEAT_PENALTY, params.repeatPenalty)
            .putInt(KEY_REPEAT_LAST_N, params.repeatLastN)
            .putInt(KEY_MAX_TOKENS, params.maxTokens)
            .putLong(KEY_SEED, params.seed)
            .apply()
    }

    fun saveSystemPrompt(value: String) {
        prefs.edit().putString(KEY_SYSTEM_PROMPT, value).apply()
    }

    fun loadSystemPrompt(default: String): String =
        prefs.getString(KEY_SYSTEM_PROMPT, default) ?: default

    private companion object {
        const val KEY_CONTEXT = "context_size"
        const val KEY_THREADS = "threads"
        const val KEY_GPU_LAYERS = "gpu_layers"
        const val KEY_CHAT_TEMPLATE = "chat_template"
        const val KEY_TEMPERATURE = "temperature"
        const val KEY_TOP_P = "top_p"
        const val KEY_TOP_K = "top_k"
        const val KEY_MIN_P = "min_p"
        const val KEY_TYPICAL_P = "typical_p"
        const val KEY_REPEAT_PENALTY = "repeat_penalty"
        const val KEY_REPEAT_LAST_N = "repeat_last_n"
        const val KEY_MAX_TOKENS = "max_tokens"
        const val KEY_SEED = "seed"
        const val KEY_SYSTEM_PROMPT = "system_prompt"
    }
}
