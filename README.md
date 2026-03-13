# Redox rev1 QMK Keymap

Personal QMK firmware keymap for the [Redox rev1](https://github.com/mattdibi/redox-keyboard) split ergonomic keyboard, plus a companion Python host tool for Raw HID communication.

## Features

- **Dvorak base layer** with symbols, numpad, F-keys, mouse keys, and macro layers
- **Raw HID clipboard injection** — type clipboard text directly from the keyboard via a host daemon
- **Autocorrect** — on-device correction of common typos
- **Dynamic macros** — record and replay keystroke sequences on the fly
- **Private macro system** — gitignored `private/` snippets injected at build time to keep sensitive strings out of the repo

## Repository layout

```
keymap.template.c           Public keymap template (markers for private injection)
keymap.c                    Generated — do not edit directly
hid_clipboard.c/.h          Raw HID clipboard module (standalone, no community module)
rules.mk                    QMK feature flags
autocorrect_dictionary.txt  Source for on-device autocorrect
gen_keymap.py               Injects private/keycodes.inc + private/cases.inc → keymap.c
build.sh                    One-shot: generate → compile → flash
qmk_hid_tool.py             Host-side HID tool (ping, echo, type-clipboard, listen)
qmk_buttons.example.json    Example button action config — copy to ~/.config/qmk_buttons.json
private/                    Gitignored — personal keycodes and macro strings
```

## Build and flash

Requires the [QMK CLI](https://docs.qmk.fm/newbs_getting_started) and the QMK repo at `~/git/qmk_firmware/`.

```bash
# Install QMK if needed
curl -fsSL https://install.qmk.fm | sh
qmk setup

# Generate, compile, and flash in one step
./build.sh
```

`build.sh` will:
1. Run `gen_keymap.py` to inject private snippets into `keymap.template.c` → `keymap.c`
2. Sync source files into `~/git/qmk_firmware/keyboards/redox/keymaps/sebastiansam55/`
3. Regenerate `autocorrect_data.h` from the dictionary
4. Compile with `qmk compile`
5. Flash the resulting `.hex` with `qmk flash`

To regenerate only the autocorrect header:

```bash
cd ~/git/qmk_firmware
qmk generate-autocorrect-data -kb redox/rev1 -km sebastiansam55 \
  keyboards/redox/keymaps/sebastiansam55/autocorrect_dictionary.txt \
  > keyboards/redox/keymaps/sebastiansam55/autocorrect_data.h
```

For debug output: set `CONSOLE_ENABLE = yes` in `rules.mk` and run `qmk console`.

## Layers

| Layer | Trigger | Purpose |
|-------|---------|---------|
| `_DVORAK` | default | Base layer |
| `_SYMB` | `SYM_L` (both sides) | Symbols + numpad |
| `_COMBO` | `KC_CMB` (left outer thumb) | F-keys, mouse keys, cut/copy/paste |
| `_FN1` | `OSL(_FN1)` (left inner thumb) | Clipboard (`TYPCLIP`), QMK programmable buttons (`PB_1`/`PB_2`) |
| `_FN2` | `OSL(_FN2)` (right inner thumb) | HID daemon buttons `HIDB_1`–`HIDB_8` (top row) |
| `_ADJUST` | `LT(_ADJUST, KC_END/KC_LBRC)` | Macros, dynamic macros, media |

## HID clipboard (`hid_clipboard.c`)

Allows the host to push text to the keyboard, which then types it out as keystrokes via `send_string()`.

Two transfer modes selected automatically by text size:

| Mode | Condition | Behaviour |
|------|-----------|-----------|
| **Buffered** | text ≤ 504 bytes (18 × 28 B chunks) | All chunks accumulated on keyboard; typed after last chunk received |
| **Streaming** | text > 504 bytes | One chunk at a time; ACK deferred until after typing (natural back-pressure) |

**Wiring in `keymap.c`** (already done — reference only):

```c
void raw_hid_receive(uint8_t *data, uint8_t length) {
    raw_hid_receive_hid_clipboard(data, length);
}
void housekeeping_task_user(void) {
    housekeeping_task_hid_clipboard();
}
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_hid_clipboard(keycode, record)) return false;
    // ...
}
```

> `send_string()` must be called from `housekeeping_task_user()`, not from `raw_hid_receive()` (USB interrupt context). This is why chunk staging and deferred ACK exist.

### Protocol summary

| Cmd | Direction | Description |
|-----|-----------|-------------|
| `0x01` | host → keyboard | Ping (keyboard echoes back) |
| `0x02` | host → keyboard | Echo payload |
| `0x03` | host → keyboard | Clipboard chunk (chunked text transfer) |
| `0x03` | keyboard → host | ACK per chunk |
| `0x04` | keyboard → host | Clipboard request (sent when `TYPCLIP` pressed) |
| `0x05` | keyboard → host | Programmable button press (`HIDB_1`–`HIDB_8`); byte 1 = button ID (1–8) |

## Host tool (`qmk_hid_tool.py`)

```bash
pip install hidapi

python qmk_hid_tool.py --list-devices
python qmk_hid_tool.py --vid 0xXXXX --pid 0xXXXX ping
python qmk_hid_tool.py --vid 0xXXXX --pid 0xXXXX echo "hello"
python qmk_hid_tool.py --vid 0xXXXX --pid 0xXXXX type-clipboard
python qmk_hid_tool.py --vid 0xXXXX --pid 0xXXXX listen   # daemon mode
```

**`listen` mode** runs as a background daemon. When `TYPCLIP` is pressed on the keyboard, the keyboard sends a `0x04` request; the daemon reads the host clipboard and streams it back as `0x03` chunks. When a `HIDB_N` key is pressed, the keyboard sends a `0x05` packet; the daemon looks up the button ID in a JSON config and runs the configured shell command.

Additional options:

```
--timeout MS        HID read timeout (default: 1000 ms; use ~8000 in streaming mode)
--debug             Verbose HID packet logging
--config PATH       Button action config (default: ~/.config/qmk_buttons.json)
type-clipboard --text "..."   Send literal text instead of clipboard
type-clipboard --delay MS     Inter-chunk delay
```

### Programmable buttons (`HIDB_1`–`HIDB_8`)

Copy the example config and edit it to suit:

```bash
cp qmk_buttons.example.json ~/.config/qmk_buttons.json
```

Config format (`~/.config/qmk_buttons.json`):

```json
{
  "buttons": {
    "1": {"action": "shell", "command": "notify-send 'Button 1 pressed'"},
    "2": {"action": "shell", "command": "playerctl play-pause"},
    "3": {"action": "shell", "command": "/home/sam/scripts/my_script.sh"}
  }
}
```

Keys are string button IDs `"1"`–`"8"`. Only buttons with entries do anything; unrecognised button IDs print a warning to stderr and are ignored. The `listen` daemon must be running for button presses to trigger actions.

> **Note:** `HIDB_N` (HID daemon buttons) are distinct from QMK's native `PB_N` (`QK_PROGRAMMABLE_BUTTON_N`) keys. `PB_N` generate OS-level HID button reports on usage page 0x0C that the desktop can bind to actions without a daemon. `HIDB_N` send Raw HID `0x05` packets that require the Python `listen` daemon to act on.

## Running `listen` as a systemd user service

### Wayland / clipboard access caveat

`wl-paste` requires two environment variables that systemd user services do not
inherit automatically from the desktop session:

| Variable | Notes |
|----------|-------|
| `XDG_RUNTIME_DIR` | Set automatically for `systemctl --user` services |
| `WAYLAND_DISPLAY` | **Must be set manually** in the unit file |

Check the correct value in your terminal before installing:

```bash
echo $WAYLAND_DISPLAY   # typically wayland-0 or wayland-1
```

Edit `WAYLAND_DISPLAY=` in `qmk-hid-clipboard.service` to match, then follow
the steps below.  If you use X11 instead of Wayland, replace the line with
`Environment=DISPLAY=:0` and ensure `xclip` or `xsel` is installed.

### HID device permissions

Without a udev rule the HID device is root-only.  Create
`/etc/udev/rules.d/50-qmk-hid.rules` (replace VID/PID with your keyboard's):

```
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="feed", ATTRS{idProduct}=="0000", TAG+="uaccess"
```

Reload udev and re-plug the keyboard:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

`TAG+="uaccess"` grants access to the currently logged-in user without needing
a static group.

### Install and enable the service

```bash
# 1. Copy the unit file to the systemd user directory
mkdir -p ~/.config/systemd/user
cp qmk-hid-clipboard.service ~/.config/systemd/user/

# 2. Edit VID, PID, script path, and WAYLAND_DISPLAY inside the file
nano ~/.config/systemd/user/qmk-hid-clipboard.service

# 3. Reload systemd and enable the service
systemctl --user daemon-reload
systemctl --user enable --now qmk-hid-clipboard.service

# Check status / logs
systemctl --user status qmk-hid-clipboard.service
journalctl --user -u qmk-hid-clipboard.service -f
```

The service starts automatically when your graphical session starts and restarts
itself if it crashes (e.g. after a USB disconnect/reconnect).

### Propagating the environment automatically (optional)

Instead of hardcoding `WAYLAND_DISPLAY` in the unit file you can propagate it
from the session startup.  Add this to your `~/.profile`, `~/.bash_profile`, or
session autostart:

```bash
systemctl --user import-environment WAYLAND_DISPLAY XDG_RUNTIME_DIR DISPLAY
```

Then remove the `Environment=WAYLAND_DISPLAY=…` line from the unit file and run
`systemctl --user daemon-reload`.

## Private macro system

Sensitive strings (passwords, URLs, work paths) live in gitignored files:

```
private/keycodes.inc    # enum entries starting from HID_CLIPBOARD_SAFE_RANGE
private/cases.inc       # switch case handlers
```

`gen_keymap.py` replaces `// %%PRIVATE_KEYCODES%%` and `// %%PRIVATE_CASES%%` markers in `keymap.template.c` to produce `keymap.c`. Both generated files are gitignored.

## Autocorrect

Edit `autocorrect_dictionary.txt` (format: `typo -> correction`, one per line). The dictionary is compiled to `autocorrect_data.h` automatically by `build.sh`.

## License

Keymap based on the original Redox keymap by Mattia Dal Ben, licensed under GPL-2.0-or-later.
`hid_clipboard.c/.h` and `qmk_hid_tool.py` are original work, also GPL-2.0-or-later.
