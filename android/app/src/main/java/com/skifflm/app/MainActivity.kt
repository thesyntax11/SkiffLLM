package com.skifflm.app

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SkiffTheme {
                ChatScreen()
            }
        }
    }
}

@Composable
private fun SkiffTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Color(0xFF6BA6FF),
            secondary = Color(0xFF4A7DFF),
            background = Color(0xFF0B0F14),
            surface = Color(0xFF151A21),
            onPrimary = Color(0xFF0B0F14),
            onBackground = Color(0xFFE6EBF2),
            onSurface = Color(0xFFE6EBF2)
        ),
        content = content
    )
}

@Composable
private fun ChatScreen() {
    val context = LocalContext.current
    val controller = remember { EngineController(context.applicationContext) }
    val messages = remember { mutableStateListOf<ChatMessage>() }
    val systemPrompt = remember { mutableStateOf("You are SkiffLLM, a helpful local assistant.") }
    val loadParams = remember { mutableStateOf(LoadParams()) }
    val sampling = remember { mutableStateOf(SamplingParams()) }

    var input by remember { mutableStateOf("") }
    var draft by remember { mutableStateOf("") }
    var stats by remember { mutableStateOf<GenerationStats?>(null) }
    var generating by remember { mutableStateOf(false) }
    var statusText by remember { mutableStateOf("No model loaded") }
    var errorText by remember { mutableStateOf<String?>(null) }
    var modelName by remember { mutableStateOf<String?>(null) }
    var showSettings by remember { mutableStateOf(false) }
    val listState = rememberLazyListState()

    DisposableEffect(Unit) {
        onDispose { controller.release() }
    }

    LaunchedEffect(Unit) {
        if (controller.hasStoredModel()) {
            statusText = "Loading saved model..."
            controller.loadStoredModel(
                loadParams.value,
                onLoaded = { name ->
                    modelName = name
                    statusText = "Model ready"
                },
                onError = { message ->
                    statusText = "Model load failed"
                    errorText = message
                }
            )
        }
    }

    LaunchedEffect(messages.size, draft, generating) {
        if (messages.isNotEmpty() || draft.isNotEmpty()) {
            val last = listState.layoutInfo.totalItemsCount - 1
            if (last >= 0) {
                listState.scrollToItem(last)
            }
        }
    }

    val modelPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            errorText = null
            statusText = "Loading model..."
            controller.loadModel(
                uri,
                loadParams.value,
                onLoaded = { name ->
                    modelName = name
                    statusText = "Model ready"
                    errorText = null
                },
                onError = { message ->
                    statusText = "Model load failed"
                    errorText = message
                }
            )
        }
    }

    fun send() {
        val text = input.trim()
        if (text.isEmpty() || generating) {
            return
        }
        messages.add(ChatMessage("user", text))
        input = ""
        draft = ""
        stats = null
        generating = true
        val full = listOf(ChatMessage("system", systemPrompt.value)) + messages.toList()
        controller.generate(
            full,
            sampling.value,
            onToken = { part -> draft += part },
            onDone = { result ->
                stats = result
                messages.add(ChatMessage("assistant", draft))
                draft = ""
                generating = false
            },
            onError = { message ->
                errorText = message
                generating = false
                draft = ""
            }
        )
    }

    fun stop() {
        controller.stop()
    }

    fun clearConversation() {
        if (!generating) {
            messages.clear()
            draft = ""
            stats = null
            input = ""
        }
    }

    fun shareConversation() {
        val markdown = buildMarkdown(systemPrompt.value, messages.toList())
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_TEXT, markdown)
        }
        context.startActivity(Intent.createChooser(intent, "Export conversation"))
    }

    Scaffold(
        topBar = {
            ChatTopBar(
                statusText = statusText,
                modelName = modelName,
                onSettings = { showSettings = true },
                onExport = { shareConversation() },
                onClear = { clearConversation() }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(12.dp)
        ) {
            if (generating) {
                LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                Spacer(Modifier.height(8.dp))
            }
            LazyColumn(
                state = listState,
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth(),
                arrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(messages) { message ->
                    if (message.role != "system") {
                        MessageBubble(message)
                    }
                }
                if (generating) {
                    MessageBubble(ChatMessage("assistant", draft))
                }
            }
            errorText?.let {
                Text(
                    it,
                    color = Color(0xFFFF7A7A),
                    modifier = Modifier.padding(vertical = 6.dp)
                )
            }
            stats?.let {
                Text(
                    "${it.generatedTokens} tokens in ${"%.1f".format(it.generationMs / 1000.0)} s at ${"%.1f".format(it.tokensPerSecond)} tok/s",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
            }
            Spacer(Modifier.height(8.dp))
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth()
            ) {
                OutlinedTextField(
                    value = input,
                    onValueChange = { input = it },
                    modifier = Modifier.weight(1f),
                    placeholder = { Text("Message") },
                    maxLines = 4,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Text)
                )
                Spacer(Modifier.width(8.dp))
                if (generating) {
                    OutlinedButton(onClick = { stop() }) {
                        Text("Stop")
                    }
                } else {
                    Button(onClick = { send() }) {
                        Text("Send")
                    }
                }
            }
        }
    }

    if (showSettings) {
        SettingsDialog(
            loadParams = loadParams.value,
            onLoadParams = { loadParams.value = it },
            sampling = sampling.value,
            onSampling = { sampling.value = it },
            systemPrompt = systemPrompt.value,
            onSystemPrompt = { systemPrompt.value = it },
            onPickModel = {
                showSettings = false
                modelPicker.launch(arrayOf("*/*"))
            },
            onDismiss = { showSettings = false }
        )
    }
}

@Composable
private fun ChatTopBar(
    statusText: String,
    modelName: String?,
    onSettings: () -> Unit,
    onExport: () -> Unit,
    onClear: () -> Unit
) {
    Surface(color = MaterialTheme.colorScheme.surface) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 8.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    "SkiffLLM",
                    style = MaterialTheme.typography.titleLarge,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.weight(1f)
                )
                TextButton(onClick = { onExport() }) {
                    Text("Export")
                }
                TextButton(onClick = { onClear() }) {
                    Text("Clear")
                }
                TextButton(onClick = { onSettings() }) {
                    Text("Settings")
                }
            }
            Text(
                modelName?.let { "Model: $it" } ?: statusText,
                style = MaterialTheme.typography.bodySmall,
                color = Color(0xFF8A94A6)
            )
        }
    }
}

@Composable
private fun MessageBubble(message: ChatMessage) {
    val isUser = message.role == "user"
    val alignment = if (isUser) Alignment.End else Alignment.Start
    val bubbleColor = if (isUser) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surface
    val textColor = if (isUser) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface
    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = alignment
    ) {
        Surface(
            color = bubbleColor,
            shape = RoundedCornerShape(14.dp),
            modifier = Modifier.heightIn(min = 34.dp)
        ) {
            Text(
                message.content,
                color = textColor,
                modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp)
            )
        }
    }
}

@Composable
private fun SettingsDialog(
    loadParams: LoadParams,
    onLoadParams: (LoadParams) -> Unit,
    sampling: SamplingParams,
    onSampling: (SamplingParams) -> Unit,
    systemPrompt: String,
    onSystemPrompt: (String) -> Unit,
    onPickModel: () -> Unit,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = { onDismiss() },
        confirmButton = {
            TextButton(onClick = { onDismiss() }) {
                Text("Done")
            }
        },
        dismissButton = {
            TextButton(onClick = { onPickModel() }) {
                Text("Load model")
            }
        },
        title = { Text("Settings") },
        text = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState())
            ) {
                NumberField(
                    label = "Context size",
                    value = loadParams.contextSize.toString(),
                    onValue = { value ->
                        value.toIntOrNull()?.let { onLoadParams(loadParams.copy(contextSize = it)) }
                    }
                )
                NumberField(
                    label = "Threads",
                    value = loadParams.threads.toString(),
                    onValue = { value ->
                        value.toIntOrNull()?.let { onLoadParams(loadParams.copy(threads = it)) }
                    }
                )
                NumberField(
                    label = "GPU layers",
                    value = loadParams.gpuLayers.toString(),
                    onValue = { value ->
                        value.toIntOrNull()?.let { onLoadParams(loadParams.copy(gpuLayers = it)) }
                    }
                )
                OutlinedTextField(
                    value = loadParams.chatTemplate,
                    onValueChange = { onLoadParams(loadParams.copy(chatTemplate = it)) },
                    label = { Text("Chat template (next load)") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
                Text(
                    "Model settings apply the next time a model is loaded.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                NumberField(
                    label = "Temperature",
                    value = sampling.temperature.toString(),
                    decimal = true,
                    onValue = { value ->
                        value.toFloatOrNull()?.let { onSampling(sampling.copy(temperature = it)) }
                    }
                )
                NumberField(
                    label = "Top-p",
                    value = sampling.topP.toString(),
                    decimal = true,
                    onValue = { value ->
                        value.toFloatOrNull()?.let { onSampling(sampling.copy(topP = it)) }
                    }
                )
                NumberField(
                    label = "Top-k",
                    value = sampling.topK.toString(),
                    onValue = { value ->
                        value.toIntOrNull()?.let { onSampling(sampling.copy(topK = it)) }
                    }
                )
                NumberField(
                    label = "Max tokens",
                    value = sampling.maxTokens.toString(),
                    onValue = { value ->
                        value.toIntOrNull()?.let { onSampling(sampling.copy(maxTokens = it)) }
                    }
                )
                OutlinedTextField(
                    value = systemPrompt,
                    onValueChange = { onSystemPrompt(it) },
                    label = { Text("System prompt") },
                    modifier = Modifier.fillMaxWidth()
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "Recommended models: Qwen2.5-0.5B-Instruct-GGUF Q4_K_M, Qwen3-0.6B-Instruct-GGUF Q4_K_M, Llama-3.2-1B-Instruct-GGUF Q4_K_M, Phi-3.5-mini-instruct-GGUF Q4_K_M, SmolLM2-1.7B-Instruct-GGUF Q4_K_M",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
            }
        }
    )
}

@Composable
private fun NumberField(
    label: String,
    value: String,
    decimal: Boolean = false,
    onValue: (String) -> Unit
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValue,
        label = { Text(label) },
        singleLine = true,
        keyboardOptions = KeyboardOptions(
            keyboardType = if (decimal) KeyboardType.Decimal else KeyboardType.Number
        ),
        modifier = Modifier.fillMaxWidth()
    )
}

private fun buildMarkdown(systemPrompt: String, messages: List<ChatMessage>): String {
    val out = StringBuilder()
    out.append("# SkiffLLM Conversation\n\n")
    out.append("## System\n\n").append(systemPrompt).append("\n\n")
    val formatter = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US)
    out.append("Exported: ").append(formatter.format(Date())).append("\n\n")
    for (message in messages) {
        val role = when (message.role) {
            "user" -> "User"
            "assistant" -> "Assistant"
            else -> message.role
        }
        out.append("## ").append(role).append("\n\n")
        out.append(message.content).append("\n\n")
    }
    return out.toString()
}
