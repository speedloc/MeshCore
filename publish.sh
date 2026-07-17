#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [[ ! -d .git ]]; then
  echo "FEHLER: Dieses Verzeichnis ist kein Git-Repository." >&2
  exit 1
fi

BRANCH="$(git branch --show-current)"
if [[ -z "$BRANCH" ]]; then
  echo "FEHLER: Kein aktiver Git-Branch gefunden." >&2
  exit 1
fi

if [[ "$BRANCH" != "solar-repeater" ]]; then
  echo "WARNUNG: Aktiver Branch ist '$BRANCH', erwartet wurde 'solar-repeater'."
  read -r -p "Trotzdem fortfahren? [j/N] " answer
  [[ "$answer" =~ ^[jJyY]$ ]] || exit 1
fi

COMMIT_MESSAGE="${*:-Solar repeater update $(date '+%Y-%m-%d %H:%M')}"

echo "==> Prüfe Änderungen"
git status --short

# Alle Quell- und Dokumentationsdateien aufnehmen. Nur reproduzierbare
# Build-Ausgaben und lokale PlatformIO-Dateien bleiben bewusst lokal.
git add -A

if git diff --cached --quiet; then
  echo "Keine Quellcode-Änderungen zum Committen vorhanden."
else
  echo "==> Folgende Dateien werden committed"
  git diff --cached --name-status
  git commit -m "$COMMIT_MESSAGE"
fi

echo "==> Baue exakt den soeben committed Quellstand"
./build.sh --clean

if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "FEHLER: Der Build hat versionierte Dateien verändert. Push abgebrochen." >&2
  git status --short
  exit 1
fi

echo "==> Push nach origin/$BRANCH"
git push origin "$BRANCH"

echo "SUCCESS – GitHub wurde aktualisiert."
