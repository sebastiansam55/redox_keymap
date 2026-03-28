/*
Copyright 2018 Mattia Dal Ben <matthewdibi@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H
#include "hid_clipboard.h"
#include "print.h"
#include "digitizer.h"
#include "os_detection.h"

// Private custom keycodes — injected from private/keycodes.inc at build time.
// To add your own: create private/keycodes.inc with an enum custom_keycodes block
// starting from HID_CLIPBOARD_SAFE_RANGE.
enum custom_keycodes {
    WINTEMP = HID_CLIPBOARD_SAFE_RANGE,
    WINLOCAL,
    WORK1,
    COPYLINE,
    WRAPQU,   /* '' unshifted, "" shifted  */
    WRAPBRF,  /* [] unshifted, {} shifted  */
    WRAPPR,   /* ()                        */
    WRAPTK,   /* ``                        */
    WRAPAG,   /* <>                        */
    SONG_NEXT, /* cycle through user_song_list songs */
    PRINT_WPM, /* print current WPM to QMK console */
    WARP_CTR,  /* warp cursor to center of screen via digitizer */
    MPLY_OS,   /* KC_MPLY on Windows, HID_PLY on Linux */
    MPRV_OS,   /* KC_MPRV on Windows, HID_PRV on Linux */
    MNXT_OS,   /* KC_MNXT on Windows, HID_NXT on Linux */
    VOLD_OS,   /* KC_VOLD on Windows, HID_VLD on Linux */
    VOLU_OS,   /* KC_VOLU on Windows, HID_VLU on Linux */
// %%PRIVATE_KEYCODES%%
};

// Song arrays for cycling — order matches user_song_list.h top-to-bottom
float song_list_imperial_march[][2]        = SONG(IMPERIAL_MARCH);
float song_list_coin_sound[][2]            = SONG(COIN_SOUND);
float song_list_one_up_sound[][2]          = SONG(ONE_UP_SOUND);
float song_list_sonic_ring[][2]            = SONG(SONIC_RING);
float song_list_zelda_puzzle[][2]          = SONG(ZELDA_PUZZLE);
float song_list_zelda_treasure[][2]        = SONG(ZELDA_TREASURE);
float song_list_overwatch_theme[][2]       = SONG(OVERWATCH_THEME);
float song_list_mario_theme[][2]           = SONG(MARIO_THEME);
float song_list_mario_gameover[][2]        = SONG(MARIO_GAMEOVER);
float song_list_mario_mushroom[][2]        = SONG(MARIO_MUSHROOM);
float song_list_e1m1_doom[][2]             = SONG(E1M1_DOOM);
float song_list_disney_song[][2]           = SONG(DISNEY_SONG);
float song_list_number_one[][2]            = SONG(NUMBER_ONE);
float song_list_cabbage_song[][2]          = SONG(CABBAGE_SONG);
float song_list_old_spice[][2]             = SONG(OLD_SPICE);
float song_list_victory_fanfare_short[][2] = SONG(VICTORY_FANFARE_SHORT);
float song_list_all_star[][2]              = SONG(ALL_STAR);
float song_list_rick_roll[][2]             = SONG(RICK_ROLL);
float song_list_ff_prelude[][2]            = SONG(FF_PRELUDE);
float song_list_to_boldly_go[][2]          = SONG(TO_BOLDLY_GO);
float song_list_kataware_doki[][2]         = SONG(KATAWARE_DOKI);
float song_list_megalovania[][2]           = SONG(MEGALOVANIA);
float song_list_michishirube[][2]          = SONG(MICHISHIRUBE);
float song_list_liebesleid[][2]            = SONG(LIEBESLEID);
float song_list_melodies_of_life[][2]      = SONG(MELODIES_OF_LIFE);
float song_list_eyes_on_me[][2]            = SONG(EYES_ON_ME);
float song_list_song_of_the_ancients[][2]  = SONG(SONG_OF_THE_ANCIENTS);
float song_list_nier_amusement_park[][2]   = SONG(NIER_AMUSEMENT_PARK);
float song_list_copied_city[][2]           = SONG(COPIED_CITY);
float song_list_vague_hope_cold_rain[][2]  = SONG(VAGUE_HOPE_COLD_RAIN);
float song_list_kaine_salvation[][2]       = SONG(KAINE_SALVATION);
float song_list_weight_of_the_world[][2]   = SONG(WEIGHT_OF_THE_WORLD);
float song_list_isabellas_lullaby[][2]     = SONG(ISABELLAS_LULLABY);

#define NUM_USER_SONGS 33
static uint8_t song_cycle_index = 0;

enum tap_dance_codes {
    TD_ESC,
};

tap_dance_action_t tap_dance_actions[] = {
    [TD_ESC] = ACTION_TAP_DANCE_DOUBLE(KC_NO, KC_ESC),
};

enum layers {
   _DVORAK,
   _SYMB,
   _COMBO,
   _ADJUST,
   _FN1,
   _FN2,
   _CMB_GUI
};

// Shortcut to make keymap more readable
#define SYM_L   MO(_SYMB)

// HID daemon button aliases — send 0x05 packets to the Python listen daemon.
// Distinct from QMK's built-in PB_1..PB_32 (QK_PROGRAMMABLE_BUTTON_N) which
// generate OS-level HID button reports on usage page 0x0C.
#define CB_PRV  KC_HID_BTN_1  // previous clipboard history entry
#define CB_NXT  KC_HID_BTN_2  // next clipboard history entry
#define MIC_TOG KC_HID_BTN_3  // toggle mic mute
#define ASHOT   KC_HID_BTN_4  // area screenshot (draw zone)
#define HID_PLY KC_HID_BTN_5  // media play/pause toggle
#define CASE_UP KC_HID_BTN_6  // case-transform: UPPERCASE
#define CASE_LO KC_HID_BTN_7  // case-transform: lowercase
#define CASE_SN KC_HID_BTN_8  // case-transform: Sentence case
#define HID_PRV KC_HID_BTN_9  // media previous track (Linux)
#define HID_NXT KC_HID_BTN_10 // media next track (Linux)
#define WRP_4K  KC_HID_BTN_11 // warp monitor center 1 (4k)
#define WRP_14  KC_HID_BTN_12 // warp monitor center 2 (1440p)
#define WRP_11A KC_HID_BTN_13 // warp monitor center 3 (1080p 1)
#define WRP_11B KC_HID_BTN_14 // warp monitor center 4 (1080p 2)
#define WRC_TL  KC_HID_BTN_15 // warp current monitor: top-left corner
#define WRC_TR  KC_HID_BTN_16 // warp current monitor: top-right corner
#define WRC_BL  KC_HID_BTN_17 // warp current monitor: bottom-left corner
#define WRC_BR  KC_HID_BTN_18 // warp current monitor: bottom-right corner
#define HID_VLD KC_HID_BTN_19 // volume down (Linux)
#define HID_VLU KC_HID_BTN_20 // volume up (Linux)
#define HIDB_21 KC_HID_BTN_21 // warp active to monitor 4kmon
#define HIDB_22 KC_HID_BTN_22 // warp active to monitor 1440p
#define HIDB_23 KC_HID_BTN_23 // warp active to monitor 
#define HIDB_24 KC_HID_BTN_24 // warp active to monitor
#define HIDB_25 KC_HID_BTN_25
#define HIDB_26 KC_HID_BTN_26
#define HIDB_27 KC_HID_BTN_27
#define HIDB_28 KC_HID_BTN_28
#define HIDB_29 KC_HID_BTN_29
#define HIDB_30 KC_HID_BTN_30
#define HIDB_31 KC_HID_BTN_31 // activate google chat
#define HIDB_32 KC_HID_BTN_32 // relative move left
#define HIDB_33 KC_HID_BTN_33 // relative move down
#define HIDB_34 KC_HID_BTN_34 // relative move up
#define HIDB_35 KC_HID_BTN_35 // relavite move right
#define HIDB_36 KC_HID_BTN_36
#define HIDB_37 KC_HID_BTN_37
#define HIDB_38 KC_HID_BTN_38
#define HIDB_39 KC_HID_BTN_39
#define HIDB_40 KC_HID_BTN_40
#define HIDB_41 KC_HID_BTN_41
#define HIDB_42 KC_HID_BTN_42
#define HIDB_43 KC_HID_BTN_43
#define HIDB_44 KC_HID_BTN_44
#define HIDB_45 KC_HID_BTN_45
#define HIDB_46 KC_HID_BTN_46
#define HIDB_47 KC_HID_BTN_47
#define HIDB_48 KC_HID_BTN_48
#define HIDB_49 KC_HID_BTN_49
#define HIDB_50 KC_HID_BTN_50
#define HIDB_51 KC_HID_BTN_51
#define HIDB_52 KC_HID_BTN_52
#define HIDB_53 KC_HID_BTN_53
#define HIDB_54 KC_HID_BTN_54
#define HIDB_55 KC_HID_BTN_55
#define HIDB_56 KC_HID_BTN_56
#define HIDB_57 KC_HID_BTN_57
#define HIDB_58 KC_HID_BTN_58
#define HIDB_59 KC_HID_BTN_59
#define HIDB_60 KC_HID_BTN_60
#define HIDB_61 KC_HID_BTN_61
#define HIDB_62 KC_HID_BTN_62
#define HIDB_63 KC_HID_BTN_63
#define HIDB_64 KC_HID_BTN_64

#define KC_ALAS LALT_T(KC_PAST)
#define KC_CTPL LCTL_T(KC_BSLS)

#define KC_NAGR LT(_COMBO, KC_GRV)
#define KC_NAMI LT(_COMBO, KC_MINS)

#define KC_ADEN LT(_ADJUST, KC_END)
#define KC_ADLBR LT(_ADJUST, KC_LBRC)

#define KC_CMB LT(_COMBO, KC_NO)

void raw_hid_receive(uint8_t *data, uint8_t length) {
    raw_hid_receive_hid_clipboard(data, length);
}

void housekeeping_task_user(void) {
    housekeeping_task_hid_clipboard();
}

void keyboard_post_init_user(void) {
    // Customize these values to desired behaviour
    debug_enable=true;
    // debug_matrix=true;
    // debug_keyboard=true;
    // debug_mouse=true;
    audio_on();
}


#define WRAP_SEL(open, close)   \
    PLAY_SONG(song_list_coin_sound); \
    tap_code16(C(KC_C));        \
    wait_ms(50);                \
    SEND_STRING(open);          \
    tap_code16(C(KC_V));        \
    SEND_STRING(close)

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == TYPCLIP && record->event.pressed) {
        PLAY_SONG(song_list_coin_sound);
    }
    // CASE_UP/LO/SN are case-transform buttons: copy selection first, then send button packet
    if (record->event.pressed &&
        (keycode == CASE_UP || keycode == CASE_LO || keycode == CASE_SN)) {
        PLAY_SONG(song_list_coin_sound);
        tap_code16(C(KC_C));
        wait_ms(50);
    }
    // WRP_* with shift held sends a corner warp key for the current monitor instead
    if (record->event.pressed &&
        (keycode == WRP_4K || keycode == WRP_14 || keycode == WRP_11A || keycode == WRP_11B) &&
        (get_mods() & MOD_MASK_SHIFT)) {
        uint16_t corner_key;
        switch (keycode) {
            case WRP_4K:  corner_key = WRC_TL; break;
            case WRP_14:  corner_key = WRC_TR; break;
            case WRP_11A: corner_key = WRC_BL; break;
            case WRP_11B: corner_key = WRC_BR; break;
            default:      return true;
        }
        process_record_hid_clipboard(corner_key, record);
        return false;
    }
    if (!process_record_hid_clipboard(keycode, record)) return false;
    switch (keycode) {
        case WINTEMP:
            if (record->event.pressed) {
                PLAY_SONG(song_list_coin_sound);
                SEND_STRING("\%temp\%");
            }
            break;
        case WINLOCAL:
            if (record->event.pressed) {
                PLAY_SONG(song_list_coin_sound);
                SEND_STRING("\%localappdata\%");
            }
            break;
        case WORK2:
            if (record -> event.pressed) {
                PLAY_SONG(song_list_coin_sound);
                SEND_STRING("C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319");
                wait_ms(100);
                SEND_STRING("\\Temporary ASP.NET Files");
            }
            break;
        case COPYLINE:
            if(record -> event.pressed){
                PLAY_SONG(song_list_coin_sound);
                tap_code16(KC_HOME);
                wait_ms(10);
                register_code(KC_LSFT);
                wait_ms(10);
                tap_code16(KC_END);
                wait_ms(10);
                unregister_code(KC_LSFT);
                wait_ms(10);
                tap_code16(C(KC_C));
                wait_ms(10);
                tap_code16(KC_RGHT);
            }
            break;
        case WRAPQU:
            if (record->event.pressed) {
                PLAY_SONG(song_list_coin_sound);
                bool shifted = get_mods() & MOD_MASK_SHIFT;
                del_mods(MOD_MASK_SHIFT);
                if (shifted) { WRAP_SEL("\"", "\""); }
                else         { WRAP_SEL("'",  "'");  }
            }
            return false;
        case WRAPBRF:
            if (record->event.pressed) {
                PLAY_SONG(song_list_coin_sound);
                bool shifted = get_mods() & MOD_MASK_SHIFT;
                del_mods(MOD_MASK_SHIFT);
                if (shifted) { WRAP_SEL("{", "}"); }
                else         { WRAP_SEL("[", "]"); }
            }
            return false;
        case WRAPPR: 
            if (record->event.pressed) { 
                PLAY_SONG(song_list_coin_sound);
                WRAP_SEL("(", ")");
            }
            return false;
        case WRAPTK: 
            if (record->event.pressed) {
                PLAY_SONG(song_list_coin_sound);
                WRAP_SEL("`", "`");
            }
            return false;
        case WRAPAG:
            if (record->event.pressed) {
                PLAY_SONG(song_list_coin_sound);
                WRAP_SEL("<", ">");
            }
            return false;
        case SONG_NEXT:
            if (record->event.pressed) {
                switch (song_cycle_index) {
                    case  0: PLAY_SONG(song_list_imperial_march);        break;
                    case  1: PLAY_SONG(song_list_coin_sound);            break;
                    case  2: PLAY_SONG(song_list_one_up_sound);          break;
                    case  3: PLAY_SONG(song_list_sonic_ring);            break;
                    case  4: PLAY_SONG(song_list_zelda_puzzle);          break;
                    case  5: PLAY_SONG(song_list_zelda_treasure);        break;
                    case  6: PLAY_SONG(song_list_overwatch_theme);       break;
                    case  7: PLAY_SONG(song_list_mario_theme);           break;
                    case  8: PLAY_SONG(song_list_mario_gameover);        break;
                    case  9: PLAY_SONG(song_list_mario_mushroom);        break;
                    case 10: PLAY_SONG(song_list_e1m1_doom);             break;
                    case 11: PLAY_SONG(song_list_disney_song);           break;
                    case 12: PLAY_SONG(song_list_number_one);            break;
                    case 13: PLAY_SONG(song_list_cabbage_song);          break;
                    case 14: PLAY_SONG(song_list_old_spice);             break;
                    case 15: PLAY_SONG(song_list_victory_fanfare_short); break;
                    case 16: PLAY_SONG(song_list_all_star);              break;
                    case 17: PLAY_SONG(song_list_rick_roll);             break;
                    case 18: PLAY_SONG(song_list_ff_prelude);            break;
                    case 19: PLAY_SONG(song_list_to_boldly_go);          break;
                    case 20: PLAY_SONG(song_list_kataware_doki);         break;
                    case 21: PLAY_SONG(song_list_megalovania);           break;
                    case 22: PLAY_SONG(song_list_michishirube);          break;
                    case 23: PLAY_SONG(song_list_liebesleid);            break;
                    case 24: PLAY_SONG(song_list_melodies_of_life);      break;
                    case 25: PLAY_SONG(song_list_eyes_on_me);            break;
                    case 26: PLAY_SONG(song_list_song_of_the_ancients);  break;
                    case 27: PLAY_SONG(song_list_nier_amusement_park);   break;
                    case 28: PLAY_SONG(song_list_copied_city);           break;
                    case 29: PLAY_SONG(song_list_vague_hope_cold_rain);  break;
                    case 30: PLAY_SONG(song_list_kaine_salvation);       break;
                    case 31: PLAY_SONG(song_list_weight_of_the_world);   break;
                    case 32: PLAY_SONG(song_list_isabellas_lullaby);     break;
                }
                song_cycle_index = (song_cycle_index + 1) % NUM_USER_SONGS;
            }
            return false;
        case WARP_CTR:
            if (record->event.pressed) {
                PLAY_SONG(song_list_sonic_ring);
                digitizer_set_position(0.5, 0.5);
            }
            break;
        case PRINT_WPM:
            if (record->event.pressed) {
                PLAY_SONG(song_list_coin_sound);
                uprintf("WPM: %u\n", get_current_wpm());
            }
            break;
        case MPLY_OS:
            if (record->event.pressed) {
                if (detected_host_os() == OS_LINUX) {
                    process_record_hid_clipboard(HID_PLY, record);
                } else {
                    tap_code(KC_MPLY);
                }
            }
            return false;
        case MNXT_OS:
            if (record->event.pressed) {
                if (detected_host_os() == OS_LINUX) {
                    process_record_hid_clipboard(HID_NXT, record);
                } else {
                    tap_code(KC_MNXT);
                }
            }
            return false;
        case MPRV_OS:
            if (record->event.pressed) {
                if (detected_host_os() == OS_LINUX) {
                    process_record_hid_clipboard(HID_PRV, record);
                } else {
                    tap_code(KC_MPRV);
                }
            }
            return false;
        case VOLD_OS:
            if (record->event.pressed) {
                if (detected_host_os() == OS_LINUX) {
                    process_record_hid_clipboard(HID_VLD, record);
                } else {
                    tap_code(KC_VOLD);
                }
            }
            return false;
        case VOLU_OS:
            if (record->event.pressed) {
                if (detected_host_os() == OS_LINUX) {
                    process_record_hid_clipboard(HID_VLU, record);
                } else {
                    tap_code(KC_VOLU);
                }
            }
            return false;
        // %%PRIVATE_CASES%%
    }
    return true;
}

void leader_end_user(void) {
    // git shortcuts
    if (leader_sequence_two_keys(KC_G, KC_S)) {
        SEND_STRING("git status\n");
    } else if (leader_sequence_two_keys(KC_G, KC_A)) {
        SEND_STRING("git add -p\n");
    } else if (leader_sequence_two_keys(KC_G, KC_C)) {
        SEND_STRING("git commit\n");
    } else if (leader_sequence_two_keys(KC_G, KC_D)) {
        SEND_STRING("git diff\n");
    } else if (leader_sequence_two_keys(KC_G, KC_P)) {
        SEND_STRING("git push\n");
    } else if (leader_sequence_two_keys(KC_G, KC_L)) {
        SEND_STRING("git log --oneline\n");
    } else if (leader_sequence_two_keys(KC_G, KC_B)) {
        SEND_STRING("git branch\n");
    } else if (leader_sequence_two_keys(KC_G, KC_Z)) {
        SEND_STRING("git stash\n");
    } else if (leader_sequence_two_keys(KC_G, KC_R)) {
        SEND_STRING("git rebase -i HEAD~");  // user types count
    // shell shortcuts
    } else if (leader_sequence_two_keys(KC_L, KC_S)) {
        SEND_STRING("ls -la\n");
    } else if (leader_sequence_two_keys(KC_M, KC_K)) {
        SEND_STRING("make\n");
    } else if (leader_sequence_three_keys(KC_C, KC_D, KC_1)) {
        SEND_STRING("cd ../\n");
    } else if (leader_sequence_three_keys(KC_C, KC_D, KC_2)) {
        SEND_STRING("cd ../../\n");
    } else if (leader_sequence_three_keys(KC_C, KC_D, KC_3)) {
        SEND_STRING("cd ../../../\n");
    } else if (leader_sequence_three_keys(KC_C, KC_D, KC_4)) {
        SEND_STRING("cd ../../../../\n");
    } else if (leader_sequence_three_keys(KC_C, KC_D, KC_5)) {
        SEND_STRING("cd ../../../../../\n");
    } else if (leader_sequence_two_keys(KC_C, KC_D)) {
        SEND_STRING("cd ../\n");
    }
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [_DVORAK] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     KC_NAGR ,KC_1    ,KC_2    ,KC_3    ,KC_4    ,KC_5    ,                                            KC_6    ,KC_7    ,KC_8    ,KC_9    ,KC_0    ,KC_NAMI ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     KC_TAB  ,KC_QUOT , KC_COMM, KC_DOT ,KC_P    ,KC_Y    ,SYM_L   ,                          SYM_L   ,KC_F    ,KC_G    ,KC_C    ,KC_R    ,KC_L    ,KC_BSLS ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     TD(TD_ESC),KC_A    ,KC_O    ,KC_E    ,KC_U    ,KC_I    ,OSL(_FN1),                        OSL(_FN2),KC_D    ,KC_H    ,KC_T    ,KC_N    ,KC_S    ,KC_SLSH ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     KC_LSFT ,KC_SCLN ,KC_Q    ,KC_J    ,KC_K    ,KC_X    ,KC_HOME ,KC_ADEN ,       KC_ADLBR ,KC_RBRC ,KC_B    ,KC_M    ,KC_W    ,KC_V    ,KC_Z    ,QK_LEAD ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     KC_CMB  ,KC_LCTL ,KC_LGUI ,KC_LALT ,     KC_EQL  ,    KC_SPC  ,KC_BSPC ,        KC_DEL  ,KC_ENT  ,    KC_MINS ,     KC_LEFT ,KC_DOWN ,KC_UP   ,KC_RGHT
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),

  [_SYMB] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     _______ ,KC_F1   ,KC_F2   ,KC_F3   ,KC_F4   ,KC_F5   ,                                            XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,KC_F6   ,KC_F7   ,KC_F8   ,KC_F9   ,KC_F10  ,_______ ,                          _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,KC_F11  ,KC_F12  ,XXXXXXX ,XXXXXXX ,XXXXXXX ,_______ ,                          _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,_______ ,_______ ,        _______ ,_______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,     _______ ,    _______ ,_______ ,        _______ ,_______ ,    XXXXXXX ,     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),
  //left pinky layer
  [_COMBO] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                                  ┌────────┬────────┬────────┬────────┬────────┬────────┐
     KC_F1   ,KC_F2   ,KC_F3   ,KC_F4   ,KC_F5   ,KC_F6   ,                                                   WRP_4K  ,WRP_14  ,WRP_11A ,WRP_11B ,AC_TOGG ,QK_BOOT ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                                ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     KC_F7   ,KC_F8   ,KC_F9   ,KC_F10  ,KC_F11  ,KC_F12  ,_______ ,                                 _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                                ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,COPYLINE,                                 _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐              ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,KC_F5   , KC_F12 ,XXXXXXX ,XXXXXXX ,XXXXXXX ,CB_NXT , CB_PRV,                 KC_PGUP ,KC_PGDN ,XXXXXXX ,XXXXXXX ,MS_WHLL, MS_WHLD ,MS_WHLU, MS_WHLR ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤              ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,_______ ,MO(_CMB_GUI),_______ ,   LCTL(KC_X),LCTL(KC_C),LCTL(KC_V),               MS_BTN1 ,MS_BTN2,     MS_BTN3 ,     MS_LEFT ,MS_DOWN ,MS_UP   ,MS_RGHT
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘              └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),
  // left interior layer
  [_FN1] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     WRAPTK  ,PB_1    ,PB_2    ,CASE_UP ,CASE_LO ,CASE_SN ,                                            _______ ,_______ ,WARP_CTR,WRAPPR  ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,WRAPQU  ,WRAPAG  ,_______ ,ASHOT   ,_______ ,MIC_TOG ,                          _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,OSL(_FN1),                        OSL(_FN2),_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,KC_APP ,_______ ,         WRAPBRF ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,     _______ ,    _______ ,TYPCLIP ,        _______ ,_______ ,    _______ ,     _______ ,_______ ,_______ ,_______
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),
  // right interior layer — HIDB_N keys send Raw HID 0x05 packets to the Python daemon
  [_FN2] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     _______ ,CB_PRV  ,CB_NXT  ,MIC_TOG ,ASHOT   ,_______ ,                                            _______ ,HID_PLY ,CASE_UP ,CASE_LO ,CASE_SN ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,                          _______ ,_______ ,_______ ,CK_TOGG ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,OSL(_FN1),                        OSL(_FN2),_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,        _______ ,_______ ,SONG_NEXT,MU_TOGG,PRINT_WPM,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,     _______ ,    _______ ,_______ ,        _______ ,_______ ,    _______ ,     _______ ,_______ ,_______ ,_______
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),

  //left thumb above backspace
  [_ADJUST] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     XXXXXXX ,WINTEMP ,WINLOCAL,WORK1   ,WORK2   ,XXXXXXX ,                                            DM_REC1 ,DM_REC2 ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,VM      ,CALENDLY,VMx2    ,XXXXXXX ,XXXXXXX ,XXXXXXX ,                          XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,MPLY_OS ,                          XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,DOMPWD  ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,_______ ,XXXXXXX ,        XXXXXXX ,_______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,     DM_RSTP ,    DM_PLY1 ,DM_PLY2 ,        XXXXXXX ,XXXXXXX ,    XXXXXXX ,     MPRV_OS ,VOLD_OS ,VOLU_OS ,MNXT_OS
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),

  // left pinky + super
  [_CMB_GUI] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     XXXXXXX ,HIDB_21 ,HIDB_22 ,HIDB_23 ,HIDB_24 ,HIDB_25 ,                                            HIDB_26 ,HIDB_27 ,HIDB_28 ,HIDB_29 ,HIDB_30 ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,                          XXXXXXX ,XXXXXXX ,XXXXXXX ,HIDB_31 ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,                          XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,        XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,_______ ,XXXXXXX ,     XXXXXXX ,    XXXXXXX ,XXXXXXX ,        XXXXXXX ,XXXXXXX ,    XXXXXXX ,     HIDB_32 ,HIDB_33 ,HIDB_34 ,HIDB_35
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  )
};
