// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2025 qmk_hid_tool contributors

#include "hid_clipboard.h"
#include "raw_hid.h"
#include "debug.h"
#include <string.h>

/* -- Internal state -------------------------------------------------------- */

static char     hid_clip_buf[HID_CLIP_MAX_LEN + 1];
static uint16_t hid_clip_len    = 0;
static bool     hid_clip_ready  = false;  /* set when last chunk received   */
static bool     hid_clip_typing = false;  /* guard against re-entrant sends */
static bool     hid_clip_cancelled = false; /* set by KC_ESC to abort transfer */

/* Streaming state (total_chunks == HID_CLIP_STREAM_SENTINEL path) */
static char    hid_clip_stream_buf[HID_CLIP_DATA_SIZE + 1]; /* one chunk + NUL */
static uint8_t hid_clip_stream_len     = 0;
static uint8_t hid_clip_stream_ack_idx = 0;   /* chunk_index to echo in deferred ACK */
static bool    hid_clip_stream_ready   = false; /* chunk staged, waiting for housekeeping */

/* Button repeat state */
static uint64_t hid_btn_held_mask    = 0;     /* bit (btn_id-1) set while key held */
static uint32_t hid_btn_repeat_timer = 0;
static bool     hid_btn_in_delay     = false; /* true = waiting for initial delay */

/* -- Public API ------------------------------------------------------------ */

const char *hid_clipboard_get_text(void) { return hid_clip_buf; }
uint16_t    hid_clipboard_get_len(void)  { return hid_clip_len; }

/* -- raw_hid_receive hook -------------------------------------------------
 *
 * Called from raw_hid_receive_user() in keymap.c for every incoming Raw HID
 * packet.  Handles ping (0x01), echo (0x02), and type-clipboard (0x03).
 *
 * Runs in the USB task context — do NOT call send_string() here.
 */
void raw_hid_receive_hid_clipboard(uint8_t *data, uint8_t length) {
    uint8_t response[RAW_EPSIZE] = {0};

    dprintf("hid_clipboard: rx cmd=0x%02X len=%u\n", data[0], length);

    switch (data[0]) {

    case HID_PING_CMD:
        dprintf("hid_clipboard: ping\n");
        response[0] = HID_PING_CMD;
        raw_hid_send(response, RAW_EPSIZE);
        return;

    case HID_ECHO_CMD:
        dprintf("hid_clipboard: echo\n");
        response[0] = HID_ECHO_CMD;
        memcpy(response + 1, data + 1, RAW_EPSIZE - 1);
        raw_hid_send(response, RAW_EPSIZE);
        return;

    case HID_CLIP_CMD:
        break;  /* handled below */

    default:
        dprintf("hid_clipboard: unknown cmd=0x%02X, ignoring\n", data[0]);
        return;
    }

    uint8_t chunk_index = data[1];
    uint8_t total       = data[2];
    uint8_t data_len    = data[3];

    response[0] = HID_CLIP_CMD;
    response[1] = chunk_index;

    dprintf("hid_clipboard: clip chunk=%u/%u data_len=%u\n", chunk_index, total, data_len);

    if (total == HID_CLIP_STREAM_SENTINEL) {
        /* -- Streaming path: stage chunk, defer ACK until after typing -- */
        if (hid_clip_stream_ready || hid_clip_typing) {
            dprintf("hid_clipboard: stream busy, rejecting chunk %u\n", chunk_index);
            response[2] = HID_CLIP_STATUS_BUSY;
            raw_hid_send(response, RAW_EPSIZE);
            return;
        }
        if (data_len > HID_CLIP_DATA_SIZE) {
            dprintf("hid_clipboard: stream data_len %u clamped to %u\n", data_len, HID_CLIP_DATA_SIZE);
            data_len = HID_CLIP_DATA_SIZE;
        }
        memcpy(hid_clip_stream_buf, data + HID_CLIP_DATA_OFF, data_len);
        hid_clip_stream_len     = data_len;
        hid_clip_stream_ack_idx = chunk_index;
        hid_clip_stream_ready   = true;
        /* ACK sent after typing in housekeeping_task_hid_clipboard() */
        dprintf("hid_clipboard: stream chunk %u staged (%u bytes), ACK deferred\n", chunk_index, data_len);
        return;
    }

    /* -- Buffered path (total > 0) -- */

    /* Reject while typing to avoid corrupting the buffer mid-send */
    if (hid_clip_typing) {
        dprintf("hid_clipboard: busy, rejecting chunk %u\n", chunk_index);
        response[2] = HID_CLIP_STATUS_BUSY;
        raw_hid_send(response, RAW_EPSIZE);
        return;
    }

    /* Reset buffer on the first chunk of a new transfer */
    if (chunk_index == 0) {
        dprintf("hid_clipboard: new transfer, resetting buffer\n");
        hid_clip_len       = 0;
        hid_clip_ready     = false;
        hid_clip_cancelled = false;
        memset(hid_clip_buf, 0, sizeof(hid_clip_buf));
    }

    /* Clamp and append */
    if (data_len > HID_CLIP_DATA_SIZE) {
        dprintf("hid_clipboard: data_len %u clamped to %u\n", data_len, HID_CLIP_DATA_SIZE);
        data_len = HID_CLIP_DATA_SIZE;
    }
    if (hid_clip_len + data_len <= HID_CLIP_MAX_LEN) {
        memcpy(hid_clip_buf + hid_clip_len, data + HID_CLIP_DATA_OFF, data_len);
        hid_clip_len += data_len;
        dprintf("hid_clipboard: buf_len now %u\n", hid_clip_len);
    } else {
        dprintf("hid_clipboard: buffer full, dropping %u bytes\n", data_len);
    }

    response[2] = HID_CLIP_STATUS_OK;
    raw_hid_send(response, RAW_EPSIZE);

    /* Mark ready when the last chunk of the transfer is received */
    if (chunk_index == (uint8_t)(total - 1)) {
        hid_clip_buf[hid_clip_len] = '\0';
        hid_clip_ready = true;
        dprintf("hid_clipboard: transfer complete, %u bytes ready\n", hid_clip_len);
    }
}

/* -- housekeeping_task hook -----------------------------------------------
 *
 * Called from housekeeping_task_user() in keymap.c (safe to call send_string).
 * Types out the buffered text once hid_clip_ready is signalled.
 */
void housekeeping_task_hid_clipboard(void) {
    /* -- Button repeat -- */
    if (hid_btn_held_mask != 0) {
        uint32_t threshold = hid_btn_in_delay
                             ? HID_BTN_REPEAT_DELAY_MS
                             : HID_BTN_REPEAT_RATE_MS;
        if (timer_elapsed32(hid_btn_repeat_timer) >= threshold) {
            hid_btn_repeat_timer = timer_read32();
            hid_btn_in_delay     = false;
            for (uint8_t i = 0; i < 64; i++) {
                if (hid_btn_held_mask & ((uint64_t)1 << i)) {
                    uint8_t pkt[RAW_EPSIZE] = {0};
                    pkt[0] = HID_CMD_BUTTON;
                    pkt[1] = i + 1;
                    raw_hid_send(pkt, RAW_EPSIZE);
                }
            }
        }
    }

    /* -- Streaming path: type chunk, then send deferred ACK -- */
    if (hid_clip_stream_ready && !hid_clip_typing) {
        /*
         * Cancel check BEFORE typing.
         *
         * send_string() busy-waits internally, so the matrix is not scanned
         * while it runs.  An ESC pressed during chunk N's send_string() is
         * only picked up by process_record in the next keyboard_task()
         * iteration — which runs before housekeeping, so hid_clip_cancelled
         * will be true here when chunk N+1 is about to be typed.  Checking
         * after send_string() is always one cycle too early to be useful.
         */
        if (hid_clip_cancelled) {
            dprintf("hid_clipboard: cancelled by ESC, rejecting chunk %u\n", hid_clip_stream_ack_idx);
            hid_clip_cancelled    = false;
            hid_clip_stream_ready = false;
            uint8_t nack[RAW_EPSIZE] = {0};
            nack[0] = HID_CLIP_CMD;
            nack[1] = hid_clip_stream_ack_idx;
            nack[2] = HID_CLIP_STATUS_BUSY;
            raw_hid_send(nack, RAW_EPSIZE);
            return;
        }

        hid_clip_stream_buf[hid_clip_stream_len] = '\0';
        hid_clip_stream_ready = false;
        hid_clip_typing       = true;
        dprintf("hid_clipboard: stream typing %u bytes\n", hid_clip_stream_len);
        send_string(hid_clip_stream_buf);
        dprintf("hid_clipboard: stream typing done\n");
#if HID_CLIP_INTER_CHUNK_DELAY_MS > 0
        wait_ms(HID_CLIP_INTER_CHUNK_DELAY_MS);
#endif
        hid_clip_typing = false;

        uint8_t ack[RAW_EPSIZE] = {0};
        ack[0] = HID_CLIP_CMD;
        ack[1] = hid_clip_stream_ack_idx;
        ack[2] = HID_CLIP_STATUS_OK;
        raw_hid_send(ack, RAW_EPSIZE);
        dprintf("hid_clipboard: stream ACK sent for chunk %u\n", hid_clip_stream_ack_idx);
        return;  /* one action per housekeeping call */
    }

    /* -- Buffered path -- */
    if (hid_clip_ready && !hid_clip_typing) {
        if (hid_clip_cancelled) {
            dprintf("hid_clipboard: cancelled by ESC before buffered typing\n");
            hid_clip_cancelled = false;
            hid_clip_ready     = false;
            return;
        }
        dprintf("hid_clipboard: typing %u bytes\n", hid_clip_len);
        hid_clip_ready  = false;
        hid_clip_typing = true;
        send_string(hid_clip_buf);
        hid_clip_typing = false;
        dprintf("hid_clipboard: typing done\n");
    }
}

/* -- process_record hook --------------------------------------------------
 *
 * KC_TYPE_CLIP (alias TYPCLIP) — sends a 0x04 clipboard request to the host.
 * The host daemon (listen mode) responds with 0x03 chunks; housekeeping_task
 * then types them out.  Returns false when handled, true otherwise.
 */
bool process_record_hid_clipboard(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= KC_HID_BTN_1 && keycode < HID_CLIPBOARD_SAFE_RANGE) {
        uint8_t  btn_id  = keycode - KC_HID_BTN_1 + 1;  /* 1-based */
        uint64_t btn_bit = (uint64_t)1 << (btn_id - 1);

        if (record->event.pressed) {
            uint8_t pkt[RAW_EPSIZE] = {0};
            pkt[0] = HID_CMD_BUTTON;
            pkt[1] = btn_id;
            dprintf("hid_clipboard: button %u pressed, sending HID_CMD_BUTTON\n", btn_id);
            raw_hid_send(pkt, RAW_EPSIZE);

            if (HID_BTN_REPEAT_MASK & btn_bit) {
                hid_btn_held_mask   |= btn_bit;
                hid_btn_repeat_timer = timer_read32();
                hid_btn_in_delay     = true;
            }
        } else {
            hid_btn_held_mask &= ~btn_bit;
        }
        return false;
    }

    if (keycode == KC_TYPE_CLIP && record->event.pressed) {
        dprintf("hid_clipboard: KC_TYPE_CLIP pressed, requesting clipboard from host\n");
        if (!hid_clip_typing) {
            hid_clip_cancelled = false;  /* clear any stale cancel before new transfer */
            uint8_t request[RAW_EPSIZE] = {0};
            request[0] = HID_CLIP_REQ_CMD;
            raw_hid_send(request, RAW_EPSIZE);
        }
        return false;
    }

    if (keycode == KC_ESC && record->event.pressed) {
        // if (hid_clip_typing || hid_clip_stream_ready || hid_clip_ready) {
            dprintf("hid_clipboard: ESC pressed, cancelling clipboard transfer\n");
            hid_clip_cancelled = true;
            /* pass the keypress through so ESC still works in the active app */
        // }
    }

    return true;
}
