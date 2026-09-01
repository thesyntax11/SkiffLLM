# Pourquoi SkiffLLM et pas simplement Ollama ?

C'est la question que pose chaque nouvel utilisateur et elle mérite une réponse
directe et honnête. SkiffLLM et Ollama résolvent des problèmes qui se
chevauchent ; le bon choix dépend de votre modèle de déploiement.

En bref : **Utilisez Ollama lorsque vous voulez un serveur de modèles agile avec
un large catalogue et des téléchargements en une commande. Utilisez SkiffLLM
lorsque vous voulez que le modèle se comporte comme un outil Unix natif, une
composante isolée du réseau (air-gapped) ou un moteur d'inférence embarqué dans
un flux CI, bureau ou mobile — sans démon et sans réseau à l'exécution.**

## Matrice de décision

| Dimension | SkiffLLM | Ollama |
| --- | --- | --- |
| Réseau à l'exécution | aucun à l'inférence | service de téléchargement/pull par défaut |
| Démon / service en arrière-plan | aucun (un seul processus) | oui (`ollama serve`) |
| Fichier de modèle | votre propre GGUF | catalogue géré, pull automatique |
| Épinglage / intégrité | sidecar SHA-256 + `model verify` | références de registre, moins explicite |
| Unix-first | pipelines natives, `--project`, sous-commandes `git` | serveur + client, pas pipe-first |
| Contexte projet/code | index de fichiers intégré + tranche de code | via outils externes, non intégré |
| Clients mobiles natifs | Android + iOS dans ce dépôt | orienté serveur, apps communautaires |
| Air-gapped / offline-first | objectif explicite | configurable, pas la posture par défaut |
| API compatible OpenAI | `--serve` (streaming, auth Bearer) | oui, modèle principal |
| Contrôle du moteur | votre build contrôle les backends llama.cpp | runtime embarqué |
| Empreinte | un petit binaire | démon + runtime + dépôt de modèles |
| Largeur du catalogue | vous choisissez n'importe quel GGUF | vaste, pratique |
| Écosystème / GUI (Open WebUI, etc.) | fait maison | mature |
| Limites matérielles du serveur | une génération, pas de rate-limit interne | passe à l'échelle plus facilement, empreinte plus grande |

Aucune ligne n'est un défaut en soi. Le tableau vise à supprimer le doute.

## Où SkiffLLM est clairement le meilleur choix

### 1. Vous voulez un outil Unix, pas un service

Ollama est server-first : vous exploitez un service et parlez à une API HTTP.
SkiffLLM est CLI-first :

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
skifflm --project . "where is authentication handled?"
skifflm --code --project . "propose a fix for src/server.cpp"
```

Pas de processus en arrière-plan, pas de port à gérer, pas de conteneur avec
contrôle de résidence. Il se compose avec `jq`, `xargs`, `git` et cron comme
n'importe quel autre outil.

### 2. Vous êtes air-gapped ou offline-first

L'exécution ne télécharge pas de modèles et ne contient aucun code réseau. Vous
apportez un fichier GGUF ; SkiffLLM fait l'inférence. C'est une posture
explicite, pas un paramètre à retenir. Pour les réseaux cloisonnés ou restreints,
c'est la différence entre « fonctionne par défaut » et « fonctionne seulement
après avoir désactivé le réseau ».

### 3. Vous avez besoin de contrôle sur le moteur d'inférence

SkiffLLM est compilé contre le llama.cpp de votre choix. Le backend est choisi à
la configuration via `SKIFFLLM_LLAMA_SOURCE_DIR` et le flag de build ;
`--backend-info` indique ce qui est réellement lié. Vous possédez le compilateur,
le backend et le binaire. Ollama empaquette et gère son runtime ; pratique mais
moins transparent.

### 4. Vous voulez des preuves de chaîne d'approvisionnement

`skifflm model verify` vérifie l'en-tête magique GGUF et le sidecar SHA-256.
La taille du catalogue est indicative, afin qu'une révision amont plus récente
ne soit pas rejetée à cause d'un décompte d'octets obsolète. `model_fetch.py --checksum` enregistre le sidecar et
`--verify` vérifie un téléchargement existant sans retélécharger. C'est le type
de preuve qu'un audit veut : quel modèle, quel hash, d'où vient-il, qu'a-t-on
mesuré.

### 5. Vous voulez la parité mobile depuis le même moteur

Le dépôt contient des clients Android et iOS natifs contre les mêmes fichiers de
modèle et la même promesse offline. Ce n'est pas une fonctionnalité d'Ollama ;
des projets mobiles communautaires existent mais ne font pas partie du projet
principal.

### 6. Vous voulez des benchmarks reproductibles et honnêtes

`--benchmark` exécute de vraies générations sur votre machine et rapporte le
temps de prompt mesuré, le temps de génération et les tokens/s. Les docs exigent
explicitement la sortie de la commande et le SHA-256 du modèle avant d'accepter
un résultat. Pas de chiffre marketing.

## Où Ollama est mieux adapté

- **Installation de modèle en une commande.** `ollama pull llama3` est plus
  simple que de trouver, télécharger et vérifier un GGUF soi-même.
- **Ampleur du catalogue.** La bibliothèque officielle de modèles est beaucoup
  plus grande et facile à explorer qu'une recherche GGUF manuelle.
- **Charges server-first.** Si l'interface principale est HTTP, le modèle démon
  convient et l'écosystème Open WebUI est mature.
- **Pas besoin d'un modèle objet natif Unix.** Si la tâche est « donner une boîte
  de chat locale à l'équipe », Ollama est le chemin de moindre friction.
- **Concurrence multi-requêtes.** Le serveur d'Ollama est conçu pour servir de
  nombreux clients. SkiffLLM sérialise délibérément la génération derrière un
  mutex car un contexte llama.cpp n'est pas thread-safe.

## Réserves honnêtes sur SkiffLLM

- Il n'inclut pas de modèles ; le coût de mise en place est plus élevé qu'un
  catalogue géré.
- Le mode `--serve` est un serveur local compact, pas une passerelle
  multithread : une génération à la fois, pas de rate-limit interne, token
  Bearer partagé facultatif.
- Il n'existe pas encore de grand écosystème de GUI.
- Vous devez vérifier que votre backend de build llama.cpp prend en charge votre
  matériel GPU.

## Recommandation

| Situation | Utiliser |
| --- | --- |
| Shell-first, CI, revue git, ordinateurs hors ligne | SkiffLLM |
| Air-gapped / réseau restreint | SkiffLLM |
| Embarqué dans un flux bureau/mobile | SkiffLLM |
| Épinglage de chaîne d'approvisionnement et benchmarks reproductibles | SkiffLLM |
| Boîte de chat rapide avec grand catalogue | Ollama |
| Service HTTP-first avec concurrence | Ollama |

Choisissez honnêtement, pas par habitude. Si vous comparez, partez de votre
modèle de déploiement plutôt que d'une liste de fonctionnalités. Pour la
production, voyez [docs/ENTERPRISE.md](../ENTERPRISE.md) et pour traduire les
habitudes Ollama, [docs/OLLAMA_MIGRATION.md](../OLLAMA_MIGRATION.md) (en
anglais).
