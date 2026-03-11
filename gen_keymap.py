#!/usr/bin/env python3
"""Generate keymap.c by injecting private snippets into keymap.template.c.

Reads:
  keymap.template.c       — committed, public template with marker comments
  private/keycodes.inc    — gitignored: enum custom_keycodes { ... } block
  private/cases.inc       — gitignored: switch case statements

Writes:
  keymap.c                — gitignored, passed to QMK for compilation

Markers in keymap.template.c:
  // %%PRIVATE_KEYCODES%%   replaced by contents of private/keycodes.inc
  // %%PRIVATE_CASES%%      replaced by contents of private/cases.inc
"""

import sys
from pathlib import Path

REPO = Path(__file__).parent
TEMPLATE = REPO / "keymap.template.c"
OUTPUT   = REPO / "keymap.c"
PRIVATE  = REPO / "private"

MARKER_KEYCODES = "// %%PRIVATE_KEYCODES%%"
MARKER_CASES    = "// %%PRIVATE_CASES%%"


def read_snippet(name: str) -> str:
    path = PRIVATE / name
    if not path.exists():
        print(f"  warning: {path} not found — skipping", file=sys.stderr)
        return ""
    return path.read_text()


def main() -> None:
    if not TEMPLATE.exists():
        sys.exit(f"error: {TEMPLATE} not found")

    text = TEMPLATE.read_text()

    keycodes = read_snippet("keycodes.inc")
    cases    = read_snippet("cases.inc")

    text = text.replace(MARKER_KEYCODES, keycodes.rstrip())
    text = text.replace(MARKER_CASES,    cases.rstrip())

    OUTPUT.write_text(text)
    print(f"Generated {OUTPUT}")


if __name__ == "__main__":
    main()
