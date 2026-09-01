package com.skifflm.app

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.StatFs
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.security.DigestInputStream
import java.security.MessageDigest
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class ModelDownloader(private val appContext: Context) {

    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private val uiHandler = Handler(Looper.getMainLooper())
    private val active = ConcurrentHashMap<String, HttpURLConnection>()
    private val cancelled = ConcurrentHashMap.newKeySet<String>()
    private val modelDir = File(appContext.filesDir, "models").apply { mkdirs() }

    fun download(
        entry: ModelCatalogEntry,
        onProgress: (Int, Long, Long) -> Unit,
        onDone: (File) -> Unit,
        onError: (String) -> Unit
    ) {
        cancelled.remove(entry.id)
        executor.execute {
            var connection: HttpURLConnection? = null
            var temp: File? = null
            try {
                if (!hasEnoughSpace(entry.bytes)) {
                    throw IOException(
                        "Not enough free storage. Need ${entry.sizeText}, " +
                            "but the device is low on space."
                    )
                }
                val url = URL(ModelCatalog.downloadUrl(entry))
                connection = (url.openConnection() as HttpURLConnection).apply {
                    instanceFollowRedirects = true
                    connectTimeout = 30_000
                    readTimeout = 30_000
                    setRequestProperty("User-Agent", "SkiffLLM-Android/1.6.0")
                }
                active[entry.id] = connection

                val code = connection.responseCode
                if (code !in 200..299) {
                    throw IOException("Hugging Face returned HTTP $code")
                }

                val total = connection.contentLengthLong.takeIf { it > 0 } ?: 0L
                temp = File(modelDir, entry.file + ".part")

                val input = connection.inputStream
                val output = FileOutputStream(temp)
                var downloaded = 0L
                var lastPercent = -1
                val buffer = ByteArray(64 * 1024)

                try {
                    while (true) {
                        if (cancelled.contains(entry.id)) {
                            throw IOException("Download cancelled")
                        }
                        val read = input.read(buffer)
                        if (read < 0) {
                            break
                        }
                        output.write(buffer, 0, read)
                        downloaded += read
                        val percent = if (total > 0) {
                            ((downloaded * 100L) / total).toInt().coerceIn(0, 100)
                        } else {
                            -1
                        }
                        if (percent != lastPercent) {
                            lastPercent = percent
                            uiHandler.post { onProgress(percent, downloaded, total) }
                        }
                    }
                } finally {
                    input.close()
                    output.close()
                }

                if (cancelled.contains(entry.id)) {
                    throw IOException("Download cancelled")
                }
                if (total > 0L && downloaded < total) {
                    throw IOException("Download incomplete")
                }
                val completed = temp ?: throw IOException("Missing temporary file")
                if (entry.bytes > 0L && completed.length() != entry.bytes) {
                    throw IOException(
                        "Size mismatch: expected ${entry.sizeText}, found ${completed.length()} bytes"
                    )
                }
                if (!completed.isGgufFile()) {
                    throw IOException("Downloaded file is not a valid GGUF model")
                }

                val target = File(modelDir, entry.file)
                if (target.exists() && !target.delete()) {
                    throw IOException("Unable to replace the existing model file")
                }
                if (!completed.renameTo(target)) {
                    throw IOException("Unable to store the downloaded model")
                }
                writeSha256Sidecar(target)
                uiHandler.post { onProgress(100, target.length(), target.length()) }
                uiHandler.post { onDone(target) }
            } catch (t: Throwable) {
                temp?.delete()
                uiHandler.post { onError(t.message ?: "Download failed") }
            } finally {
                active.remove(entry.id)
                cancelled.remove(entry.id)
                connection?.disconnect()
            }
        }
    }

    fun cancel(entryId: String) {
        cancelled.add(entryId)
        active[entryId]?.disconnect()
    }

    fun shutdown() {
        active.values.forEach { it.disconnect() }
        active.clear()
        cancelled.clear()
        executor.shutdownNow()
    }

    private fun hasEnoughSpace(bytes: Long): Boolean {
        if (bytes <= 0L) {
            return true
        }
        return runCatching {
            val stat = StatFs(modelDir.absolutePath)
            stat.availableBytes > bytes + (64L * 1024L * 1024L)
        }.getOrDefault(true)
    }

    private fun writeSha256Sidecar(file: File) {
        val digest = MessageDigest.getInstance("SHA-256")
        DigestInputStream(file.inputStream(), digest).use { input ->
            val buffer = ByteArray(64 * 1024)
            while (input.read(buffer) != -1) {
                // Continue draining the stream so the digest covers the whole file.
            }
        }
        val hex = digest.digest().joinToString("") { byte ->
            "%02x".format(byte.toInt() and 0xFF)
        }
        File(file.absolutePath + ".sha256").writeText("$hex  ${file.name}\n")
    }

    companion object {
        fun deleteWithSidecar(file: File): Boolean {
            val deleted = file.delete()
            val sidecar = File(file.absolutePath + ".sha256")
            if (sidecar.exists()) {
                sidecar.delete()
            }
            return deleted
        }
    }
}

private fun File.isGgufFile(): Boolean {
    if (!exists() || length() < 4L) {
        return false
    }
    val header = ByteArray(4)
    inputStream().use { input ->
        val read = input.read(header)
        if (read < 4) {
            return false
        }
    }
    return header[0] == 'G'.code.toByte() &&
        header[1] == 'G'.code.toByte() &&
        header[2] == 'U'.code.toByte() &&
        header[3] == 'F'.code.toByte()
}
