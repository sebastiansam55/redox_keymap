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
// %%PRIVATE_KEYCODES%%
};

enum layers {
   _DVORAK,
   // _QWERTY,
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
// HID 1 / HID 2 used to 
#define HIDB_1 KC_HID_BTN_1 //previous clipboard history entry
#define HIDB_2 KC_HID_BTN_2 //next clipboard history entry
#define HIDB_3 KC_HID_BTN_3 //toggle mic mute VisibilityUnobscured
#define HIDB_4 KC_HID_BTN_4 //run gnome-screenshot
#define HIDB_5 KC_HID_BTN_5
#define HIDB_6 KC_HID_BTN_6
#define HIDB_7 KC_HID_BTN_7
#define HIDB_8 KC_HID_BTN_8

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

// uncomment to enable debugging
// void keyboard_post_init_user(void) {
//   // Customise these values to desired behaviour
//   debug_enable=true;
//   // debug_matrix=true;
//   // debug_keyboard=true;
//   // debug_mouse=true;
// }


#define WRAP_SEL(open, close)   \
    tap_code16(C(KC_C));        \
    wait_ms(50);                \
    SEND_STRING(open);          \
    tap_code16(C(KC_V));        \
    SEND_STRING(close)

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_hid_clipboard(keycode, record)) return false;
    switch (keycode) {
        case WINTEMP:
            if (record->event.pressed) {
                SEND_STRING("\%temp\%");
            }
            break;
        case WINLOCAL:
            if (record->event.pressed) {
                SEND_STRING("\%localappdata\%");
            }
            break;
        case WORK2:
            if (record -> event.pressed) {
                SEND_STRING("C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319");
                wait_ms(100);
                SEND_STRING("\\Temporary ASP.NET Files");
            }
            break;
        case COPYLINE:
            if(record -> event.pressed){
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
                bool shifted = get_mods() & MOD_MASK_SHIFT;
                del_mods(MOD_MASK_SHIFT);
                if (shifted) { WRAP_SEL("\"", "\""); }
                else         { WRAP_SEL("'",  "'");  }
            }
            return false;
        case WRAPBRF:
            if (record->event.pressed) {
                bool shifted = get_mods() & MOD_MASK_SHIFT;
                del_mods(MOD_MASK_SHIFT);
                if (shifted) { WRAP_SEL("{", "}"); }
                else         { WRAP_SEL("[", "]"); }
            }
            return false;
        case WRAPPR: if (record->event.pressed) { WRAP_SEL("(", ")"); } return false;
        case WRAPTK: if (record->event.pressed) { WRAP_SEL("`", "`"); } return false;
        case WRAPAG: if (record->event.pressed) { WRAP_SEL("<", ">"); } return false;
        // %%PRIVATE_CASES%%
    }
    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [_DVORAK] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     KC_NAGR ,KC_1    ,KC_2    ,KC_3    ,KC_4    ,KC_5    ,                                            KC_6    ,KC_7    ,KC_8    ,KC_9    ,KC_0    ,KC_NAMI ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     KC_TAB  ,KC_QUOT , KC_COMM, KC_DOT ,KC_P    ,KC_Y    ,SYM_L   ,                          SYM_L   ,KC_F    ,KC_G    ,KC_C    ,KC_R    ,KC_L    ,KC_BSLS  ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     KC_ESC  ,KC_A    ,KC_O    ,KC_E    ,KC_U    ,KC_I    ,OSL(_FN1),                          OSL(_FN2),KC_D    ,KC_H    ,KC_T    ,KC_N    ,KC_S    ,KC_SLSH ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     KC_LSFT ,KC_SCLN ,KC_Q    ,KC_J    ,KC_K    ,KC_X    ,KC_HOME ,KC_ADEN ,       KC_ADLBR  ,KC_RBRC ,KC_B    ,KC_M    ,KC_W    ,KC_V    ,KC_Z    ,KC_NO   ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     KC_CMB  ,KC_LCTL ,KC_LGUI ,KC_LALT ,     KC_EQL  ,    KC_SPC  ,KC_BSPC ,        KC_DEL  ,KC_ENT  ,    KC_MINS ,     KC_LEFT ,KC_DOWN ,KC_UP   ,KC_RGHT
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),


  // [_QWERTY] = LAYOUT(
  // //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
  //    KC_NAGR ,KC_1    ,KC_2    ,KC_3    ,KC_4    ,KC_5    ,                                            KC_6    ,KC_7    ,KC_8    ,KC_9    ,KC_0    ,KC_NAMI ,
  // //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
  //    KC_TAB  ,KC_Q    ,KC_W    ,KC_E    ,KC_R    ,KC_T    ,SYM_L   ,                          SYM_L   ,KC_Y    ,KC_U    ,KC_I    ,KC_O    ,KC_P    ,KC_EQL  ,
  // //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
  //    KC_ESC  ,KC_A    ,KC_S    ,KC_D    ,KC_F    ,KC_G    ,OSL(_FN1),                          OSL(_FN2),KC_H    ,KC_J    ,KC_K    ,KC_L    ,KC_SCLN ,KC_QUOT ,
  // //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
  //    KC_LSFT ,KC_Z    ,KC_X    ,KC_C    ,KC_V    ,KC_B    ,KC_HOME ,KC_ADEN ,       KC_PGUP  ,KC_PGDN ,KC_N    ,KC_M    ,KC_COMM ,KC_DOT  ,KC_SLSH ,KC_NO ,
  // //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
  //    KC_CMB  ,KC_LCTL ,KC_LGUI ,KC_LALT ,     KC_EQL  ,    KC_SPC  ,KC_BSPC ,        KC_DEL  ,KC_ENT  ,   KC_MINS ,      KC_LEFT ,KC_DOWN ,KC_UP   ,KC_RGHT
  // //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  // ),

  [_SYMB] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     _______ ,KC_F1   ,KC_F2   ,KC_F3   ,KC_F4   ,KC_F5   ,                                            KC_F6   ,KC_F7   ,KC_F8   ,KC_F9   ,KC_F10  ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,KC_EXLM ,KC_AT   ,KC_LCBR ,KC_RCBR ,KC_PIPE ,_______ ,                          _______ ,KC_PSLS ,KC_P7   ,KC_P8   ,KC_P9   ,KC_PMNS ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,KC_HASH ,KC_DLR  ,KC_LBRC ,KC_RBRC ,KC_GRV  ,_______ ,                          _______ ,KC_PAST ,KC_P4   ,KC_P5   ,KC_P6   ,KC_PPLS ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,KC_PERC ,KC_CIRC ,KC_LPRN ,KC_RPRN ,KC_TILD ,_______ ,_______ ,        _______ ,_______ ,XXXXXXX ,KC_P1   ,KC_P2   ,KC_P3   ,KC_PENT ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,     _______ ,    _______ ,_______ ,        _______ ,_______ ,    KC_P0   ,     KC_P0   ,KC_PDOT ,KC_PENT ,XXXXXXX
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),
  //left pinky layer
  [_COMBO] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                                  ┌────────┬────────┬────────┬────────┬────────┬────────┐
     KC_F1   ,KC_F2   ,KC_F3   ,KC_F4   ,KC_F5   ,KC_F6   ,                                                   _______ ,_______ ,_______ ,_______ ,AC_TOGG,QK_BOOT ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                                ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     KC_F7   ,KC_F8   ,KC_F9   ,KC_F10  ,KC_F11  ,KC_F12 ,_______ ,                                 _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                                ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,COPYLINE,                                 _______ ,XXXXXXX ,XXXXXXX ,XXXXXXX   ,XXXXXXX,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐              ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,KC_F5   , KC_F12 ,XXXXXXX ,XXXXXXX ,XXXXXXX ,HIDB_2 , HIDB_1,                 KC_PGUP ,KC_PGDN ,XXXXXXX ,XXXXXXX ,MS_WHLL,MS_WHLD ,MS_WHLU, MS_WHLR,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤              ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,   LCTL(KC_X),LCTL(KC_C),LCTL(KC_V),               MS_BTN1 ,MS_BTN2,     MS_BTN3 ,     MS_LEFT ,MS_DOWN ,MS_UP   ,MS_RGHT
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘              └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),
  // left interior layer
  [_FN1] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     WRAPTK  ,PB_1    ,PB_2    ,_______ ,_______ ,_______ ,                                            _______ ,_______ ,_______ ,WRAPPR  ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,WRAPQU  ,WRAPAG  ,_______ ,HIDB_4  ,_______ ,HIDB_3  ,                          _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,OSL(_FN1),                          OSL(_FN2),_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,KC_APP ,_______ ,        WRAPBRF ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,     _______ ,    _______ ,TYPCLIP ,        _______ ,_______ ,    _______ ,     _______ ,_______ ,_______ ,_______
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  ),
  // right interior layer — HIDB_N keys send Raw HID 0x05 packets to the Python daemon
  [_FN2] = LAYOUT(
  //┌────────┬────────┬────────┬────────┬────────┬────────┐                                           ┌────────┬────────┬────────┬────────┬────────┬────────┐
     _______ ,HIDB_1  ,HIDB_2  ,HIDB_3  ,HIDB_4  ,_______ ,                                            _______ ,HIDB_5  ,HIDB_6  ,HIDB_7  ,HIDB_8  ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┐                         ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,                          _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┤                         ├────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,OSL(_FN1),                          OSL(_FN2),_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,        _______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,_______ ,
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
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,KC_MPLY ,                          XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┐       ┌────────┼────────┼────────┼────────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,DOMPWD  ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,_______ ,XXXXXXX ,        XXXXXXX ,_______ ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,
  //├────────┼────────┼────────┼────────┼────┬───┴────┬───┼────────┼────────┤       ├────────┼────────┼───┬────┴───┬────┼────────┼────────┼────────┼────────┤
     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX ,     DM_RSTP ,    DM_PLY1 ,DM_PLY2 ,        XXXXXXX ,XXXXXXX ,    XXXXXXX ,     XXXXXXX ,XXXXXXX ,XXXXXXX ,XXXXXXX
  //└────────┴────────┴────────┴────────┘    └────────┘   └────────┴────────┘       └────────┴────────┘   └────────┘    └────────┴────────┴────────┴────────┘
  )
};
