#!/usr/bin/env python3
"""
type_text.py
Thin wrapper around send_keys.py --type-string.

Accepts text directly as a positional argument or from stdin.

Usage:
    type_text.py "Hello, World!"
    echo "Hello" | type_text.py
    type_text.py --delay 30 "some text"
"""

import argparse
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from send_keys import type_string


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="type_text.py",
        description="Type a string character by character via uinput.",
    )
    parser.add_argument(
        "text",
        nargs="?",
        help="Text to type. Reads from stdin if omitted.",
    )
    parser.add_argument(
        "--delay", type=int, default=20, metavar="MS",
        help="Milliseconds between key events (default: 20).",
    )
    args = parser.parse_args()

    text = args.text if args.text is not None else sys.stdin.read()
    type_string(text, args.delay)


if __name__ == "__main__":
    main()
