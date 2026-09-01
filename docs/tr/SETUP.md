# Kurulum

SkiffLLM'in çalışma zamanında ağ bağımlılığı yoktur. Bir kez derlersiniz ve
yerel bir GGUF modeliyle çalıştırırsınız.

## Gereksinimler

- C++17 derleyicisi (GCC 10+, Clang 12+ veya MSVC 2019+)
- CMake 3.20 veya daha yeni
- İsteğe bağlı: satır düzenleme ve kabuk geçmişi için GNU Readline
- İsteğe bağlı: GPU boşaltma için CUDA, Metal, ROCm, SYCL veya Vulkan arka ucu
- Bir GGUF model dosyası

## Derleme

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Açık bir GPU/NPU arka ucuyla kolaylaştırıcı kurulumu kullanın:

```bash
bash scripts/install.sh                            # CPU / platform varsayılanı
BACKEND=cuda bash scripts/install.sh               # CUDA
BACKEND=vulkan bash scripts/install.sh             # Vulkan
BACKEND=metal bash scripts/install.sh              # macOS Metal
./build/skifflm --backend-info                     # bağlı arka uçları görüntüle
```

İsteğe bağlı yapılandırma sırasındaki indirmeyi önlemek için mevcut bir llama.cpp
kopyasını kullanın:

```bash
scripts/setup.sh --source-dir /path/to/llama.cpp --backend cuda
```

## Kurulum

```bash
scripts/setup.sh --prefix /usr/local
cmake --install build --prefix /usr/local
```

## Model edinin

SkiffLLM çalışma zamanı asla model indirmez. Elinizde olan bir GGUF dosyasını
model dizinine kopyalayabilir veya yolunu doğrudan verebilirsiniz. Önerilen
örnek bir model isterseniz isteğe bağlı indirme yardımcısını bir kez çalıştırın:

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Yardımcı, dosyayı `~/.local/share/skifflm/models` dizinine kaydeder. Çıkarım
tamamen çevrimdışı kalır.

Ya da kendiniz kopyalayın:

```bash
mkdir -p ~/.local/share/skifflm/models
cp /path/to/model-q4_k_m.gguf ~/.local/share/skifflm/models/
```

Hızlı ve yalnızca CPU ile başlamak için önerilen küçük modeller:

- Qwen2.5-0.5B-Instruct GGUF
- Qwen2.5-1.5B-Instruct GGUF
- Llama-3.2-1B-Instruct GGUF
- Phi-3.5-mini-instruct GGUF
- SmolLM2-1.7B-Instruct GGUF

## İlk çalıştırma

```bash
./build/skifflm --doctor
./build/skifflm --model ~/.local/share/skifflm/models/model-q4_k_m.gguf --model-info
./build/skifflm --model ~/.local/share/skifflm/models/model-q4_k_m.gguf
```

## Testleri çalıştırma

```bash
ctest --test-dir build --output-on-failure
```

Ya da tüm yerel kontrolleri çalıştırın:

```bash
scripts/ci-local.sh
```

## Yapılandırma

Varsayılan yapılandırma dosyası `~/.config/skifflm/config` yolundadır. Tam bir
örnek `configs/skifflm.example.conf` dosyasındadır. Komut satırı seçenekleri
yapılandırma dosyasını geçersiz kılar.

## Sorun giderme

- Model bulunamazsa `--list-models` çalıştırın veya `--model` verin.
- Üretim bağlam sınırında duruyorsa `--ctx` değerini artırın veya konuşmayı
  kısaltın.
- Hazır llama.cpp varsa `SKIFFLLM_LLAMA_SOURCE_DIR` değişkenini ayarlayın.
- Çok soketli sistemlerde `--numa` deneyin.
- Daha hızlı CPU çıkarımı için `--profile fast` ve örnek bir model kullanın.
