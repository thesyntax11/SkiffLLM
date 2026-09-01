package com.skifflm.app

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.lightColorScheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import android.widget.Toast
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        SharedIntent.offer(intent)
        val settingsStore = SettingsStore(applicationContext)
        val themeName = mutableStateOf(settingsStore.loadTheme("dark"))
        setContent {
            SkiffTheme(themeName.value) {
                ChatScreen(
                    themeName = themeName.value,
                    onThemeName = { name ->
                        themeName.value = name
                        settingsStore.saveTheme(name)
                    }
                )
            }
        }
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        setIntent(intent)
        SharedIntent.offer(intent)
    }
}

@Composable
private fun SkiffTheme(themeName: String = "dark", content: @Composable () -> Unit) {
    val scheme = when (themeName) {
        "light" -> lightColorScheme(
            primary = Color(0xFF3B6FE0),
            secondary = Color(0xFF2E5BB5),
            background = Color(0xFFF4F6FB),
            surface = Color(0xFFFFFFFF),
            onPrimary = Color(0xFFFFFFFF),
            onBackground = Color(0xFF1A1F27),
            onSurface = Color(0xFF1A1F27)
        )
        "system" -> (
            if (isSystemInDarkTheme()) {
                darkColorScheme(
                    primary = Color(0xFF6BA6FF),
                    secondary = Color(0xFF4A7DFF),
                    background = Color(0xFF0B0F14),
                    surface = Color(0xFF151A21),
                    onPrimary = Color(0xFF0B0F14),
                    onBackground = Color(0xFFE6EBF2),
                    onSurface = Color(0xFFE6EBF2)
                )
            } else {
                lightColorScheme(
                    primary = Color(0xFF3B6FE0),
                    secondary = Color(0xFF2E5BB5),
                    background = Color(0xFFF4F6FB),
                    surface = Color(0xFFFFFFFF),
                    onPrimary = Color(0xFFFFFFFF),
                    onBackground = Color(0xFF1A1F27),
                    onSurface = Color(0xFF1A1F27)
                )
            }
            )
        else -> darkColorScheme(
            primary = Color(0xFF6BA6FF),
            secondary = Color(0xFF4A7DFF),
            background = Color(0xFF0B0F14),
            surface = Color(0xFF151A21),
            onPrimary = Color(0xFF0B0F14),
            onBackground = Color(0xFFE6EBF2),
            onSurface = Color(0xFFE6EBF2)
        )
    }
    MaterialTheme(
        colorScheme = scheme,
        content = content
    )
}

@Composable
private fun ChatScreen(themeName: String, onThemeName: (String) -> Unit) {
    val context = LocalContext.current
    val controller = remember { EngineController(context.applicationContext) }
    val downloader = remember { ModelDownloader(context.applicationContext) }
    val conversationStore = remember { ConversationStore(context.applicationContext) }
    val settingsStore = remember { SettingsStore(context.applicationContext) }
    val messages = remember { mutableStateListOf<ChatMessage>() }
    val localModels = remember { mutableStateListOf<File>() }
    val downloading = remember { mutableStateMapOf<String, Boolean>() }
    val downloadProgress = remember { mutableStateMapOf<String, Float>() }
    val downloadErrors = remember { mutableStateMapOf<String, String>() }
    val defaultThreads = remember {
        Runtime.getRuntime().availableProcessors().coerceIn(2, 8)
    }
    val systemPrompt = remember {
        mutableStateOf(settingsStore.loadSystemPrompt("You are SkiffLLM, a helpful local assistant."))
    }
    val persistentFacts = remember {
        mutableStateOf(settingsStore.loadPersistentFacts(""))
    }
    val quickPrompts = remember { mutableStateListOf<String>().also { it.addAll(settingsStore.loadQuickPrompts()) } }
    val stopSequences = remember { mutableStateListOf<String>().also { it.addAll(settingsStore.loadStopSequences()) } }
    val codeMode = remember { mutableStateOf(settingsStore.loadCodeMode()) }
    val loadParams = remember { mutableStateOf(settingsStore.loadLoadParams(defaultThreads)) }
    val sampling = remember { mutableStateOf(settingsStore.loadSampling()) }
    val conversationName = remember { mutableStateOf(conversationStore.currentName()) }
    val conversationNames = remember { mutableStateListOf<String>().also { it.addAll(conversationStore.listConversations()) } }
    val sharedText = SharedIntent.text

    var input by remember { mutableStateOf("") }
    var draft by remember { mutableStateOf("") }
    var stats by remember { mutableStateOf<GenerationStats?>(null) }
    var generating by remember { mutableStateOf(false) }
    var statusText by remember { mutableStateOf("No model loaded") }
    var errorText by remember { mutableStateOf<String?>(null) }
    var modelName by remember { mutableStateOf<String?>(null) }
    var showSettings by remember { mutableStateOf(false) }
    var showModels by remember { mutableStateOf(false) }
    var showConversations by remember { mutableStateOf(false) }
    var showAbout by remember { mutableStateOf(false) }
    var modelInfo by remember { mutableStateOf<String?>(null) }
    var sessionStats by remember { mutableStateOf(SessionStats()) }
    var warmingUp by remember { mutableStateOf(false) }
    var benchmarkResult by remember { mutableStateOf<String?>(null) }
    val listState = rememberLazyListState()

    fun effectiveSystemPrompt(): String {
        val base = systemPrompt.value
        val facts = persistentFacts.value.trim()
        val withFacts = if (facts.isNotEmpty()) {
            "$base\n\nPersistent facts:\n$facts"
        } else {
            base
        }
        return if (codeMode.value) {
            "$withFacts\n\nYou are a careful coding assistant. Propose concrete " +
                "edits as a unified diff (file paths, +/-, context). Use the code " +
                "context below. Never claim a file was changed; output the proposal " +
                "only and wait for a human to apply it."
        } else {
            withFacts
        }
    }

    fun refreshLocalModels() {
        val files = controller.listDownloadedModels()
        localModels.clear()
        localModels.addAll(files)
    }

    fun handleModelLoaded(name: String) {
        modelName = name
        modelInfo = controller.modelDescription()?.take(200)
        statusText = "Model ready"
        errorText = null
        refreshLocalModels()
    }

    fun handleModelError(message: String) {
        statusText = "Model load failed"
        errorText = message
    }

    fun recordStats(result: GenerationStats) {
        val prompt = sessionStats.promptTokens + result.promptTokens
        val generated = sessionStats.generatedTokens + result.generatedTokens
        val elapsed = sessionStats.generationMs + result.generationMs
        val average = if (elapsed > 0.0) {
            (generated.toDouble() / (elapsed / 1000.0))
        } else {
            sessionStats.avgTokensPerSecond
        }
        sessionStats = SessionStats(
            messageCount = messages.size,
            promptTokens = prompt,
            generatedTokens = generated,
            generationMs = elapsed,
            avgTokensPerSecond = average
        )
    }

    fun warmModel() {
        if (generating || warmingUp || modelName == null) {
            return
        }
        warmingUp = true
        errorText = null
        controller.warmup { ok ->
            warmingUp = false
            if (ok) {
                statusText = "Model warmed"
            } else {
                errorText = "Warm-up failed"
            }
        }
    }

    fun runBenchmark() {
        if (generating || warmingUp || modelName == null) {
            return
        }
        val runs = mutableListOf<GenerationStats>()
        benchmarkResult = null
        stats = null
        errorText = null
        generating = true

        fun nextRound() {
            val prompt = "Write one short sentence explaining what a local LLM is."
            controller.generate(
                listOf(ChatMessage("user", prompt)),
                sampling.value.copy(temperature = 0.0f, maxTokens = 128),
                emptyList(),
                onToken = {},
                onDone = { result ->
                    runs.add(result)
                    if (runs.size < 3) {
                        nextRound()
                    } else {
                        val averageTps = runs.map { it.tokensPerSecond }.average()
                        val averageMs = runs.map { it.generationMs }.average()
                        val totalTokens = runs.sumOf { it.generatedTokens }
                        benchmarkResult = "Benchmark (3 rounds): $totalTokens tokens, " +
                            "avg %.1f s per round, avg %.1f tok/s".format(averageMs / 1000.0, averageTps)
                        generating = false
                    }
                },
                onError = { message ->
                    benchmarkResult = "Benchmark failed: $message"
                    generating = false
                }
            )
        }
        nextRound()
    }

    DisposableEffect(Unit) {
        onDispose {
            controller.release()
            downloader.shutdown()
        }
    }

    LaunchedEffect(Unit) {
        refreshLocalModels()
        conversationNames.clear()
        conversationNames.addAll(conversationStore.listConversations())
        val saved = conversationStore.load()
        systemPrompt.value = saved.systemPrompt
        messages.clear()
        messages.addAll(saved.messages)
        if (controller.hasStoredModel()) {
            statusText = "Loading saved model..."
            controller.loadStoredModel(
                loadParams.value,
                onLoaded = { handleModelLoaded(it) },
                onError = { handleModelError(it) }
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
                onLoaded = { handleModelLoaded(it) },
                onError = { handleModelError(it) }
            )
        }
    }

    val textPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            errorText = null
            try {
                val content = context.contentResolver.openInputStream(uri)?.use { stream ->
                    stream.bufferedReader(Charsets.UTF_8).use { it.readText() }
                } ?: throw java.io.IOException("Unable to read the attached file")
                val bounded = content.take(MAX_ATTACH_BYTES)
                input = if (input.isBlank()) bounded else "$input\n\n$bounded"
                if (bounded.length < content.length) {
                    errorText = "Attached the first ${MAX_ATTACH_BYTES / 1024} KB of the file."
                }
            } catch (t: Throwable) {
                errorText = t.message ?: "Unable to read the attached file"
            }
        }
    }

    fun startDownload(entry: ModelCatalogEntry) {
        errorText = null
        downloadErrors.remove(entry.id)
        downloading[entry.id] = true
        downloadProgress[entry.id] = 0f
        downloader.download(
            entry,
            onProgress = { percent, _, _ ->
                downloadProgress[entry.id] = if (percent >= 0) {
                    percent / 100f
                } else {
                    -1f
                }
            },
            onDone = { file ->
                downloading.remove(entry.id)
                downloadProgress[entry.id] = 1f
                downloadErrors.remove(entry.id)
                statusText = "Download complete. Loading..."
                controller.loadModelFile(
                    file,
                    loadParams.value,
                    onLoaded = { handleModelLoaded(it) },
                    onError = { handleModelError(it) }
                )
            },
            onError = { message ->
                downloading.remove(entry.id)
                downloadErrors[entry.id] = message
                errorText = message
            }
        )
    }

    fun useModel(file: File) {
        errorText = null
        statusText = "Loading model..."
        controller.loadModelFile(
            file,
            loadParams.value,
            onLoaded = { handleModelLoaded(it) },
            onError = { handleModelError(it) }
        )
    }

    fun applyProfile(profile: String) {
        val base = sampling.value
        sampling.value = when (profile) {
            "fast" -> base.copy(
                temperature = 0.60f, topP = 0.90f, topK = 30, minP = 0.00f,
                typicalP = 0.0f, repeatPenalty = 1.05f, maxTokens = 256
            )
            "creative" -> base.copy(
                temperature = 1.00f, topP = 0.98f, topK = 60, minP = 0.05f,
                typicalP = 0.0f, repeatPenalty = 1.20f, maxTokens = 512
            )
            "code" -> base.copy(
                temperature = 0.20f, topP = 0.90f, topK = 20, minP = 0.00f,
                typicalP = 0.0f, repeatPenalty = 1.20f, maxTokens = 1024
            )
            "precise" -> base.copy(
                temperature = 0.00f, topP = 0.90f, topK = 20, minP = 0.00f,
                typicalP = 0.0f, repeatPenalty = 1.00f, maxTokens = 650
            )
            else -> base.copy(
                temperature = 0.70f, topP = 0.95f, topK = 40, minP = 0.00f,
                typicalP = 0.0f, repeatPenalty = 1.10f, maxTokens = 512
            )
        }
        settingsStore.saveSampling(sampling.value)
    }

    fun compactConversation() {
        if (generating || modelName == null || messages.size < 2) {
            return
        }
        val transcript = StringBuilder()
        for (message in messages) {
            if (message.role != "system") {
                transcript.append(message.role).append(": ").append(message.content).append("\n\n")
            }
        }
        val ask = listOf(
            ChatMessage(
                "system",
                "You are a memory compression assistant. Preserve facts, decisions, the user's preferences, file paths, and unfinished work. Keep it compact and complete."
            ),
            ChatMessage(
                "user",
                "Compress this conversation into a compact bullet summary.\n\n<conversation>\n$transcript</conversation>\n"
            )
        )
        draft = ""
        stats = null
        errorText = null
        generating = true
        controller.generate(
            ask,
            sampling.value.copy(temperature = 0.2f, maxTokens = 512),
            emptyList(),
            onToken = { part -> draft += part },
            onDone = { result ->
                val summary = draft.trim()
                messages.clear()
                messages.add(ChatMessage("user", "[Compacted conversation]"))
                if (summary.isNotEmpty()) {
                    messages.add(ChatMessage("assistant", summary))
                }
                conversationStore.save(systemPrompt.value, messages.toList())
                recordStats(result)
                stats = result
                generating = false
                draft = ""
            },
            onError = { message ->
                errorText = message
                generating = false
                draft = ""
            }
        )
    }

    fun send() {
        val text = input.trim()
        if (text.isEmpty() || generating) {
            return
        }
        messages.add(ChatMessage("user", text))
        conversationStore.save(systemPrompt.value, messages.toList())
        input = ""
        draft = ""
        stats = null
        generating = true
        val full = listOf(ChatMessage("system", effectiveSystemPrompt())) + messages.toList()
        controller.generate(
            full,
            sampling.value,
            stopSequences.toList(),
            onToken = { part -> draft += part },
            onDone = { result ->
                stats = result
                messages.add(ChatMessage("assistant", draft))
                recordStats(result)
                conversationStore.save(systemPrompt.value, messages.toList())
                draft = ""
                generating = false
            },
            onError = { message ->
                errorText = message
                conversationStore.save(systemPrompt.value, messages.toList())
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
            conversationStore.clear()
            draft = ""
            stats = null
            input = ""
            sessionStats = SessionStats()
        }
    }

    fun shareConversation() {
        val markdown = buildMarkdown(effectiveSystemPrompt(), messages.toList())
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_TEXT, markdown)
        }
        context.startActivity(Intent.createChooser(intent, "Export conversation"))
    }

    fun copyLastAnswer() {
        val text = messages.lastOrNull { it.role == "assistant" }?.content
            ?: draft.takeIf { it.isNotBlank() }
            ?: return
        val clipboard = context.getSystemService(android.content.ClipboardManager::class.java)
        clipboard.setPrimaryClip(android.content.ClipData.newPlainText("SkiffLLM answer", text))
        android.widget.Toast.makeText(context, "Answer copied", Toast.LENGTH_SHORT).show()
    }

        Scaffold(
            topBar = {
                ChatTopBar(
                    statusText = statusText,
                    modelName = modelName,
                    modelInfo = modelInfo,
                    conversationName = conversationName.value,
                    onConversations = { showConversations = true },
                    onSettings = { showSettings = true },
                    onCopy = { copyLastAnswer() },
                    onExport = { shareConversation() },
                    onClear = { clearConversation() }
                )
            }
        )
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(12.dp)
        ) {
            if (generating || warmingUp) {
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
                val capacity = loadParams.value.contextSize.coerceAtLeast(1)
                val used = it.promptTokens + it.generatedTokens
                val fraction = (used.toFloat() / capacity.toFloat()).coerceIn(0f, 1f)
                LinearProgressIndicator(
                    progress = { fraction },
                    modifier = Modifier.fillMaxWidth().padding(top = 6.dp)
                )
                Text(
                    "Context: $used/$capacity tokens (${(fraction * 100).toInt()}%) · " +
                        "${it.generatedTokens} tokens in ${"%.1f".format(it.generationMs / 1000.0)} s " +
                        "at ${"%.1f".format(it.tokensPerSecond)} tok/s",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
            }
            if (sessionStats.generatedTokens > 0L || sessionStats.messageCount > 0) {
                Text(
                    "Session: ${sessionStats.messageCount} messages · " +
                        "${sessionStats.promptTokens} prompt tokens · " +
                        "${sessionStats.generatedTokens} generated · " +
                        "avg ${"%.1f".format(sessionStats.avgTokensPerSecond)} tok/s",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
            }
            benchmarkResult?.let { result ->
                Surface(
                    color = MaterialTheme.colorScheme.surface,
                    shape = RoundedCornerShape(12.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Column(modifier = Modifier.padding(10.dp)) {
                        Text(
                            "Benchmark",
                            style = MaterialTheme.typography.labelLarge,
                            color = MaterialTheme.colorScheme.primary
                        )
                        Text(result, style = MaterialTheme.typography.bodySmall)
                        Row {
                            TextButton(onClick = { benchmarkResult = null }) {
                                Text("Dismiss")
                            }
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
            }
            sharedText?.let { shared ->
                Surface(
                    color = MaterialTheme.colorScheme.surface,
                    shape = RoundedCornerShape(12.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Column(modifier = Modifier.padding(10.dp)) {
                        Text(
                            "Shared from another app",
                            style = MaterialTheme.typography.labelLarge,
                            color = MaterialTheme.colorScheme.primary
                        )
                        Text(
                            shared.replace("\n", " ").take(160),
                            style = MaterialTheme.typography.bodySmall,
                            color = Color(0xFF8A94A6),
                            maxLines = 3,
                            overflow = TextOverflow.Ellipsis
                        )
                        Row {
                            TextButton(onClick = {
                                input = shared
                                SharedIntent.clear()
                            }) {
                                Text("Use")
                            }
                            TextButton(onClick = { SharedIntent.clear() }) {
                                Text("Dismiss")
                            }
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
            }
            if (quickPrompts.isNotEmpty()) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState())
                ) {
                    quickPrompts.forEach { prompt ->
                        OutlinedButton(
                            onClick = { input = prompt },
                            shape = RoundedCornerShape(14.dp)
                        ) {
                            Text(prompt, maxLines = 1)
                        }
                        Spacer(Modifier.width(6.dp))
                    }
                }
                Spacer(Modifier.height(6.dp))
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
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Text,
                        imeAction = ImeAction.Send
                    ),
                    keyboardActions = KeyboardActions(onSend = { send() })
                )
                Spacer(Modifier.width(8.dp))
                OutlinedButton(onClick = { textPicker.launch(arrayOf("text/*", "application/json", "application/xml")) }) {
                    Text("Attach")
                }
                Spacer(Modifier.width(6.dp))
                if (generating) {
                    OutlinedButton(onClick = { stop() }) {
                        Text("Stop")
                    }
                } else {
                    Button(onClick = { send() }, enabled = !warmingUp) {
                        Text("Send")
                    }
                }
            }
        }
    }

    if (showSettings) {
        SettingsDialog(
            loadParams = loadParams.value,
            onLoadParams = {
                loadParams.value = it
                settingsStore.saveLoadParams(it)
            },
            sampling = sampling.value,
            onSampling = {
                sampling.value = it
                settingsStore.saveSampling(it)
            },
            systemPrompt = systemPrompt.value,
            onSystemPrompt = {
                systemPrompt.value = it
                settingsStore.saveSystemPrompt(it)
            },
            persistentFacts = persistentFacts.value,
            onPersistentFacts = {
                persistentFacts.value = it
                settingsStore.savePersistentFacts(it)
            },
            quickPrompts = quickPrompts.toList(),
            onAddQuickPrompt = { value ->
                settingsStore.addQuickPrompt(value)
                quickPrompts.clear()
                quickPrompts.addAll(settingsStore.loadQuickPrompts())
            },
            onRemoveQuickPrompt = { value ->
                settingsStore.removeQuickPrompt(value)
                quickPrompts.clear()
                quickPrompts.addAll(settingsStore.loadQuickPrompts())
            },
            stopSequences = stopSequences.toList(),
            onAddStopSequence = { value ->
                settingsStore.addStopSequence(value)
                stopSequences.clear()
                stopSequences.addAll(settingsStore.loadStopSequences())
            },
            onRemoveStopSequence = { value ->
                settingsStore.removeStopSequence(value)
                stopSequences.clear()
                stopSequences.addAll(settingsStore.loadStopSequences())
            },
            onApplyProfile = { profile -> applyProfile(profile) },
            onCompact = {
                showSettings = false
                compactConversation()
            },
            onWarmup = {
                showSettings = false
                warmModel()
            },
            onBenchmark = {
                showSettings = false
                runBenchmark()
            },
            codeMode = codeMode.value,
            onCodeMode = { enabled ->
                codeMode.value = enabled
                settingsStore.saveCodeMode(enabled)
            },
            themeName = themeName,
            onThemeName = onThemeName,
            onOpenModels = {
                showSettings = false
                showModels = true
            },
            onAbout = {
                showSettings = false
                showAbout = true
            },
            onDismiss = { showSettings = false }
        )
    }

    if (showConversations) {
        ConversationsDialog(
            conversations = conversationNames.toList(),
            current = conversationName.value,
            onOpen = { name ->
                conversationStore.switchTo(name)
                val snap = conversationStore.load()
                systemPrompt.value = snap.systemPrompt
                messages.clear()
                messages.addAll(snap.messages)
                conversationName.value = name
                conversationNames.clear()
                conversationNames.addAll(conversationStore.listConversations())
                stats = null
                draft = ""
                input = ""
                sessionStats = SessionStats()
                showConversations = false
            },
            onCreate = { name ->
                val finalName = conversationStore.create(name)
                val snap = conversationStore.load()
                systemPrompt.value = snap.systemPrompt
                messages.clear()
                messages.addAll(snap.messages)
                conversationName.value = finalName
                conversationNames.clear()
                conversationNames.addAll(conversationStore.listConversations())
                stats = null
                draft = ""
                input = ""
                sessionStats = SessionStats()
                showConversations = false
            },
            onDelete = { name ->
                conversationStore.delete(name)
                if (conversationName.value == name) {
                    conversationName.value = conversationStore.currentName()
                    val snap = conversationStore.load()
                    systemPrompt.value = snap.systemPrompt
                    messages.clear()
                    messages.addAll(snap.messages)
                    stats = null
                    draft = ""
                    input = ""
                    sessionStats = SessionStats()
                }
                conversationNames.clear()
                conversationNames.addAll(conversationStore.listConversations())
            },
            onDismiss = { showConversations = false }
        )
    }

    if (showAbout) {
        AboutDialog(onDismiss = { showAbout = false })
    }

    if (showModels) {
        ModelsDialog(
            localModels = localModels.toList(),
            loadedModelPath = controller.currentModelPath(),
            downloading = downloading.toMap(),
            downloadProgress = downloadProgress.toMap(),
            downloadErrors = downloadErrors.toMap(),
            onUse = { file ->
                showModels = false
                useModel(file)
            },
            onDelete = { file ->
                controller.deleteModel(file)
                refreshLocalModels()
            },
            onDownload = { entry ->
                startDownload(entry)
            },
            onCancel = { entry -> downloader.cancel(entry.id) },
            onBrowse = {
                showModels = false
                modelPicker.launch(arrayOf("*/*"))
            },
            onDismiss = { showModels = false }
        )
    }
}

@Composable
private fun ChatTopBar(
    statusText: String,
    modelName: String?,
    modelInfo: String?,
    conversationName: String,
    onConversations: () -> Unit,
    onSettings: () -> Unit,
    onCopy: () -> Unit,
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
                    color = MaterialTheme.colorScheme.primary
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    conversationName,
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.secondary,
                    modifier = Modifier.weight(1f)
                )
                TextButton(onClick = { onConversations() }) {
                    Text("Chats")
                }
                TextButton(onClick = { onCopy() }) {
                    Text("Copy")
                }
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
                color = Color(0xFF8A94A6),
                maxLines = 1
            )
            modelInfo?.let {
                Text(
                    it,
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
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
    persistentFacts: String,
    onPersistentFacts: (String) -> Unit,
    quickPrompts: List<String>,
    onAddQuickPrompt: (String) -> Unit,
    onRemoveQuickPrompt: (String) -> Unit,
    stopSequences: List<String>,
    onAddStopSequence: (String) -> Unit,
    onRemoveStopSequence: (String) -> Unit,
    onApplyProfile: (String) -> Unit,
    onCompact: () -> Unit,
    onWarmup: () -> Unit,
    onBenchmark: () -> Unit,
    codeMode: Boolean,
    onCodeMode: (Boolean) -> Unit,
    themeName: String,
    onThemeName: (String) -> Unit,
    onOpenModels: () -> Unit,
    onAbout: () -> Unit,
    onDismiss: () -> Unit
) {
    var quickPromptInput by remember { mutableStateOf("") }
    var stopSequenceInput by remember { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = { onDismiss() },
        confirmButton = {
            TextButton(onClick = { onDismiss() }) {
                Text("Done")
            }
        },
        dismissButton = {
            TextButton(onClick = { onOpenModels() }) {
                Text("Models")
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
                Spacer(Modifier.height(8.dp))
                Text(
                    "Profile presets",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                listOf("balanced", "fast", "creative", "code", "precise").forEach { name ->
                    TextButton(onClick = { onApplyProfile(name) }) {
                        Text(name.replaceFirstChar { it.uppercase() })
                    }
                }
                Text(
                    "Applies the sampling values shown below immediately. Fine-tune after choosing a preset.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                Spacer(Modifier.height(8.dp))
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
                    label = "Min-p",
                    value = sampling.minP.toString(),
                    decimal = true,
                    onValue = { value ->
                        value.toFloatOrNull()?.let { onSampling(sampling.copy(minP = it)) }
                    }
                )
                NumberField(
                    label = "Typical-p",
                    value = sampling.typicalP.toString(),
                    decimal = true,
                    onValue = { value ->
                        value.toFloatOrNull()?.let { onSampling(sampling.copy(typicalP = it)) }
                    }
                )
                NumberField(
                    label = "Repeat penalty",
                    value = sampling.repeatPenalty.toString(),
                    decimal = true,
                    onValue = { value ->
                        value.toFloatOrNull()?.let { onSampling(sampling.copy(repeatPenalty = it)) }
                    }
                )
                NumberField(
                    label = "Repeat last N",
                    value = sampling.repeatLastN.toString(),
                    onValue = { value ->
                        value.toIntOrNull()?.let { onSampling(sampling.copy(repeatLastN = it)) }
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
                OutlinedTextField(
                    value = persistentFacts,
                    onValueChange = { onPersistentFacts(it) },
                    label = { Text("Persistent facts (always injected)") },
                    minLines = 2,
                    modifier = Modifier.fillMaxWidth()
                )
                Text(
                    "Keep facts like \"user prefers concise answers\" here. They stay on this device and are applied every turn.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "Quick prompts",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                if (quickPrompts.isEmpty()) {
                    Text(
                        "No quick prompts yet. Add one below.",
                        style = MaterialTheme.typography.bodySmall,
                        color = Color(0xFF8A94A6)
                    )
                } else {
                    quickPrompts.forEach { prompt ->
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                prompt,
                                style = MaterialTheme.typography.bodySmall,
                                modifier = Modifier
                                    .weight(1f)
                                    .padding(vertical = 2.dp)
                            )
                            TextButton(onClick = { onRemoveQuickPrompt(prompt) }) {
                                Text("Remove")
                            }
                        }
                    }
                }
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    OutlinedTextField(
                        value = quickPromptInput,
                        onValueChange = { quickPromptInput = it },
                        label = { Text("New quick prompt") },
                        singleLine = true,
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    )
                    Spacer(Modifier.width(6.dp))
                    TextButton(onClick = {
                        onAddQuickPrompt(quickPromptInput)
                        quickPromptInput = ""
                    }) {
                        Text("Add")
                    }
                }
                Spacer(Modifier.height(8.dp))
                Text(
                    "Stop sequences",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                if (stopSequences.isEmpty()) {
                    Text(
                        "No stop sequences yet. Add one below.",
                        style = MaterialTheme.typography.bodySmall,
                        color = Color(0xFF8A94A6)
                    )
                } else {
                    stopSequences.forEach { sequence ->
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                sequence,
                                style = MaterialTheme.typography.bodySmall,
                                modifier = Modifier
                                    .weight(1f)
                                    .padding(vertical = 2.dp)
                            )
                            TextButton(onClick = { onRemoveStopSequence(sequence) }) {
                                Text("Remove")
                            }
                        }
                    }
                }
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    OutlinedTextField(
                        value = stopSequenceInput,
                        onValueChange = { stopSequenceInput = it },
                        label = { Text("New stop sequence") },
                        singleLine = true,
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    )
                    Spacer(Modifier.width(6.dp))
                    TextButton(onClick = {
                        onAddStopSequence(stopSequenceInput)
                        stopSequenceInput = ""
                    }) {
                        Text("Add")
                    }
                }
                Spacer(Modifier.height(8.dp))
                Button(onClick = onCompact) {
                    Text("Compact conversation")
                }
                Text(
                    "Compacts the current conversation into a bullet summary using the loaded model.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                Spacer(Modifier.height(8.dp))
                Button(onClick = onWarmup) {
                    Text("Warm up model")
                }
                Text(
                    "Runs one short decode pass to reduce first-answer latency.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                Spacer(Modifier.height(6.dp))
                Button(onClick = onBenchmark) {
                    Text("Run 3-round benchmark")
                }
                Text(
                    "Runs three real generations and reports measured tokens, time, and tok/s.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "Code mode",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(
                        "Request unified-diff proposals (never editing files)",
                        style = MaterialTheme.typography.bodySmall,
                        modifier = Modifier.weight(1f)
                    )
                    Switch(
                        checked = codeMode,
                        onCheckedChange = onCodeMode
                    )
                }
                Text(
                    "Appends the safe-code instructions to the system prompt. Outputs are proposals; apply them manually.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "Theme",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    listOf("dark", "light", "system").forEach { name ->
                        if (themeName == name) {
                            Button(onClick = { onThemeName(name) }, modifier = Modifier.padding(end = 6.dp)) {
                                Text(name.replaceFirstChar { it.uppercase() })
                            }
                        } else {
                            OutlinedButton(onClick = { onThemeName(name) }, modifier = Modifier.padding(end = 6.dp)) {
                                Text(name.replaceFirstChar { it.uppercase() })
                            }
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
                TextButton(onClick = { onAbout() }) {
                    Text("About SkiffLLM")
                }
                Text(
                    "Use Models to download a recommended Q4_K_M GGUF from Hugging Face, or load your own file from the device. Downloads use HTTPS only and no telemetry is sent.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
            }
        }
    )
}

@Composable
private fun ConversationsDialog(
    conversations: List<String>,
    current: String,
    onOpen: (String) -> Unit,
    onCreate: (String) -> Unit,
    onDelete: (String) -> Unit,
    onDismiss: () -> Unit
) {
    var newName by remember { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = { onDismiss() },
        confirmButton = {
            TextButton(onClick = { onDismiss() }) {
                Text("Done")
            }
        },
        title = { Text("Conversations") },
        text = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState())
            ) {
                Text(
                    "Current: $current",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.primary
                )
                Spacer(Modifier.height(8.dp))
                conversations.forEach { name ->
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            name,
                            style = MaterialTheme.typography.bodyMedium,
                            modifier = Modifier
                                .weight(1f)
                                .padding(vertical = 4.dp)
                        )
                        if (name == current) {
                            Text(
                                "Active",
                                style = MaterialTheme.typography.bodySmall,
                                color = Color(0xFF8A94A6)
                            )
                        } else {
                            TextButton(onClick = { onOpen(name) }) {
                                Text("Open")
                            }
                            TextButton(onClick = { onDelete(name) }) {
                                Text("Delete")
                            }
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    OutlinedTextField(
                        value = newName,
                        onValueChange = { newName = it },
                        label = { Text("New conversation") },
                        singleLine = true,
                        modifier = Modifier.weight(1f)
                    )
                    Spacer(Modifier.width(6.dp))
                    TextButton(onClick = {
                        val name = newName.trim()
                        if (name.isNotBlank()) {
                            onCreate(name)
                            newName = ""
                        }
                    }) {
                        Text("Create")
                    }
                }
                Spacer(Modifier.height(8.dp))
                Text(
                    "Conversations stay on this device. Clear removes the current messages.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
            }
        }
    )
}

@Composable
private fun ModelsDialog(
    localModels: List<File>,
    loadedModelPath: String?,
    downloading: Map<String, Boolean>,
    downloadProgress: Map<String, Float>,
    downloadErrors: Map<String, String>,
    onUse: (File) -> Unit,
    onDelete: (File) -> Unit,
    onDownload: (ModelCatalogEntry) -> Unit,
    onCancel: (ModelCatalogEntry) -> Unit,
    onBrowse: () -> Unit,
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
            TextButton(onClick = { onBrowse() }) {
                Text("Browse device")
            }
        },
        title = { Text("Models") },
        text = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState())
            ) {
                if (localModels.isEmpty()) {
                    Text(
                        "No models downloaded yet. Pick one below or load a GGUF from your device.",
                        style = MaterialTheme.typography.bodySmall,
                        color = Color(0xFF8A94A6)
                    )
                    Spacer(Modifier.height(8.dp))
                } else {
                    Text(
                        "Downloaded on this device",
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.primary
                    )
                    localModels.forEach { file ->
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                file.name,
                                style = MaterialTheme.typography.bodyMedium,
                                modifier = Modifier
                                    .weight(1f)
                                    .padding(vertical = 4.dp)
                            )
                            TextButton(onClick = { onUse(file) }) {
                                Text("Use")
                            }
                            if (file.absolutePath != loadedModelPath) {
                                TextButton(onClick = { onDelete(file) }) {
                                    Text("Delete")
                                }
                            }
                        }
                    }
                    Spacer(Modifier.height(8.dp))
                }

                Text(
                    "Recommended on-device models",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                Spacer(Modifier.height(4.dp))

                ModelCatalog.models.forEach { entry ->
                    val isDownloaded = localModels.any { it.name == entry.file }
                    val isDownloading = downloading[entry.id] == true
                    val progress = downloadProgress[entry.id] ?: 0f

                    Column(modifier = Modifier.fillMaxWidth()) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    entry.name,
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = MaterialTheme.colorScheme.onSurface
                                )
                                Text(
                                    "${entry.sizeText} · ${entry.ramNote}",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = Color(0xFF8A94A6)
                                )
                            }
                            if (isDownloaded) {
                                TextButton(onClick = {
                                    localModels.firstOrNull { it.name == entry.file }?.let { onUse(it) }
                                }) {
                                    Text("Use")
                                }
                            } else if (isDownloading) {
                                TextButton(onClick = { onCancel(entry) }) {
                                    Text("Cancel")
                                }
                            } else {
                                Button(onClick = { onDownload(entry) }) {
                                    Text("Download")
                                }
                            }
                        }
                        if (!isDownloaded && isDownloading) {
                            if (progress >= 0f) {
                                LinearProgressIndicator(
                                    progress = { progress },
                                    modifier = Modifier.fillMaxWidth()
                                )
                                Text(
                                    "${(progress * 100).toInt()}%",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = Color(0xFF8A94A6)
                                )
                            } else {
                                LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                                Text(
                                    "Downloading…",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = Color(0xFF8A94A6)
                                )
                            }
                        }
                        if (entry.recommended) {
                            Text(
                                "Recommended · ${entry.description}",
                                style = MaterialTheme.typography.bodySmall,
                                color = Color(0xFF8A94A6)
                            )
                        } else {
                            Text(
                                entry.description,
                                style = MaterialTheme.typography.bodySmall,
                                color = Color(0xFF8A94A6)
                            )
                        }
                        downloadErrors[entry.id]?.let { error ->
                            Text(
                                error,
                                style = MaterialTheme.typography.bodySmall,
                                color = Color(0xFFFF7A7A)
                            )
                        }
                        Spacer(Modifier.height(8.dp))
                    }
                }
            }
        }
    )
}

@Composable
private fun AboutDialog(onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = { onDismiss() },
        confirmButton = {
            TextButton(onClick = { onDismiss() }) {
                Text("Done")
            }
        },
        title = { Text("About SkiffLLM") },
        text = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState())
            ) {
                Text(
                    "Version 1.6.0 (Android)",
                    style = MaterialTheme.typography.titleMedium,
                    color = MaterialTheme.colorScheme.primary
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "An offline-first local LLM assistant built on llama.cpp. " +
                        "Models run entirely on this device.",
                    style = MaterialTheme.typography.bodyMedium
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "Privacy",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                Text(
                    "Inference, chat, export, and file loading stay on device. " +
                        "The only network use is optional HTTPS model downloads from Hugging Face. " +
                        "No prompts, history, analytics, or crash reports are sent.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "Models",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                Text(
                    "Use the Models dialog to download a recommended Q4_K_M GGUF " +
                        "or load your own file from the device.",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color(0xFF8A94A6)
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "License: MIT",
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

private const val MAX_ATTACH_BYTES = 64 * 1024

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
