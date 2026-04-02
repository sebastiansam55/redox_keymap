#!/usr/bin/env python3
"""
QMK Raw HID Tool
~~~~~~~~~~~~~~~~
Send and receive Raw HID packets to/from a QMK keyboard.

Packet format: exactly 32 bytes (RAW_EPSIZE).
  Byte 0    : command ID
  Bytes 1-31: payload (command-specific)

Adding a new command
--------------------
1. Create a class that inherits from Command.
2. Implement build_packet() -> bytes  (must return exactly RAW_EPSIZE bytes)
3. Implement handle_response(data: bytes)  (called with the 32-byte reply)
4. Register the class in COMMANDS.

Example::

    python qmk_hid_tool.py --list-devices
    python qmk_hid_tool.py ping
    python qmk_hid_tool.py echo "hello"
    python qmk_hid_tool.py raw 01 02 03
"""

import argparse
import json
import logging
import os
import platform
import re
import subprocess
import sys
import time
from abc import ABC, abstractmethod

log = logging.getLogger("qmk_hid")

try:
    import hid
except ImportError:
    sys.exit(
        "hid package not found. Install it with:  pip install hidapi"
    )

# ---------------------------------------------------------------------------
# Constants (match your keyboard's config.h / info.json)
# ---------------------------------------------------------------------------

RAW_EPSIZE = 32          # QMK default packet size
DEFAULT_USAGE_PAGE = 0xFF60
DEFAULT_USAGE = 0x61


# ---------------------------------------------------------------------------
# Command base class
# ---------------------------------------------------------------------------

class Command(ABC):
    """Base class for all HID commands."""

    # Override in subclass — used for --help listing
    name: str = ""
    description: str = ""

    @abstractmethod
    def build_packet(self) -> bytes:
        """Return exactly RAW_EPSIZE bytes to send to the keyboard."""

    @abstractmethod
    def handle_response(self, data: bytes) -> None:
        """Process the RAW_EPSIZE-byte response from the keyboard."""

    def run(self, device_info: dict, timeout_ms: int) -> None:
        """Execute the full command exchange.

        The default implementation sends one packet and reads one response.
        Override this for multi-packet or stateful exchanges.
        """
        packet = self.build_packet()
        log.debug("TX (%d bytes): %s", len(packet), self.pretty(packet))
        print(f"Sending  ({len(packet)} bytes): {self.pretty(packet)}")
        response = send_and_receive(device_info, packet, timeout_ms)
        log.debug("RX (%d bytes): %s", len(response), self.pretty(response))
        print(f"Received ({len(response)} bytes): {self.pretty(response)}")
        self.handle_response(response)

    # Helpers ----------------------------------------------------------------

    @staticmethod
    def packet(cmd_id: int, payload: bytes = b"") -> bytes:
        """Build a zero-padded packet: [cmd_id] + payload + padding."""
        if len(payload) > RAW_EPSIZE - 1:
            raise ValueError(
                f"Payload too long ({len(payload)} bytes); max {RAW_EPSIZE - 1}"
            )
        raw = bytes([cmd_id]) + payload
        return raw.ljust(RAW_EPSIZE, b"\x00")

    @staticmethod
    def pretty(data: bytes) -> str:
        return " ".join(f"{b:02X}" for b in data)


# ---------------------------------------------------------------------------
# Built-in commands
# ---------------------------------------------------------------------------

class PingCommand(Command):
    name = "ping"
    description = "Send a ping (cmd=0x01) and print the response."

    CMD_ID = 0x01

    def build_packet(self) -> bytes:
        return self.packet(self.CMD_ID)

    def handle_response(self, data: bytes) -> None:
        if data[0] == self.CMD_ID:
            print(f"Pong received (cmd=0x{data[0]:02X})")
        else:
            print(f"Unexpected response: {self.pretty(data)}")


class EchoCommand(Command):
    name = "echo"
    description = "Send a string and expect the keyboard to echo it back."

    CMD_ID = 0x02

    def __init__(self, text: str) -> None:
        encoded = text.encode()[:RAW_EPSIZE - 1]
        self._payload = encoded

    def build_packet(self) -> bytes:
        return self.packet(self.CMD_ID, self._payload)

    def handle_response(self, data: bytes) -> None:
        payload = data[1:].rstrip(b"\x00")
        print(f"Echo: {payload.decode(errors='replace')!r}")


class RawCommand(Command):
    name = "raw"
    description = "Send arbitrary hex bytes (e.g. 'raw 01 02 03')."

    def __init__(self, hex_bytes: list[str]) -> None:
        try:
            self._data = bytes(int(h, 16) for h in hex_bytes)
        except ValueError as e:
            raise ValueError(f"Invalid hex byte: {e}") from e
        if len(self._data) > RAW_EPSIZE:
            raise ValueError(f"Too many bytes; max {RAW_EPSIZE}")

    def build_packet(self) -> bytes:
        return self._data.ljust(RAW_EPSIZE, b"\x00")

    def handle_response(self, data: bytes) -> None:
        print(f"Response: {self.pretty(data)}")


# ---------------------------------------------------------------------------
# Clipboard helper
# ---------------------------------------------------------------------------

def read_clipboard() -> str:
    """Read the system clipboard contents.  No external Python deps required."""
    system = platform.system()
    if system == "Darwin":
        cmds = [["pbpaste"]]
    elif system == "Windows":
        cmds = [["powershell", "-NoProfile", "-Command", "Get-Clipboard"]]
    else:  # Linux / BSD — try Wayland then X11 tools
        cmds = [
            ["wl-paste", "--no-newline"],
            ["xclip", "-selection", "clipboard", "-o"],
            ["xsel", "--clipboard", "--output"],
        ]

    for cmd in cmds:
        try:
            result = subprocess.run(cmd, capture_output=True, timeout=5)
            if result.returncode == 0:
                text = result.stdout.decode("utf-8", errors="replace")
                # powershell appends \r\n; strip trailing newline only for that case
                if system == "Windows":
                    text = text.rstrip("\r\n")
                return text
        except FileNotFoundError:
            continue
        except subprocess.TimeoutExpired:
            continue

    raise RuntimeError(
        "Could not read clipboard. "
        "On Linux, install one of: wl-paste (Wayland), xclip, or xsel."
    )



def to_sentence_case(text: str) -> str:
    """Lowercase everything, then capitalize the first letter of each sentence."""
    if not text:
        return text
    lowered = text.lower()
    # Capitalize the very first character
    result = lowered[0].upper() + lowered[1:]
    # Capitalize the first letter after . ! ? followed by whitespace
    result = re.sub(
        r'([.!?]\s+)([a-z])',
        lambda m: m.group(1) + m.group(2).upper(),
        result,
    )
    return result



class TypeClipboardCommand(Command):
    """Send clipboard text to the keyboard chunk-by-chunk; keyboard types it out.

    Packet layout (host → keyboard):
        [0]     CMD_ID = 0x03
        [1]     chunk_index  (0-based, uint8)
        [2]     total_chunks (uint8)
        [3]     data_len     (number of valid bytes in [4 .. 4+data_len-1])
        [4..31] text data    (up to 28 bytes of UTF-8 / ASCII)

    Response (keyboard → host):
        [0]  CMD_ID = 0x03
        [1]  chunk_index echoed back
        [2]  status: 0x00 = OK, non-zero = error / busy
    """

    name = "type-clipboard"
    description = "Read the clipboard and send its text to the keyboard to type out."

    CMD_ID = 0x03
    CHUNK_DATA_OFFSET = 4
    CHUNK_DATA_SIZE = RAW_EPSIZE - CHUNK_DATA_OFFSET  # 28 bytes per chunk
    # Must match HID_CLIP_MAX_CHUNKS in hid_clipboard/hid_clipboard.h
    MAX_CHUNKS = 18
    MAX_TEXT_BYTES = MAX_CHUNKS * CHUNK_DATA_SIZE      # 504 bytes

    def __init__(self, text: str | None = None, delay_ms: int = 0) -> None:
        raw_text = text if text is not None else read_clipboard()
        encoded = raw_text.encode("ascii", errors="replace")
        if not encoded:
            raise ValueError("Clipboard is empty — nothing to send.")
        self._data = encoded
        self._delay_ms = delay_ms
        self._streaming = len(encoded) > self.MAX_TEXT_BYTES

    # build_packet / handle_response are unused (run() is overridden),
    # but the ABC requires concrete implementations.
    def build_packet(self) -> bytes:  # pragma: no cover
        raise NotImplementedError("TypeClipboardCommand uses run() directly.")

    def handle_response(self, data: bytes) -> None:  # pragma: no cover
        raise NotImplementedError("TypeClipboardCommand uses run() directly.")

    def _make_chunk_packet(self, chunk_index: int, total_chunks: int, chunk: bytes) -> bytes:
        payload = bytes([chunk_index, total_chunks, len(chunk)]) + chunk
        return self.packet(self.CMD_ID, payload)

    def run(self, device_info: dict, timeout_ms: int) -> None:
        chunks = [
            self._data[i : i + self.CHUNK_DATA_SIZE]
            for i in range(0, len(self._data), self.CHUNK_DATA_SIZE)
        ]
        total = 0 if self._streaming else len(chunks)
        mode_label = "streaming (total=0, ACK deferred)" if self._streaming else "buffered"
        if self._streaming and timeout_ms <= 1000:
            print(
                f"Warning: streaming mode selected but --timeout={timeout_ms} ms may be too "
                f"short (ACK arrives after typing). Consider --timeout 8000.",
                file=sys.stderr,
            )
        print(
            f"Sending {len(self._data)} bytes as {len(chunks)} chunk(s) "
            f"({self.CHUNK_DATA_SIZE} bytes/chunk, {mode_label}) …"
        )

        for idx, chunk in enumerate(chunks):
            packet = self._make_chunk_packet(idx, total, chunk)
            log.debug("chunk %d/%d TX: %s", idx, len(chunks) - 1, self.pretty(packet))
            print(f"  chunk {idx + 1}/{len(chunks)}: {self.pretty(packet)}")
            response = send_and_receive(device_info, packet, timeout_ms)
            log.debug("chunk %d/%d RX: %s", idx, len(chunks) - 1, self.pretty(response))
            status = response[2] if len(response) > 2 else 0xFF
            ack_idx = response[1] if len(response) > 1 else 0xFF
            log.debug("chunk %d: ack_idx=0x%02X status=0x%02X", idx, ack_idx, status)
            if status != 0x00:
                sys.exit(
                    f"Keyboard returned error status 0x{status:02X} "
                    f"on chunk {idx} (ack_idx={ack_idx})."
                )
            if ack_idx != idx:
                sys.exit(
                    f"Chunk index mismatch: sent {idx}, keyboard acked {ack_idx}."
                )
            if self._delay_ms > 0:
                time.sleep(self._delay_ms / 1000.0)

        print("Done — keyboard is typing the clipboard text.")


class ListenCommand(Command):
    """Daemon mode: wait for 0x04 clipboard-request packets from the keyboard,
    read the host clipboard, and send it back as 0x03 chunks.  Also handles
    0x05 programmable-button packets by running shell commands from a JSON
    config file.

    Run this once and leave it running; press TYPCLIP on the keyboard to trigger.
    """

    name = "listen"
    description = "Daemon: respond to keyboard clipboard requests (TYPCLIP key)."

    CMD_CLIP     = TypeClipboardCommand.CMD_ID          # 0x03
    CMD_CLIP_REQ = 0x04                                 # keyboard → host
    CMD_BUTTON   = 0x05                                 # keyboard → host
    CHUNK_DATA_OFFSET = TypeClipboardCommand.CHUNK_DATA_OFFSET
    CHUNK_DATA_SIZE   = TypeClipboardCommand.CHUNK_DATA_SIZE
    MAX_CHUNKS        = TypeClipboardCommand.MAX_CHUNKS
    MAX_TEXT_BYTES    = TypeClipboardCommand.MAX_TEXT_BYTES

    def __init__(self, config_path: str = "~/.config/qmk_buttons.json") -> None:
        path = os.path.expanduser(config_path)
        if os.path.exists(path):
            with open(path) as f:
                config = json.load(f)
            self._button_config = config.get("buttons", {})
        else:
            self._button_config = {}
            print(f"[warn] button config not found: {path}", file=sys.stderr)

    # build_packet / handle_response are unused; run() drives everything.
    def build_packet(self) -> bytes:  # pragma: no cover
        raise NotImplementedError

    def handle_response(self, data: bytes) -> None:  # pragma: no cover
        raise NotImplementedError

    def _send_text(self, dev: hid.Device, timeout_ms: int, text: str) -> None:
        """Push text to the keyboard as 0x03 chunks; keyboard types it via send_string()."""
        encoded = text.encode("ascii", errors="replace")
        if not encoded:
            print("  nothing to send (empty text)")
            return

        chunks = [
            encoded[i : i + self.CHUNK_DATA_SIZE]
            for i in range(0, len(encoded), self.CHUNK_DATA_SIZE)
        ]
        streaming = len(encoded) > self.MAX_TEXT_BYTES
        total = 0 if streaming else len(chunks)
        mode = "streaming" if streaming else "buffered"
        print(f"  sending {len(encoded)} bytes in {len(chunks)} chunk(s) [{mode}]")

        for idx, chunk in enumerate(chunks):
            payload = bytes([idx, total, len(chunk)]) + chunk
            packet = bytes([self.CMD_CLIP]) + payload
            packet = packet.ljust(RAW_EPSIZE, b"\x00")
            write_buf = b"\x00" + packet
            log.debug("chunk %d/%d TX: %s", idx, len(chunks) - 1, self.pretty(packet))
            dev.write(write_buf)

            ack = bytes(dev.read(RAW_EPSIZE, timeout_ms))
            log.debug("chunk %d/%d RX: %s", idx, len(chunks) - 1, self.pretty(ack))
            if not ack or ack[0] != self.CMD_CLIP:
                print(f"  unexpected ack on chunk {idx}: {self.pretty(ack)}")
                return
            status = ack[2] if len(ack) > 2 else 0xFF
            if status != 0x00:
                print(f"  keyboard busy/error on chunk {idx} (status=0x{status:02X})")
                return

        print("  done — keyboard is typing the text")

    def _send_clipboard(self, dev: hid.Device, timeout_ms: int) -> None:
        """Read clipboard and push it to the keyboard as 0x03 chunks."""
        try:
            text = read_clipboard()
        except Exception as exc:
            print(f"  clipboard read failed: {exc}")
            return
        self._send_text(dev, timeout_ms, text)

    def _case_transform(self, dev: hid.Device, timeout_ms: int, transform: str) -> None:
        """Read clipboard, apply case transform, send result to keyboard to type out.

        The keyboard sent Ctrl+C before firing this button; a brief sleep lets
        the OS settle so the selection is on the clipboard when we read it.
        """
        time.sleep(0.1)
        try:
            text = read_clipboard()
        except Exception as exc:
            print(f"  clipboard read failed: {exc}", file=sys.stderr)
            return

        if transform == "upper":
            result = text.upper()
        elif transform == "lower":
            result = text.lower()
        elif transform == "sentence":
            result = to_sentence_case(text)
        else:
            print(f"  unknown transform {transform!r} (expected upper/lower/sentence)", file=sys.stderr)
            return

        self._send_text(dev, timeout_ms, result)

    def _reconnect(self, device_info: dict, retry_interval: float = 2.0) -> tuple[hid.Device, dict]:
        """Block until the device reappears, then return a new (dev, device_info) pair."""
        vid        = device_info["vendor_id"]
        pid        = device_info["product_id"]
        usage_page = device_info["usage_page"]
        usage      = device_info["usage"]
        print(f"Keyboard disconnected. Waiting to reconnect (VID=0x{vid:04X} PID=0x{pid:04X})…")
        while True:
            time.sleep(retry_interval)
            new_info = find_device(vid, pid, usage_page, usage)
            if new_info is None:
                continue
            try:
                dev = hid.Device(path=new_info["path"])
                path_str = (new_info["path"].decode(errors="replace")
                            if isinstance(new_info["path"], bytes)
                            else new_info["path"])
                print(f"Reconnected on {path_str}")
                return dev, new_info
            except Exception as exc:
                log.debug("Reconnect attempt failed: %s", exc)

    def run(self, device_info: dict, timeout_ms: int) -> None:
        path = device_info["path"]
        path_str = path.decode(errors="replace") if isinstance(path, bytes) else path
        print(f"Listening on {path_str} (Ctrl+C to stop)…")

        dev = hid.Device(path=path)
        try:
            while True:
                try:
                    # Poll with a short timeout so KeyboardInterrupt stays responsive
                    data = dev.read(RAW_EPSIZE, 200)
                    if not data:
                        continue
                    data = bytes(data)
                    log.debug("RX: %s", self.pretty(data))
                    if data[0] == self.CMD_CLIP_REQ:
                        print("Keyboard requested clipboard")
                        self._send_clipboard(dev, timeout_ms)
                    elif data[0] == self.CMD_BUTTON:
                        btn_id = str(data[1] | (data[2] << 8))
                        entry = self._button_config.get(btn_id)
                        if entry:
                            action = entry.get("action")
                            if action == "shell":
                                cmd = entry.get("command", "")
                                print(f"[btn {btn_id}] running: {cmd}", file=sys.stderr)
                                subprocess.Popen(cmd, shell=True)
                            elif action == "case-transform":
                                transform = entry.get("transform", "")
                                print(f"[btn {btn_id}] case-transform: {transform}", file=sys.stderr)
                                self._case_transform(dev, timeout_ms, transform)
                            else:
                                print(f"[btn {btn_id}] unknown action: {action}", file=sys.stderr)
                        else:
                            print(f"[btn {btn_id}] no config entry", file=sys.stderr)
                except KeyboardInterrupt:
                    raise
                except Exception as exc:
                    log.debug("HID error: %s", exc)
                    try:
                        dev.close()
                    except Exception:
                        pass
                    dev, device_info = self._reconnect(device_info)
        except KeyboardInterrupt:
            print("\nStopped.")
        finally:
            try:
                dev.close()
            except Exception:
                pass


# ---------------------------------------------------------------------------
# Command registry  {name -> factory}
# ---------------------------------------------------------------------------

COMMANDS: dict[str, type[Command]] = {
    PingCommand.name: PingCommand,
    EchoCommand.name: EchoCommand,
    RawCommand.name: RawCommand,
    TypeClipboardCommand.name: TypeClipboardCommand,
    ListenCommand.name: ListenCommand,
}


# ---------------------------------------------------------------------------
# Device helpers
# ---------------------------------------------------------------------------

def find_device(
    vid: int,
    pid: int,
    usage_page: int = DEFAULT_USAGE_PAGE,
    usage: int = DEFAULT_USAGE,
) -> hid.Device | None:
    """Return the first matching HID device, or None."""
    log.debug(
        "Enumerating HID devices (looking for VID=0x%04X PID=0x%04X "
        "usage_page=0x%04X usage=0x%02X)",
        vid, pid, usage_page, usage,
    )
    all_devices = hid.enumerate()
    log.debug("Found %d HID device(s) total", len(all_devices))
    for info in all_devices:
        log.debug(
            "  VID=0x%04X PID=0x%04X usage_page=0x%04X usage=0x%02X path=%s  %r",
            info["vendor_id"], info["product_id"],
            info["usage_page"], info["usage"],
            info.get("path", b"").decode(errors="replace"),
            info.get("product_string", ""),
        )
        if (
            info["vendor_id"] == vid
            and info["product_id"] == pid
            and info["usage_page"] == usage_page
            and info["usage"] == usage
        ):
            log.debug("  --> matched: %s", info.get("path", b"").decode(errors="replace"))
            return info
    log.debug("No matching device found.")
    return None


def list_devices() -> None:
    """Print all connected HID devices."""
    devices = hid.enumerate()
    if not devices:
        print("No HID devices found.")
        return
    print(f"{'VID':>6}  {'PID':>6}  {'Usage Page':>12}  {'Usage':>8}  Product")
    print("-" * 70)
    for d in devices:
        print(
            f"0x{d['vendor_id']:04X}  0x{d['product_id']:04X}"
            f"  0x{d['usage_page']:08X}  0x{d['usage']:04X}"
            f"  {d['product_string']}"
        )


# ---------------------------------------------------------------------------
# HID transport
# ---------------------------------------------------------------------------

def send_and_receive(
    device_info: dict,
    packet: bytes,
    timeout_ms: int = 1000,
) -> bytes:
    """Open device, send packet, read response, close device."""
    path = device_info["path"]
    path_str = path.decode(errors="replace") if isinstance(path, bytes) else path
    log.debug("hid.Device(path=%s)", path_str)
    try:
        dev = hid.Device(path=path)
    except Exception as exc:
        log.debug("hid.Device() raised %s: %s", type(exc).__name__, exc)
        raise

    try:
        dev.nonblocking = False
        write_buf = b"\x00" + packet
        log.debug("dev.write() %d bytes (incl. report-ID prefix): %s",
                  len(write_buf), " ".join(f"{b:02X}" for b in write_buf))
        # HID write requires a leading 0x00 report-ID byte on most platforms
        try:
            n = dev.write(write_buf)
            log.debug("dev.write() returned %s", n)
        except Exception as exc:
            log.debug("dev.write() raised %s: %s  (hid error: %s)",
                      type(exc).__name__, exc, getattr(dev, "error", "n/a"))
            raise

        log.debug("dev.read() up to %d bytes, timeout=%d ms", RAW_EPSIZE, timeout_ms)
        try:
            response = dev.read(RAW_EPSIZE, timeout_ms)
        except Exception as exc:
            log.debug("dev.read() raised %s: %s  (hid error: %s)",
                      type(exc).__name__, exc, getattr(dev, "error", "n/a"))
            raise

        log.debug("dev.read() returned %d byte(s): %s",
                  len(response) if response else 0,
                  " ".join(f"{b:02X}" for b in response) if response else "<empty>")
        if not response:
            raise TimeoutError("No response from keyboard (timeout).")
        return bytes(response)
    finally:
        log.debug("dev.close()")
        dev.close()


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="qmk_hid_tool",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--vid", type=lambda x: int(x, 0), required=False,
                        help="USB Vendor ID (hex or decimal, e.g. 0x4B42)")
    parser.add_argument("--pid", type=lambda x: int(x, 0), required=False,
                        help="USB Product ID (hex or decimal, e.g. 0x6061)")
    parser.add_argument("--usage-page", type=lambda x: int(x, 0),
                        default=DEFAULT_USAGE_PAGE,
                        help=f"HID Usage Page (default: 0x{DEFAULT_USAGE_PAGE:04X})")
    parser.add_argument("--usage", type=lambda x: int(x, 0),
                        default=DEFAULT_USAGE,
                        help=f"HID Usage (default: 0x{DEFAULT_USAGE:02X})")
    parser.add_argument("--timeout", type=int, default=1000,
                        help="Read timeout in ms (default: 1000)")
    parser.add_argument("--list-devices", action="store_true",
                        help="List all connected HID devices and exit.")
    parser.add_argument("--debug", action="store_true",
                        help="Enable verbose debug logging to stderr.")

    subparsers = parser.add_subparsers(dest="command", metavar="COMMAND")

    # ping
    subparsers.add_parser(PingCommand.name, help=PingCommand.description)

    # echo
    echo_p = subparsers.add_parser(EchoCommand.name, help=EchoCommand.description)
    echo_p.add_argument("text", help="Text to echo")

    # raw
    raw_p = subparsers.add_parser(RawCommand.name, help=RawCommand.description)
    raw_p.add_argument("bytes", nargs="+", metavar="HEX",
                       help="Hex bytes to send, e.g. 01 02 03")

    # type-clipboard
    tc_p = subparsers.add_parser(
        TypeClipboardCommand.name,
        help=TypeClipboardCommand.description,
    )
    tc_p.add_argument(
        "--text", default=None,
        help="Send this text instead of reading from the clipboard.",
    )
    tc_p.add_argument(
        "--delay", type=int, default=0, metavar="MS",
        help="Delay in milliseconds between chunks (default: 0).",
    )

    # listen (daemon)
    listen_p = subparsers.add_parser(ListenCommand.name, help=ListenCommand.description)
    listen_p.add_argument(
        "--config", default="~/.config/qmk_buttons.json", metavar="PATH",
        help="Path to button action config JSON (default: ~/.config/qmk_buttons.json)",
    )

    return parser


def build_command(args: argparse.Namespace) -> Command:
    if args.command == "ping":
        return PingCommand()
    if args.command == "echo":
        return EchoCommand(args.text)
    if args.command == "raw":
        return RawCommand(args.bytes)
    if args.command == "type-clipboard":
        return TypeClipboardCommand(text=args.text, delay_ms=args.delay)
    if args.command == "listen":
        return ListenCommand(config_path=args.config)
    raise ValueError(f"Unknown command: {args.command!r}")


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.debug else logging.WARNING,
        format="%(levelname)s %(name)s: %(message)s",
        stream=sys.stderr,
    )

    if args.debug:
        # Ask the hidapi C library to emit its own debug output to stderr.
        # This works on hidapi >= 0.13 (hid.set_debug_output) and is a no-op
        # on older builds where the attribute doesn't exist.
        try:
            hid.set_debug_output(True)
            log.debug("hidapi native debug output enabled via hid.set_debug_output()")
        except AttributeError:
            log.debug(
                "hid.set_debug_output() not available on this hidapi version; "
                "set LIBUSB_DEBUG=4 in the environment for lower-level USB traces."
            )

    if args.list_devices:
        list_devices()
        return

    if not args.command:
        parser.print_help()
        return

    if not args.vid or not args.pid:
        parser.error("--vid and --pid are required to send a command.")

    device_info = find_device(args.vid, args.pid, args.usage_page, args.usage)
    if device_info is None:
        sys.exit(
            f"Device VID=0x{args.vid:04X} PID=0x{args.pid:04X} not found "
            f"(usage_page=0x{args.usage_page:04X} usage=0x{args.usage:02X}).\n"
            "Run with --list-devices to see connected devices."
        )

    cmd = build_command(args)
    cmd.run(device_info, args.timeout)


if __name__ == "__main__":
    main()
