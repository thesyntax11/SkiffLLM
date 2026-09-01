# Guide d'utilisation

## Mode pipeline Unix

SkiffLLM détecte automatiquement une entrée standard redirigée, ce qui permet au
modèle de faire partie d'un flux shell :

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
cat README.md | skifflm "summarize this"
skifflm --project . "where is authentication handled?"
```

Sans argument d'instruction, le texte redirigé est lui-même le prompt. Avec un
argument d'instruction, le texte redirigé devient `<context>`.

## Contexte de projet

```bash
skifflm --project . "where is authentication handled?"
```

`--project <dir>` construit un index de fichiers limité plus une tranche du
contenu source/config avant la génération. Il ignore `.git`, les répertoires de
compilation, les caches et les dépendances vendues.

## Sessions et mémoire

```bash
skifflm session list
skifflm session show coding
skifflm session rename coding writing
skifflm session remove old-draft

skifflm --remember "the user prefers concise answers"
skifflm --forget concise
```

Commandes de shell interactives : `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (compresser une longue conversation
en un résumé à puces tout en conservant les faits) et `/regenerate` ou `/retry`
(rejoue le dernier message utilisateur avec les paramètres d'échantillonnage
actuels).

## Raccourci de résumé

```bash
skifflm --summarize README.md
skifflm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Gestionnaire de modèles

```bash
skifflm model list
skifflm model info qwen2.5-0.5b
skifflm model install qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b --update
skifflm model remove qwen2.5-0.5b --force
```

`model verify` vérifie l'en-tête magique GGUF, la taille de fichier attendue et un
fichier annexe SHA-256 lorsqu'il existe. `--update` enregistre un fichier annexe
SHA-256 local. Les téléchargements via `model_fetch.py` écrivent également ce
fichier annexe automatiquement et prennent en charge `--verify` sans
retéléchargement.

L'inférence reste hors ligne. `model install` exécute l'utilitaire explicite
`model_fetch.py` via HTTPS.

## Exécution ponctuelle et gabarits de chat

```bash
skifflm run "Hello" --ctx 2048 --temp 0.3 --threads 4
skifflm chat-template list
skifflm chat-template detect --model model.gguf
```

## Client OpenAI

```bash
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

## Accélération matérielle

```bash
skifflm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_LLAMA_BACKEND=cuda
./build/skifflm --model model.gguf --gpu-layers -1 --flash-attn
```

Les backends sont choisis à la compilation ; `--backend-info` rapporte ce qui est
réellement lié.

## Intégration Git

```bash
skifflm git review --cached
skifflm git explain
skifflm git commit --cached
skifflm git log
skifflm git status
```

## Mode code sûr

```bash
skifflm --code --project . "fix the bug in src/server.cpp"
```

`--code` produit une proposition de diff unifié et ne modifie jamais lui-même un
fichier.

## Mode interactif

```bash
skifflm --model ~/models/model-q4_k_m.gguf
```

L'interface est un prompt nommé `you>`. Écrivez un message et appuyez sur Entrée.
Le texte est diffusé au fur et à mesure qu'il est généré.

## Mode ponctuel

```bash
skifflm --model model.gguf --prompt "What is recursion?"
```

## Sessions nommées

```bash
skifflm --model model.gguf --session writing
skifflm --model model.gguf --session coding
```

Chaque session nommée possède son propre fichier d'historique.

## Prompt système

```bash
skifflm --model model.gguf --system "You are a patient Python tutor."
```

## Profils

```bash
skifflm --model model.gguf --profile code
```

Profils disponibles : `balanced`, `fast`, `creative`, `code`, `precise`.

## Contexte de fichier

```bash
skifflm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
skifflm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

Répétez `--attach`, utilisez `@path` dans le prompt ou gérez les pièces jointes
dans le shell avec `/file` et `/clear-attach`.

## Export de conversation

```bash
skifflm --export conversation.md
```

Exporter la session chargée ne nécessite aucun modèle et écrit du Markdown. Dans
le shell, utilisez `/export <path>`.

## Gabarit de chat et préchauffage

```bash
skifflm --model model.gguf --chat-template chatml
skifflm --model model.gguf --warmup
```

`--chat-template` remplace le nom du format de prompt intégré du modèle.
`--warmup` exécute une génération d'un seul token au démarrage pour réduire la
latence de la première réponse. Dans le shell, vous pouvez aussi réchauffer le
modèle avec `/warmup`.

## Édition de ligne interactive

Lorsque GNU Readline est disponible, SkiffLLM active la navigation par flèches, la
recherche dans l'historique et des lignes de saisie éditables. Sans Readline, il
retombe sur un simple `getline`. La sortie en streaming affiche également un
compteur de tokens en direct sur les terminaux interactifs.

## Séquences d'arrêt

```bash
skifflm --model model.gguf --stop "END" --stop "STOP"
```

La génération s'arrête à la première séquence configurée.

## Mode JSON

```bash
skifflm --model model.gguf --prompt "Say hello" --json
```

Cela désactive le shell interactif et écrit un seul objet JSON sur stdout.

## Redirection

```bash
cat prompt.txt | skifflm --model model.gguf
printf 'Explain this command.' | skifflm --model model.gguf --prompt-file /dev/stdin
```

## Fichiers de sortie

```bash
skifflm --model model.gguf --prompt-file input.txt --output output.md
```

## Fichier de configuration

```bash
skifflm --config ~/.config/skifflm/config
```

L'emplacement par défaut est utilisé automatiquement lorsqu'il existe.

## Diagnostic

```bash
skifflm --doctor
skifflm --model model.gguf --model-info
skifflm --model model.gguf --smoke
skifflm --model model.gguf --tokenize "hello world"
```

## Serveur API local

```bash
# local uniquement
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# écouteur non loopback protégé
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

Points d'accès :

```text
GET  /health                 contrôle de santé public
GET  /v1/models              protégé par bearer lorsque --api-key est défini
POST /v1/chat/completions    protégé par bearer lorsque --api-key est défini
```

Le serveur est hors ligne, se lie à localhost par défaut et prend en charge le
streaming de style OpenAI avec `"stream": true`. Les points d'accès rapides
répondent pendant qu'une génération s'exécute ; la génération de chat est
sérialisée derrière un mutex. Lorsque `--api-key` est configuré, `/v1/*`
exige `Authorization: Bearer <key>` et renvoie `401` sinon.

Un client rapide est inclus :

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$SKIFFLLM_SERVER_KEY" "Say hello."
```

## Benchmark

```bash
skifflm --model model.gguf --benchmark 3
skifflm --model model.gguf --benchmark 3 --json
```

Exécute une vraie génération et rapporte le temps de prompt mesuré, le temps de
génération et les tokens par seconde. `--n-predict` contrôle la longueur
d'exécution jusqu'à 128 tokens par exécution.

## Déchargement GPU

Sur une machine CUDA :

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
skifflm --model model.gguf --gpu-layers -1
```

Sous macOS, le backend Metal est disponible par défaut.

## Liste des modèles

```bash
skifflm --model-dir ~/models --list-models
```
