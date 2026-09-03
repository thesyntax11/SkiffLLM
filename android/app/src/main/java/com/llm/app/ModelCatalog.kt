package com.llm.app

data class ModelCatalogEntry(
    val id: String,
    val name: String,
    val description: String,
    val repo: String,
    val file: String,
    val bytes: Long,
    val license: String,
    val ramNote: String,
    val recommended: Boolean
) {
    val sizeText: String
        get() = formatBytes(bytes)

    private fun formatBytes(value: Long): String {
        val gb = value / (1024.0 * 1024.0 * 1024.0)
        if (gb >= 0.1) {
            return "%.1f GB".format(gb)
        }
        return "%.0f MB".format(value / (1024.0 * 1024.0))
    }
}

object ModelCatalog {
    const val BASE_URL = "https://huggingface.co"

    val models = listOf(
        ModelCatalogEntry(
            id = "qwen2.5-0.5b",
            name = "Qwen2.5 0.5B Instruct",
            description = "Best first model on older phones. Fast, small, follows chat well.",
            repo = "Qwen/Qwen2.5-0.5B-Instruct-GGUF",
            file = "qwen2.5-0.5b-instruct-q4_k_m.gguf",
            bytes = 491400032L,
            license = "Apache-2.0",
            ramNote = "~1 GB working set",
            recommended = true
        ),
        ModelCatalogEntry(
            id = "qwen3-0.6b",
            name = "Qwen3 0.6B Instruct",
            description = "Small Qwen3 with optional thinking mode. Good quality for its size.",
            repo = "bartowski/Qwen_Qwen3-0.6B-GGUF",
            file = "Qwen_Qwen3-0.6B-Q4_K_M.gguf",
            bytes = 484220320L,
            license = "Apache-2.0",
            ramNote = "~1 GB working set",
            recommended = true
        ),
        ModelCatalogEntry(
            id = "llama3.2-1b",
            name = "Llama 3.2 1B Instruct",
            description = "Balanced quality and speed for typical modern phones.",
            repo = "bartowski/Llama-3.2-1B-Instruct-GGUF",
            file = "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
            bytes = 807694464L,
            license = "Llama 3.2 Community License",
            ramNote = "~1.5 GB working set",
            recommended = true
        ),
        ModelCatalogEntry(
            id = "smollm2-1.7b",
            name = "SmolLM2 1.7B Instruct",
            description = "Mid-size option with a solid quality-to-speed balance.",
            repo = "bartowski/SmolLM2-1.7B-Instruct-GGUF",
            file = "SmolLM2-1.7B-Instruct-Q4_K_M.gguf",
            bytes = 1055609824L,
            license = "Apache-2.0",
            ramNote = "~2 GB working set",
            recommended = false
        ),
        ModelCatalogEntry(
            id = "phi3.5-mini",
            name = "Phi-3.5 Mini Instruct",
            description = "Better reasoning. Use only on an 8 GB+ phone.",
            repo = "bartowski/Phi-3.5-mini-instruct-GGUF",
            file = "Phi-3.5-mini-instruct-Q4_K_M.gguf",
            bytes = 2393232672L,
            license = "MIT",
            ramNote = "~4 GB working set",
            recommended = false
        )
    )

    fun downloadUrl(entry: ModelCatalogEntry): String =
        "$BASE_URL/${entry.repo}/resolve/main/${entry.file}"
}
