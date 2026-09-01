import SwiftUI
import UIKit

final class AppState: ObservableObject {
    let engine = NativeEngine()
    let settings = SettingsStore()
    let conversations = ConversationStore()
    let downloader = ModelDownloader()

    @Published var messages: [ChatMessage] = []
    @Published var draft = ""
    @Published var input = ""
    @Published var generating = false
    @Published var loadingModel = false
    @Published var stats: GenerationStats?
    @Published var errorMessage: String?
    @Published var modelName: String?
    @Published var currentConversation = "default"
    @Published var conversationList: [String] = []
    @Published var theme: Theme = .system
    @Published var loadParams: LoadParams = LoadParams()
    @Published var sampling: SamplingParams = SamplingParams()
    @Published var systemPrompt: String = ""
    @Published var localModels: [URL] = []
    @Published var showSettings = false
    @Published var showModels = false
    @Published var showConversations = false

    init() {
        let threads = max(2, ProcessInfo.processInfo.activeProcessorCount)
        loadParams = settings.loadLoadParams(defaultThreads: threads)
        sampling = settings.loadSampling()
        systemPrompt = settings.loadSystemPrompt()
        theme = settings.loadTheme()
        currentConversation = conversations.currentName()
        let snap = conversations.load()
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        refreshModels()
        refreshConversations()
        autoLoadLastModel()
    }

    deinit {
        engine.close()
    }

    func effectiveSystemPrompt() -> String {
        systemPrompt.isEmpty ? "You are SkiffLLM, a helpful local assistant." : systemPrompt
    }

    func refreshModels() {
        localModels = downloader.listDownloadedModels()
    }

    func refreshConversations() {
        conversationList = conversations.listConversations()
        currentConversation = conversations.currentName()
    }

    private func autoLoadLastModel() {
        guard let path = settings.loadLastModelPath(),
              FileManager.default.fileExists(atPath: path) else { return }
        useModel(URL(fileURLWithPath: path))
    }

    func useModel(_ url: URL) {
        guard !generating, !loadingModel else { return }
        modelName = nil
        errorMessage = nil
        loadingModel = true
        let params = loadParams
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self else { return }
            var error: NSError?
            let ok = self.engine.loadModelAtPath(url.path,
                                                 contextSize: Int32(params.contextSize),
                                                 threads: Int32(params.threads),
                                                 gpuLayers: Int32(params.gpuLayers),
                                                 chatTemplate: params.chatTemplate,
                                                 error: &error)
            DispatchQueue.main.async {
                self.loadingModel = false
                if ok {
                    self.modelName = url.lastPathComponent
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
        let stopSequences: [String] = []
        engine.generateMessages(msgs,
                                temperature: sampling.temperature,
                                topP: sampling.topP,
                                topK: Int32(sampling.topK),
                                minP: sampling.minP,
                                repeatPenalty: sampling.repeatPenalty,
                                repeatLastN: Int32(sampling.repeatLastN),
                                maxTokens: Int32(sampling.maxTokens),
                                seed: 0xFFFFFFFF,
                                stopSequences: stopSequences,
                                tokenCallback: { [weak self] part in
                                    DispatchQueue.main.async { self?.draft += part ?? "" }
                                },
                                completion: { [weak self] result, error in
                                    guard let self else { return }
                                    self.generating = false
                                    if error != nil || self.draft.isEmpty {
                                        self.errorMessage = error?.localizedDescription ?? "Generation failed."
                                        if !self.messages.isEmpty { self.messages.removeLast() }
                                    } else {
                                        self.messages.append(ChatMessage(role: "assistant", content: self.draft))
                                        self.conversations.save(systemPrompt: self.effectiveSystemPrompt(), messages: self.messages)
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
        stats = nil
        conversations.save(systemPrompt: effectiveSystemPrompt(), messages: [])
    }

    func newConversation(_ name: String) {
        let finalName = conversations.create(name)
        let snap = conversations.load()
        currentConversation = finalName
        refreshConversations()
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        draft = ""
        input = ""
        stats = nil
    }

    func openConversation(_ name: String) {
        _ = conversations.switchTo(name)
        let snap = conversations.load()
        refreshConversations()
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        draft = ""
        input = ""
        stats = nil
    }

    func deleteConversation(_ name: String) {
        conversations.delete(name)
        refreshConversations()
        currentConversation = conversations.currentName()
        let snap = conversations.load()
        messages = snap.messages
        systemPrompt = snap.systemPrompt
        stats = nil
    }
}

struct ChatView: View {
    @StateObject private var app = AppState()

    var body: some View {
        VStack(spacing: 0) {
            topBar
            Divider()
            messageList
            if let stats = app.stats, !app.generating {
                contextBar(stats)
            }
            if let error = app.errorMessage {
                Text(error)
                    .font(.caption)
                    .foregroundColor(.red)
                    .padding(.horizontal, 12)
            }
            composer
        }
        .preferredColorScheme(app.theme == .system ? nil
                              : app.theme == .dark ? .dark : .light)
        .sheet(isPresented: $app.showSettings) { SettingsSheet(app: app) }
        .sheet(isPresented: $app.showModels) { ModelsSheet(app: app) }
        .sheet(isPresented: $app.showConversations) { ConversationsSheet(app: app) }
    }

    private var topBar: some View {
        HStack {
            Text("SkiffLLM")
                .font(.headline)
                .foregroundColor(.accentColor)
            Text(app.currentConversation)
                .font(.caption)
                .foregroundColor(.secondary)
                .lineLimit(1)
            Spacer()
            Button("Chats") { app.showConversations = true }
                .buttonStyle(.borderless)
            Button("Models") { app.showModels = true }
                .buttonStyle(.borderless)
            Button("Settings") { app.showSettings = true }
                .buttonStyle(.borderless)
            Button("Clear") { app.clear() }
                .buttonStyle(.borderless)
    }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
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

    private var composer: some View {
        HStack(alignment: .bottom, spacing: 8) {
            TextEditor(text: $app.input)
                .frame(minHeight: 38, maxHeight: 120)
                .padding(6)
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)

            if app.generating {
                Button("Stop") { app.stop() }
                    .buttonStyle(.bordered)
            } else {
                Button("Send") { app.send() }
                    .buttonStyle(.borderedProminent)
                    .disabled(app.modelName == nil || app.input.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
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

    var body: some View {
        NavigationView {
            Form {
                Section("Model") {
                    Stepper("Context: \(app.loadParams.contextSize)", value: $app.loadParams.contextSize, in: 512...32768, step: 256)
                    Stepper("Threads: \(app.loadParams.threads)", value: $app.loadParams.threads, in: 1...16)
                    Stepper("GPU layers: \(app.loadParams.gpuLayers)", value: $app.loadParams.gpuLayers, in: -1...64)
                    TextField("Chat template", text: $app.loadParams.chatTemplate)
                        .autocorrectionDisabled()
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
                    Slider(value: $app.sampling.repeatPenalty, in: 1...2, step: 0.01) {
                        Text("Repeat: \(String(format: "%.2f", app.sampling.repeatPenalty))")
                    }
                    Stepper("Max tokens: \(app.sampling.maxTokens)", value: $app.sampling.maxTokens, in: 16...4096, step: 16)
                }
                Section("System prompt") {
                    TextEditor(text: $app.systemPrompt)
                        .frame(minHeight: 80)
                }
                Picker("Theme", selection: $app.theme) {
                    ForEach(Theme.allCases, id: \.self) { theme in
                        Text(theme.label).tag(theme)
                    }
                }
                .pickerStyle(.segmented)
            }
            .navigationTitle("Settings")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") {
                        app.settings.saveLoadParams(app.loadParams)
                        app.settings.saveSampling(app.sampling)
                        app.settings.saveSystemPrompt(app.systemPrompt)
                        app.settings.saveTheme(app.theme)
                        app.showSettings = false
                    }
                }
            }
        }
    }
}

struct ModelsSheet: View {
    @ObservedObject var app: AppState

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
                                if isDownloading, let pct = app.downloader.progress[entry.id] {
                                    ProgressView(value: pct)
                                        .frame(maxWidth: 160)
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
    @State private var newName = ""

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
                        }
                    }
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
