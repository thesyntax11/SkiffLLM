package com.skifflm.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

class ConversationStore(private val appContext: Context) {

    private val dir = File(appContext.filesDir, "conversations")
    private val currentFile = File(appContext.filesDir, "current_conversation.txt")
    private val legacyFile = File(appContext.filesDir, "conversation.json")

    private fun sanitize(name: String): String {
        val cleaned = name.trim()
            .replace(Regex("[^A-Za-z0-9._-]"), "_")
        return cleaned.ifBlank { "default" }
    }

    private fun fileFor(name: String): File = File(dir, "${sanitize(name)}.json")

    private fun ensureCurrent(): String {
        val name = currentName()
        currentFile.writeText(name)
        dir.mkdirs()
        return name
    }

    fun currentName(): String {
        if (currentFile.exists()) {
            val name = currentFile.readText().trim()
            if (name.isNotBlank()) return name
        }
        return "default"
    }

    fun listConversations(): List<String> {
        val existing = dir.listFiles()?.filter { it.isFile && it.extension == "json" }
            ?.map { it.nameWithoutExtension } ?: emptyList()
        val names = (existing + listOf("default")).distinct().sorted()
        return names
    }

    fun create(name: String): String {
        val finalName = sanitize(name)
        dir.mkdirs()
        if (!fileFor(finalName).exists()) {
            fileFor(finalName).writeText(
                JSONObject()
                    .put("system_prompt", "You are SkiffLLM, a helpful local assistant.")
                    .put("messages", JSONArray())
                    .toString()
            )
        }
        currentFile.writeText(finalName)
        return finalName
    }

    fun switchTo(name: String): String {
        val finalName = sanitize(name)
        dir.mkdirs()
        if (!fileFor(finalName).exists()) {
            fileFor(finalName).writeText(
                JSONObject()
                    .put("system_prompt", "You are SkiffLLM, a helpful local assistant.")
                    .put("messages", JSONArray())
                    .toString()
            )
        }
        currentFile.writeText(finalName)
        return finalName
    }

    fun rename(oldName: String, newName: String): String {
        val oldFinal = sanitize(oldName)
        val newFinal = sanitize(newName)
        if (oldFinal == newFinal) {
            return oldFinal
        }
        val oldFile = fileFor(oldFinal)
        val newFile = fileFor(newFinal)
        if (!oldFile.exists()) {
            return oldFinal
        }
        if (newFile.exists()) {
            return oldFinal
        }
        if (oldFile.renameTo(newFile)) {
            if (sanitize(currentName()) == oldFinal) {
                currentFile.writeText(newFinal)
            }
            return newFinal
        }
        return oldFinal
    }

    fun delete(name: String) {
        fileFor(name).delete()
        // Never leave the user without a conversation.
        val current = currentName()
        if (sanitize(name) == sanitize(current)) {
            currentFile.writeText("default")
            dir.mkdirs()
            if (!fileFor("default").exists()) {
                fileFor("default").writeText(
                    JSONObject()
                        .put("system_prompt", "You are SkiffLLM, a helpful local assistant.")
                        .put("messages", JSONArray())
                        .toString()
                )
            }
        }
    }

    fun load(): ConversationSnapshot {
        migrateLegacy()
        val name = ensureCurrent()
        val file = fileFor(name)
        if (!file.exists()) {
            return ConversationSnapshot(
                systemPrompt = "You are SkiffLLM, a helpful local assistant.",
                messages = emptyList()
            )
        }
        return runCatching {
            val json = JSONObject(file.readText())
            val system = json.optString("system_prompt")
                .ifBlank { "You are SkiffLLM, a helpful local assistant." }
            val raw = json.optJSONArray("messages") ?: JSONArray()
            val messages = buildList {
                for (i in 0 until raw.length()) {
                    val item = raw.optJSONObject(i) ?: continue
                    val role = item.optString("role")
                    val content = item.optString("content")
                    if (role.isNotBlank() && content.isNotBlank()) {
                        add(ChatMessage(role, content))
                    }
                }
            }
            ConversationSnapshot(
                system,
                messages,
                sampling = samplingFromJson(json),
                codeMode = if (json.has("code_mode")) json.optBoolean("code_mode") else null
            )
        }.getOrElse {
            ConversationSnapshot(
                systemPrompt = "You are SkiffLLM, a helpful local assistant.",
                messages = emptyList()
            )
        }
    }

    private fun samplingFromJson(json: JSONObject): SamplingParams? {
        if (!json.has("sampling")) {
            return null
        }
        val obj = json.optJSONObject("sampling") ?: return null
        return SamplingParams(
            temperature = obj.optDouble("temperature", 0.7).toFloat(),
            topP = obj.optDouble("top_p", 0.95).toFloat(),
            topK = obj.optInt("top_k", 40),
            minP = obj.optDouble("min_p", 0.0).toFloat(),
            typicalP = obj.optDouble("typical_p", 0.0).toFloat(),
            repeatPenalty = obj.optDouble("repeat_penalty", 1.10).toFloat(),
            repeatLastN = obj.optInt("repeat_last_n", 64),
            maxTokens = obj.optInt("max_tokens", 512),
            seed = obj.optLong("seed", 0xFFFFFFFFL)
        )
    }

    fun save(
        systemPrompt: String,
        messages: List<ChatMessage>,
        sampling: SamplingParams? = null,
        codeMode: Boolean? = null
    ) {
        migrateLegacy()
        val name = ensureCurrent()
        val raw = JSONArray()
        messages.forEach { message ->
            raw.put(JSONObject().apply {
                put("role", message.role)
                put("content", message.content)
            })
        }
        val json = JSONObject().apply {
            put("system_prompt", systemPrompt)
            put("messages", raw)
            if (sampling != null) {
                put("sampling", JSONObject().apply {
                    put("temperature", sampling.temperature)
                    put("top_p", sampling.topP)
                    put("top_k", sampling.topK)
                    put("min_p", sampling.minP)
                    put("typical_p", sampling.typicalP)
                    put("repeat_penalty", sampling.repeatPenalty)
                    put("repeat_last_n", sampling.repeatLastN)
                    put("max_tokens", sampling.maxTokens)
                    put("seed", sampling.seed)
                })
            }
            if (codeMode != null) {
                put("code_mode", codeMode)
            }
        }
        fileFor(name).writeText(json.toString())
    }

    fun clear() {
        fileFor(currentName()).delete()
    }

    private fun migrateLegacy() {
        if (legacyFile.exists() && !fileFor("default").exists()) {
            runCatching {
                dir.mkdirs()
                legacyFile.copyTo(fileFor("default"), overwrite = true)
                legacyFile.delete()
            }
        }
    }
}

data class ConversationSnapshot(
    val systemPrompt: String,
    val messages: List<ChatMessage>,
    val sampling: SamplingParams? = null,
    val codeMode: Boolean? = null
)
