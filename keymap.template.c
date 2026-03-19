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
    MPLY_OS,   /* KC_MPLY on Windows, HIDB_5  on Linux */
    MPRV_OS,   /* KC_MPRV on Windows, HIDB_9  on Linux */
    MNXT_OS,   /* KC_MNXT on Windows, HIDB_10 on Linux */
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
   _FN2
};

// Shortcut to make keymap more readable
#define SYM_L   MO(_SYMB)

// HID daemon button aliases — send 0x05 packets to the Python listen daemon.
// Distinct from QMK's built-in PB_1..PB_32 (QK_PROGRAMMABLE_BUTTON_N) which
// generate OS-level HID button reports on usage page 0x0C.
#define HIDB_1  KC_HID_BTN_1 //previous clipboard history entry
#define HIDB_2  KC_HID_BTN_2 //next clipboard history entry
#define HIDB_3  KC_HID_BTN_3 //toggle mic mute VisibilityUnobscured
#define HIDB_4  KC_HID_BTN_4 //run gnome-screenshot
#define HIDB_5  KC_HID_BTN_5
#define HIDB_6  KC_HID_BTN_6 // case-transform: UPPERCASE
#define HIDB_7  KC_HID_BTN_7 // case-transform: lowercase
#define HIDB_8  KC_HID_BTN_8  // case-transform: Sentence case
#define HIDB_9  KC_HID_BTN_9  // media previous track (Linux)
#define HIDB_10 KC_HID_BTN_10 // media next track (Linux)
#define HIDB_11 KC_HID_BTN_11
#define HIDB_12 KC_HID_BTN_12
#define HIDB_13 KC_HID_BTN_13
#define HIDB_14 KC_HID_BTN_14
#define HIDB_15 KC_HID_BTN_15
#define HIDB_16 KC_HID_BTN_16

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
    // HIDB_6/7/8 are case-transform buttons: copy selection first, then send button packet
    if (record->event.pressed &&
        (keycode == HIDB_6 || keycode == HIDB_7 || keycode == HIDB_8)) {
        PLAY_SONG(song_list_coin_sound);
        tap_code16(C(KC_C));
        wait_ms(50);
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
                    process_record_hid_clipboard(HIDB_5, record);
                } else {
                    tap_code(KC_MPLY);
                }
            }
            return false;
        case MNXT_OS:
            if (record->event.pressed) {
                if (detected_host_os() == OS_LINUX) {
                    process_record_hid_clipboard(HIDB_10, record);
                } else {
                    tap_code(KC_MNXT);
                }
            }
            return false;
        case MPRV_OS:
            if (record->event.pressed) {
                if (detected_host_os() == OS_LINUX) {
                    process_record_hid_clipboard(HIDB_9, record);
                } else {
                    tap_code(KC_MPRV);
                }
            }
            return false;
        // %%PRIVATE_CASES%%
    }
    return true;
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
     KC_LSFT ,KC_SCLN ,KC_Q    ,KC_J    ,KC_K    ,KC_X    ,KC_HOME ,KC_ADEN ,       KC_ADLBR ,KC_RBRC ,KC_B    ,KC_M    ,KC_W    ,KC_V    ,KC_Z    ,KC_NO   ,
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
     KC_F1   ,KC_F2   ,KC_F3   ,KC_F4   ,KC_F5   ,KC_F6   ,                                                   _______ ,_______ ,_______ ,_______ ,AC_TOGG,QK_BOOT ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                                ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     KC_F7   ,KC_F8   ,KC_F9   ,KC_F10  ,KC_F11  ,KC_F12  ,_______ ,                                 _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                                ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,COPYLINE,                                 _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐              ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,KC_F5   , KC_F12 ,XXXXXXX ,XXXXXXX ,XXXXXXX ,HIDB_2 , HIDB_1,                 KC_PGUP ,KC_PGDN ,XXXXXXX ,XXXXXXX ,MS_WHLL, MS_WHLD ,MS_WHLU, MS_WHLR ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤              ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,   LCTL(KC_X),LCTL(KC_C),LCTL(KC_V),               MS_BTN1 ,MS_BTN2,     MS_BTN3 ,     MS_LEFT ,MS_DOWN ,MS_UP   ,MS_RGHT
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘              └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),
  // left interior layer
  [_FN1] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     WRAPTK  ,PB_1    ,PB_2    ,HIDB_6  ,HIDB_7  ,HIDB_8  ,                                            _______ ,_______ ,WARP_CTR,WRAPPR  ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,WRAPQU  ,WRAPAG  ,_______ ,HIDB_4  ,_______ ,HIDB_3  ,                          _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
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
     _______ ,HIDB_1  ,HIDB_2  ,HIDB_3  ,HIDB_4  ,_______ ,                                            _______ ,HIDB_5  ,HIDB_6  ,HIDB_7  ,HIDB_8  ,_______ ,
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
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,     DM_RSTP ,    DM_PLY1 ,DM_PLY2 ,        XXXXXXX ,XXXXXXX ,    XXXXXXX ,     MPRV_OS ,XXXXXXX ,XXXXXXX ,MNXT_OS
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  )
};
