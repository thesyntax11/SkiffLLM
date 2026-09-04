## SkiffLLM

<p align="center">
  <img src="docs/logo.svg" alt="Logo SkiffLLM" width="128" height="128"/>
</p>

<p align="center">
  <strong>Exécutez n'importe quel modèle GGUF comme un outil Unix.</strong><br/>
  Local. Hors ligne. Pas de compte cloud, pas de télémétrie, pas de démon.
</p>

<p align="center">
  <strong>Langues :</strong>
  <a href="README.md">English</a> ·
  <a href="README.tr.md">Türkçe</a> ·
  <a href="README.de.md">Deutsch</a> ·
  <a href="README.es.md">Español</a> ·
  <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="Licence MIT"/>
  <img src="https://img.shields.io/badge/version-v1.9.0-blue" alt="Version v1.9.0"/>
  <img src="https://img.shields.io/badge/c%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android%20%7C%20iOS-lightgrey" alt="Plateformes"/>
  <img src="https://img.shields.io/badge/runtime-offline-green" alt="Hors ligne"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/ci.yml/badge.svg" alt="CI"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/release.yml/badge.svg" alt="Release"/>
</p>

SkiffLLM est un moteur d'IA local unique qui se comporte comme un outil Unix.
Il exécute n'importe quel modèle GGUF via llama.cpp sur votre CPU ou GPU,
conserve chaque jeton sur votre machine et s'intègre directement dans votre
flux de travail en shell.

```bash
git diff | skiffllm "review these changes"
cat error.log | skiffllm "find the root cause"
cat README.md | skiffllm "summarize this"
skiffllm --project . "where is this implemented?"
```

---

## Démarrage

### En une commande (depuis les sources)

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
skiffllm --version
```

### Ou compiler manuellement

```bash
cmake --preset release
cmake --build build/release -j
ctest --test-dir build/release --output-on-failure
```

### Ou installer une version publiée

```bash
bash scripts/install-from-release.sh --version v1.9.0
```

Le script associe votre système d'exploitation et votre architecture à
l'archive de la version. Il échoue rapidement si la version ne contient pas
encore d'artefacts publiés.

### Enregistrer une démo

```bash
bash scripts/demo-capture.sh ./build/release/skiffllm /path/to/model.gguf
```

Cela enregistre une vraie session de terminal pour le README ; le script
n'invente jamais de sortie.

### Obtenir un petit modèle

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Les fichiers sont enregistrés par défaut dans
`~/.local/share/skiffllm/models`. Vous pouvez aussi pointer `--model` vers
n'importe quel fichier `.gguf` existant.

Les fichiers de modèle sont téléchargés depuis Hugging Face sous leur propre
licence en amont ; SkiffLLM ne les redistribue pas. Respectez la licence de
chaque modèle avant de l'utiliser ou de le redistribuer. Licences du
catalogue fourni : Qwen2.5/Qwen3 Apache-2.0, SmolLM2 Apache-2.0, Phi-3.5 MIT
et Llama 3.2 **Llama 3.2 Community License** (avec obligations d'attribution
et de nommage).

Configuration complète : [docs/fr/INSTALL.md](docs/fr/INSTALL.md).

---

## Pourquoi SkiffLLM

| | SkiffLLM | Assistants cloud | Ollama |
| --- | --- | --- | --- |
| Cloud requis | ❌ | ✅ | ❌ |
| Clé API cloud | ❌ | ✅ | ❌ |
| Compte / inscription | ❌ | ✅ | ❌ |
| Télémétrie | ❌ | variable | ❌ |
| Démon | ❌ | ✅ | ✅ |
| Fichier GGUF direct | ✅ | ❌ | partiel |
| Machines CPU seulement | ✅ | ❌ | ✅ |
| Déchargement GPU | ✅ | n/a | ✅ |
| Tuyaux Unix | ✅ | ❌ | ❌ |
| Contexte projet/code | ✅ | ❌ | ❌ |
| API locale compatible OpenAI | ✅ | n/a | ✅ |
| Client Android natif | ✅ | ❌ | communautaire |
| Client iOS natif | ✅ | ❌ | communautaire |

Le moteur ne télécharge jamais de modèles, n'envoie jamais de données à
l'extérieur et ne nécessite jamais de compte. Vous apportez le fichier GGUF ;
SkiffLLM fait l'inférence.

### Et pourquoi pas simplement Ollama ?

Réponse courte : utilisez Ollama lorsque vous voulez un serveur de modèles
rapide, orienté catalogue, avec des téléchargements en une seule commande ;
utilisez SkiffLLM lorsque le modèle doit se comporter comme un outil Unix natif,
un composant isolé du réseau (air-gapped) ou un moteur embarqué dans un
workflow CI, bureau ou mobile — sans démon, un seul binaire, et une
inférence centrale qui ne se connecte jamais.
La matrice de décision honnête, les vrais compromis et un guide de
migration tâche par tâche sont dans [docs/fr/comparison.md](docs/fr/comparison.md)
et, complet en anglais, dans [docs/COMPARISON.md](docs/COMPARISON.md). Pour le
déploiement d'entreprise, le durcissement serveur et la chaîne
d'approvisionnement, voyez [docs/ENTERPRISE.md](docs/ENTERPRISE.md) (en
anglais).

---

## Points forts

| Fonctionnalité | Description |
| --- | --- |
| Tuyaux Unix | `cat file \| skiffllm "summarize"`, `git diff \| skiffllm "review"` |
| Contexte projet | `--project <dir>` ajoute un vrai index + un extrait de code limité |
| Gestion des modèles | `skiffllm model list / info / install / remove / verify` |
| Intégration Git | `skiffllm git review / explain / commit / log / status` |
| Shell interactif | streaming de jetons, historique, compteurs en direct |
| Contexte fichier | `--attach`, `/file` et expansion `@file` dans tout prompt |
| Export de conversation | `--export` et `/export` sauvegardent les sessions en Markdown |
| Sessions et mémoire | sessions nommées, `/remember`, `/forget`, faits persistants |
| Serveur API local | `--serve` expose un endpoint compatible OpenAI |
| Protection serveur | authentification Bearer `--api-key` optionnelle sur `/v1/*` |
| Benchmark réel | `--benchmark <runs>` mesure la vitesse réelle du prompt et de la génération |
| Profils d'échantillonnage | `balanced`, `fast`, `creative`, `code`, `precise` |
| Échantillonnage avancé | temperature, top-p, top-k, min-p, typical-p, pénalités |
| Gestion du contexte | découpage automatique, espace réservé, `/compact` |
| Mode JSON | sortie lisible par machine pour scripts et outils |
| Diagnostic | rapport `--doctor`, `--model-info`, `--tokenize` |
| Apps mobiles natives | Android (Jetpack Compose) et iOS (SwiftUI) |

---

## Exemples pratiques

```bash
# Examiner un ensemble de modifications avant de pousser
git diff | skiffllm "review these changes"

# Trouver la vraie cause dans un journal désordonné
journalctl -e | skiffllm "find suspicious errors and a likely root cause"

# Résumer un fichier que vous venez de lire
cat README.md | skiffllm "summarize this"

# Pointer vers un dépôt entier
skiffllm --project . "where is authentication implemented?"

# Sortie lisible par machine pour vos propres scripts
git diff | skiffllm --json "classify this diff"

# Revue de code sûre (propose un diff, ne modifie jamais les fichiers)
skiffllm --code --project . "fix the bug in src/server.cpp"
```

## Gestion des modèles

SkiffLLM reste hors ligne pendant l'exécution. L'obtention d'un modèle est une
commande explicite et séparée.

```bash
skiffllm model list
skiffllm model info qwen2.5-0.5b
skiffllm model install qwen2.5-0.5b
skiffllm model verify qwen2.5-0.5b --update
skiffllm model remove qwen2.5-0.5b --force
```

`model install` délègue à `scripts/model_fetch.py`, qui télécharge exactement
un GGUF via HTTPS depuis Hugging Face, vérifie l'en-tête GGUF et enregistre un
fichier latéral SHA-256 dans votre dossier de modèles. La taille du catalogue
est indicative car un mainteneur peut recharger une révision avec une taille
différente ; le fichier latéral est la vérification d'intégrité faisant
référence. L'inférence elle-même
n'ouvre jamais de connexion.

## Intégration Git

Revue et explication de code locales et hors ligne pour le diff qui se trouve
devant vous.

```bash
# Les sous-commandes git lisent le diff elles-mêmes ; aucun tuyau n'est requis.
skiffllm git review
skiffllm git review --cached
skiffllm git explain
skiffllm git commit --cached
skiffllm git log
skiffllm git status
```

`git commit --cached` propose un message de commit conventionnel à partir de
votre diff indexé ; il n'exécute pas `git commit` à votre place.

## Sessions et mémoire persistante

```bash
skiffllm --session coding --model qwen2.5-0.5b-instruct-q4_k_m.gguf
skiffllm --session writing --model qwen2.5-0.5b-instruct-q4_k_m.gguf

skiffllm session list
skiffllm session show coding
skiffllm session rename coding writing
skiffllm session remove old-draft
```

La mémoire persistante se trouve dans `~/.local/share/skiffllm/memories.txt` et
ne quitte jamais la machine.

```bash
skiffllm --remember "the user prefers concise answers"
skiffllm --forget concise
```

Dans le shell interactif, utilisez `/remember`, `/forget`, `/memories`,
`/clear-memories`, `/compact`, `/regenerate` et `/export`.

---

## Serveur local compatible OpenAI

```bash
# local uniquement
skiffllm --model model.gguf --serve --host 127.0.0.1 --port 8080

# écouteur non-local protégé par un jeton partagé
skiffllm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

Endpoints :

```text
GET  /health                 contrôle de santé public
GET  /version                contrôle de version public
GET  /v1/models              protégé par Bearer si --api-key est défini
POST /v1/chat/completions    protégé par Bearer si --api-key est défini
```

Le serveur prend en charge le streaming style OpenAI (`"stream": true`) et
répond aux endpoints rapides pendant qu'une génération s'exécute. La génération
de chat est sérialisée car un contexte llama.cpp n'est pas thread-safe.

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

Un client Python sans dépendances est inclus :

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$SKIFFLLM_SERVER_KEY" "Say hello."
```

---

## Applications mobiles

SkiffLLM inclut des clients natifs pour [Android](android/README.md) et
[iOS](ios/README.md). Les deux exécutent les modèles GGUF pris en charge
entièrement sur l'appareil, diffusent les jetons, affichent une barre d'utilisation
du contexte en direct et exposent la même surface de fonctionnalités que le CLI
de bureau :

- réglages d'échantillonnage et mode code par conversation
- création/ouverture/renommage/suppression/sauvegarde/import de conversations
- faits persistants, prompts rapides et entrée via la feuille de partage
- séquences d'arrêt, profils d'échantillonnage et compactage de conversation
- échauffement du modèle, benchmark réel en 3 tours, statistiques de session
- attachement de fichiers texte/JSON/XML (lus localement, plafonnés)
- export Markdown et copie en un geste
- import GGUF avec vérification de l'en-tête

L'inférence n'ouvre jamais de connexion. Il existe exactement deux chemins
réseau, tous deux explicites et lancés par l'utilisateur : `model install`
télécharge depuis Hugging Face, et le sous-commande `openai` parle à un
serveur HTTP vers lequel vous le pointez. Le fichier doit avoir un en-tête GGUF
valide et reçoit un fichier latéral SHA-256 ; la taille du catalogue est
indicative. Les sauvegardes Android sont désactivées pour que les prompts et
l'historique ne quittent jamais l'appareil.

---

## Ligne de commande

```text
Usage: skiffllm [options] [model.gguf]

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

Utilisation complète : [docs/fr/usage.md](docs/fr/usage.md). Commandes
interactives : `/help`, `/warmup`, `/history`, `/stats`, `/compact`,
`/regenerate`, `/tokenize`, `/file`, `/clear-attach`, `/clear`, `/reset`,
`/system`, `/model`, `/profile`, `/stop`, `/temp`, `/top-p`, `/top-k`,
`/min-p`, `/typical`, `/n`, `/ctx`, `/export`, `/save`, `/exit`.

---

## Honnêteté des benchmarks

```bash
skiffllm --model model.gguf --benchmark 3
skiffllm --model model.gguf --benchmark 3 --json
```

Chaque nombre est mesuré sur votre machine avec votre modèle et votre matériel.
SkiffLLM n'invente jamais de résultats de benchmark. Méthodologie et tableau
vide attendant de vrais apports : [docs/benchmarks.md](docs/benchmarks.md).

## Preuve de confidentialité

```bash
skiffllm --doctor --network
```

affiche les faits du runtime : la génération centrale ne fait aucun appel
sortant, il n'y a ni télémétrie ni API cloud, et l'historique est stocké
localement. Les seuls chemins réseau sont explicites et initiés par
l'utilisateur : `model install` (téléchargement Hugging Face) et le sous-commande
`openai` (le serveur vers lequel vous le pointez). `--serve` ouvre seulement
un listener local, il ne contacte jamais l'extérieur.

---

## Installation

```bash
bash scripts/install.sh --help
bash scripts/install.sh --prefix "$HOME/.local"
bash scripts/install.sh --prefix /usr/local --backend metal
```

`Makefile` de commodité :

```bash
make release
make tests
make check
make install
make help
```

Les archives précompilées suivent le motif `skiffllm-<version>-<os>-<arch>.tar.gz`
(par exemple `skiffllm-v1.9.0-linux-x86_64.tar.gz`, `.zip` sous Windows) avec
`checksums.txt` lorsqu'elles sont publiées. Voir
[docs/fr/INSTALL.md](docs/fr/INSTALL.md). Les complétions de shell se trouvent
dans [scripts/completions](scripts/completions/).

## Options de compilation

| Option | Défaut | Description |
| --- | --- | --- |
| `SKIFFLLM_BUILD_TESTS` | `ON` | Compiler et enregistrer la suite de tests |
| `SKIFFLLM_FETCH_LLAMA` | `ON` | Télécharger et compiler llama.cpp épinglé |
| `SKIFFLLM_LLAMA_SOURCE_DIR` | vide | Utiliser un checkout existant de llama.cpp |
| `SKIFFLLM_BUILD_SHARED_LLAMA` | `OFF` | Compiler llama.cpp en bibliothèque partagée |
| `SKIFFLLM_USE_READLINE` | `ON` | Activer GNU Readline si disponible |
| `SKIFFLLM_LLAMA_BACKEND` | `auto` | `cuda`, `metal`, `vulkan`, `opencl`, `blas`, `cpu` |

L'accélération matérielle est toujours explicite : choisissez le backend à la
configuration et déchargez des couches à l'exécution avec `--gpu-layers`.

---

## Prérequis

- Un compilateur C++17 (GCC 10+, Clang 12+, ou MSVC 2019+)
- CMake 3.20+
- `FetchContent` CMake optionnel pour un llama.cpp épinglé
- Un fichier de modèle GGUF
- Optionnel : CUDA/Metal/Vulkan/OpenCL/BLAS pour l'accélération matérielle

---

## Structure du projet

```text
include/skiffllm/              En-têtes d'API publique
src/                          CLI, noyau et serveur local
tests/                        Tests unitaires (sans modèle)
configs/                      Configuration d'exemple
scripts/                      CI, release, téléchargement de modèles, client API, complétions
android/                      App Android Kotlin/Compose et JNI llama.cpp
ios/                          App iOS SwiftUI et pont Objective-C++ llama.cpp
docs/                         Setup, usage, architecture, FAQ, docs de release
.github/                      Modèles d'issues, modèle de PR, workflow CI
```

---

## Feuille de route

- Mode embeddings et RAG
- Intégration gestionnaires de paquets (Homebrew, apt, vcpkg)
- Matrice de benchmarks GPU entre backends
- Génération contrainte par grammaire
- Diagnostic d'échantillonnage au niveau des jetons
- Génération multi-travailleurs entre plusieurs modèles

## Limitations connues

SkiffLLM ne fournit pas de modèles. Le serveur local est public sur
`127.0.0.1` par défaut ; utilisez `--api-key` lorsque vous vous liez à une
interface non-loopback. Voir [docs/fr/LIMITATIONS.md](docs/fr/LIMITATIONS.md).

## Contribuer

Les contributions sont les bienvenues. Gardez les changements petits, respectez
la promesse hors ligne, n'inventez jamais de chiffres de benchmark et ajoutez
des tests pour les nouveaux comportements. Voir [CONTRIBUTING.md](CONTRIBUTING.md)
et [docs/fr/GOOD_FIRST_ISSUES.md](docs/fr/GOOD_FIRST_ISSUES.md). Guide de
traduction : [docs/i18n.md](docs/i18n.md).

## Licence

MIT — voir [LICENSE](LICENSE).
