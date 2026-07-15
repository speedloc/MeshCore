#!/usr/bin/env bash
set -Eeuo pipefail

ENV_NAME="Xiao_nrf52_repeater"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="${PLATFORMIO_VENV:-$HOME/platformio-venv}"
OUT_DIR="$SCRIPT_DIR/firmware-output"

cd "$SCRIPT_DIR"

if [[ ! -f platformio.ini ]]; then
  echo "FEHLER: platformio.ini wurde in $SCRIPT_DIR nicht gefunden." >&2
  exit 1
fi

if [[ ! -x "$VENV_DIR/bin/pio" ]]; then
  echo "PlatformIO-Umgebung fehlt: $VENV_DIR"
  echo "Einmalig installieren mit:"
  echo "  sudo apt install -y python3-full python3-venv"
  echo "  python3 -m venv '$VENV_DIR'"
  echo "  '$VENV_DIR/bin/pip' install -U pip platformio"
  exit 1
fi

CLEAN=0
UPLOAD=0
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    --upload) UPLOAD=1 ;;
    -h|--help)
      echo "Verwendung: ./build.sh [--clean] [--upload]"
      exit 0
      ;;
    *) echo "Unbekannte Option: $arg" >&2; exit 2 ;;
  esac
done

PIO="$VENV_DIR/bin/pio"
START=$(date +%s)

if (( CLEAN )); then
  echo "==> Lösche alten Build"
  "$PIO" run -e "$ENV_NAME" -t clean || rm -rf .pio
fi

echo "==> Kompiliere $ENV_NAME"
"$PIO" run -e "$ENV_NAME"

if (( UPLOAD )); then
  echo "==> Lade Firmware per USB hoch"
  "$PIO" run -e "$ENV_NAME" -t upload
fi

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR"/*
BUILD_DIR="$SCRIPT_DIR/.pio/build/$ENV_NAME"
find "$BUILD_DIR" -maxdepth 1 -type f \( -name '*.bin' -o -name '*.hex' -o -name '*.uf2' -o -name '*.zip' \) -exec cp -v {} "$OUT_DIR/" \;

END=$(date +%s)
echo
echo "SUCCESS – Dauer: $((END-START)) Sekunden"
echo "Firmware-Dateien: $OUT_DIR"
ls -lh "$OUT_DIR" || true
