#!/usr/bin/env python3
"""
send_keys.py
Inject a keypress or key combination into the Linux input subsystem via uinput.

Intended to be called from qmk_hid_tool.py's button action config
(~/.config/qmk_buttons.json) when a HIDB_* key is pressed on the keyboard.

Usage:
    send_keys.py KEY_PLAYPAUSE
    send_keys.py ctrl+alt+t
    send_keys.py super+l
    send_keys.py shift+KEY_F10
    send_keys.py --delay 30 ctrl+c
    send_keys.py --type-string "Hello, World!"

Requirements:
    pip install python-uinput

    The calling user must have write access to /dev/uinput. Either:
      - Add user to 'input' group and apply udev rule (see README.md), or
      - Run as root (not recommended for a service).
"""

import argparse
import sys
import time

try:
    import uinput
except ImportError:
    sys.exit("python-uinput not found. Install with: pip install python-uinput")


# Friendly modifier aliases -> uinput constants
MODIFIER_MAP: dict[str, int] = {
    "ctrl":   uinput.KEY_LEFTCTRL,
    "lctrl":  uinput.KEY_LEFTCTRL,
    "rctrl":  uinput.KEY_RIGHTCTRL,
    "alt":    uinput.KEY_LEFTALT,
    "lalt":   uinput.KEY_LEFTALT,
    "ralt":   uinput.KEY_RIGHTALT,
    "altgr":  uinput.KEY_RIGHTALT,
    "shift":  uinput.KEY_LEFTSHIFT,
    "lshift": uinput.KEY_LEFTSHIFT,
    "rshift": uinput.KEY_RIGHTSHIFT,
    "super":  uinput.KEY_LEFTMETA,
    "meta":   uinput.KEY_LEFTMETA,
    "win":    uinput.KEY_LEFTMETA,
    "hyper":  uinput.KEY_LEFTMETA,
}

# Map each typeable character to (uinput_key, needs_shift).
# Assumes a standard US QWERTY layout.
def _build_char_map() -> dict[str, tuple[int, bool]]:
    m: dict[str, tuple[int, bool]] = {}

    # Letters
    for ch in "abcdefghijklmnopqrstuvwxyz":
        key = getattr(uinput, f"KEY_{ch.upper()}")
        m[ch] = (key, False)
        m[ch.upper()] = (key, True)

    # Digits and their shifted symbols
    digit_pairs = [
        ("0", uinput.KEY_0, ")", True),
        ("1", uinput.KEY_1, "!", True),
        ("2", uinput.KEY_2, "@", True),
        ("3", uinput.KEY_3, "#", True),
        ("4", uinput.KEY_4, "$", True),
        ("5", uinput.KEY_5, "%", True),
        ("6", uinput.KEY_6, "^", True),
        ("7", uinput.KEY_7, "&", True),
        ("8", uinput.KEY_8, "*", True),
        ("9", uinput.KEY_9, "(", True),
    ]
    for digit, key, shifted_ch, _ in digit_pairs:
        m[digit] = (key, False)
        m[shifted_ch] = (key, True)

    # Punctuation: (unshifted_char, key, shifted_char)
    punct = [
        (" ",  uinput.KEY_SPACE,       None),
        ("\n", uinput.KEY_ENTER,       None),
        ("\t", uinput.KEY_TAB,         None),
        ("-",  uinput.KEY_MINUS,       "_"),
        ("=",  uinput.KEY_EQUAL,       "+"),
        ("[",  uinput.KEY_LEFTBRACE,   "{"),
        ("]",  uinput.KEY_RIGHTBRACE,  "}"),
        ("\\", uinput.KEY_BACKSLASH,   "|"),
        (";",  uinput.KEY_SEMICOLON,   ":"),
        ("'",  uinput.KEY_APOSTROPHE,  '"'),
        (",",  uinput.KEY_COMMA,       "<"),
        (".",  uinput.KEY_DOT,         ">"),
        ("/",  uinput.KEY_SLASH,       "?"),
        ("`",  uinput.KEY_GRAVE,       "~"),
    ]
    for unshifted, key, shifted_ch in punct:
        m[unshifted] = (key, False)
        if shifted_ch is not None:
            m[shifted_ch] = (key, True)

    return m

CHAR_MAP: dict[str, tuple[int, bool]] = _build_char_map()


def resolve_key(token: str) -> int:
    """Return the uinput constant for a single key token.

    Accepts:
      - modifier aliases (ctrl, alt, shift, super, …)
      - KEY_* constant names (KEY_PLAYPAUSE, KEY_A, …)
      - bare letter/name that gets a KEY_ prefix (a -> KEY_A, f10 -> KEY_F10)
    """
    lower = token.strip().lower()

    if lower in MODIFIER_MAP:
        return MODIFIER_MAP[lower]

    # Canonicalise to KEY_<UPPER>
    upper = lower if lower.startswith("key_") else f"key_{lower}"
    attr = upper.upper()

    if not hasattr(uinput, attr):
        sys.exit(
            f"Unknown key: {token!r}\n"
            f"  Tried uinput.{attr} — check https://github.com/tuomasjjrasanen/python-uinput "
            f"or run: python -c \"import uinput; print([x for x in dir(uinput) if x.startswith('KEY_')])\""
        )

    return getattr(uinput, attr)


def parse_combo(combo_str: str) -> list[int]:
    """Parse 'ctrl+alt+t' into an ordered list of uinput key constants."""
    return [resolve_key(part) for part in combo_str.split("+")]


def emit_combo(keys: list[int], delay_ms: int) -> None:
    """Create a uinput device, press all keys (chord), then release in reverse."""
    delay_s = delay_ms / 1000.0

    with uinput.Device(keys) as device:
        # Brief pause so udev/compositors recognise the new device before events
        time.sleep(0.1)

        for key in keys:
            device.emit(key, 1)   # key-down
            time.sleep(delay_s)

        for key in reversed(keys):
            device.emit(key, 0)   # key-up
            time.sleep(delay_s)


def type_string(text: str, delay_ms: int) -> None:
    """Type each character of text as individual keypresses."""
    unknown = [ch for ch in text if ch not in CHAR_MAP]
    if unknown:
        unique = "".join(dict.fromkeys(unknown))
        sys.exit(f"Unsupported characters for --type-string: {unique!r}")

    delay_s = delay_ms / 1000.0

    # Collect all keys needed so the device can emit them
    needed_keys = set(key for key, _ in CHAR_MAP.values()) | {uinput.KEY_LEFTSHIFT}

    with uinput.Device(list(needed_keys)) as device:
        time.sleep(0.1)

        for ch in text:
            key, needs_shift = CHAR_MAP[ch]
            if needs_shift:
                device.emit(uinput.KEY_LEFTSHIFT, 1)
            device.emit(key, 1)
            time.sleep(delay_s)
            device.emit(key, 0)
            if needs_shift:
                device.emit(uinput.KEY_LEFTSHIFT, 0)
            # time.sleep(delay_s)


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="send_keys.py",
        description="Inject a keypress or chord into Linux via uinput.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
examples:
  send_keys.py KEY_PLAYPAUSE
  send_keys.py KEY_VOLUMEUP
  send_keys.py super+l
  send_keys.py ctrl+alt+t
  send_keys.py shift+KEY_F10
  send_keys.py --delay 30 ctrl+c
  send_keys.py --type-string "Hello, World!"
        """,
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "combo",
        nargs="?",
        help="Key or '+'-separated chord (e.g. KEY_PLAYPAUSE, ctrl+alt+t).",
    )
    group.add_argument(
        "--type-string", metavar="TEXT",
        help="Type out a string character by character (US QWERTY layout).",
    )
    parser.add_argument(
        "--delay", type=int, default=1, metavar="MS",
        help="Milliseconds between individual key-down/up events (default: 1).",
    )
    args = parser.parse_args()

    if args.type_string is not None:
        type_string(args.type_string, args.delay)
    else:
        keys = parse_combo(args.combo)
        emit_combo(keys, args.delay)


if __name__ == "__main__":
    main()
