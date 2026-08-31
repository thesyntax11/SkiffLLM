package com.skifflm.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

class ConversationStore(private val appContext: Context) {

    private val file = File(appContext.filesDir, "conversation.json")

    fun load(): ConversationSnapshot {
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
            ConversationSnapshot(system, messages)
        }.getOrElse {
            ConversationSnapshot(
                systemPrompt = "You are SkiffLLM, a helpful local assistant.",
                messages = emptyList()
            )
        }
    }

    fun save(systemPrompt: String, messages: List<ChatMessage>) {
        runCatching {
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
            }
            file.writeText(json.toString())
        }
    }

    fun clear() {
        runCatching { file.delete() }
    }
}

data class ConversationSnapshot(
    val systemPrompt: String,
    val messages: List<ChatMessage>
)
