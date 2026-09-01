# Limitations connues

Cette liste est volontairement honnête. Elle décrit l'état actuel du projet.

## Fichiers de modèle

SkiffLLM n'inclut pas les modèles. L'exécution de bureau exige qu'un fichier GGUF
existe déjà, ce qui maintient son propre usage réseau à zéro et augmente le coût
d'installation par rapport aux services qui incluent la récupération.

Deux options explicites existent :

- Bureau : `python3 scripts/model_fetch.py --model <id>`
- Android : `Settings` → `Models` → `Download`

Les deux sont des transferts HTTPS initiés par l'utilisateur depuis Hugging Face.
L'application Android utilise l'autorisation `INTERNET` uniquement à cette fin.

## Serveur API local

Le mode `--serve` est un serveur HTTP local compact. Ce n'est pas une passerelle
de production multithread.

- Les points d'accès rapides (`/health`, `/version`, `/v1/models`) répondent
  pendant qu'une génération de chat s'exécute ; la génération est sérialisée
  derrière un mutex car un contexte llama.cpp n'est pas thread-safe.
- Les points d'accès `/v1/*` acceptent un `--api-key` facultatif et exigent alors
  `Authorization: Bearer <key>`. Ils sont publics lorsqu'aucune clé n'est
  définie, ce qui n'est sûr que lors de l'écoute sur `127.0.0.1`.
- Il n'y a pas de limitation de débit.
- Il est conçu pour les outils locaux, les plugins d'éditeur et l'automatisation
  personnelle.

## Contexte

La génération est bornée par la taille de contexte configurée. Les conversations
très longues ne sont tronquées que lorsque `--auto-trim` est activé ; sinon, une
erreur est produite lorsque le prompt est trop grand.

## Gabarits de prompt

La substitution du gabarit de chat accepte un nom pris en charge par le modèle
llama.cpp chargé. Les noms de gabarit inconnus retombent sur `chatml` ou échouent
avec une erreur claire.

## Android

- Exécute une génération à la fois et un téléchargement à la fois.
- L'application préchauffe et charge les modèles sur l'exécuteur d'arrière-plan
  de type UI ; les gros modèles prennent encore du temps et de la mémoire.
- Les téléchargements de modèles nécessitent suffisamment d'espace libre et une
  connexion réseau.
- Le déchargement GPU dépend du support de compilation de llama.cpp et du backend
  de l'appareil.

## CI

Les fichiers de workflows CI et de publication sont présents dans l'arbre de
travail, mais n'ont pas été poussés vers cette branche tant que le dépôt n'accorde
pas l'autorisation `workflows` requise. En attendant, les vérifications locales
peuvent être exécutées avec `scripts/ci-local.sh` (bureau) et
`scripts/ci-android.sh` (Android, avec le SDK Android installé).

## Prévu

- Embeddings et génération augmentée par récupération
- Génération multi-workers sur plusieurs modèles
- Intégration aux gestionnaires de paquets
- Génération contrainte par grammaire
- Matrice de benchmarks de backends
