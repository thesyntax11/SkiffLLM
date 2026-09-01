import Foundation
import CryptoKit

struct ChatMessage: Identifiable {
    let id = UUID()
    let role: String
    let content: String
}

struct GenerationStats {
    let promptTokens: Int
    let generatedTokens: Int
    let promptMs: Double
    let generationMs: Double
    let tokensPerSecond: Double
    let stopped: Bool
}

struct SamplingParams {
    var temperature: Float = 0.7
    var topP: Float = 0.95
    var topK: Int = 40
    var minP: Float = 0.0
    var repeatPenalty: Float = 1.10
    var repeatLastN: Int = 64
    var maxTokens: Int = 512
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
            bytes: 491400032, ramNote: "~1 GB working set", recommended: true),
        ModelCatalogEntry(
            id: "qwen3-0.6b", name: "Qwen3 0.6B Instruct",
            description: "Small Qwen3 with optional thinking mode. Good quality for its size.",
            repo: "bartowski/Qwen_Qwen3-0.6B-GGUF",
            file: "Qwen_Qwen3-0.6B-Q4_K_M.gguf",
            bytes: 484220320, ramNote: "~1 GB working set", recommended: true),
        ModelCatalogEntry(
            id: "llama3.2-1b", name: "Llama 3.2 1B Instruct",
            description: "Balanced quality and speed for typical modern phones.",
            repo: "bartowski/Llama-3.2-1B-Instruct-GGUF",
            file: "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
            bytes: 807694464, ramNote: "~1.5 GB working set", recommended: true),
        ModelCatalogEntry(
            id: "smollm2-1.7b", name: "SmolLM2 1.7B Instruct",
            description: "Mid-size option with a solid quality-to-speed balance.",
            repo: "bartowski/SmolLM2-1.7B-Instruct-GGUF",
            file: "SmolLM2-1.7B-Instruct-Q4_K_M.gguf",
            bytes: 1055609824, ramNote: "~2 GB working set", recommended: false),
        ModelCatalogEntry(
            id: "phi3.5-mini", name: "Phi-3.5 Mini Instruct",
            description: "Better reasoning. Use only on an 8 GB+ phone.",
            repo: "bartowski/Phi-3.5-mini-instruct-GGUF",
            file: "Phi-3.5-mini-instruct-Q4_K_M.gguf",
            bytes: 2393232672, ramNote: "~4 GB working set", recommended: false),
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
            repeatPenalty: defaults.object(forKey: "repeat_penalty") as? Float ?? 1.10,
            repeatLastN: defaults.object(forKey: "repeat_last_n") as? Int ?? 64,
            maxTokens: defaults.object(forKey: "max_tokens") as? Int ?? 512
        )
    }

    func saveSampling(_ value: SamplingParams) {
        defaults.set(value.temperature, forKey: "temp")
        defaults.set(value.topP, forKey: "top_p")
        defaults.set(value.topK, forKey: "top_k")
        defaults.set(value.minP, forKey: "min_p")
        defaults.set(value.repeatPenalty, forKey: "repeat_penalty")
        defaults.set(value.repeatLastN, forKey: "repeat_last_n")
        defaults.set(value.maxTokens, forKey: "max_tokens")
    }

    func loadSystemPrompt() -> String {
        defaults.string(forKey: "system_prompt") ?? "You are SkiffLLM, a helpful local assistant."
    }

    func saveSystemPrompt(_ value: String) {
        defaults.set(value, forKey: "system_prompt")
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
}

struct ConversationSnapshot {
    let systemPrompt: String
    let messages: [ChatMessage]
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
        return ConversationSnapshot(systemPrompt: system, messages: messages)
    }

    func save(systemPrompt: String, messages: [ChatMessage]) {
        _ = create(currentName())
        let payload: [String: Any] = [
            "system_prompt": systemPrompt,
            "messages": messages.map { ["role": $0.role, "content": $0.content] },
        ]
        if let data = try? JSONSerialization.data(withJSONObject: payload) {
            try? data.write(to: fileFor(currentName()), options: .atomic)
        }
    }

    func clear() {
        try? FileManager.default.removeItem(at: fileFor(currentName()))
        _ = create(currentName())
    }

    private func migrateLegacy() {
        if FileManager.default.fileExists(atPath: legacyFile.path),
           !FileManager.default.fileExists(atPath: fileFor("default").path) {
            try? FileManager.default.copyItem(at: legacyFile, to: fileFor("default"))
            try? FileManager.default.removeItem(at: legacyFile)
        }
    }
}
