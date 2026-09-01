package com.skifflm.app

import android.content.Context
import android.net.Uri
import android.os.Handler
import android.os.Looper
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

data class ChatMessage(
    val role: String,
    val content: String
)

data class SamplingParams(
    val temperature: Float = 0.7f,
    val topP: Float = 0.95f,
    val topK: Int = 40,
    val minP: Float = 0.0f,
    val typicalP: Float = 0.0f,
    val repeatPenalty: Float = 1.10f,
    val repeatLastN: Int = 64,
    val maxTokens: Int = 512,
    val seed: Long = 0xFFFFFFFFL
)

data class LoadParams(
    val contextSize: Int = 2048,
    val threads: Int = 4,
    val gpuLayers: Int = 0,
    val chatTemplate: String = ""
)

data class GenerationStats(
    val promptTokens: Int,
    val generatedTokens: Int,
    val promptMs: Double,
    val generationMs: Double,
    val tokensPerSecond: Double,
    val stopped: Boolean
)

class EngineController(private val appContext: Context) {
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private val uiHandler = Handler(Looper.getMainLooper())
    private val prefs = appContext.getSharedPreferences("skifflm", Context.MODE_PRIVATE)
    private val tokenLock = Any()
    private val tokenBuffer = StringBuilder()
    private var tokenFlushScheduled = false
    @Volatile
    private var handle: Long = 0L
    @Volatile
    private var currentModelPath: String? = null
    private val modelDir = File(appContext.filesDir, "models").apply { mkdirs() }

    fun release() {
        executor.execute {
            if (handle != 0L) {
                SkiffNative.destroy(handle)
                handle = 0L
            }
            currentModelPath = null
        }
        executor.shutdown()
    }

    fun currentModelPath(): String? = currentModelPath

    fun isModelLoaded(file: File): Boolean = currentModelPath == file.absolutePath

    fun deleteModel(file: File): Boolean {
        if (isModelLoaded(file)) {
            return false
        }
        return file.delete()
    }

    fun hasStoredModel(): Boolean = prefs.getString(KEY_MODEL_PATH, null)?.let(::File)?.exists() == true

    fun loadStoredModel(
        params: LoadParams,
        onLoaded: (String) -> Unit,
        onError: (String) -> Unit
    ) {
        val path = prefs.getString(KEY_MODEL_PATH, null) ?: return
        if (!File(path).exists()) {
            prefs.edit().remove(KEY_MODEL_PATH).apply()
            return
        }
        loadFile(File(path), params, onLoaded, onError)
    }

    fun listDownloadedModels(): List<File> =
        modelDir.listFiles { file -> file.isFile && file.extension.equals("gguf", true) }
            ?.sortedBy { it.name }
            ?: emptyList()

    fun loadModelFile(
        file: File,
        params: LoadParams,
        onLoaded: (String) -> Unit,
        onError: (String) -> Unit
    ) {
        loadFile(file, params, onLoaded, onError)
    }

    fun loadModel(
        uri: Uri,
        params: LoadParams,
        onLoaded: (String) -> Unit,
        onError: (String) -> Unit
    ) {
        executor.execute {
            try {
                val copied = copyToInternal(uri)
                loadFile(copied, params, onLoaded, onError)
            } catch (t: Throwable) {
                uiHandler.post { onError(t.message ?: "Unable to copy the model") }
            }
        }
    }

    private fun loadFile(
        file: File,
        params: LoadParams,
        onLoaded: (String) -> Unit,
        onError: (String) -> Unit
    ) {
        executor.execute {
            try {
                if (handle != 0L) {
                    SkiffNative.destroy(handle)
                    handle = 0L
                }
                currentModelPath = null
                handle = SkiffNative.create(
                    params.contextSize,
                    params.threads,
                    params.gpuLayers,
                    params.chatTemplate.takeIf { it.isNotBlank() }
                )
                if (handle == 0L) {
                    uiHandler.post { onError("Unable to initialize the native engine") }
                    return@execute
                }
                val loaded = SkiffNative.load(handle, file.absolutePath)
                if (!loaded) {
                    val message = SkiffNative.loadError(handle)
                        ?: "Unable to load the model"
                    SkiffNative.destroy(handle)
                    handle = 0L
                    uiHandler.post { onError(message) }
                    return@execute
                }
                SkiffNative.warmup(handle)
                currentModelPath = file.absolutePath
                prefs.edit().putString(KEY_MODEL_PATH, file.absolutePath).apply()
                uiHandler.post { onLoaded(file.name) }
            } catch (t: Throwable) {
                if (handle != 0L) {
                    SkiffNative.destroy(handle)
                    handle = 0L
                }
                currentModelPath = null
                uiHandler.post { onError(t.message ?: "Unable to load the model") }
            }
        }
    }

    fun generate(
        messages: List<ChatMessage>,
        params: SamplingParams,
        onToken: (String) -> Unit,
        onDone: (GenerationStats) -> Unit,
        onError: (String) -> Unit
    ) {
        if (handle == 0L) {
            onError("Load a GGUF model first")
            return
        }
        val roles = messages.map { it.role }.toTypedArray()
        val contents = messages.map { it.content }.toTypedArray()
        val callback = object : SkiffNative.Callback {
            override fun onToken(text: String) {
                // Batch tokens into small UI-frame-sized updates instead of
                // posting once per generated token. This keeps the main thread
                // responsive on low-end devices.
                synchronized(tokenLock) {
                    tokenBuffer.append(text)
                    if (!tokenFlushScheduled) {
                        tokenFlushScheduled = true
                        uiHandler.post {
                            val chunk = synchronized(tokenLock) {
                                val value = tokenBuffer.toString()
                                tokenBuffer.clear()
                                tokenFlushScheduled = false
                                value
                            }
                            if (chunk.isNotEmpty()) {
                                onToken(chunk)
                            }
                        }
                    }
                }
            }

            override fun onDone(
                promptTokens: Int,
                generatedTokens: Int,
                promptMs: Double,
                generationMs: Double,
                tokensPerSecond: Double,
                stopped: Boolean
            ) {
                uiHandler.post {
                    onDone(
                        GenerationStats(
                            promptTokens,
                            generatedTokens,
                            promptMs,
                            generationMs,
                            tokensPerSecond,
                            stopped
                        )
                    )
                }
            }

            override fun onError(message: String) {
                uiHandler.post { onError(message) }
            }
        }
        executor.execute {
            val target = handle
            if (target == 0L) {
                uiHandler.post { onError("Load a GGUF model first") }
                return@execute
            }
            synchronized(tokenLock) {
                tokenBuffer.clear()
                tokenFlushScheduled = false
            }
            SkiffNative.generate(
                target,
                roles,
                contents,
                params.temperature,
                params.topP,
                params.topK,
                params.minP,
                params.typicalP,
                params.repeatPenalty,
                params.repeatLastN,
                params.maxTokens,
                params.seed,
                callback
            )
        }
    }

    fun stop() {
        if (handle != 0L) {
            SkiffNative.stop(handle)
        }
    }

    fun modelDescription(): String? {
        if (handle == 0L) {
            return null
        }
        val json = SkiffNative.infoJson(handle) ?: return null
        return runCatching { JSONObject(json).optString("description") }
            .getOrNull()
            ?.takeIf { it.isNotBlank() }
    }

    private fun copyToInternal(uri: Uri): File {
        val display = queryDisplayName(uri) ?: "model.gguf"
        val safe = display.replace(Regex("[^A-Za-z0-9._-]"), "_")
        val target = File(modelDir, safe.ifBlank { "model.gguf" })
        val temp = File(modelDir, "$safe.part")
        val stream = appContext.contentResolver.openInputStream(uri)
            ?: throw IOException("Unable to open the selected model")
        stream.use { input ->
            FileOutputStream(temp).use { output ->
                input.copyTo(output)
            }
        }
        if (target.exists() && !target.delete()) {
            temp.delete()
            throw IOException("Unable to replace the previous model file")
        }
        if (!temp.renameTo(target)) {
            temp.delete()
            throw IOException("Unable to store the model file")
        }
        return target
    }

    private fun queryDisplayName(uri: Uri): String? {
        return runCatching {
            appContext.contentResolver.query(
                uri,
                arrayOf(android.provider.OpenableColumns.DISPLAY_NAME),
                null,
                null,
                null
            )?.use { cursor ->
                if (cursor.moveToFirst()) {
                    cursor.getString(cursor.getColumnIndexOrThrow(
                        android.provider.OpenableColumns.DISPLAY_NAME
                    ))
                } else {
                    null
                }
            }
        }.getOrNull()
    }

    fun isLoaded(): Boolean = handle != 0L

    private companion object {
        const val KEY_MODEL_PATH = "model_path"
    }
}
