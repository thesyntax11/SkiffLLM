import Foundation
import CryptoKit

struct ChatMessage: Identifiable {
    let id = UUID()
    let role: String
    let content: String
}

/// Coalesces token strings and flushes them in small, main-thread batches.
/// This mirrors the Android downloader/generator pattern: many small models can
/// emit tokens faster than one main-queue hop per token, so we buffer briefly
/// instead of saturating the UI queue (which made long replies feel laggy).
final class TokenBatcher {
    private let lock = NSLock()
    private var buffer = ""
    private var scheduled = false
    private let onChunk: (String) -> Void

    init(onChunk: @escaping (String) -> Void) {
        self.onChunk = onChunk
    }

    func append(_ text: String) {
        guard !text.isEmpty else { return }
        lock.lock()
        buffer += text
        let shouldSchedule = !scheduled
        if shouldSchedule { scheduled = true }
        lock.unlock()

        guard shouldSchedule else { return }
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.lock.lock()
            let chunk = self.buffer
            self.buffer = ""
            self.scheduled = false
            self.lock.unlock()
            if !chunk.isEmpty { self.onChunk(chunk) }
        }
    }
}

struct GenerationStats {
    let promptTokens: Int
    let generatedTokens: Int
    let promptMs: Double
    let generationMs: Double
    let tokensPerSecond: Double
    let stopped: Bool
}

struct SessionStats {
    var messageCount = 0
    var promptTokens: Int = 0
    var generatedTokens: Int = 0
    var generationMs: Double = 0
    var avgTokensPerSecond: Double = 0
}

struct SamplingParams {
    var temperature: Float = 0.7
    var topP: Float = 0.95
    var topK: Int = 40
    var minP: Float = 0.0
    var typicalP: Float = 0.0
    var repeatPenalty: Float = 1.10
    var repeatLastN: Int = 64
    var maxTokens: Int = 512
    var seed: UInt32 = 0xFFFFFFFF
}

enum SamplingProfile: String, CaseIterable {
    case balanced
    case fast
    case creative
    case code
    case precise

    var label: String { rawValue.capitalized }

    func apply(to value: SamplingParams) -> SamplingParams {
        var out = value
        switch self {
        case .balanced:
            out.temperature = 0.70
            out.topP = 0.95
            out.topK = 40
            out.minP = 0.00
            out.typicalP = 0.0
            out.repeatPenalty = 1.10
            out.maxTokens = 512
        case .fast:
            out.temperature = 0.60
            out.topP = 0.90
            out.topK = 30
            out.minP = 0.00
            out.typicalP = 0.0
            out.repeatPenalty = 1.05
            out.maxTokens = 256
        case .creative:
            out.temperature = 1.00
            out.topP = 0.98
            out.topK = 60
            out.minP = 0.05
            out.typicalP = 0.0
            out.repeatPenalty = 1.20
            out.maxTokens = 512
        case .code:
            out.temperature = 0.20
            out.topP = 0.90
            out.topK = 20
            out.minP = 0.00
            out.typicalP = 0.0
            out.repeatPenalty = 1.20
            out.maxTokens = 1024
        case .precise:
            out.temperature = 0.00
            out.topP = 0.90
            out.topK = 20
            out.minP = 0.00
            out.typicalP = 0.0
            out.repeatPenalty = 1.00
            out.maxTokens = 650
        }
        return out
    }
}

struct LoadParams {
    var contextSize: Int = 2048
    var threads: Int = 4
    var gpuLayers: Int = 0
    var chatTemplate: String = ""
}

enum Theme: String, CaseIterable {
    case system = "system"
    case dark = "dark"
    case light = "light"

    var label: String { rawValue.capitalized }
}

struct ModelCatalogEntry: Identifiable {
    let id: String
    let name: String
    let description: String
    let repo: String
    let file: String
    let bytes: Int64
    let license: String
    let ramNote: String
    let recommended: Bool

    var downloadURL: URL {
        URL(string: "https://huggingface.co/\(repo)/resolve/main/\(file)")!
    }

    var sizeText: String {
        let gb = Double(bytes) / (1024.0 * 1024.0 * 1024.0)
        return gb >= 0.1 ? String(format: "%.1f GB", gb) : "\(bytes / 1024 / 1024) MB"
    }
}

enum ModelCatalog {
    static let models: [ModelCatalogEntry] = [
        ModelCatalogEntry(
            id: "qwen2.5-0.5b", name: "Qwen2.5 0.5B Instruct",
            description: "Best first model on older phones. Fast, small, follows chat well.",
            repo: "Qwen/Qwen2.5-0.5B-Instruct-GGUF",
            file: "qwen2.5-0.5b-instruct-q4_k_m.gguf",
            bytes: 491400032, license: "Apache-2.0", ramNote: "~1 GB working set", recommended: true),
        ModelCatalogEntry(
            id: "qwen3-0.6b", name: "Qwen3 0.6B Instruct",
            description: "Small Qwen3 with optional thinking mode. Good quality for its size.",
            repo: "bartowski/Qwen_Qwen3-0.6B-GGUF",
            file: "Qwen_Qwen3-0.6B-Q4_K_M.gguf",
            bytes: 484220320, license: "Apache-2.0", ramNote: "~1 GB working set", recommended: true),
        ModelCatalogEntry(
            id: "llama3.2-1b", name: "Llama 3.2 1B Instruct",
            description: "Balanced quality and speed for typical modern phones.",
            repo: "bartowski/Llama-3.2-1B-Instruct-GGUF",
            file: "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
            bytes: 807694464, license: "Llama 3.2 Community License", ramNote: "~1.5 GB working set", recommended: true),
        ModelCatalogEntry(
            id: "smollm2-1.7b", name: "SmolLM2 1.7B Instruct",
            description: "Mid-size option with a solid quality-to-speed balance.",
            repo: "bartowski/SmolLM2-1.7B-Instruct-GGUF",
            file: "SmolLM2-1.7B-Instruct-Q4_K_M.gguf",
            bytes: 1055609824, license: "Apache-2.0", ramNote: "~2 GB working set", recommended: false),
        ModelCatalogEntry(
            id: "phi3.5-mini", name: "Phi-3.5 Mini Instruct",
            description: "Better reasoning. Use only on an 8 GB+ phone.",
            repo: "bartowski/Phi-3.5-mini-instruct-GGUF",
            file: "Phi-3.5-mini-instruct-Q4_K_M.gguf",
            bytes: 2393232672, license: "MIT", ramNote: "~4 GB working set", recommended: false),
    ]
}

final class SettingsStore {
    private let defaults = UserDefaults.standard

    init() {}

    func loadLoadParams(defaultThreads: Int = 4) -> LoadParams {
        LoadParams(
            contextSize: defaults.integer(forKey: "context_size") == 0
                ? 2048 : defaults.integer(forKey: "context_size"),
            threads: defaults.integer(forKey: "threads") == 0
                ? defaultThreads : defaults.integer(forKey: "threads"),
            gpuLayers: defaults.integer(forKey: "gpu_layers"),
            chatTemplate: defaults.string(forKey: "chat_template") ?? ""
        )
    }

    func saveLoadParams(_ value: LoadParams) {
        defaults.set(value.contextSize, forKey: "context_size")
        defaults.set(value.threads, forKey: "threads")
        defaults.set(value.gpuLayers, forKey: "gpu_layers")
        defaults.set(value.chatTemplate, forKey: "chat_template")
    }

    func loadSampling() -> SamplingParams {
        SamplingParams(
            temperature: defaults.object(forKey: "temp") as? Float ?? 0.7,
            topP: defaults.object(forKey: "top_p") as? Float ?? 0.95,
            topK: defaults.object(forKey: "top_k") as? Int ?? 40,
            minP: defaults.object(forKey: "min_p") as? Float ?? 0.0,
            typicalP: defaults.object(forKey: "typical_p") as? Float ?? 0.0,
            repeatPenalty: defaults.object(forKey: "repeat_penalty") as? Float ?? 1.10,
            repeatLastN: defaults.object(forKey: "repeat_last_n") as? Int ?? 64,
            maxTokens: defaults.object(forKey: "max_tokens") as? Int ?? 512,
            seed: UInt32(defaults.object(forKey: "seed") as? Int ?? Int(0xFFFFFFFF))
        )
    }

    func saveSampling(_ value: SamplingParams) {
        defaults.set(value.temperature, forKey: "temp")
        defaults.set(value.topP, forKey: "top_p")
        defaults.set(value.topK, forKey: "top_k")
        defaults.set(value.minP, forKey: "min_p")
        defaults.set(value.typicalP, forKey: "typical_p")
        defaults.set(value.repeatPenalty, forKey: "repeat_penalty")
        defaults.set(value.repeatLastN, forKey: "repeat_last_n")
        defaults.set(value.maxTokens, forKey: "max_tokens")
        defaults.set(Int(value.seed), forKey: "seed")
    }

    func loadSystemPrompt() -> String {
        defaults.string(forKey: "system_prompt") ?? "You are SkiffLLM, a helpful local assistant."
    }

    func saveSystemPrompt(_ value: String) {
        defaults.set(value, forKey: "system_prompt")
    }

    func loadPersistentFacts() -> String {
        defaults.string(forKey: "persistent_facts") ?? ""
    }

    func savePersistentFacts(_ value: String) {
        defaults.set(value, forKey: "persistent_facts")
    }

    func loadStopSequences() -> [String] {
        (defaults.stringArray(forKey: "stop_sequences") ?? []).filter { !$0.isEmpty }
    }

    func addStopSequence(_ value: String) {
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        var sequences = loadStopSequences()
        if !sequences.contains(trimmed) {
            sequences.append(trimmed)
            defaults.set(sequences, forKey: "stop_sequences")
        }
    }

    func removeStopSequence(_ value: String) {
        defaults.set(loadStopSequences().filter { $0 != value }, forKey: "stop_sequences")
    }

    func loadQuickPrompts() -> [String] {
        (defaults.stringArray(forKey: "quick_prompts") ?? []).filter { !$0.isEmpty }
    }

    func addQuickPrompt(_ value: String) {
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        var prompts = loadQuickPrompts()
        if !prompts.contains(trimmed) {
            prompts.append(trimmed)
            defaults.set(prompts.sorted(), forKey: "quick_prompts")
        }
    }

    func removeQuickPrompt(_ value: String) {
        defaults.set(loadQuickPrompts().filter { $0 != value }, forKey: "quick_prompts")
    }

    func loadTheme() -> Theme {
        guard let raw = defaults.string(forKey: "theme"), let theme = Theme(rawValue: raw) else {
            return .system
        }
        return theme
    }

    func saveTheme(_ theme: Theme) {
        defaults.set(theme.rawValue, forKey: "theme")
    }

    func loadLastModelPath() -> String? {
        defaults.string(forKey: "last_model_path")
    }

    func saveLastModelPath(_ path: String) {
        defaults.set(path, forKey: "last_model_path")
    }

    func loadCodeMode() -> Bool {
        defaults.bool(forKey: "code_mode")
    }

    func saveCodeMode(_ enabled: Bool) {
        defaults.set(enabled, forKey: "code_mode")
    }
}

struct ConversationSnapshot {
    let systemPrompt: String
    let messages: [ChatMessage]
    let sampling: SamplingParams?
    let codeMode: Bool?

    init(systemPrompt: String,
         messages: [ChatMessage],
         sampling: SamplingParams? = nil,
         codeMode: Bool? = nil) {
        self.systemPrompt = systemPrompt
        self.messages = messages
        self.sampling = sampling
        self.codeMode = codeMode
    }
}

enum SkiffLLMAppGroup {
    static let identifier = "group.com.skifflm.app"
}

final class SharedTextStore {
    static let shared = SharedTextStore()
    private let defaults = UserDefaults(suiteName: SkiffLLMAppGroup.identifier)

    func take() -> String? {
        guard let value = defaults?.string(forKey: "shared_text")?
            .trimmingCharacters(in: .whitespacesAndNewlines), !value.isEmpty else { return nil }
        defaults?.removeObject(forKey: "shared_text")
        return value
    }

    func peek() -> String? {
        guard let value = defaults?.string(forKey: "shared_text")?
            .trimmingCharacters(in: .whitespacesAndNewlines), !value.isEmpty else { return nil }
        return value
    }

    func clear() {
        defaults?.removeObject(forKey: "shared_text")
    }
}

func conversationMarkdown(systemPrompt: String, messages: [ChatMessage]) -> String {
    let formatter = DateFormatter()
    formatter.dateFormat = "yyyy-MM-dd HH:mm"
    var out = "# SkiffLLM Conversation\n\n## System\n\n\(systemPrompt)\n\n"
    out += "Exported: \(formatter.string(from: Date()))\n\n"
    for message in messages {
        let role = message.role == "user" ? "User" : (message.role == "assistant" ? "Assistant" : message.role)
        out += "## \(role)\n\n\(message.content)\n\n"
    }
    return out
}

final class ConversationStore {
    private let dir: URL
    private let currentFile: URL
    private let legacyFile: URL

    init() {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        dir = base.appendingPathComponent("conversations")
        currentFile = base.appendingPathComponent("current_conversation.txt")
        legacyFile = base.appendingPathComponent("conversation.json")
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        migrateLegacy()
        if !currentNameFileExists() {
            _ = create("default")
        }
    }

    private func sanitize(_ name: String) -> String {
        let cleaned = name.trimmingCharacters(in: .whitespacesAndNewlines)
            .replacingOccurrences(of: "[^A-Za-z0-9._-]", with: "_", options: .regularExpression)
        return cleaned.isEmpty ? "default" : cleaned
    }

    private func fileFor(_ name: String) -> URL {
        dir.appendingPathComponent("\(sanitize(name)).json")
    }

    private func emptyConversationJSON(system: String) -> Data {
        let obj: [String: Any] = [
            "system_prompt": system,
            "messages": [] as [[String: String]],
        ]
        return (try? JSONSerialization.data(withJSONObject: obj)) ?? Data()
    }

    private func currentNameFileExists() -> Bool {
        guard !currentName().isEmpty else { return false }
        return FileManager.default.fileExists(atPath: fileFor(currentName()).path)
    }

    func currentName() -> String {
        if let data = try? String(contentsOf: currentFile, encoding: .utf8) {
            let name = data.trimmingCharacters(in: .whitespacesAndNewlines)
            if !name.isEmpty { return name }
        }
        return "default"
    }

    func listConversations() -> [String] {
        let files = (try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil))
            ?? []
        var names = files.filter { $0.pathExtension == "json" }.map { $0.deletingPathExtension().lastPathComponent }
        if !names.contains("default") { names.append("default") }
        return names.sorted()
    }

    @discardableResult
    func create(_ name: String) -> String {
        let safe = sanitize(name)
        if !FileManager.default.fileExists(atPath: fileFor(safe).path) {
            try? emptyConversationJSON(system: "You are SkiffLLM, a helpful local assistant.")
                .write(to: fileFor(safe), options: .atomic)
        }
        try? safe.write(to: currentFile, atomically: true, encoding: .utf8)
        return safe
    }

    @discardableResult
    func switchTo(_ name: String) -> String {
        create(name)
    }

    func delete(_ name: String) {
        try? FileManager.default.removeItem(at: fileFor(name))
        if sanitize(name) == currentName() {
            try? "default".write(to: currentFile, atomically: true, encoding: .utf8)
            if !FileManager.default.fileExists(atPath: fileFor("default").path) {
                _ = create("default")
            }
        }
    }

    @discardableResult
    func rename(_ oldName: String, to newName: String) -> String {
        let old = sanitize(oldName)
        let new = sanitize(newName)
        guard old != new else { return old }
        let oldURL = fileFor(old)
        let newURL = fileFor(new)
        guard FileManager.default.fileExists(atPath: oldURL.path) else { return old }
        guard !FileManager.default.fileExists(atPath: newURL.path) else { return old }
        do {
            try FileManager.default.moveItem(at: oldURL, to: newURL)
            if currentName() == old {
                try? new.write(to: currentFile, atomically: true, encoding: .utf8)
            }
            return new
        } catch {
            return old
        }
    }

    func load() -> ConversationSnapshot {
        let name = currentName()
        let file = fileFor(name)
        guard FileManager.default.fileExists(atPath: file.path) else {
            return ConversationSnapshot(systemPrompt: "You are SkiffLLM, a helpful local assistant.", messages: [])
        }
        guard let data = try? Data(contentsOf: file),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return ConversationSnapshot(systemPrompt: "You are SkiffLLM, a helpful local assistant.", messages: [])
        }
        let system = (json["system_prompt"] as? String).flatMap { $0.isEmpty ? nil : $0 }
            ?? "You are SkiffLLM, a helpful local assistant."
        let raw = json["messages"] as? [[String: Any]] ?? []
        let messages = raw.compactMap { item -> ChatMessage? in
            guard let role = item["role"] as? String,
                  let content = item["content"] as? String,
                  !role.isEmpty, !content.isEmpty else { return nil }
            return ChatMessage(role: role, content: content)
        }
        let sampling: SamplingParams? = (json["sampling"] as? [String: Any]).flatMap { obj in
            SamplingParams(
                temperature: (obj["temperature"] as? NSNumber)?.floatValue ?? 0.7,
                topP: (obj["top_p"] as? NSNumber)?.floatValue ?? 0.95,
                topK: (obj["top_k"] as? NSNumber)?.intValue ?? 40,
                minP: (obj["min_p"] as? NSNumber)?.floatValue ?? 0.0,
                typicalP: (obj["typical_p"] as? NSNumber)?.floatValue ?? 0.0,
                repeatPenalty: (obj["repeat_penalty"] as? NSNumber)?.floatValue ?? 1.10,
                repeatLastN: (obj["repeat_last_n"] as? NSNumber)?.intValue ?? 64,
                maxTokens: (obj["max_tokens"] as? NSNumber)?.intValue ?? 512,
                seed: UInt32((obj["seed"] as? NSNumber)?.intValue ?? Int(0xFFFFFFFF))
            )
        }
        let codeMode = (json["code_mode"] as? NSNumber)?.boolValue
        return ConversationSnapshot(systemPrompt: system, messages: messages, sampling: sampling, codeMode: codeMode)
    }

    func save(systemPrompt: String,
              messages: [ChatMessage],
              sampling: SamplingParams? = nil,
              codeMode: Bool? = nil) {
        _ = create(currentName())
        var payload: [String: Any] = [
            "system_prompt": systemPrompt,
            "messages": messages.map { ["role": $0.role, "content": $0.content] },
        ]
        if let sampling {
            payload["sampling"] = [
                "temperature": sampling.temperature,
                "top_p": sampling.topP,
                "top_k": sampling.topK,
                "min_p": sampling.minP,
                "typical_p": sampling.typicalP,
                "repeat_penalty": sampling.repeatPenalty,
                "repeat_last_n": sampling.repeatLastN,
                "max_tokens": sampling.maxTokens,
                "seed": Int(sampling.seed),
            ]
        }
        if let codeMode {
            payload["code_mode"] = codeMode
        }
        if let data = try? JSONSerialization.data(withJSONObject: payload) {
            try? data.write(to: fileFor(currentName()), options: .atomic)
        }
    }

    func clear() {
        try? FileManager.default.removeItem(at: fileFor(currentName()))
        _ = create(currentName())
    }

    func exportJson(systemPrompt: String,
                    messages: [ChatMessage],
                    sampling: SamplingParams?,
                    codeMode: Bool?) -> String? {
        var payload: [String: Any] = [
            "format": "skifflm-conversation",
            "version": 1,
            "system_prompt": systemPrompt,
            "messages": messages.map { ["role": $0.role, "content": $0.content] },
        ]
        if let sampling {
            payload["sampling"] = [
                "temperature": sampling.temperature,
                "top_p": sampling.topP,
                "top_k": sampling.topK,
                "min_p": sampling.minP,
                "typical_p": sampling.typicalP,
                "repeat_penalty": sampling.repeatPenalty,
                "repeat_last_n": sampling.repeatLastN,
                "max_tokens": sampling.maxTokens,
                "seed": Int(sampling.seed),
            ]
        }
        if let codeMode {
            payload["code_mode"] = codeMode
        }
        guard let data = try? JSONSerialization.data(withJSONObject: payload, options: [.prettyPrinted, .sortedKeys]) else {
            return nil
        }
        return String(data: data, encoding: .utf8)
    }

    func importJson(from text: String) -> ConversationSnapshot? {
        guard let data = text.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return nil
        }
        guard json["format"] as? String == "skifflm-conversation" || json["messages"] != nil else {
            return nil
        }
        let system = (json["system_prompt"] as? String).flatMap { $0.isEmpty ? nil : $0 }
            ?? "You are SkiffLLM, a helpful local assistant."
        let raw = json["messages"] as? [[String: Any]] ?? []
        let messages = raw.compactMap { item -> ChatMessage? in
            guard let role = item["role"] as? String,
                  let content = item["content"] as? String,
                  !role.isEmpty, !content.isEmpty else { return nil }
            return ChatMessage(role: role, content: content)
        }
        let sampling: SamplingParams? = (json["sampling"] as? [String: Any]).flatMap { obj in
            SamplingParams(
                temperature: (obj["temperature"] as? NSNumber)?.floatValue ?? 0.7,
                topP: (obj["top_p"] as? NSNumber)?.floatValue ?? 0.95,
                topK: (obj["top_k"] as? NSNumber)?.intValue ?? 40,
                minP: (obj["min_p"] as? NSNumber)?.floatValue ?? 0.0,
                typicalP: (obj["typical_p"] as? NSNumber)?.floatValue ?? 0.0,
                repeatPenalty: (obj["repeat_penalty"] as? NSNumber)?.floatValue ?? 1.10,
                repeatLastN: (obj["repeat_last_n"] as? NSNumber)?.intValue ?? 64,
                maxTokens: (obj["max_tokens"] as? NSNumber)?.intValue ?? 512,
                seed: UInt32((obj["seed"] as? NSNumber)?.intValue ?? Int(0xFFFFFFFF))
            )
        }
        let codeMode = (json["code_mode"] as? NSNumber)?.boolValue
        return ConversationSnapshot(systemPrompt: system, messages: messages, sampling: sampling, codeMode: codeMode)
    }

    private func migrateLegacy() {
        if FileManager.default.fileExists(atPath: legacyFile.path),
           !FileManager.default.fileExists(atPath: fileFor("default").path) {
            try? FileManager.default.copyItem(at: legacyFile, to: fileFor("default"))
            try? FileManager.default.removeItem(at: legacyFile)
        }
    }
}
