# Kullanım Kılavuzu

## Unix boru hattı modu

SkiffLLM, tüp aracılığıyla gelen stdin'i otomatik algılar; böylece model kabuk
akışının bir parçası olur:

```bash
git diff | skiffllm "review these changes"
cat error.log | skiffllm "find the root cause"
cat README.md | skiffllm "summarize this"
skiffllm --project . "where is authentication handled?"
```

Talimat argümanı yoksa tüpten gelen metnin kendisi istemdir. Talimat argümanı
varsa tüpten gelen metin `<context>` olur.

## Proje bağlamı

```bash
skiffllm --project . "where is authentication handled?"
```

`--project <dir>` üretimden önce sınırlı bir dosya indeksi artı kaynak/yapılandırma
içeriğinden bir dilim oluşturur. `.git`, derleme dizinleri, önbellekler ve
satıcı bağımlılıklarını atlar.

## Oturumlar ve bellek

```bash
skiffllm session list
skiffllm session show coding
skiffllm session rename coding writing
skiffllm session remove old-draft

skiffllm --remember "the user prefers concise answers"
skiffllm --forget concise
```

Etkileşimli kabuk komutları: `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (uzun bir konuşmayı bilgileri
koruyarak madde özetine sıkıştırır) ve `/regenerate` veya `/retry`
(son kullanıcı mesajını geçerli örnekleme ayarlarıyla yeniden çalıştırır).

## Özetleme kısayolu

```bash
skiffllm --summarize README.md
skiffllm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Model yöneticisi

```bash
skiffllm model list
skiffllm model info qwen2.5-0.5b
skiffllm model install qwen2.5-0.5b
skiffllm model verify qwen2.5-0.5b
skiffllm model verify qwen2.5-0.5b --update
skiffllm model remove qwen2.5-0.5b --force
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
skiffllm run "Hello" --ctx 2048 --temp 0.3 --threads 4
skiffllm chat-template list
skiffllm chat-template detect --model model.gguf
```

## OpenAI istemcisi

```bash
skiffllm openai "Merhaba" --base-url http://127.0.0.1:8080
skiffllm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
skiffllm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

## Donanım hızlandırma

```bash
skiffllm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_LLAMA_BACKEND=cuda
./build/skiffllm --model model.gguf --gpu-layers -1 --flash-attn
```

Arka uçlar derleme sırasında seçilir; `--backend-info` gerçekte bağlı olanı raporlar.

## Git entegrasyonu

```bash
skiffllm git review --cached
skiffllm git explain
skiffllm git commit --cached
skiffllm git log
skiffllm git status
```

## Güvenli kod modu

```bash
skiffllm --code --project . "fix the bug in src/server.cpp"
```

`--code` birleşik diff önerisi üretir ve dosyayı kendisi asla düzenlemez.

## Etkileşimli mod

```bash
skiffllm --model ~/models/model-q4_k_m.gguf
```

Arayüz `you>` adıyla bir istemdir. Bir mesaj yazın ve Enter'a basın. Metin
üretildikçe akış halinde gösterilir.

## Tek seferlik mod

```bash
skiffllm --model model.gguf --prompt "What is recursion?"
```

## Adlandırılmış oturumlar

```bash
skiffllm --model model.gguf --session writing
skiffllm --model model.gguf --session coding
```

Her adlandırılmış oturum kendi geçmiş dosyasını alır.

## Sistem istemi

```bash
skiffllm --model model.gguf --system "You are a patient Python tutor."
```

## Profiller

```bash
skiffllm --model model.gguf --profile code
```

Kullanılabilir profiller: `balanced`, `fast`, `creative`, `code`, `precise`.

## Dosya bağlamı

```bash
skiffllm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
skiffllm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

`--attach` tekrarlayın, istemde `@path` kullanın veya kabuk içinde `/file` ve
`/clear-attach` ile ekleri yönetin.

## Konuşma dışa aktarma

```bash
skiffllm --export conversation.md
```

Yüklü oturumu dışa aktarmak model gerektirmez ve Markdown yazar. Kabuk içinde
`/export <path>` kullanın.

## Sohbet şablonu ve ısındırma

```bash
skiffllm --model model.gguf --chat-template chatml
skiffllm --model model.gguf --warmup
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
skiffllm --model model.gguf --stop "END" --stop "STOP"
```

Üretim, yapılandırılan ilk dizide durur.

## JSON modu

```bash
skiffllm --model model.gguf --prompt "Say hello" --json
```

Bu, etkileşimli kabuğu kapatır ve stdout'a tek bir JSON nesnesi yazar.

## Tüp kullanımı

```bash
cat prompt.txt | skiffllm --model model.gguf
printf 'Explain this command.' | skiffllm --model model.gguf --prompt-file /dev/stdin
```

## Çıktı dosyaları

```bash
skiffllm --model model.gguf --prompt-file input.txt --output output.md
```

## Yapılandırma dosyası

```bash
skiffllm --config ~/.config/skiffllm/config
```

Varsa varsayılan konum otomatik kullanılır.

## Tanılama

```bash
skiffllm --doctor
skiffllm --model model.gguf --model-info
skiffllm --model model.gguf --smoke
skiffllm --model model.gguf --tokenize "hello world"
```

## Yerel API sunucusu

```bash
# yalnızca yerel
skiffllm --model model.gguf --serve --host 127.0.0.1 --port 8080

# korumalı yerel olmayan dinleyici
skiffllm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
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
skiffllm --model model.gguf --benchmark 3
skiffllm --model model.gguf --benchmark 3 --json
```

Gerçek bir üretim çalıştırır ve ölçülen istem süresini, üretim süresini ve
saniyedeki token sayısını raporlar. `--n-predict`, çalışma başına en fazla 128
token'a kadar çalışma uzunluğunu kontrol eder.

## GPU boşaltma

CUDA'lı bir makinede:

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
skiffllm --model model.gguf --gpu-layers -1
```

macOS'ta Metal arka ucu varsayılan olarak kullanılabilir.

## Model listeleme

```bash
skiffllm --model-dir ~/models --list-models
```
