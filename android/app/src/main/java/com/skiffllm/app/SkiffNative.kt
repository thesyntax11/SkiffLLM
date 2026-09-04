package com.skiffllm.app

object SkiffNative {
    init {
        System.loadLibrary("skiffllm")
    }

    external fun create(
        contextSize: Int,
        threads: Int,
        gpuLayers: Int,
        chatTemplate: String?,
        storageRoot: String?
    ): Long

    external fun destroy(handle: Long)

    external fun load(handle: Long, modelPath: String): Boolean

    external fun loadError(handle: Long): String?

    external fun warmup(handle: Long): Boolean

    external fun infoJson(handle: Long): String?

    external fun stop(handle: Long)

    external fun generate(
        handle: Long,
        roles: Array<String>,
        contents: Array<String>,
        temperature: Float,
        topP: Float,
        topK: Int,
        minP: Float,
        typicalP: Float,
        repeatPenalty: Float,
        repeatLastN: Int,
        maxTokens: Int,
        seed: Long,
        stopSequences: Array<String>,
        callback: Callback
    ): Boolean

    external fun skillCatalog(handle: Long): String?

    external fun executeSkill(
        handle: Long,
        name: String,
        argsJson: String
    ): String?

    external fun memoryLoad(handle: Long): String?

    external fun memoryAppend(handle: Long, text: String): Boolean

    external fun memoryClear(handle: Long): Boolean

    external fun usageStats(handle: Long): String?

    interface Callback {
        fun onToken(text: String)
        fun onDone(
            promptTokens: Int,
            generatedTokens: Int,
            promptMs: Double,
            generationMs: Double,
            tokensPerSecond: Double,
            stopped: Boolean
        )

        fun onError(message: String)
    }
}
