# Bonnes premières tâches pour contributeurs

Voici de bons points d'entrée pour contribuer au projet. Chaque élément doit
préserver les tests, la documentation et la parité entre les trois plateformes
(bureau/CLI, Android, iOS).

## Documentation

- Corrigez les traductions manquantes ou incorrectes dans les documents français.
- Comparez la parité de contenu entre `docs/fr/` et `docs/` et signalez les
  différences.
- Comparez les commandes de configuration et d'utilisation avec la sortie de
  `--help`.

## CLI

- Ajoutez un test unitaire pour une implémentation de nouvelle commande.
- Vérifiez la cohérence des noms de champs dans les sorties `--json`.
- Ajoutez des messages d'erreur plus clairs (par exemple, modèle manquant).

## Android

- Ajoutez des états vides pour les champs `model.json` manquants dans la liste des
  modèles.
- Concevez un petit écran affichant les fichiers d'un modèle chargé.
- Améliorez le retour pour les téléchargements annulés.

## iOS

- Ajoutez une page de test comparant la parité des fonctionnalités avec bureau et
  Android.
- Décomposez `ChatView.swift` en composants plus petits.
- Ajoutez une liste visuelle des sessions enregistrées.

## Qualité générale

- Étendez les vérifications dans `scripts/ci-local.sh` et `scripts/ci-android.sh`.
- Ajoutez une vraie entrée de benchmark dans `docs/benchmarks.md` (uniquement une
  génération réelle sur votre propre matériel).
- Vérifiez le tableau des fonctionnalités du README contre le comportement réel.
