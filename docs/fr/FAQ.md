# FAQ

## SkiffLLM téléverse-t-il mes conversations ?

Non. L'exécution de bureau ne contient aucun code réseau. Les prompts,
l'historique, les paramètres et le texte généré restent sur la machine.
L'application Android n'envoie aucun prompt nulle part.

## SkiffLLM télécharge-t-il des modèles ?

Pas automatiquement. L'exécution de bureau exige un fichier GGUF. Deux utilitaires
facultatifs sont disponibles :

- Bureau : `python3 scripts/model_fetch.py --model qwen2.5-0.5b`
- Android : `Settings` → `Models` → `Download`

Les deux utilisent HTTPS depuis Hugging Face et sont des actions explicites de
l'utilisateur.

## Quel modèle devrais-je utiliser ?

Commencez par un petit GGUF instruct au format Q4_K_M :

- Qwen2.5-0.5B-Instruct
- Qwen3-0.6B-Instruct
- Llama-3.2-1B-Instruct

Sur un téléphone avec moins de 4 Go de RAM, utilisez les modèles 0.5B ou 0.6B.

## Pourquoi ma première réponse est-elle lente ?

La première passe après le chargement du modèle inclut le traitement du prompt et
le réchauffement des défauts de page. Utilisez `--warmup` sur le bureau ou
lancez l'application Android une fois avant une vraie conversation. L'application
Android préchauffe le modèle automatiquement après le chargement.

## Où sont stockées les sessions ?

Bureau : le répertoire de sessions de `--session`/`--history` (par défaut sous
`~/.local/share/skifflm`). Android : `conversation.json` interne à l'application ;
effacer les données de l'application le supprime.

## Comment exposer l'API locale ?

```bash
# local uniquement
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# accessible depuis une autre machine, protégée par un jeton partagé
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "local-token"
```

Avec `--api-key` défini, `/v1/models` et `/v1/chat/completions` exigent
`Authorization: Bearer <key>` et renvoient sinon `401`. `/health`, `/version` et
`/` restent publics. Conservez la liaison par défaut `127.0.0.1` autant que
possible ; les écouteurs non loopback doivent toujours définir `--api-key`.

## Pourquoi `--benchmark` diffère-t-il des chiffres des fournisseurs ?

Chaque benchmark dans SkiffLLM est mesuré sur votre machine, avec le fichier de
modèle et le matériel que vous fournissez. Les chiffres dépendent du CPU/GPU, de
la quantification, de la taille de contexte, des threads et de la charge système.
Il n'y a pas de chiffres faux ou marketing.

## L'application Android fonctionne-t-elle sans connexion réseau ?

Oui, après le chargement d'un modèle. Charger un modèle enregistré, discuter,
exporter et effacer fonctionnent tous hors ligne. Le réseau n'est utilisé que si
vous choisissez de télécharger un nouveau modèle depuis Hugging Face.

## La télémétrie est-elle activée ?

Non. Il n'y a aucune analytique, aucun rapport de crash ni aucun suivi d'utilisation
dans l'exécution de bureau ou l'application Android.
