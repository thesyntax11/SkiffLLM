# Kullanım Kılavuzu

## Unix boru hattı modu

SkiffLLM, tüp aracılığıyla gelen stdin'i otomatik algılar; böylece model kabuk
akışının bir parçası olur:

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
cat README.md | skifflm "summarize this"
skifflm --project . "where is authentication handled?"
```

Talimat argümanı yoksa tüpten gelen metnin kendisi istemdir. Talimat argümanı
varsa tüpten gelen metin `<context>` olur.

## Proje bağlamı

```bash
skifflm --project . "where is authentication handled?"
```

`--project <dir>` üretimden önce sınırlı bir dosya indeksi artı kaynak/yapılandırma
içeriğinden bir dilim oluşturur. `.git`, derleme dizinleri, önbellekler ve
satıcı bağımlılıklarını atlar.

## Oturumlar ve bellek

```bash
skifflm session list
skifflm session show coding
skifflm session rename coding writing
skifflm session remove old-draft

skifflm --remember "the user prefers concise answers"
skifflm --forget concise
```

Etkileşimli kabuk komutları: `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (uzun bir konuşmayı bilgileri
koruyarak madde özetine sıkıştırır) ve `/regenerate` veya `/retry`
(son kullanıcı mesajını geçerli örnekleme ayarlarıyla yeniden çalıştırır).

## Özetleme kısayolu

```bash
skifflm --summarize README.md
skifflm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Model yöneticisi

```bash
skifflm model list
skifflm model info qwen2.5-0.5b
skifflm model install qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b --update
skifflm model remove qwen2.5-0.5b --force
```

`model verify`, GGUF sihirli başlığını ve varsa SHA-256 yan dosyasını denetler.
Katalog boyutu yalnızca bilgilendiricidir; yeni bir üst sürüm yalnızca bayt
sayısı değiştiği için reddedilmez. `--update` yerel bir SHA-256 yan dosyası kaydeder.
`model_fetch.py` ile indirmeler de bu yan dosyayı otomatik yazar ve yeniden
indirmeden `--verify` destekler.

Çıkarım çevrimdışı kalır. `model install`, açık `model_fetch.py` yardımcısını
HTTPS üzerinden çalıştırır.

## Tek seferlik çalıştırma ve sohbet şablonları

```bash
skifflm run "Hello" --ctx 2048 --temp 0.3 --threads 4
skifflm chat-template list
skifflm chat-template detect --model model.gguf
```

## OpenAI istemcisi

```bash
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

## Donanım hızlandırma

```bash
skifflm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_LLAMA_BACKEND=cuda
./build/skifflm --model model.gguf --gpu-layers -1 --flash-attn
```

Arka uçlar derleme sırasında seçilir; `--backend-info` gerçekte bağlı olanı raporlar.

## Git entegrasyonu

```bash
skifflm git review --cached
skifflm git explain
skifflm git commit --cached
skifflm git log
skifflm git status
```

## Güvenli kod modu

```bash
skifflm --code --project . "fix the bug in src/server.cpp"
```

`--code` birleşik diff önerisi üretir ve dosyayı kendisi asla düzenlemez.

## Etkileşimli mod

```bash
skifflm --model ~/models/model-q4_k_m.gguf
```

Arayüz `you>` adıyla bir istemdir. Bir mesaj yazın ve Enter'a basın. Metin
üretildikçe akış halinde gösterilir.

## Tek seferlik mod

```bash
skifflm --model model.gguf --prompt "What is recursion?"
```

## Adlandırılmış oturumlar

```bash
skifflm --model model.gguf --session writing
skifflm --model model.gguf --session coding
```

Her adlandırılmış oturum kendi geçmiş dosyasını alır.

## Sistem istemi

```bash
skifflm --model model.gguf --system "You are a patient Python tutor."
```

## Profiller

```bash
skifflm --model model.gguf --profile code
```

Kullanılabilir profiller: `balanced`, `fast`, `creative`, `code`, `precise`.

## Dosya bağlamı

```bash
skifflm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
skifflm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

`--attach` tekrarlayın, istemde `@path` kullanın veya kabuk içinde `/file` ve
`/clear-attach` ile ekleri yönetin.

## Konuşma dışa aktarma

```bash
skifflm --export conversation.md
```

Yüklü oturumu dışa aktarmak model gerektirmez ve Markdown yazar. Kabuk içinde
`/export <path>` kullanın.

## Sohbet şablonu ve ısındırma

```bash
skifflm --model model.gguf --chat-template chatml
skifflm --model model.gguf --warmup
```

`--chat-template`, modelin yerleşik istem biçimi adını geçersiz kılar.
`--warmup`, ilk yanıt gecikmesini azaltmak için başlangıçta tek token'lık bir
üretim çalıştırır. Kabuk içinde `/warmup` ile modeli yeniden ısıtabilirsiniz.

## Etkileşimli satır düzenleme

GNU Readline varsa SkiffLLM ok tuşu gezinmesi, geçmiş arama ve düzenlenebilir
girdi satırlarını etkinleştirir. Readline yoksa düz `getline` kullanılır.
Akış çıktısı, etkileşimli terminallerde canlı token sayacı da gösterir.

## Durdurma dizileri

```bash
skifflm --model model.gguf --stop "END" --stop "STOP"
```

Üretim, yapılandırılan ilk dizide durur.

## JSON modu

```bash
skifflm --model model.gguf --prompt "Say hello" --json
```

Bu, etkileşimli kabuğu kapatır ve stdout'a tek bir JSON nesnesi yazar.

## Tüp kullanımı

```bash
cat prompt.txt | skifflm --model model.gguf
printf 'Explain this command.' | skifflm --model model.gguf --prompt-file /dev/stdin
```

## Çıktı dosyaları

```bash
skifflm --model model.gguf --prompt-file input.txt --output output.md
```

## Yapılandırma dosyası

```bash
skifflm --config ~/.config/skifflm/config
```

Varsa varsayılan konum otomatik kullanılır.

## Tanılama

```bash
skifflm --doctor
skifflm --model model.gguf --model-info
skifflm --model model.gguf --smoke
skifflm --model model.gguf --tokenize "hello world"
```

## Yerel API sunucusu

```bash
# yalnızca yerel
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# korumalı yerel olmayan dinleyici
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

Uç noktalar:

```text
GET  /health                 herkese açık sağlık kontrolü
GET  /v1/models              --api-key ayarlıysa Bearer ile korunur
POST /v1/chat/completions    --api-key ayarlıysa Bearer ile korunur
```

Sunucu çevrimdışıdır, varsayılan olarak localhost'a bağlanır ve `"stream": true`
ile OpenAI tarzı akışı destekler. Bir üretim çalışırken hızlı uç noktalar yanıt
verir; sohbet üretimi bir mutex arkasında seri hale getirilir.
`--api-key` ayarlandığında `/v1/*` yolları `Authorization: Bearer <key>` ister,
aksi halde `401` döner.

Hızlı bir istemci dahildir:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$SKIFFLLM_SERVER_KEY" "Say hello."
```

## Benchmark

```bash
skifflm --model model.gguf --benchmark 3
skifflm --model model.gguf --benchmark 3 --json
```

Gerçek bir üretim çalıştırır ve ölçülen istem süresini, üretim süresini ve
saniyedeki token sayısını raporlar. `--n-predict`, çalışma başına en fazla 128
token'a kadar çalışma uzunluğunu kontrol eder.

## GPU boşaltma

CUDA'lı bir makinede:

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
skifflm --model model.gguf --gpu-layers -1
```

macOS'ta Metal arka ucu varsayılan olarak kullanılabilir.

## Model listeleme

```bash
skifflm --model-dir ~/models --list-models
```
