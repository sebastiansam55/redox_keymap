// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2025 qmk_hid_tool contributors
#pragma once

#include "quantum.h"
#include "raw_hid.h"

/* RAW_EPSIZE may not be visible at header inclusion time; provide the default. */
#ifndef RAW_EPSIZE
#  define RAW_EPSIZE 32
#endif

/*
 * hid_clipboard — direct QMK integration (no community module)
 * ─────────────────────────────────────────────────────────────
 * Receives clipboard text from the host tool (qmk_hid_tool type-clipboard)
 * over Raw HID and replays it as real keystrokes.
 *
 * WIRING (keymap.c):
 *
 *   void raw_hid_receive(uint8_t *data, uint8_t length) {
 *       raw_hid_receive_hid_clipboard(data, length);
 *   }
 *
 *   void housekeeping_task_user(void) {
 *       housekeeping_task_hid_clipboard();
 *   }
 *
 *   bool process_record_user(uint16_t keycode, keyrecord_t *record) {
 *       if (!process_record_hid_clipboard(keycode, record)) return false;
 *       // ... rest of your process_record_user
 *   }
 *
 * PROTOCOL
 * ────────
 * Ping (0x01) — host → keyboard: [0x01, 0x00...]
 *               keyboard → host: [0x01, 0x00...]
 *
 * Echo (0x02) — host → keyboard: [0x02, <payload bytes 1..31>]
 *               keyboard → host: [0x02, <same payload bytes 1..31>]
 *
 * Type-clipboard (0x03) — host → keyboard packet:
 *   [0]     0x03
 *   [1]     chunk_index  (0-based uint8)
 *   [2]     total_chunks (uint8)
 *               > 0 : buffered mode — keyboard accumulates all chunks then types at once
 *               = 0 : streaming mode — keyboard types each chunk immediately (no size limit)
 *   [3]     data_len     (number of valid bytes in [4..])
 *   [4..31] text data    (up to 28 bytes of ASCII)
 *
 *   In streaming mode the ACK is sent AFTER typing completes (deferred),
 *   providing natural back-pressure so the host cannot outrun the keyboard.
 *
 *   ACK (keyboard → host):
 *   [0]  0x03
 *   [1]  chunk_index (echoed)
 *   [2]  0x00 = OK, 0x01 = busy/error
 *   [3..31] zeroes
 *
 * Clipboard request (0x04) — keyboard → host: [0x04, 0x00...]
 *   Sent by the keyboard when TYPCLIP is pressed.
 *   Host daemon (listen mode) responds by sending 0x03 chunks.
 */

/* Command IDs — keep in sync with CMD_ID constants in qmk_hid_tool.py */
#define HID_PING_CMD        0x01  /* host→kb: ping                          */
#define HID_ECHO_CMD        0x02  /* host→kb: echo payload back             */
#define HID_CLIP_CMD        0x03  /* host→kb: chunked clipboard text        */
#define HID_CLIP_REQ_CMD    0x04  /* kb→host: request clipboard from host   */
#define HID_CMD_BUTTON      0x05  /* kb→host: programmable button press     */

/* Chunk geometry */
#define HID_CLIP_DATA_OFF   4
#define HID_CLIP_DATA_SIZE  (RAW_EPSIZE - HID_CLIP_DATA_OFF)  /* 28 bytes */
#define HID_CLIP_MAX_CHUNKS 18
#define HID_CLIP_MAX_LEN    (HID_CLIP_MAX_CHUNKS * HID_CLIP_DATA_SIZE)  /* 504 bytes */

/* total_chunks == 0 selects streaming mode (no size limit, ACK deferred until typed) */
#define HID_CLIP_STREAM_SENTINEL  0

/* ACK status byte values */
#define HID_CLIP_STATUS_OK        0x00
#define HID_CLIP_STATUS_BUSY      0x01

/*
 * Inter-chunk delay (streaming mode only).
 * A pause inserted after send_string() and before the deferred ACK is sent,
 * which prevents the host from delivering the next chunk until the OS has
 * settled from the previous one.  Helps with shifted characters on slow hosts.
 * Override in config.h: #define HID_CLIP_INTER_CHUNK_DELAY_MS 50
 */
#ifndef HID_CLIP_INTER_CHUNK_DELAY_MS
#  define HID_CLIP_INTER_CHUNK_DELAY_MS 0
#endif

/* Custom keycodes — chain from SAFE_RANGE so keymap.c can use HID_CLIPBOARD_SAFE_RANGE */
enum hid_clipboard_keycodes {
    KC_TYPE_CLIP = SAFE_RANGE,
    KC_HID_BTN_1,   /* programmable button 1 */
    KC_HID_BTN_2,
    KC_HID_BTN_3,
    KC_HID_BTN_4,
    KC_HID_BTN_5,
    KC_HID_BTN_6,
    KC_HID_BTN_7,
    KC_HID_BTN_8,
    KC_HID_BTN_9,
    KC_HID_BTN_10,
    KC_HID_BTN_11,
    KC_HID_BTN_12,
    KC_HID_BTN_13,
    KC_HID_BTN_14,
    KC_HID_BTN_15,
    KC_HID_BTN_16,
    HID_CLIPBOARD_SAFE_RANGE   /* keymap.c chains its custom codes here */
};
#define TYPCLIP KC_TYPE_CLIP

/* Public API */
const char    *hid_clipboard_get_text(void);
uint16_t       hid_clipboard_get_len(void);

/* Call these from your keymap.c hooks (see WIRING above) */
void raw_hid_receive_hid_clipboard(uint8_t *data, uint8_t length);
void housekeeping_task_hid_clipboard(void);
bool process_record_hid_clipboard(uint16_t keycode, keyrecord_t *record);
