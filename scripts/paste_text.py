#!/usr/bin/env python3
"""
paste_text.py
Copy text to the Wayland clipboard then inject Ctrl+V via uinput.

Usage:
    paste_text.py "Hello, World!"
    echo "Hello" | paste_text.py
    paste_text.py --delay 30 "some text"
"""

import argparse
import subprocess
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from send_keys import parse_combo, emit_combo


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="paste_text.py",
        description="Copy text to Wayland clipboard and send Ctrl+V via uinput.",
    )
    parser.add_argument(
        "text",
        nargs="?",
        help="Text to paste. Reads from stdin if omitted.",
    )
    parser.add_argument(
        "--delay", type=int, default=1, metavar="MS",
        help="Milliseconds between key events for Ctrl+V (default: 1).",
    )
    args = parser.parse_args()

    text = args.text if args.text is not None else sys.stdin.read()

    try:
        subprocess.run(
            ["wl-copy"],
            input=text.encode(),
            check=True,
        )
    except FileNotFoundError:
        sys.exit("wl-copy not found. Install with: sudo apt install wl-clipboard")
    except subprocess.CalledProcessError as e:
        sys.exit(f"wl-copy failed: {e}")

    keys = parse_combo("ctrl+v")
    emit_combo(keys, args.delay)


if __name__ == "__main__":
    main()
