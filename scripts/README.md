# scripts/

Helper scripts called by `qmk_hid_tool.py` when running in `listen` (daemon) mode.

The daemon reads `~/.config/qmk_buttons.json` and executes the configured shell
command whenever a `HIDB_*` key is pressed on the keyboard (Raw HID `0x05` packet).
Scripts here are meant to be wired up as those commands.

---

## send_keys.py

Injects a keypress or key chord into the Linux input subsystem via **uinput**.
Useful when you want a keyboard button to trigger a system shortcut or media key
without depending on a running desktop environment daemon.

### Requirements

```
pip install python-uinput
```

The script needs write access to `/dev/uinput`. The recommended approach is a
udev rule plus adding your user to the `input` group:

```bash
# 1. Create udev rule
sudo tee /etc/udev/rules.d/99-uinput.rules <<'EOF'
KERNEL=="uinput", MODE="0660", GROUP="input", OPTIONS+="static_node=uinput"
EOF

# 2. Add your user to the input group
sudo usermod -aG input $USER

# 3. Reload rules (or reboot)
sudo udevadm control --reload-rules && sudo udevadm trigger

# 4. Log out and back in for group membership to take effect
```

### Usage

```
send_keys.py [--delay MS] <combo>
send_keys.py [--delay MS] --type-string "text to type"
```

**Chord mode:** `<combo>` is either a single key name or a `+`-separated chord:

| Argument | Result |
|---|---|
| `KEY_PLAYPAUSE` | media play/pause |
| `KEY_VOLUMEUP` | volume up |
| `KEY_VOLUMEDOWN` | volume down |
| `KEY_MUTE` | mute toggle |
| `super+l` | lock screen (most DEs) |
| `ctrl+alt+t` | open terminal (most DEs) |
| `shift+KEY_F10` | context menu |
| `ctrl+c` | copy / interrupt |

Modifier aliases: `ctrl`, `lctrl`, `rctrl`, `alt`, `lalt`, `ralt`, `altgr`,
`shift`, `lshift`, `rshift`, `super`, `meta`, `win`, `hyper`.

Any `KEY_*` constant from the Linux input event headers is accepted by name
(e.g. `KEY_PLAYPAUSE`, `KEY_F5`, `KEY_HOME`). Bare names without the `KEY_`
prefix also work (`f5` → `KEY_F5`).

**String mode:** `--type-string "text"` types each character individually using US QWERTY layout. Supports letters, digits, common punctuation, space, tab, and newline.

`--delay MS` sets the pause between individual key-down/up events (default 1 ms).

### Wiring into qmk_buttons.json

```json
{
  "buttons": {
    "1": {"action": "shell", "command": "python /home/sam/redox/scripts/send_keys.py KEY_PLAYPAUSE"},
    "2": {"action": "shell", "command": "python /home/sam/redox/scripts/send_keys.py KEY_VOLUMEUP"},
    "3": {"action": "shell", "command": "python /home/sam/redox/scripts/send_keys.py KEY_VOLUMEDOWN"},
    "4": {"action": "shell", "command": "python /home/sam/redox/scripts/send_keys.py KEY_MUTE"},
    "5": {"action": "shell", "command": "python /home/sam/redox/scripts/send_keys.py super+l"},
    "6": {"action": "shell", "command": "python /home/sam/redox/scripts/send_keys.py ctrl+alt+t"},
    "7": {"action": "shell", "command": "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+"},
    "8": {"action": "shell", "command": "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"}
  }
}
```

---

## type_text.py

Thin wrapper around `send_keys.py --type-string`. Accepts text as a positional
argument or from stdin — no flag needed.

### Usage

```
type_text.py [--delay MS] "text to type"
echo "text" | type_text.py [--delay MS]
```

### Requirements

Same as `send_keys.py` (`python-uinput`, `/dev/uinput` access).

---

## paste_text.py

Copies text to the **Wayland clipboard** then injects **Ctrl+V** via uinput.
Faster than `type_text.py` for long strings and preserves formatting exactly.

### Requirements

```
sudo apt install wl-clipboard   # provides wl-copy
pip install python-uinput
```

Also requires `/dev/uinput` access (see `send_keys.py` requirements above).

### Usage

```
paste_text.py [--delay MS] "text to paste"
echo "text" | paste_text.py [--delay MS]
```

---

### Listing available KEY_ constants

```bash
python -c "import uinput; print('\n'.join(x for x in dir(uinput) if x.startswith('KEY_')))"
```
