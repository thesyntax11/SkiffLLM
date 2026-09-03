import SwiftUI
import UIKit
import UniformTypeIdentifiers

private let MAX_ATTACH_BYTES = 64 * 1024

final class AppState: ObservableObject {
    let engine = NativeEngine()
    let settings = SettingsStore()
    let conversations = ConversationStore()
    let downloader = ModelDownloader()

    @Published var messages: [ChatMessage] = []
    @Published var draft = ""
    @Published var input = ""
    @Published var generating = false
    @Published var warmingUp = false
    @Published var loadingModel = false
    @Published var benchmarkResult: String?
    @Published var stats: GenerationStats?
    @Published var sessionStats = SessionStats()
    @Published var errorMessage: String?
    @Published var modelName: String?
    @Published var modelInfo: String?
    @Published var currentConversation = "default"
    @Published var conversationList: [String] = []
    @Published var theme: Theme = .system
    @Published var loadParams: LoadParams = LoadParams()
    @Published var sampling: SamplingParams = SamplingParams()
    @Published var systemPrompt: String = ""
    @Published var persistentFacts: String = ""
    @Published var quickPrompts: [String] = []
    @Published var stopSequences: [String] = []
    @Published var codeMode = false
    @Published var sharedText: String?
    @Published var localModels: [URL] = []
    @Published var showSettings = false
    @Published var showModels = false
    @Published var showConversations = false

    init() {
        let threads = max(2, ProcessInfo.processInfo.activeProcessorCount)
        loadParams = settings.loadLoadParams(defaultThreads: threads)
        sampling = settings.loadSampling()
        systemPrompt = settings.loadSystemPrompt()
        persistentFacts = settings.loadPersistentFacts()
        quickPrompts = settings.loadQuickPrompts()
        stopSequences = settings.loadStopSequences()
        codeMode = settings.loadCodeMode()
        theme = settings.loadTheme()
        currentConversation = conversations.currentName()
        let snap = conversations.load()
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        if let savedSampling = snap.sampling {
            sampling = savedSampling
            settings.saveSampling(savedSampling)
        }
        if let savedCodeMode = snap.codeMode {
            codeMode = savedCodeMode
            settings.saveCodeMode(savedCodeMode)
        }
        refreshModels()
        refreshConversations()
        sharedText = SharedTextStore.shared.peek()
        downloader.onDownloadCompleted = { [weak self] url in
            DispatchQueue.main.async { self?.useModel(url) }
        }
        autoLoadLastModel()
    }

    deinit {
        engine.close()
    }

    func effectiveSystemPrompt() -> String {
        let base = systemPrompt.isEmpty ? "You are SkiffLLM, a helpful local assistant." : systemPrompt
        let facts = persistentFacts.trimmingCharacters(in: .whitespacesAndNewlines)
        let withFacts = facts.isEmpty ? base : "\(base)\n\nPersistent facts:\n\(facts)"
        guard codeMode else { return withFacts }
        return withFacts + "\n\nYou are a careful coding assistant. Propose concrete " +
            "edits as a unified diff (file paths, +/-, context). Use the code " +
            "context below. Never claim a file was changed; output the proposal " +
            "only and wait for a human to apply it."
    }

    func refreshModels() {
        localModels = downloader.listDownloadedModels()
    }

    func refreshConversations() {
        conversationList = conversations.listConversations()
        currentConversation = conversations.currentName()
    }

    func refreshSharedText() {
        if let value = SharedTextStore.shared.take() {
            sharedText = value
        }
    }

    func useSharedText() {
        if let value = sharedText {
            input = value
        }
        sharedText = nil
        SharedTextStore.shared.clear()
    }

    func dismissSharedText() {
        sharedText = nil
        SharedTextStore.shared.clear()
    }

    func exportMarkdown() -> String {
        conversationMarkdown(systemPrompt: effectiveSystemPrompt(), messages: messages)
    }

    func copyLastAnswer() {
        let text = messages.last(where: { $0.role == "assistant" })?.content ?? draft
        guard !text.isEmpty else { return }
        UIPasteboard.general.string = text
    }

    func applyProfile(_ profile: SamplingProfile) {
        sampling = profile.apply(to: sampling)
        settings.saveSampling(sampling)
    }

    private func recordStats(_ result: GenerationStats, messageCount: Int) {
        let prompt = sessionStats.promptTokens + result.promptTokens
        let generated = sessionStats.generatedTokens + result.generatedTokens
        let elapsed = sessionStats.generationMs + result.generationMs
        let average = elapsed > 0 ? Double(generated) / (elapsed / 1000.0) : sessionStats.avgTokensPerSecond
        sessionStats = SessionStats(messageCount: messageCount,
                                    promptTokens: prompt,
                                    generatedTokens: generated,
                                    generationMs: elapsed,
                                    avgTokensPerSecond: average)
    }

    private func resetSessionStats() {
        sessionStats = SessionStats()
        stats = nil
        benchmarkResult = nil
    }

    func persistConversationSettings() {
        conversations.save(systemPrompt: systemPrompt, messages: messages, sampling: sampling, codeMode: codeMode)
    }

    func warmUp() {
        guard !generating, !warmingUp, !loadingModel, modelName != nil else { return }
        warmingUp = true
        errorMessage = nil
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self else { return }
            var error: NSError?
            let ok = self.engine.warmup(&error)
            DispatchQueue.main.async {
                self.warmingUp = false
                if ok {
                    self.errorMessage = nil
                } else {
                    self.errorMessage = error?.localizedDescription ?? "Warm-up failed."
                }
            }
        }
    }

    func runBenchmark() {
        guard !generating, !warmingUp, !loadingModel, modelName != nil else { return }
        benchmarkResult = nil
        stats = nil
        errorMessage = nil
        generating = true
        var results: [GenerationStats] = []
        let payload: [[String: String]] = [
            ["role": "user", "content": "Write one short sentence explaining what a local LLM is."],
        ]

        func nextRound() {
            engine.generateMessages(payload,
                                    temperature: 0.0,
                                    topP: sampling.topP,
                                    topK: Int32(sampling.topK),
                                    minP: sampling.minP,
                                    typicalP: sampling.typicalP,
                                    repeatPenalty: sampling.repeatPenalty,
                                    repeatLastN: Int32(sampling.repeatLastN),
                                    maxTokens: 128,
                                    seed: sampling.seed,
                                    stopSequences: [],
                                    tokenCallback: { _ in },
                                    completion: { [weak self] result, error in
                guard let self else { return }
                if let result {
                    results.append(GenerationStats(promptTokens: Int(result.promptTokens),
                                                   generatedTokens: Int(result.generatedTokens),
                                                   promptMs: result.promptMs,
                                                   generationMs: result.generationMs,
                                                   tokensPerSecond: result.tokensPerSecond,
                                                   stopped: result.stopped))
                    if results.count < 3 {
                        nextRound()
                    } else {
                        let averageTps = results.map { $0.tokensPerSecond }.reduce(0, +) / Double(results.count)
                        let averageMs = results.map { $0.generationMs }.reduce(0, +) / Double(results.count)
                        let totalTokens = results.map { $0.generatedTokens }.reduce(0, +)
                        self.generating = false
                        self.benchmarkResult = String(format: "Benchmark (3 rounds): %d tokens, avg %.1f s per round, avg %.1f tok/s",
                                                      totalTokens, averageMs / 1000.0, averageTps)
                    }
                } else {
                    self.generating = false
                    self.benchmarkResult = "Benchmark failed: \(error?.localizedDescription ?? "Unknown error")"
                }
            })
        }
        nextRound()
    }

    func compactConversation() {
        guard !generating, !loadingModel, modelName != nil, messages.count >= 2 else { return }
        let transcript = messages
            .filter { $0.role != "system" }
            .map { "\($0.role): \($0.content)" }
            .joined(separator: "\n\n")
        let payload: [[String: String]] = [
            ["role": "system",
             "content": "You are a memory compression assistant. Preserve facts, decisions, the user's preferences, file paths, and unfinished work. Keep it compact and complete."],
            ["role": "user",
             "content": "Compress this conversation into a compact bullet summary.\n\n<conversation>\n\(transcript)\n</conversation>\n"],
        ]
        generating = true
        draft = ""
        stats = nil
        errorMessage = nil
        let compactBatcher = TokenBatcher { [weak self] chunk in
            self?.draft += chunk
        }
        engine.generateMessages(payload,
                                temperature: 0.2,
                                topP: sampling.topP,
                                topK: Int32(sampling.topK),
                                minP: sampling.minP,
                                typicalP: sampling.typicalP,
                                repeatPenalty: sampling.repeatPenalty,
                                repeatLastN: Int32(sampling.repeatLastN),
                                maxTokens: 512,
                                seed: sampling.seed,
                                stopSequences: [],
                                tokenCallback: { [ compactBatcher ] part in
                                    compactBatcher.append(part ?? "")
                                },
                                completion: { [weak self] result, error in
                                    guard let self else { return }
                                    self.generating = false
                                    if error != nil || self.draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                                        self.errorMessage = error?.localizedDescription ?? "Compaction failed."
                                    } else {
                                        let summary = self.draft
                                        self.messages.removeAll()
                                        self.messages.append(ChatMessage(role: "user", content: "[Compacted conversation]"))
                                        self.messages.append(ChatMessage(role: "assistant", content: summary))
                                        self.conversations.save(systemPrompt: self.systemPrompt, messages: self.messages, sampling: self.sampling, codeMode: self.codeMode)
                                    }
                                    if let result {
                                        self.recordStats(GenerationStats(promptTokens: Int(result.promptTokens),
                                                                        generatedTokens: Int(result.generatedTokens),
                                                                        promptMs: result.promptMs,
                                                                        generationMs: result.generationMs,
                                                                        tokensPerSecond: result.tokensPerSecond,
                                                                        stopped: result.stopped),
                                                         messageCount: self.messages.count)
                                        self.stats = GenerationStats(
                                            promptTokens: Int(result.promptTokens),
                                            generatedTokens: Int(result.generatedTokens),
                                            promptMs: result.promptMs,
                                            generationMs: result.generationMs,
                                            tokensPerSecond: result.tokensPerSecond,
                                            stopped: result.stopped
                                        )
                                    }
                                    self.draft = ""
                                })
    }

    private func autoLoadLastModel() {
        guard let path = settings.loadLastModelPath(),
              FileManager.default.fileExists(atPath: path) else { return }
        useModel(URL(fileURLWithPath: path))
    }

    func useModel(_ url: URL) {
        guard !generating, !loadingModel else { return }
        modelName = nil
        modelInfo = nil
        errorMessage = nil
        loadingModel = true
        let params = loadParams
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self else { return }
            var error: NSError?
            let ok = self.engine.loadModel(atPath: url.path,
                                           contextSize: Int32(params.contextSize),
                                           threads: Int32(params.threads),
                                           gpuLayers: Int32(params.gpuLayers),
                                           chatTemplate: params.chatTemplate,
                                           error: &error)
            var description: String?
            if ok {
                description = self.engine.modelDescription()
                var warmError: NSError?
                _ = self.engine.warmup(&warmError)
            }
            DispatchQueue.main.async {
                self.loadingModel = false
                if ok {
                    self.modelName = url.lastPathComponent
                    self.modelInfo = description
                    self.settings.saveLastModelPath(url.path)
                } else {
                    self.errorMessage = error?.localizedDescription ?? "Failed to load model."
                }
            }
        }
    }

    func send() {
        let text = input.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty, !generating, modelName != nil else { return }
        messages.append(ChatMessage(role: "user", content: text))
        input = ""
        draft = ""
        stats = nil
        errorMessage = nil
        generating = true

        var payload: [[String: String]] = []
        let system = effectiveSystemPrompt()
        if !system.isEmpty {
            payload.append(["role": "system", "content": system])
        }
        payload.append(contentsOf: messages.map { ["role": $0.role, "content": $0.content] })
        let msgs: [[String: String]] = payload
        let sendBatcher = TokenBatcher { [weak self] chunk in
            self?.draft += chunk
        }
        engine.generateMessages(msgs,
                                temperature: sampling.temperature,
                                topP: sampling.topP,
                                topK: Int32(sampling.topK),
                                minP: sampling.minP,
                                typicalP: sampling.typicalP,
                                repeatPenalty: sampling.repeatPenalty,
                                repeatLastN: Int32(sampling.repeatLastN),
                                maxTokens: Int32(sampling.maxTokens),
                                seed: sampling.seed,
                                stopSequences: stopSequences,
                                tokenCallback: { [ sendBatcher ] part in
                                    sendBatcher.append(part ?? "")
                                },
                                completion: { [weak self] result, error in
                                    guard let self else { return }
                                    self.generating = false
                                    if error != nil || self.draft.isEmpty {
                                        self.errorMessage = error?.localizedDescription ?? "Generation failed."
                                        if !self.messages.isEmpty { self.messages.removeLast() }
                                    } else {
                                        self.messages.append(ChatMessage(role: "assistant", content: self.draft))
                                        self.conversations.save(systemPrompt: self.systemPrompt, messages: self.messages, sampling: self.sampling, codeMode: self.codeMode)
                                        if let result {
                                            self.recordStats(GenerationStats(promptTokens: Int(result.promptTokens),
                                                                            generatedTokens: Int(result.generatedTokens),
                                                                            promptMs: result.promptMs,
                                                                            generationMs: result.generationMs,
                                                                            tokensPerSecond: result.tokensPerSecond,
                                                                            stopped: result.stopped),
                                                             messageCount: self.messages.count)
                                        }
                                    }
                                    if let result {
                                        self.stats = GenerationStats(
                                            promptTokens: Int(result.promptTokens),
                                            generatedTokens: Int(result.generatedTokens),
                                            promptMs: result.promptMs,
                                            generationMs: result.generationMs,
                                            tokensPerSecond: result.tokensPerSecond,
                                            stopped: result.stopped
                                        )
                                    }
                                    self.draft = ""
                                })
    }

    func regenerate() {
        guard !generating, !warmingUp, !loadingModel, modelName != nil,
              let lastUser = messages.lastIndex(where: { $0.role == "user" }) else { return }
        while messages.count > lastUser + 1 {
            messages.removeLast()
        }
        conversations.save(systemPrompt: systemPrompt, messages: messages, sampling: sampling, codeMode: codeMode)
        draft = ""
        stats = nil
        errorMessage = nil
        generating = true
        var payload: [[String: String]] = []
        let system = effectiveSystemPrompt()
        if !system.isEmpty {
            payload.append(["role": "system", "content": system])
        }
        payload.append(contentsOf: messages.map { ["role": $0.role, "content": $0.content] })
        let regenerateBatcher = TokenBatcher { [weak self] chunk in
            self?.draft += chunk
        }
        engine.generateMessages(payload,
                                temperature: sampling.temperature,
                                topP: sampling.topP,
                                topK: Int32(sampling.topK),
                                minP: sampling.minP,
                                typicalP: sampling.typicalP,
                                repeatPenalty: sampling.repeatPenalty,
                                repeatLastN: Int32(sampling.repeatLastN),
                                maxTokens: Int32(sampling.maxTokens),
                                seed: sampling.seed,
                                stopSequences: stopSequences,
                                tokenCallback: { [ regenerateBatcher ] part in
                                    regenerateBatcher.append(part ?? "")
                                },
                                completion: { [weak self] result, error in
                                    guard let self else { return }
                                    self.generating = false
                                    if error != nil || self.draft.isEmpty {
                                        self.errorMessage = error?.localizedDescription ?? "Generation failed."
                                    } else {
                                        self.messages.append(ChatMessage(role: "assistant", content: self.draft))
                                        self.conversations.save(systemPrompt: self.systemPrompt, messages: self.messages, sampling: self.sampling, codeMode: self.codeMode)
                                        if let result {
                                            self.recordStats(GenerationStats(promptTokens: Int(result.promptTokens),
                                                                            generatedTokens: Int(result.generatedTokens),
                                                                            promptMs: result.promptMs,
                                                                            generationMs: result.generationMs,
                                                                            tokensPerSecond: result.tokensPerSecond,
                                                                            stopped: result.stopped),
                                                             messageCount: self.messages.count)
                                        }
                                    }
                                    if let result {
                                        self.stats = GenerationStats(
                                            promptTokens: Int(result.promptTokens),
                                            generatedTokens: Int(result.generatedTokens),
                                            promptMs: result.promptMs,
                                            generationMs: result.generationMs,
                                            tokensPerSecond: result.tokensPerSecond,
                                            stopped: result.stopped
                                        )
                                    }
                                    self.draft = ""
                                })
    }

    func stop() {
        engine.stop()
    }

    func clear() {
        guard !generating else { return }
        messages.removeAll()
        draft = ""
        input = ""
        resetSessionStats()
        conversations.save(systemPrompt: systemPrompt, messages: [], sampling: sampling, codeMode: codeMode)
    }

    func newConversation(_ name: String) {
        let finalName = conversations.create(name)
        let snap = conversations.load()
        currentConversation = finalName
        refreshConversations()
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        if let savedSampling = snap.sampling {
            sampling = savedSampling
            settings.saveSampling(savedSampling)
        }
        if let savedCodeMode = snap.codeMode {
            codeMode = savedCodeMode
            settings.saveCodeMode(savedCodeMode)
        }
        draft = ""
        input = ""
        resetSessionStats()
    }

    func openConversation(_ name: String) {
        _ = conversations.switchTo(name)
        let snap = conversations.load()
        refreshConversations()
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        if let savedSampling = snap.sampling {
            sampling = savedSampling
            settings.saveSampling(savedSampling)
        }
        if let savedCodeMode = snap.codeMode {
            codeMode = savedCodeMode
            settings.saveCodeMode(savedCodeMode)
        }
        draft = ""
        input = ""
        resetSessionStats()
    }

    func deleteConversation(_ name: String) {
        conversations.delete(name)
        refreshConversations()
        currentConversation = conversations.currentName()
        let snap = conversations.load()
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        if let savedSampling = snap.sampling {
            sampling = savedSampling
            settings.saveSampling(savedSampling)
        }
        if let savedCodeMode = snap.codeMode {
            codeMode = savedCodeMode
            settings.saveCodeMode(savedCodeMode)
        }
        resetSessionStats()
    }

    func renameConversation(_ oldName: String, to newName: String) {
        let finalName = conversations.rename(oldName, to: newName)
        refreshConversations()
        if currentConversation == oldName {
            currentConversation = finalName
        }
    }

    func conversationBackupString() -> String {
        conversations.exportJson(systemPrompt: systemPrompt,
                                 messages: messages,
                                 sampling: sampling,
                                 codeMode: codeMode) ?? ""
    }

    @discardableResult
    func importConversationBackup(text: String, name: String) -> String? {
        guard let snap = conversations.importJson(from: text) else {
            return "Invalid conversation backup."
        }
        let finalName = conversations.create(name)
        conversations.save(systemPrompt: snap.systemPrompt,
                           messages: snap.messages,
                           sampling: snap.sampling,
                           codeMode: snap.codeMode)
        refreshConversations()
        currentConversation = finalName
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        if let savedSampling = snap.sampling {
            sampling = savedSampling
            settings.saveSampling(savedSampling)
        }
        if let savedCodeMode = snap.codeMode {
            codeMode = savedCodeMode
            settings.saveCodeMode(savedCodeMode)
        }
        resetSessionStats()
        return nil
    }
}

struct ChatView: View {
    @StateObject private var app = AppState()
    @Environment(\.scenePhase) private var scenePhase
    @State private var showFileImporter = false
    @State private var showTextImporter = false
    @State private var showBackupImporter = false

    var body: some View {
        VStack(spacing: 0) {
            topBar
            Divider()
            modelStatus
            messageList
            if let stats = app.stats, !app.generating {
                contextBar(stats)
            }
            if app.sessionStats.generatedTokens > 0 || app.sessionStats.messageCount > 0 {
                Text("Session: \(app.sessionStats.messageCount) messages · "
                     + "\(app.sessionStats.promptTokens) prompt tokens · "
                     + "\(app.sessionStats.generatedTokens) generated · "
                     + String(format: "avg %.1f tok/s", app.sessionStats.avgTokensPerSecond))
                    .font(.caption2)
                    .foregroundColor(.secondary)
                    .padding(.horizontal, 12)
                    .padding(.top, 2)
            }
            if let benchmark = app.benchmarkResult {
                HStack(spacing: 8) {
                    Text(benchmark)
                        .font(.caption2)
                        .foregroundColor(.secondary)
                    Spacer()
                    Button("Dismiss") { app.benchmarkResult = nil }
                        .font(.caption2)
                }
                .padding(.horizontal, 12)
                .padding(.top, 4)
            }
            if let error = app.errorMessage {
                Text(error)
                    .font(.caption)
                    .foregroundColor(.red)
                    .padding(.horizontal, 12)
            }
            sharedTextBar
            quickPromptsRow
            composer
        }
        .preferredColorScheme(app.theme == .system ? nil
                              : app.theme == .dark ? .dark : .light)
        .sheet(isPresented: $app.showSettings) {
            SettingsSheet(app: app)
        }
        .sheet(isPresented: $app.showModels) {
            ModelsSheet(app: app, onImport: {
                app.showModels = false
                showFileImporter = true
            })
        }
        .sheet(isPresented: $app.showConversations) {
            ConversationsSheet(app: app, onImportBackup: {
                app.showConversations = false
                showBackupImporter = true
            })
        }
        .fileImporter(isPresented: $showFileImporter,
                      allowedContentTypes: [.data],
                      allowsMultipleSelection: false) { result in
            if case .success(let urls) = result, let url = urls.first {
                importModel(url)
            }
        }
        .fileImporter(isPresented: $showTextImporter,
                      allowedContentTypes: [.text, .data],
                      allowsMultipleSelection: false) { result in
            if case .success(let urls) = result, let url = urls.first {
                attachImportedText(url)
            }
        }
        .fileImporter(isPresented: $showBackupImporter,
                      allowedContentTypes: [.json, .text, .data],
                      allowsMultipleSelection: false) { result in
            if case .success(let urls) = result, let url = urls.first {
                importBackup(url)
            }
        }
        .onAppear { app.refreshSharedText() }
        .onChange(of: scenePhase) { phase in
            if phase == .active {
                app.refreshSharedText()
            }
        }
    }

    private func importModel(_ url: URL) {
        let accessed = url.startAccessingSecurityScopedResource()
        DispatchQueue.global(qos: .userInitiated).async { [app] in
            defer {
                if accessed { url.stopAccessingSecurityScopedResource() }
            }
            let dest = app.downloader.modelURL(name: url.lastPathComponent)
            do {
                if !FileManager.default.fileExists(atPath: dest.path) {
                    try FileManager.default.copyItem(at: url, to: dest)
                }
                if let verification = app.downloader.verifyImportedModel(at: dest) {
                    DispatchQueue.main.async {
                        app.errorMessage = verification
                    }
                } else {
                    DispatchQueue.main.async {
                        app.refreshModels()
                        app.errorMessage = nil
                        app.useModel(dest)
                    }
                }
            } catch {
                DispatchQueue.main.async {
                    app.errorMessage = "Unable to import model: \(error.localizedDescription)"
                }
            }
        }
    }

    private func attachImportedText(_ url: URL) {
        let accessed = url.startAccessingSecurityScopedResource()
        DispatchQueue.global(qos: .userInitiated).async { [app] in
            defer {
                if accessed { url.stopAccessingSecurityScopedResource() }
            }
            do {
                let content = try String(contentsOf: url, encoding: .utf8)
                let bounded = String(content.prefix(MAX_ATTACH_BYTES))
                DispatchQueue.main.async {
                    app.errorMessage = content.count > MAX_ATTACH_BYTES
                        ? "Attached the first \(MAX_ATTACH_BYTES / 1024) KB of the file."
                        : nil
                    let existing = app.input.trimmingCharacters(in: .whitespacesAndNewlines)
                    app.input = existing.isEmpty ? bounded : existing + "\n\n" + bounded
                }
            } catch {
                DispatchQueue.main.async {
                    app.errorMessage = "Unable to read the attached file: \(error.localizedDescription)"
                }
            }
        }
    }

    private func importBackup(_ url: URL) {
        let accessed = url.startAccessingSecurityScopedResource()
        DispatchQueue.global(qos: .userInitiated).async { [app] in
            defer {
                if accessed { url.stopAccessingSecurityScopedResource() }
            }
            do {
                let text = try String(contentsOf: url, encoding: .utf8)
                let suggested = url.deletingPathExtension().lastPathComponent
                DispatchQueue.main.async {
                    let error = app.importConversationBackup(text: text, name: suggested)
                    if let error {
                        app.errorMessage = error
                    } else {
                        app.errorMessage = nil
                        app.showConversations = false
                    }
                }
            } catch {
                DispatchQueue.main.async {
                    app.errorMessage = "Unable to read the conversation backup: \(error.localizedDescription)"
                }
            }
        }
    }

    private var topBar: some View {
        HStack(spacing: 10) {
            Text("SkiffLLM")
                .font(.headline)
                .foregroundColor(.accentColor)
            Text(app.currentConversation)
                .font(.caption)
                .foregroundColor(.secondary)
                .lineLimit(1)
            Spacer()
            Button { app.showConversations = true } label: {
                Image(systemName: "bubble.left.and.bubble.right")
            }
            Button { app.copyLastAnswer() } label: {
                Image(systemName: "doc.on.doc")
            }
            Button { app.showModels = true } label: {
                Image(systemName: "ellipsis.circle")
            }
            ShareLink(item: app.exportMarkdown()) {
                Image(systemName: "square.and.arrow.up")
            }
            Button { app.showSettings = true } label: {
                Image(systemName: "gearshape")
            }
            Button { app.clear() } label: {
                Image(systemName: "trash")
            }
        }
        .font(.body)
        .buttonStyle(.borderless)
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
    }

    private var modelStatus: some View {
        HStack(spacing: 6) {
            Text(app.modelName ?? "No model loaded")
                .font(.caption)
                .foregroundColor(.secondary)
                .lineLimit(1)
            if app.loadingModel || app.warmingUp {
                ProgressView().controlSize(.small)
            }
            Spacer()
            if let info = app.modelInfo {
                Text(info)
                    .font(.caption2)
                    .foregroundColor(.secondary)
                    .lineLimit(1)
                    .truncationMode(.middle)
            } else if !app.loadingModel, !app.warmingUp {
                Text(app.engine.activeBackends())
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 4)
    }

    private var messageList: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(spacing: 8) {
                    ForEach(app.messages) { message in
                        MessageBubble(message: message)
                    }
                    if app.generating {
                        MessageBubble(message: ChatMessage(role: "assistant", content: app.draft),
                                      streaming: true)
                    }
                }
                .padding(12)
            }
            .onChange(of: app.messages.count) { _ in
                if let last = app.messages.last {
                    proxy.scrollTo(last.id, anchor: .bottom)
                }
            }
            .onChange(of: app.draft) { _ in
                if let last = app.messages.last {
                    proxy.scrollTo(last.id, anchor: .bottom)
                }
            }
        }
    }

    private func contextBar(_ stats: GenerationStats) -> some View {
        let capacity = max(1, app.engine.contextCapacity())
        let used = stats.promptTokens + stats.generatedTokens
        let fraction = min(1.0, Double(used) / Double(capacity))
        return VStack(alignment: .leading, spacing: 2) {
            ProgressView(value: fraction)
                .progressViewStyle(.linear)
            Text("Context: \(used)/\(capacity) tokens (\(Int(fraction * 100))%) · "
                 + "\(stats.generatedTokens) tokens in "
                 + String(format: "%.1f", stats.generationMs / 1000) + " s at "
                 + String(format: "%.1f", stats.tokensPerSecond) + " tok/s")
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .padding(.horizontal, 12)
        .padding(.top, 4)
    }

    private var sharedTextBar: some View {
        Group {
            if let shared = app.sharedText {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Shared from another app")
                        .font(.caption)
                        .foregroundColor(.accentColor)
                    Text(shared.replacingOccurrences(of: "\n", with: " ").prefix(160))
                        .font(.caption2)
                        .foregroundColor(.secondary)
                        .lineLimit(2)
                    HStack {
                        Button("Use") { app.useSharedText() }
                        Spacer()
                        Button("Dismiss") { app.dismissSharedText() }
                    }
                    .font(.caption)
                }
                .padding(8)
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)
                .padding(.horizontal, 12)
                .padding(.top, 4)
            }
        }
    }

    private var quickPromptsRow: some View {
        Group {
            if !app.quickPrompts.isEmpty {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 6) {
                        ForEach(app.quickPrompts, id: \.self) { prompt in
                            Button(prompt) { app.input = prompt }
                                .font(.caption)
                                .buttonStyle(.bordered)
                        }
                    }
                    .padding(.horizontal, 12)
                    .padding(.top, 4)
                }
            }
        }
    }

    private var composer: some View {
        HStack(alignment: .bottom, spacing: 8) {
            TextEditor(text: $app.input)
                .frame(minHeight: 38, maxHeight: 120)
                .padding(6)
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)

            Button { app.regenerate() } label: {
                Image(systemName: "arrow.clockwise")
            }
            .buttonStyle(.bordered)
            .disabled(app.generating || app.warmingUp || !app.messages.contains { $0.role == "user" })

            Button { showTextImporter = true } label: {
                Image(systemName: "paperclip")
            }
            .buttonStyle(.bordered)
            .disabled(app.generating)

            if app.generating {
                Button("Stop") { app.stop() }
                    .buttonStyle(.bordered)
            } else {
                Button("Send") { app.send() }
                    .buttonStyle(.borderedProminent)
                    .disabled(app.modelName == nil || app.warmingUp || app.input.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
        .padding(12)
    }
}

struct MessageBubble: View {
    let message: ChatMessage
    var streaming = false
    var isUser: Bool { message.role == "user" }

    var body: some View {
        HStack {
            if isUser { Spacer(minLength: 60) }
            Text(message.content)
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .foregroundColor(isUser ? .white : .primary)
                .background(isUser ? Color.accentColor : Color(.secondarySystemBackground))
                .cornerRadius(14)
                .opacity(streaming ? 0.8 : 1)
            if !isUser { Spacer(minLength: 60) }
        }
        .frame(maxWidth: .infinity)
    }
}

struct SettingsSheet: View {
    @ObservedObject var app: AppState
    @State private var newQuickPrompt = ""
    @State private var newStopSequence = ""

    var body: some View {
        NavigationView {
            Form {
                Section("Model") {
                    Stepper("Context: \(app.loadParams.contextSize)", value: $app.loadParams.contextSize, in: 512...32768, step: 256)
                    Stepper("Threads: \(app.loadParams.threads)", value: $app.loadParams.threads, in: 1...16)
                    Stepper("GPU layers: \(app.loadParams.gpuLayers)", value: $app.loadParams.gpuLayers, in: -1...64)
                    TextField("Chat template (next load)", text: $app.loadParams.chatTemplate)
                        .autocorrectionDisabled()
                }
                Section("Profile presets") {
                    ForEach(SamplingProfile.allCases, id: \.self) { profile in
                        Button(profile.label) { app.applyProfile(profile) }
                    }
                    Text("Applies the values shown below immediately. Fine-tune after choosing a preset.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Section("Sampling") {
                    Slider(value: $app.sampling.temperature, in: 0...2, step: 0.05) {
                        Text("Temp: \(String(format: "%.2f", app.sampling.temperature))")
                    }
                    Slider(value: $app.sampling.topP, in: 0...1, step: 0.01) {
                        Text("Top-p: \(String(format: "%.2f", app.sampling.topP))")
                    }
                    Stepper("Top-k: \(app.sampling.topK)", value: $app.sampling.topK, in: 0...200, step: 1)
                    Slider(value: $app.sampling.minP, in: 0...1, step: 0.01) {
                        Text("Min-p: \(String(format: "%.2f", app.sampling.minP))")
                    }
                    Slider(value: $app.sampling.typicalP, in: 0...1, step: 0.01) {
                        Text("Typical-p: \(String(format: "%.2f", app.sampling.typicalP))")
                    }
                    Slider(value: $app.sampling.repeatPenalty, in: 1...2, step: 0.01) {
                        Text("Repeat: \(String(format: "%.2f", app.sampling.repeatPenalty))")
                    }
                    Stepper("Max tokens: \(app.sampling.maxTokens)", value: $app.sampling.maxTokens, in: 16...4096, step: 16)
                    TextField("Seed (blank = random)", text: Binding(
                        get: { app.sampling.seed == 0xFFFFFFFF ? "" : String(app.sampling.seed) },
                        set: { value in
                            if let number = UInt32(value) {
                                app.sampling.seed = number
                            } else {
                                app.sampling.seed = 0xFFFFFFFF
                            }
                        }
                    ))
                    .keyboardType(.numberPad)
                }
                Section("System prompt") {
                    TextEditor(text: $app.systemPrompt)
                        .frame(minHeight: 80)
                }
                Section("Persistent facts") {
                    TextEditor(text: $app.persistentFacts)
                        .frame(minHeight: 80)
                    Text("Keep facts like “the user prefers concise answers” here. They stay on this device and are injected every turn.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Section {
                    Button("Compact conversation") {
                        app.showSettings = false
                        app.compactConversation()
                    }
                    Button("Warm up model") {
                        app.showSettings = false
                        app.warmUp()
                    }
                    Button("Run 3-round benchmark") {
                        app.showSettings = false
                        app.runBenchmark()
                    }
                    Text("Warm-up reduces first-answer latency; benchmark reports measured tokens, time, and tok/s.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Section("Stop sequences") {
                    ForEach(app.stopSequences, id: \.self) { sequence in
                        HStack {
                            Text(sequence).font(.caption)
                            Spacer()
                            Button("Remove") { app.settings.removeStopSequence(sequence); app.stopSequences = app.settings.loadStopSequences() }
                                .font(.caption)
                        }
                    }
                    HStack {
                        TextField("New stop sequence", text: $newStopSequence)
                        Button("Add") {
                            app.settings.addStopSequence(newStopSequence)
                            app.stopSequences = app.settings.loadStopSequences()
                            newStopSequence = ""
                        }
                    }
                }
                Section {
                    Toggle("Code mode — request unified diffs", isOn: $app.codeMode)
                    Text("Appends the safe-code instructions to the system prompt. Outputs are proposals; apply them manually.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Section("Quick prompts") {
                    ForEach(app.quickPrompts, id: \.self) { prompt in
                        HStack {
                            Text(prompt).font(.caption)
                            Spacer()
                            Button("Remove") { app.settings.removeQuickPrompt(prompt); app.quickPrompts = app.settings.loadQuickPrompts() }
                                .font(.caption)
                        }
                    }
                    HStack {
                        TextField("New quick prompt", text: $newQuickPrompt)
                        Button("Add") {
                            app.settings.addQuickPrompt(newQuickPrompt)
                            app.quickPrompts = app.settings.loadQuickPrompts()
                            newQuickPrompt = ""
                        }
                    }
                }
                Section("Theme") {
                    Picker("Theme", selection: $app.theme) {
                        ForEach(Theme.allCases, id: \.self) { theme in
                            Text(theme.label).tag(theme)
                        }
                    }
                    .pickerStyle(.segmented)
                }
                Section {
                    NavigationLink("About SkiffLLM") { AboutSheet() }
                }
            }
            .navigationTitle("Settings")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") {
                        app.settings.saveLoadParams(app.loadParams)
                        app.settings.saveSampling(app.sampling)
                        app.settings.saveSystemPrompt(app.systemPrompt)
                        app.settings.savePersistentFacts(app.persistentFacts)
                        app.settings.saveCodeMode(app.codeMode)
                        app.settings.saveTheme(app.theme)
                        app.persistConversationSettings()
                        app.showSettings = false
                    }
                }
            }
        }
    }
}

struct ModelsSheet: View {
    @ObservedObject var app: AppState
    var onImport: () -> Void

    var body: some View {
        NavigationView {
            List {
                Section("Downloaded") {
                    if app.localModels.isEmpty {
                        Text("No models downloaded yet.").foregroundColor(.secondary)
                    } else {
                        ForEach(app.localModels, id: \.self) { url in
                            HStack {
                                Text(url.lastPathComponent).lineLimit(1)
                                Spacer()
                                if app.modelName == url.lastPathComponent {
                                    Text("Active").foregroundColor(.secondary)
                                } else {
                                    Button("Use") { app.useModel(url) }
                                        .disabled(app.generating || app.loadingModel)
                                }
                                Button("Delete", role: .destructive) {
                                    app.downloader.delete(url)
                                    app.refreshModels()
                                }
                                .disabled(app.generating || app.loadingModel)
                            }
                        }
                    }
                }
                Section("Import") {
                    Button {
                        onImport()
                    } label: {
                        Label("Import a GGUF from Files", systemImage: "folder")
                    }
                    Text("A local GGUF is copied into the app’s model folder and verified before use.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Section("Recommended") {
                    ForEach(ModelCatalog.models) { entry in
                        let isDownloaded = app.localModels.contains { $0.lastPathComponent == entry.file }
                        let isDownloading = app.downloader.downloading.contains(entry.id)
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(entry.name)
                                Text("\(entry.sizeText) · \(entry.ramNote)")
                                    .font(.caption).foregroundColor(.secondary)
                                Text(entry.description)
                                    .font(.caption2).foregroundColor(.secondary)
                                Text("License: \(entry.license)")
                                    .font(.caption2).foregroundColor(.secondary)
                                if isDownloading, let pct = app.downloader.progress[entry.id] {
                                    ProgressView(value: pct)
                                        .frame(maxWidth: 160)
                                }
                                if let downloadError = app.downloader.errors[entry.id] {
                                    Text(downloadError)
                                        .font(.caption2)
                                        .foregroundColor(.red)
                                }
                            }
                            Spacer()
                            if isDownloaded {
                                Button("Use") { app.useModel(app.downloader.modelURL(name: entry.file)) }
                                    .disabled(app.generating || app.loadingModel)
                            } else if isDownloading {
                                Button("Cancel") { app.downloader.cancel(entry) }
                            } else {
                                Button("Download") { app.downloader.download(entry) }
                            }
                        }
                        .padding(.vertical, 4)
                    }
                }
            }
            .navigationTitle("Models")
            .onAppear { app.refreshModels() }
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") {
                        app.refreshModels()
                        app.showModels = false
                    }
                }
            }
        }
    }
}

struct ConversationsSheet: View {
    @ObservedObject var app: AppState
    let onImportBackup: () -> Void
    @State private var newName = ""
    @State private var renameTarget: String?
    @State private var renameInput = ""

    var body: some View {
        NavigationView {
            List {
                Section("Current: \(app.currentConversation)") {
                    ForEach(app.conversationList, id: \.self) { name in
                        HStack {
                            Text(name)
                            Spacer()
                            if name == app.currentConversation {
                                Text("Active").foregroundColor(.secondary)
                            } else {
                                Button("Open") { app.openConversation(name); app.showConversations = false }
                                Button("Delete") { app.deleteConversation(name) }
                            }
                            Button("Rename") {
                                renameTarget = name
                                renameInput = name
                            }
                        }
                        if renameTarget == name {
                            HStack {
                                TextField("New name", text: $renameInput)
                                Button("Apply") {
                                    let target = renameInput.trimmingCharacters(in: .whitespacesAndNewlines)
                                    if !target.isEmpty {
                                        app.renameConversation(name, to: target)
                                        renameTarget = nil
                                        renameInput = ""
                                    }
                                }
                                Button("Cancel") {
                                    renameTarget = nil
                                    renameInput = ""
                                }
                            }
                        }
                    }
                }
                Section("Backup") {
                    ShareLink(item: app.conversationBackupString()) {
                        Label("Export current conversation", systemImage: "square.and.arrow.up")
                    }
                    Button {
                        onImportBackup()
                    } label: {
                        Label("Import backup", systemImage: "square.and.arrow.down")
                    }
                    Text("Backups are JSON files kept on this device; importing creates a new conversation.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Section("New") {
                    HStack {
                        TextField("Name", text: $newName)
                        Button("Create") {
                            let name = newName.trimmingCharacters(in: .whitespacesAndNewlines)
                            guard !name.isEmpty else { return }
                            app.newConversation(name)
                            newName = ""
                            app.showConversations = false
                        }
                    }
                }
            }
            .navigationTitle("Conversations")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") { app.showConversations = false }
                }
            }
        }
    }
}

struct AboutSheet: View {
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        Form {
                Section {
                    Text("SkiffLLM")
                        .font(.title)
                        .foregroundColor(.accentColor)
                    Text("Version 1.6.0 (iOS)")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Section("Privacy") {
                    Text("Inference, chat, import, and export stay on this device. The only network use is optional HTTPS model downloads from Hugging Face. No prompts, history, analytics, or crash reports are sent.")
                        .font(.caption)
                }
                Section("Models") {
                    Text("Use Models to download a recommended Q4_K_M GGUF or import your own GGUF from Files. Downloads verify the GGUF header and record a SHA-256 sidecar; the catalog byte count is advisory.")
                        .font(.caption)
                }
                Section("License") {
                    Text("MIT")
                }
            }
            .navigationTitle("About")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") { dismiss() }
                }
            }
    }
}
