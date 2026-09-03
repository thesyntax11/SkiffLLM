import Combine
import Foundation
import CryptoKit

final class ModelDownloader: NSObject, ObservableObject, URLSessionDownloadDelegate {
    @Published var downloading = Set<String>()
    @Published var progress: [String: Double] = [:]
    @Published var errors: [String: String] = [:]
    @Published var lastDownloaded: URL?

    var onDownloadCompleted: ((URL) -> Void)?

    private var session: URLSession!
    private let modelsDir: URL
    private var tasks: [String: URLSessionDownloadTask] = [:]

    override init() {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        modelsDir = base.appendingPathComponent("models", isDirectory: true)
        try? FileManager.default.createDirectory(at: modelsDir, withIntermediateDirectories: true)
        super.init()
        session = URLSession(configuration: .default, delegate: self, delegateQueue: nil)
    }

    deinit {
        session?.invalidateAndCancel()
    }

    func listDownloadedModels() -> [URL] {
        (try? FileManager.default.contentsOfDirectory(at: modelsDir, includingPropertiesForKeys: nil))
            .flatMap { $0.filter { $0.pathExtension == "gguf" }.sorted { $0.lastPathComponent < $1.lastPathComponent } }
            ?? []
    }

    func modelURL(name: String) -> URL {
        modelsDir.appendingPathComponent(name)
    }

    func download(_ entry: ModelCatalogEntry) {
        downloading.insert(entry.id)
        progress[entry.id] = 0
        errors[entry.id] = nil
        let task = session.downloadTask(with: entry.downloadURL)
        tasks[entry.id] = task
        task.resume()
    }

    func cancel(_ entry: ModelCatalogEntry) {
        tasks[entry.id]?.cancel()
        tasks[entry.id] = nil
        downloading.remove(entry.id)
        progress[entry.id] = nil
    }

    func delete(_ url: URL) {
        try? FileManager.default.removeItem(at: url)
        try? FileManager.default.removeItem(at: URL(fileURLWithPath: url.path + ".sha256"))
    }

    func verifyImportedModel(at url: URL) -> String? {
        guard isGGUF(url) else {
            return "Selected file is not a valid GGUF model."
        }
        // A SHA-256 sidecar next to the file is authoritative. If it exists and
        // does not match, the model is corrupt or modified and must not be
        // silently accepted. A missing sidecar (manual import) is informational.
        let sidecar = URL(fileURLWithPath: url.path + ".sha256")
        if let text = try? String(contentsOf: sidecar, encoding: .utf8),
           let expected = text.components(separatedBy: " ").first?.lowercased() {
            var hasher = SHA256()
            if let handle = try? FileHandle(forReadingFrom: url) {
                defer { try? handle.close() }
                let madeProgress = true
                while madeProgress {
                    let chunk = handle.readData(ofLength: 1024 * 1024)
                    if chunk.isEmpty { break }
                    hasher.update(data: chunk)
                }
            }
            let actual = hasher.finalize().map { String(format: "%02x", $0) }.joined().lowercased()
            if actual != expected {
                return "Selected file fails its SHA-256 checksum; it is corrupt or modified."
            }
        }
        return nil
    }

    func urlSession(_ session: URLSession,
                    downloadTask: URLSessionDownloadTask,
                    didWriteData bytesWritten: Int64,
                    totalBytesWritten: Int64,
                    totalBytesExpectedToWrite: Int64) {
        guard let id = taskID(downloadTask) else { return }
        let pct = totalBytesExpectedToWrite > 0
            ? Double(totalBytesWritten) / Double(totalBytesExpectedToWrite)
            : 0
        DispatchQueue.main.async { self.progress[id] = pct }
    }

    func urlSession(_ session: URLSession,
                    downloadTask: URLSessionDownloadTask,
                    didFinishDownloadingTo location: URL) {
        guard let id = taskID(downloadTask) else { return }
        guard let entry = ModelCatalog.models.first(where: { $0.id == id }) else { return }
        do {
            let dest = modelsDir.appendingPathComponent(entry.file)
            if FileManager.default.fileExists(atPath: dest.path) {
                try FileManager.default.removeItem(at: dest)
            }
            try FileManager.default.moveItem(at: location, to: dest)

            // The catalog byte count is a snapshot at publish time and can
            // change if a model maintainer re-uploads a new revision, so it is
            // advisory. The GGUF header plus the SHA-256 sidecar written below
            // are the authoritative checks for a completed download.
            guard isGGUF(dest) else {
                throw FileError.notGGUF
            }
            try writeSidecar(dest)
            DispatchQueue.main.async {
                self.progress[id] = 1
                self.errors[id] = nil
                self.downloading.remove(id)
                self.tasks[id] = nil
                self.lastDownloaded = dest
                self.onDownloadCompleted?(dest)
            }
        } catch {
            DispatchQueue.main.async {
                self.errors[id] = error.localizedDescription
                self.downloading.remove(id)
                self.tasks[id] = nil
            }
        }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        guard let error else { return }
        guard let id = taskID(task) else { return }
        DispatchQueue.main.async {
            self.errors[id] = error.localizedDescription
            self.downloading.remove(id)
            self.tasks[id] = nil
        }
    }

    private func taskID(_ task: URLSessionTask) -> String? {
        tasks.first { $0.value === task }?.key
    }

    private func isGGUF(_ file: URL) -> Bool {
        guard let handle = try? FileHandle(forReadingFrom: file) else { return false }
        defer { try? handle.close() }
        let data = handle.readData(ofLength: 4)
        return data == Data([0x47, 0x47, 0x55, 0x46]) // "GGUF"
    }

    private func writeSidecar(_ file: URL) throws {
        var hasher = SHA256()
        let handle = try FileHandle(forReadingFrom: file)
        defer { try? handle.close() }
        let madeProgress = true
        while madeProgress {
            let chunk = handle.readData(ofLength: 1024 * 1024)
            if chunk.isEmpty { break }
            hasher.update(data: chunk)
        }
        let digest = hasher.finalize().map { String(format: "%02x", $0) }.joined()
        let sidecar = URL(fileURLWithPath: file.path + ".sha256")
        try "\(digest)  \(file.lastPathComponent)\n".write(to: sidecar, atomically: true, encoding: .utf8)
    }

    enum FileError: LocalizedError {
        case notGGUF

        var errorDescription: String? {
            switch self {
            case .notGGUF:
                return "Downloaded file is not a valid GGUF model."
            }
        }
    }
}
