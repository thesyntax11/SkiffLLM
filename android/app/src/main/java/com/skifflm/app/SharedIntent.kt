package com.skifflm.app

import android.content.Intent
import android.net.Uri
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/**
 * A tiny bridge between an incoming Android share/view intent and the Compose
 * screen. The Activity writes it; ChatScreen reads it and offers to use or
 * dismiss the text.
 */
object SharedIntent {
    var text: String? by mutableStateOf(null)
        private set

    fun offer(intent: Intent?) {
        if (intent == null) return
        val value = when {
            intent.action == Intent.ACTION_SEND || intent.action == Intent.ACTION_VIEW ->
                intent.getStringExtra(Intent.EXTRA_TEXT)
            else -> null
        }
        if (!value.isNullOrBlank()) {
            text = value
        }
    }

    fun offerUri(uri: Uri?) {
        // Reserved for future file-sharing flows.
    }

    fun clear() {
        text = null
    }
}
