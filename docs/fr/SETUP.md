# Configuration

SkiffLLM n'a aucune dépendance réseau à l'exécution. Vous le compilez une fois puis
l'exécutez contre un modèle GGUF local.

## Prérequis

- Un compilateur C++17 (GCC 10+, Clang 12+ ou MSVC 2019+)
- CMake 3.20 ou plus récent
- Facultatif : GNU Readline pour l'édition de ligne et l'historique du shell
- Facultatif : un backend CUDA, Metal, ROCm, SYCL ou Vulkan pour le déchargement
  GPU
- Un fichier de modèle GGUF

## Compilation

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Ou utilisez l'installateur pratique avec un backend GPU/NPU explicite :

```bash
bash scripts/install.sh                            # CPU / valeur par défaut de la plateforme
BACKEND=cuda bash scripts/install.sh               # CUDA
BACKEND=vulkan bash scripts/install.sh             # Vulkan
BACKEND=metal bash scripts/install.sh              # macOS Metal
./build/skifflm --backend-info                     # inspecter les backends liés
```

Utilisez une copie existante de llama.cpp pour éviter le téléchargement facultatif
lors de la configuration :

```bash
scripts/setup.sh --source-dir /path/to/llama.cpp --backend cuda
```

## Installation

```bash
scripts/setup.sh --prefix /usr/local
cmake --install build --prefix /usr/local
```

## Obtenir un modèle

L'exécution de SkiffLLM ne télécharge jamais de modèle. Vous pouvez copier un
fichier GGUF déjà en votre possession dans le répertoire de modèles ou passer son
chemin directement. Si vous voulez un modèle quantifié recommandé, exécutez
l'utilitaire de récupération facultatif une fois :

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

L'utilitaire enregistre le fichier dans `~/.local/share/skifflm/models`.
L'inférence reste entièrement hors ligne.

Ou copiez-le vous-même :

```bash
mkdir -p ~/.local/share/skifflm/models
cp /path/to/model-q4_k_m.gguf ~/.local/share/skifflm/models/
```

Petits modèles recommandés pour un démarrage rapide uniquement CPU :

- Qwen2.5-0.5B-Instruct GGUF
- Qwen2.5-1.5B-Instruct GGUF
- Llama-3.2-1B-Instruct GGUF
- Phi-3.5-mini-instruct GGUF
- SmolLM2-1.7B-Instruct GGUF

## Première exécution

```bash
./build/skifflm --doctor
./build/skifflm --model ~/.local/share/skifflm/models/model-q4_k_m.gguf --model-info
./build/skifflm --model ~/.local/share/skifflm/models/model-q4_k_m.gguf
```

## Exécuter les tests

```bash
ctest --test-dir build --output-on-failure
```

Ou exécutez toutes les vérifications locales :

```bash
scripts/ci-local.sh
```

## Configuration

Le fichier de configuration par défaut est `~/.config/skifflm/config`. Un exemple
complet se trouve dans `configs/skifflm.example.conf`. Les options CLI priment
sur les valeurs du fichier de configuration.

## Dépannage

- Si aucun modèle n'est trouvé, exécutez `--list-models` ou passez `--model`.
- Si la génération s'arrête à la limite de contexte, augmentez `--ctx` ou
  raccourcissez la conversation.
- Si un llama.cpp précompilé est disponible, définissez
  `SKIFFLLM_LLAMA_SOURCE_DIR`.
- Sur les systèmes multi-processeurs, essayez `--numa`.
- Pour une inférence CPU plus rapide, conservez `--profile fast` et un modèle
  quantifié.
