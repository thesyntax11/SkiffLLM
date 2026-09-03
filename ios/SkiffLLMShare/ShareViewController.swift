import UIKit
import UniformTypeIdentifiers

final class ShareViewController: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        let shared = UserDefaults(suiteName: "group.com.skiffllm.app")
        if let text = extractText(), !text.isEmpty {
            shared?.set(text, forKey: "shared_text")
            shared?.synchronize()
        }
        extensionContext?.completeRequest(returningItems: nil)
    }

    private func extractText() -> String? {
        guard let items = extensionContext?.inputItems as? [NSExtensionItem] else { return nil }
        for item in items {
            for provider in item.attachments ?? [] {
                if provider.hasItemConformingToTypeIdentifier(UTType.plainText.identifier) {
                    if let value = loadString(provider, type: .plainText) { return value }
                }
                if provider.hasItemConformingToTypeIdentifier(UTType.url.identifier) {
                    if let value = loadString(provider, type: .url) { return value }
                }
            }
        }
        return nil
    }

    private func loadString(_ provider: NSItemProvider, type: UTType) -> String? {
        var result: String?
        let semaphore = DispatchSemaphore(value: 0)
        provider.loadItem(forTypeIdentifier: type.identifier, options: nil) { item, _ in
            if let text = item as? String {
                result = text
            } else if let url = item as? URL {
                result = url.absoluteString
            } else if let data = item as? Data {
                result = String(data: data, encoding: .utf8)
            }
            semaphore.signal()
        }
        if semaphore.wait(timeout: .now() + 5) == .timedOut {
            return nil
        }
        return result
    }
}
