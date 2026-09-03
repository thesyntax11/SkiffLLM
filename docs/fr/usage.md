# Guide d'utilisation

## Mode pipeline Unix

SkiffLLM détecte automatiquement une entrée standard redirigée, ce qui permet au
modèle de faire partie d'un flux shell :

```bash
git diff | llm "review these changes"
cat error.log | llm "find the root cause"
cat README.md | llm "summarize this"
llm --project . "where is authentication handled?"
```

Sans argument d'instruction, le texte redirigé est lui-même le prompt. Avec un
argument d'instruction, le texte redirigé devient `<context>`.

## Contexte de projet

```bash
llm --project . "where is authentication handled?"
```

`--project <dir>` construit un index de fichiers limité plus une tranche du
contenu source/config avant la génération. Il ignore `.git`, les répertoires de
compilation, les caches et les dépendances vendues.

## Sessions et mémoire

```bash
llm session list
llm session show coding
llm session rename coding writing
llm session remove old-draft

llm --remember "the user prefers concise answers"
llm --forget concise
```

Commandes de shell interactives : `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (compresser une longue conversation
en un résumé à puces tout en conservant les faits) et `/regenerate` ou `/retry`
(rejoue le dernier message utilisateur avec les paramètres d'échantillonnage
actuels).

## Raccourci de résumé

```bash
llm --summarize README.md
llm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Gestionnaire de modèles

```bash
llm model list
llm model info qwen2.5-0.5b
llm model install qwen2.5-0.5b
llm model verify qwen2.5-0.5b
llm model verify qwen2.5-0.5b --update
llm model remove qwen2.5-0.5b --force
```

`model verify` vérifie l'en-tête magique GGUF et le fichier annexe SHA-256
lorsqu'il existe. La taille du catalogue est indicative ; une révision amont
plus récente n'est pas rejetée uniquement parce que son nombre d'octets a changé. `--update` enregistre un fichier annexe
SHA-256 local. Les téléchargements via `model_fetch.py` écrivent également ce
fichier annexe automatiquement et prennent en charge `--verify` sans
retéléchargement.

L'inférence reste hors ligne. `model install` exécute l'utilitaire explicite
`model_fetch.py` via HTTPS.

## Exécution ponctuelle et gabarits de chat

```bash
llm run "Hello" --ctx 2048 --temp 0.3 --threads 4
llm chat-template list
llm chat-template detect --model model.gguf
```

## Client OpenAI

```bash
llm openai "Merhaba" --base-url http://127.0.0.1:8080
llm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
llm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

## Accélération matérielle

```bash
llm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLLM_LLAMA_BACKEND=cuda
./build/llm --model model.gguf --gpu-layers -1 --flash-attn
```

Les backends sont choisis à la compilation ; `--backend-info` rapporte ce qui est
réellement lié.

## Intégration Git

```bash
llm git review --cached
llm git explain
llm git commit --cached
llm git log
llm git status
```

## Mode code sûr

```bash
llm --code --project . "fix the bug in src/server.cpp"
```

`--code` produit une proposition de diff unifié et ne modifie jamais lui-même un
fichier.

## Mode interactif

```bash
llm --model ~/models/model-q4_k_m.gguf
```

L'interface est un prompt nommé `you>`. Écrivez un message et appuyez sur Entrée.
Le texte est diffusé au fur et à mesure qu'il est généré.

## Mode ponctuel

```bash
llm --model model.gguf --prompt "What is recursion?"
```

## Sessions nommées

```bash
llm --model model.gguf --session writing
llm --model model.gguf --session coding
```

Chaque session nommée possède son propre fichier d'historique.

## Prompt système

```bash
llm --model model.gguf --system "You are a patient Python tutor."
```

## Profils

```bash
llm --model model.gguf --profile code
```

Profils disponibles : `balanced`, `fast`, `creative`, `code`, `precise`.

## Contexte de fichier

```bash
llm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
llm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

Répétez `--attach`, utilisez `@path` dans le prompt ou gérez les pièces jointes
dans le shell avec `/file` et `/clear-attach`.

## Export de conversation

```bash
llm --export conversation.md
```

Exporter la session chargée ne nécessite aucun modèle et écrit du Markdown. Dans
le shell, utilisez `/export <path>`.

## Gabarit de chat et préchauffage

```bash
llm --model model.gguf --chat-template chatml
llm --model model.gguf --warmup
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
llm --model model.gguf --stop "END" --stop "STOP"
```

La génération s'arrête à la première séquence configurée.

## Mode JSON

```bash
llm --model model.gguf --prompt "Say hello" --json
```

Cela désactive le shell interactif et écrit un seul objet JSON sur stdout.

## Redirection

```bash
cat prompt.txt | llm --model model.gguf
printf 'Explain this command.' | llm --model model.gguf --prompt-file /dev/stdin
```

## Fichiers de sortie

```bash
llm --model model.gguf --prompt-file input.txt --output output.md
```

## Fichier de configuration

```bash
llm --config ~/.config/llm/config
```

L'emplacement par défaut est utilisé automatiquement lorsqu'il existe.

## Diagnostic

```bash
llm --doctor
llm --model model.gguf --model-info
llm --model model.gguf --smoke
llm --model model.gguf --tokenize "hello world"
```

## Serveur API local

```bash
# local uniquement
llm --model model.gguf --serve --host 127.0.0.1 --port 8080

# écouteur non loopback protégé
llm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$LLM_SERVER_KEY"
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
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$LLM_SERVER_KEY" "Say hello."
```

## Benchmark

```bash
llm --model model.gguf --benchmark 3
llm --model model.gguf --benchmark 3 --json
```

Exécute une vraie génération et rapporte le temps de prompt mesuré, le temps de
génération et les tokens par seconde. `--n-predict` contrôle la longueur
d'exécution jusqu'à 128 tokens par exécution.

## Déchargement GPU

Sur une machine CUDA :

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
llm --model model.gguf --gpu-layers -1
```

Sous macOS, le backend Metal est disponible par défaut.

## Liste des modèles

```bash
llm --model-dir ~/models --list-models
```
