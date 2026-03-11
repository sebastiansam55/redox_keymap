#!/usr/bin/env bash
set -euo pipefail

KEYBOARD="redox/rev1"
KEYMAP="sebastiansam55"
QMK_DIR="${HOME}/git/qmk_firmware/"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Ensure qmk is available
if ! command -v qmk >/dev/null 2>&1; then
  echo "Error: qmk CLI not found in PATH"
  echo "curl -fsSL https://install.qmk.fm | sh"
  echo "qmk setup"
  exit 1
fi

KEYMAP_DIR="$QMK_DIR/keyboards/redox/keymaps/sebastiansam55"
mkdir -p "$KEYMAP_DIR"

# Generate keymap.c from template + private snippets
echo "Generating keymap.c..."
python3 "$REPO_DIR/gen_keymap.py"

# Sync repo files into QMK keymap directory
echo "Syncing files to $KEYMAP_DIR..."
cp "$REPO_DIR/keymap.c"                  "$KEYMAP_DIR/keymap.c"
cp "$REPO_DIR/hid_clipboard.c"           "$KEYMAP_DIR/hid_clipboard.c"
cp "$REPO_DIR/hid_clipboard.h"           "$KEYMAP_DIR/hid_clipboard.h"
cp "$REPO_DIR/rules.mk"                  "$KEYMAP_DIR/rules.mk"
cp "$REPO_DIR/autocorrect_dictionary.txt" "$KEYMAP_DIR/autocorrect_dictionary.txt"

echo "Generating autocorrect data..."
cd "$QMK_DIR"
qmk generate-autocorrect-data -kb "$KEYBOARD" -km "$KEYMAP" \
  "$KEYMAP_DIR/autocorrect_dictionary.txt" >"$KEYMAP_DIR/autocorrect_data.h"

echo "Compiling ${KEYBOARD}:${KEYMAP}..."
qmk compile -kb "$KEYBOARD" -km "$KEYMAP"

# Find the most recent .hex file in current directory
HEX_FILE="$(ls -t ./*.hex 2>/dev/null | head -n1 || true)"

if [[ -z "$HEX_FILE" ]]; then
  echo "Error: No .hex file found after compilation."
  exit 1
fi

echo "Flashing $HEX_FILE..."
qmk flash "$HEX_FILE"
