# SkiffLLM Kurulumu

SkiffLLM, llama.cpp ile çalışan tek parça ve küçük bir C++ programıdır. Çalışma
zamanı model paketlemez, ağa çağrı yapmaz ve bir arka plan servisi gerektirmez.

## 1. Hazır arşiv kurulumu

Bir sürüm yayınlandığında içinde `llm-<version>-<os>-<arch>.tar.gz`
(Windows'ta `.zip`) biçiminde platform arşivleri ve `checksums.txt` bulunur.

```bash
# Linux / macOS
tar -xzf llm-v1.6.0-linux-x86_64.tar.gz
sudo install -m 0755 bin/llm /usr/local/bin/llm
```

Hazır bir yardımcı script dahildir:

```bash
bash scripts/install-from-release.sh --version v1.6.0
bash scripts/install-from-release.sh --help
```

Bu script, geçerli işletim sistemini ve mimariyi sürüm ürün dosyasıyla eşleştirir
ve `$HOME/.local/bin` dizinine kurar. Arşiv henüz yayınlanmamışsa hızlıca hata verir.

## 2. Kaynak koddan kurulum

### Hızlı yerel kurulum

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
llm --version
```

Yararlı seçenekler:

```bash
bash scripts/install.sh --help                 # tüm seçenekler
bash scripts/install.sh --backend vulkan        # GPU arka uç derlemesi
bash scripts/install.sh --skip-tests            # yalnızca derleme
```

### CMake ile kurulum

```bash
cmake --preset release
cmake --build build/release -j
cmake --install build/release --prefix /usr/local
```

## 3. Elle derleme

Gereksinimler:

- CMake 3.20+
- C++17 derleyicisi (GCC 10+, Clang 12+ veya MSVC 2019+)
- Ninja (önerilir) veya Make
- Python 3 yalnızca isteğe bağlı model indirme yardımcısı için gerekir

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Elinizde derlenmiş ve kurulmuş bir llama.cpp kopyası varsa:

```bash
cmake -S . -B build \
  -DLLM_FETCH_LLAMA=OFF \
  -DLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp \
  -DCMAKE_BUILD_TYPE=Release
```

## 4. Model edinin

Çalışma zamanı asla model indirmez.

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Dosyalar varsayılan olarak `~/.local/share/llm/models` dizinine kaydedilir.
Mevcut herhangi bir `.gguf` dosyasına da `--model` ile işaret edebilirsiniz.

## 5. Kabuk tamamlamaları

Üretilen dosyaları kullanın veya kopyalayın:

```bash
# bash
source scripts/completions/llm.bash

# zsh
cp scripts/completions/llm.zsh ~/.zsh_functions/

# fish (tamamlamalar dizini)
cp scripts/completions/llm.fish ~/.config/fish/completions/
```

## 6. macOS / iOS

- macOS aynı CMake yolundan derlenir; Metal için `--backend metal` kullanın.
- iOS için macOS/Xcode + XcodeGen gerekir: `bash scripts/ios-setup.sh`.

## 7. Android

- Bu kurulum yolu Android Studio gerektirmez ama APK üretmek için Android SDK
  gerekir.
- `bash scripts/ci-android.sh`, Android SDK kuruluysa hata ayıklama APK'sı üretir.

## 8. Demo kaydı

Gerçek bir binary ve gerçek bir GGUF modeliyle `scripts/demo-capture.sh`
çalıştırarak dürüst bir terminal kaydı oluşturabilirsiniz. Script asla sahte çıktı
üretmez.

```bash
bash scripts/demo-capture.sh ./build/release/llm /path/model.gguf
```
