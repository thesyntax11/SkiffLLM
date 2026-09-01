## SkiffLLM

<p align="center">
  <img src="docs/logo.svg" alt="SkiffLLM logo" width="128" height="128"/>
</p>

<p align="center">
  <strong>Herhangi bir GGUF modelini Unix komut hattı gibi çalıştırın.</strong><br/>
  Yerel. Çevrimdışı. Bulut hesabı yok, telemetri yok, arka plan servisi yok.
</p>

<p align="center">
  <strong>Diller:</strong>
  <a href="README.md">English</a> ·
  <a href="README.tr.md">Türkçe</a> ·
  <a href="README.de.md">Deutsch</a> ·
  <a href="README.es.md">Español</a> ·
  <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License MIT"/>
  <img src="https://img.shields.io/badge/version-v1.6.0-blue" alt="Version v1.6.0"/>
  <img src="https://img.shields.io/badge/c%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android%20%7C%20iOS-lightgrey" alt="Platforms"/>
  <img src="https://img.shields.io/badge/runtime-offline-green" alt="Offline"/>
</p>

SkiffLLM, Unix aracı gibi hissettiren tek parça bir yerel yapay zekâ çalışma
zamanıdır. Herhangi bir GGUF modelini CPU veya GPU’nuzda llama.cpp üzerinden
çalıştırır, her token'ı kendi makinanızda tutar ve doğrudan kabuk akışınıza
girer.

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
cat README.md | skifflm "summarize this"
skifflm --project . "where is this implemented?"
```

---

## Başlarken

### Tek komutla (kaynak koddan)

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
skifflm --version
```

### Ya da elle derleyin

```bash
cmake --preset release
cmake --build build/release -j
ctest --test-dir build/release --output-on-failure
```

### Ya da yayınlanmış bir sürümü kurun

```bash
bash scripts/install-from-release.sh --version v1.6.0
```

Bu yardımcı script, işletim sisteminizi ve mimarinizi sürüm arşiviyle eşleştirir.
Sürümde henüz ürün dosyası yoksa hızlıca hata verir.

### Demo kaydedin

```bash
bash scripts/demo-capture.sh ./build/release/skifflm /path/to/model.gguf
```

Bu, README için gerçek bir terminal oturumu kaydeder; asla sahte çıktı üretmez.

### Küçük bir model edinin

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Dosyalar varsayılan olarak `~/.local/share/skifflm/models` dizinine kaydedilir.
Mevcut herhangi bir `.gguf` dosyasına da `--model` ile işaret edebilirsiniz.

Model dosyaları Hugging Face'ten kendi üst lisansları altında indirilir;
SkiffLLM bunları yeniden dağıtmaz. Kullanmadan önce her modelin lisansına uyun.
Bundled katalogdaki lisanslar: Qwen2.5/Qwen3 Apache-2.0, SmolLM2 Apache-2.0,
Phi-3.5 MIT ve Llama 3.2 **Llama 3.2 Community License**'dır (atıf ve adlandırma
koşulları içerir).

Tam kurulum: [docs/tr/INSTALL.md](docs/tr/INSTALL.md).

---

## Neden SkiffLLM

| | SkiffLLM | Bulut asistanları | Ollama |
| --- | --- | --- | --- |
| Bulut gerekli | ❌ | ✅ | ❌ |
| Bulut API anahtarı | ❌ | ✅ | ❌ |
| Hesap / kayıt | ❌ | ✅ | ❌ |
| Telemetri | ❌ | değişir | ❌ |
| Arka plan servisi | ❌ | ✅ | ✅ |
| Doğrudan GGUF dosyası | ✅ | ❌ | kısmen |
| Yalnızca CPU makineler | ✅ | ❌ | ✅ |
| GPU offload | ✅ | yok | ✅ |
| Unix boru hatları | ✅ | ❌ | ❌ |
| Proje/kod bağlamı | ✅ | ❌ | ❌ |
| Yerel OpenAI uyumlu API | ✅ | yok | ✅ |
| Yerel Android istemci | ✅ | ❌ | topluluk |
| Yerel iOS istemci | ✅ | ❌ | topluluk |

Çalışma zamanı asla model indirmez, dışarıya veri göndermez ve hesap gerektirmez.
Siz GGUF dosyasını getirirsiniz; SkiffLLM çıkarımı yapar.

### Peki Ollama varken neden SkiffLLM?

Kısa cevap: Büyük bir model kataloğu ve tek komutla indirme isteyen hızlı bir
model sunucusu için Ollama'yı; modelin Unix aracı gibi davranmasını, hava
boşluklu (air-gapped) bir bileşen ya da CI/masaüstü/mobil iş akışına gömülü bir
motor olmasını istiyorsan SkiffLLM'i kullan. SkiffLLM arka plan servisi
çalıştırmaz (daemon yok), tek bir binary'dir ve çekirdek çıkarımı asla ağa
bağlanmaz. Dürüst karar matrisi, gerçek ödünler ve görev görev geçiş rehberi
[docs/tr/comparison.md](docs/tr/comparison.md) ile İngilizce tam sürümde:
[docs/COMPARISON.md](docs/COMPARISON.md). Kurumsal dağıtım, sunucu sıkılaştırma
ve tedarik zinciri için İngilizce [docs/ENTERPRISE.md](docs/ENTERPRISE.md)
sayfasına bakın.

---

## Öne çıkanlar

| Özellik | Açıklama |
| --- | --- |
| Unix boru hatları | `cat file \| skifflm "summarize"`, `git diff \| skifflm "review"` |
| Proje bağlamı | `--project <dir>` gerçek dosya indeksi + sınırlı kaynak kod dilimi ekler |
| Model yöneticisi | `skifflm model list / info / install / remove / verify` |
| Git entegrasyonu | `skifflm git review / explain / commit / log / status` |
| Etkileşimli kabuk | Akış halinde token çıktısı, geçmiş, canlı sayaçlar |
| Dosya bağlamı | Her istemde `--attach`, `/file` ve `@file` genişletme |
| Konuşma dışa aktarma | `--export` ve `/export` oturumları Markdown olarak kaydeder |
| Oturum ve bellek | Adlandırılmış oturumlar, `/remember`, `/forget`, kalıcı bilgiler |
| Yerel API sunucusu | `--serve` OpenAI uyumlu bir uç nokta sunar |
| Sunucu koruması | `/v1/*` üzerinde isteğe bağlı `--api-key` Bearer kimlik doğrulaması |
| Gerçek benchmark | `--benchmark <runs>` gerçek istem ve üretim hızını ölçer |
| Örnekleme profilleri | `balanced`, `fast`, `creative`, `code`, `precise` |
| Gelişmiş örnekleme | temperature, top-p, top-k, min-p, typical-p, penalizasyonlar |
| Bağlam yönetimi | otomatik kırpma, ayrılmış üretim alanı, `/compact` |
| JSON modu | Betikler ve araçlar için makine tarafından okunabilir çıktı |
| Tanılama | `--doctor` sistem raporu, `--model-info`, `--tokenize` |
| Yerel mobil uygulamalar | Android (Jetpack Compose) ve iOS (SwiftUI) |

---

## Pratik örnekler

```bash
# Göndermeden önce değişiklik kümesini inceleyin
git diff | skifflm "review these changes"

# Karmaşık bir günlükteki gerçek nedeni bulun
journalctl -e | skifflm "find suspicious errors and a likely root cause"

# Yeni okuduğunuz bir dosyayı özetleyin
cat README.md | skifflm "summarize this"

# Tüm depoya işaret edin
skifflm --project . "where is authentication implemented?"

# Kendi betikleriniz için makine tarafından okunabilir çıktı
git diff | skifflm --json "classify this diff"

# Güvenli kod incelemesi (diff önerir, dosyaları asla düzenlemez)
skifflm --code --project . "fix the bug in src/server.cpp"
```

## Model yöneticisi

SkiffLLM çalışma zamanında çevrimdışı kalır. Model elde etme açık ve ayrı bir
komuttur.

```bash
skifflm model list
skifflm model info qwen2.5-0.5b
skifflm model install qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b --update
skifflm model remove qwen2.5-0.5b --force
```

`model install`, `scripts/model_fetch.py` dosyasına devreder. Bu script Hugging
Face üzerinden HTTPS ile tam olarak bir GGUF indirir, GGUF başlığını denetler ve
model dizininize bir SHA-256 yan dosyası kaydeder. Katalog boyutu yalnızca
bilgilendiricidir; model bakımcısı farklı boyutta yeni bir sürüm yükleyebilir.
Bütünlük için esas kontrol yan dosyadır. Çıkarım kendisi asla bağlantı açmaz.

## Git entegrasyonu

Karşınızdaki diff için yerel, çevrimdışı kod incelemesi ve açıklama.

```bash
# git alt komutları diff’i kendisi okur; boru hattı gerekmez.
skifflm git review
skifflm git review --cached
skifflm git explain
skifflm git commit --cached
skifflm git log
skifflm git status
```

`git commit --cached`, hazırlanmış diff'inizden geleneksel bir commit mesajı
önerir; `git commit` komutunu sizin için çalıştırmaz.

## Oturumlar ve kalıcı bellek

```bash
skifflm --session coding --model qwen2.5-0.5b-instruct-q4_k_m.gguf
skifflm --session writing --model qwen2.5-0.5b-instruct-q4_k_m.gguf

skifflm session list
skifflm session show coding
skifflm session rename coding writing
skifflm session remove old-draft
```

Kalıcı bellek `~/.local/share/skifflm/memories.txt` dosyasında durur ve makineden
asıla ayrılmaz.

```bash
skifflm --remember "the user prefers concise answers"
skifflm --forget concise
```

Etkileşimli kabukta `/remember`, `/forget`, `/memories`,
`/clear-memories`, `/compact`, `/regenerate` ve `/export` komutlarını kullanın.

---

## Yerel OpenAI uyumlu sunucu

```bash
# yalnızca yerel
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# paylaşılan bir anahtarla korunan yerel olmayan dinleyici
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

Uç noktalar:

```text
GET  /health                 herkese açık sağlık kontrolü
GET  /version                herkese açık sürüm kontrolü
GET  /v1/models              --api-key ayarlıysa Bearer ile korunur
POST /v1/chat/completions    --api-key ayarlıysa Bearer ile korunur
```

Sunucu OpenAI tarzı akışı destekler (`"stream": true`) ve bir üretim
çalışırken hızlı uç noktalara yanıt verir. Bir llama.cpp bağlamı iş parçacığı
güvenli olmadığı için sohbet üretimi seri hale getirilir.

```python
from openai import OpenAI

client = OpenAI(base_url="http://127.0.0.1:8080/v1", api_key="local-token")
resp = client.chat.completions.create(
    model="qwen2.5-0.5b-instruct-q4_k_m",
    messages=[{"role": "user", "content": "Explain this diff."}],
    stream=True,
)
for chunk in resp:
    print(chunk.choices[0].delta.content or "", end="")
```

Bağımlılıksız bir Python istemcisi dahildir:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$SKIFFLLM_SERVER_KEY" "Say hello."
```

---

## Mobil uygulamalar

SkiffLLM, [Android](android/README.md) ve [iOS](ios/README.md) için yerel
istemciler sunar. Her ikisi de desteklenen GGUF modellerini tamamen cihazda
çalıştırır, token'ları akış halinde gösterir, canlı bağlam kullanım çubuğu görüntüler
ve masaüstü CLI ile aynı özellik yüzeyini sunar:

- konuşma başına örnekleme ayarları ve kod modu
- çoklu konuşma oluşturma/açma/yeniden adlandırma/silme/yedekleme/içe aktarma
- kalıcı bilgiler, hızlı istemler ve paylaşım sayfası girişi
- durdurma dizileri, örnekleme profilleri ve konuşma sıkıştırma
- model ısındırma, gerçek 3 turlu benchmark, oturum istatistikleri
- metin/JSON/XML dosya ekleme (yerel okunur, sınırlı)
- Markdown dışa aktarma ve tek dokunuşla kopyalama
- başlık doğrulamalı GGUF içe aktarma

Çıkarım kendisi asla bağlantı açmaz. İki ağ yolu vardır, ikisi de açık ve
kullanıcı tarafından başlatılır: `model install` Hugging Face'ten indirir,
`openai` alt komutu ise işaret ettiğiniz HTTP sunucusuyla konuşur. İndirilen
dosyanın geçerli bir GGUF başlığı olmalıdır ve SHA-256 yan dosyası yazılır;
katalog boyutu bilgilendiricidir. Android cihaz yedekleri kapatıldığı için
istemler ve geçmiş cihazdan asla ayrılmaz.

---

## Komut satırı

```text
Usage: skifflm [options] [model.gguf]

Core options:
  --model <path>             Path to a GGUF model file
  --model-dir <path>         Directory scanned for a GGUF model
  --list-models              Print discovered GGUF models
  --model-info               Print model metadata and exit
  --smoke                    Run a quick generation smoke test
  --warmup                   Warm the model before the first answer
  --doctor                   Print system diagnostics
  --tokenize <text>          Tokenize text and print token counts
  --profile <name>           balanced, fast, creative, code or precise
  --session <name>           Use a named conversation
  --system <text>            System prompt
  --stop <text>              Stop sequence; can be repeated
  --attach <path>            Attach a file; can be repeated
  --file <path>             Alias for --attach; can be repeated
  --chat-template <name>     Override the model chat template
  --export <path>            Export the loaded conversation as Markdown
  --serve                    Serve a local OpenAI-compatible API
  --host <addr>              Local server bind address (default: 127.0.0.1)
  --port <n>                 Local server port (default: 8080)
  --api-key <key>            Require Bearer auth on the local server
  --benchmark <runs>         Run a real generation benchmark

Subcommands:
  run [prompt] [opts]        One-shot prompt
  model list|info|install|remove|verify
  chat-template list|detect|info
  openai [prompt] [opts]     Send a prompt to a local OpenAI-compatible server
  config path|show|init      Manage the config file
  server health [--json]     Check a running local server
```

Tam kullanım: [docs/tr/usage.md](docs/tr/usage.md). Etkileşimli komutlar:
`/help`, `/warmup`, `/history`, `/stats`, `/compact`, `/regenerate`,
`/tokenize`, `/file`, `/clear-attach`, `/clear`, `/reset`, `/system`,
`/model`, `/profile`, `/stop`, `/temp`, `/top-p`, `/top-k`, `/min-p`,
`/typical`, `/n`, `/ctx`, `/export`, `/save`, `/exit`.

---

## Benchmark dürüstlüğü

```bash
skifflm --model model.gguf --benchmark 3
skifflm --model model.gguf --benchmark 3 --json
```

Her sayı, modeliniz ve donanımınızla kendi makinanızda ölçülür. SkiffLLM asla
benchmark sonucu uydurmaz. Yöntem ve gerçek katkıları bekleyen boş sonuç
tablosu için [docs/benchmarks.md](docs/benchmarks.md) dosyasına bakın.

## Gizlilik kanıtı

```bash
skifflm --doctor --network
```

çalışma zamanı gerçeklerini yazdırır: çekirdek üretim hiçbir giden çağrı yapmaz,
telemetri yok, bulut API'si yok, geçmiş yerel olarak saklanır. Tek ağ yolları
açık ve kullanıcı tarafından başlatılan işlemlerdir: `model install` (Hugging
Face indirmesi) ve `openai` alt komutu (işaret ettiğiniz sunucu). `--serve`
yalnızca yerel bir dinleyici açar, asla dışarı bağlanmaz.

---

## Kurulum

```bash
bash scripts/install.sh --help
bash scripts/install.sh --prefix "$HOME/.local"
bash scripts/install.sh --prefix /usr/local --backend metal
```

Kolaylaştırıcı `Makefile`:

```bash
make release
make tests
make check
make install
make help
```

Yayınlandığında hazır arşivler `skifflm-<version>-<os>-<arch>.tar.gz`
(ör. `skifflm-v1.6.0-linux-x86_64.tar.gz`, Windows’ta `.zip`) ve
`checksums.txt` biçimindedir. Bkz. [docs/tr/INSTALL.md](docs/tr/INSTALL.md).
Kabuk tamamlamaları [scripts/completions](scripts/completions/) içindedir.

## Derleme seçenekleri

| Seçenek | Varsayılan | Açıklama |
| --- | --- | --- |
| `SKIFFLLM_BUILD_TESTS` | `ON` | Test paketini derler ve kaydeder |
| `SKIFFLLM_FETCH_LLAMA` | `ON` | Sabitlenmiş llama.cpp'yi indirir ve derler |
| `SKIFFLLM_LLAMA_SOURCE_DIR` | boş | Mevcut bir llama.cpp kopyasını kullanır |
| `SKIFFLLM_BUILD_SHARED_LLAMA` | `OFF` | llama.cpp'yi paylaşılan kütüphane olarak derler |
| `SKIFFLLM_USE_READLINE` | `ON` | Varsa GNU Readline'ı etkinleştirir |
| `SKIFFLLM_LLAMA_BACKEND` | `auto` | `cuda`, `metal`, `vulkan`, `opencl`, `blas`, `cpu` |

Donanım hızlandırma her zaman açıktır: yapılandırma sırasında arka ucu seçin ve
çalışma zamanında `--gpu-layers` ile katmanları boşaltın.

---

## Gereksinimler

- C++17 derleyicisi (GCC 10+, Clang 12+ veya MSVC 2019+)
- CMake 3.20+
- Sabitlenmiş llama.cpp için isteğe bağlı CMake `FetchContent`
- Bir GGUF model dosyası
- İsteğe bağlı: donanım hızlandırma için CUDA/Metal/Vulkan/OpenCL/BLAS

---

## Proje düzeni

```text
include/skifflm/              Genel API başlıkları
src/                          CLI, çekirdek ve yerel sunucu uygulaması
tests/                        Birim testleri (modelsiz)
configs/                      Örnek yapılandırma
scripts/                      CI, sürüm, model indirme, API istemcisi, tamamlamalar
android/                      Kotlin/Compose Android uygulaması ve llama.cpp JNI
ios/                          SwiftUI iOS uygulaması ve llama.cpp Objective-C++ köprüsü
docs/                         Kurulum, kullanım, mimari, SSS, sürüm dokümanları
.github/                      Sorun şablonları, PR şablonu, CI iş akışı
```

---

## Yol haritası

- Embedding ve RAG modu
- Paket yöneticisi entegrasyonu (Homebrew, apt, vcpkg)
- Arka uçlar arası GPU benchmark matrisi
- Dilbilgisi kısıtlı üretim
- Token düzeyinde örnekleme tanılaması
- Çoklu modellerde çoklu işçi üretim

## Bilinen sınırlamalar

SkiffLLM model paketlemez. Yerel sunucu varsayılan olarak `127.0.0.1` üzerinde
herkese açıktır; yerel olmayan bir arayüze bağlarken `--api-key` kullanın. Bkz.
[docs/tr/LIMITATIONS.md](docs/tr/LIMITATIONS.md).

## Katkıda bulunma

Katkılar memnuniyetle karşılanır. Değişiklikleri küçük tutun, çevrimdışı
sözünü koruyun, asla benchmark sayısı uydurmayın ve yeni davranışlar için test
ekleyin. Bkz. [CONTRIBUTING.md](CONTRIBUTING.md) ve
[docs/tr/GOOD_FIRST_ISSUES.md](docs/tr/GOOD_FIRST_ISSUES.md). Çeviri rehberi için:
[docs/i18n.md](docs/i18n.md).

## Lisans

MIT — bkz. [LICENSE](LICENSE).
