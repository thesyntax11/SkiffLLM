# Installation de SkiffLLM

SkiffLLM est un binaire C++ unique et léger qui communique avec llama.cpp.
L'exécution n'embarque pas de modèle, n'appelle pas le réseau et ne nécessite
aucun démon.

## 1. Installer une archive précompilée

Lorsqu'une version est publiée, elle inclut des archives de plateforme nommées
`llm-<version>-<os>-<arch>.tar.gz` (ou `.zip` sous Windows) ainsi que
`checksums.txt`.

```bash
# Linux / macOS
tar -xzf llm-v1.6.0-linux-x86_64.tar.gz
sudo install -m 0755 bin/llm /usr/local/bin/llm
```

Un script d'aide prêt à l'emploi est inclus :

```bash
bash scripts/install-from-release.sh --version v1.6.0
bash scripts/install-from-release.sh --help
```

Le script associe le système/architecture courant à la ressource de version et
installe dans `$HOME/.local/bin`. Il échoue rapidement si l'archive n'est pas
encore publiée.

## 2. Installer depuis une copie des sources

### Installation locale rapide

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
llm --version
```

Options utiles :

```bash
bash scripts/install.sh --help                 # toutes les options
bash scripts/install.sh --backend vulkan        # compilation backend GPU
bash scripts/install.sh --skip-tests            # compiler uniquement
```

### Installation avec CMake

```bash
cmake --preset release
cmake --build build/release -j
cmake --install build/release --prefix /usr/local
```

## 3. Compilation manuelle

Prérequis :

- CMake 3.20+
- Un compilateur C++17 (GCC 10+, Clang 12+ ou MSVC 2019+)
- Ninja (recommandé) ou Make
- Python 3 n'est nécessaire que pour l'utilitaire facultatif de récupération de
  modèles

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Si vous possédez déjà une copie de llama.cpp compilée et installée :

```bash
cmake -S . -B build \
  -DLLM_FETCH_LLAMA=OFF \
  -DLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp \
  -DCMAKE_BUILD_TYPE=Release
```

## 4. Obtenir un modèle

L'exécution ne télécharge jamais les modèles.

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Les fichiers sont enregistrés par défaut dans `~/.local/share/llm/models`.
Vous pouvez aussi pointer `--model` vers n'importe quel `.gguf` existant.

## 5. Complétion du shell

Chargez ou copiez les fichiers générés :

```bash
# bash
source scripts/completions/llm.bash

# zsh
cp scripts/completions/llm.zsh ~/.zsh_functions/

# fish (répertoire des complétions)
cp scripts/completions/llm.fish ~/.config/fish/completions/
```

## 6. macOS / iOS

- macOS se compile via le même chemin CMake ; utilisez `--backend metal` pour
  Metal.
- iOS nécessite macOS/Xcode + XcodeGen : `bash scripts/ios-setup.sh`.

## 7. Android

- Android Studio n'est pas nécessaire sur ce chemin d'installation.
- `bash scripts/ci-android.sh` produit un APK de débogage lorsque le SDK Android
  est installé.

## 8. Enregistrer une démo

Exécutez `scripts/demo-capture.sh` avec un vrai binaire et un vrai modèle GGUF
pour produire un enregistrement de terminal honnête. Le script ne crée jamais de
sortie factice.

```bash
bash scripts/demo-capture.sh ./build/release/llm /chemin/model.gguf
```
