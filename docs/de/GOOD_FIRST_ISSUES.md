# Gute erste Aufgaben für Einsteiger

Dies sind gute Einstiegspunkte, um zum Projekt beizutragen. Jeder Punkt sollte
Tests, Dokumentation und Parität über die drei Plattformen (Desktop/CLI,
Android, iOS) erhalten.

## Dokumentation

- Korrigieren Sie fehlende oder falsche Übersetzungen in den deutschen
  Dokumenten.
- Vergleichen Sie die Inhaltsparität zwischen `docs/de/` und `docs/` und
  melden Sie Unterschiede.
- Gleichen Sie Setup- und Verwendungsbefehle mit der `--help`-Ausgabe ab.

## CLI

- Fügen Sie einen Unit-Test für eine neue Kurzbefehl-Implementierung hinzu.
- Prüfen Sie die Konsistenz der Feldnamen in `--json`-Ausgaben.
- Fügen Sie klarere Hinweise für Fehlermeldungen hinzu (z. B. fehlendes Modell).

## Android

- Fügen Sie Leerzustände für fehlende `model.json`-Felder in der Modellliste
  hinzu.
- Entwerfen Sie einen kleinen Bildschirm, der die Dateien eines geladenen Modells
  auflistet.
- Verbessern Sie das Feedback für abgebrochene Downloads.

## iOS

- Fügen Sie eine Testseite hinzu, die die Feature-Parität mit Desktop und
  Android vergleicht.
- Teilen Sie `ChatView.swift` in kleinere Komponenten auf.
- Fügen Sie eine visuelle Liste gespeicherter Sitzungen hinzu.

## Allgemeine Qualität

- Erweitern Sie die Prüfungen in `scripts/ci-local.sh` und `scripts/ci-android.sh`.
- Fügen Sie einen echten Benchmark-Eintrag zu `docs/benchmarks.md` hinzu (nur
  echte Generierung auf Ihrer eigenen Hardware).
- Überprüfen Sie die Feature-Tabelle in der README gegen das tatsächliche
  Verhalten.
