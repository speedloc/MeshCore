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

echo "==> Baue Firmware vor dem Veröffentlichen"
./build.sh --clean

echo "==> Prüfe Änderungen"
git status --short

# Build-Ausgaben und lokale PlatformIO-Dateien gehören nicht ins Repository.
git add -A -- ':!firmware-output' ':!.pio'

if git diff --cached --quiet; then
  echo "Keine Quellcode-Änderungen zum Committen vorhanden."
else
  git commit -m "$COMMIT_MESSAGE"
fi

echo "==> Push nach origin/$BRANCH"
git push origin "$BRANCH"

echo "SUCCESS – GitHub wurde aktualisiert."
