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
cp "$REPO_DIR/keymap.c" "$KEYMAP_DIR/keymap.c"
cp "$REPO_DIR/config.h" "$KEYMAP_DIR/config.h"
cp "$REPO_DIR/halconf.h" "$KEYMAP_DIR/halconf.h"
cp "$REPO_DIR/mcuconf.h" "$KEYMAP_DIR/mcuconf.h"
cp "$REPO_DIR/user_song_list.h" "$KEYMAP_DIR/user_song_list.h"
cp "$REPO_DIR/hid_clipboard.c" "$KEYMAP_DIR/hid_clipboard.c"
cp "$REPO_DIR/hid_clipboard.h" "$KEYMAP_DIR/hid_clipboard.h"
cp "$REPO_DIR/rules.mk" "$KEYMAP_DIR/rules.mk"
cp "$REPO_DIR/autocorrect_dictionary.txt" "$KEYMAP_DIR/autocorrect_dictionary.txt"

echo "Generating autocorrect data..."
cd "$QMK_DIR"
qmk generate-autocorrect-data -kb "$KEYBOARD" -km "$KEYMAP" \
  "$KEYMAP_DIR/autocorrect_dictionary.txt" >"$KEYMAP_DIR/autocorrect_data.h"

echo "Compiling ${KEYBOARD}:${KEYMAP}..."
qmk compile -kb "$KEYBOARD" -km "$KEYMAP" -e CONVERT_TO=elite_pi

# Find the most recent .uf2 file in current directory
UF2_FILE="$(ls -t ./*.uf2 2>/dev/null | head -n1 || true)"

if [[ -z "$UF2_FILE" ]]; then
  echo "Error: No .uf2 file found after compilation."
  exit 1
fi

# Find the Elite-Pi / RP2040 drive (shows up as RPI-RP2 in bootloader mode)
RPI_DRIVE="$(findmnt -rno TARGET -S LABEL=RPI-RP2 2>/dev/null || true)"
if [[ -z "$RPI_DRIVE" ]]; then
  for d in /media/"$USER"/RPI-RP2 /run/media/"$USER"/RPI-RP2 /mnt/RPI-RP2; do
    if [[ -d "$d" ]]; then
      RPI_DRIVE="$d"
      break
    fi
  done
fi

if [[ -z "$RPI_DRIVE" ]]; then
  echo "RPI-RP2 drive not found. Waiting for it to appear..."
  echo "  (hold BOOT/RESET while plugging in, or use QK_BOOT keycode)"
  for i in $(seq 1 30); do
    sleep 2
    RPI_DRIVE="$(findmnt -rno TARGET -S LABEL=RPI-RP2 2>/dev/null || true)"
    if [[ -z "$RPI_DRIVE" ]]; then
      for d in /media/"$USER"/RPI-RP2 /run/media/"$USER"/RPI-RP2 /mnt/RPI-RP2; do
        if [[ -d "$d" ]]; then
          RPI_DRIVE="$d"
          break
        fi
      done
    fi
    if [[ -n "$RPI_DRIVE" ]]; then
      echo "Drive found at $RPI_DRIVE"
      break
    fi
    echo "  Still waiting... (${i}/30)"
  done
fi

if [[ -z "$RPI_DRIVE" ]]; then
  echo "Error: RPI-RP2 drive not found after 60 seconds. Giving up."
  exit 1
fi

echo "Copying $UF2_FILE to $RPI_DRIVE..."
cp "$UF2_FILE" "$RPI_DRIVE/"

echo "Waiting to restart espanso service"
sleep 5
espanso restart
