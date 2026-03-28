#define HID_CLIP_INTER_CHUNK_DELAY_MS 50
// KC_HID_BTN_32 -> KC_HID_BTN_35 will repeat when held
#define HID_BTN_REPEAT_MASK ((1ULL << 31) | (1ULL << 32) | (1ULL << 33) | (1ULL << 34))


// Tapping term for Tap Dance, LT, etc. (default is 200)
#define TAPPING_TERM 400

// Leader key
#define LEADER_TIMEOUT 400
#define LEADER_PER_KEY_TIMING

// RGB underglow on by default
// #define RGBLIGHT_DEFAULT_ON true
// #define RGBLIGHT_DEFAULT_MODE RGBLIGHT_MODE_STATIC_LIGHT
// #define RGBLIGHT_DEFAULT_HUE 0
// #define RGBLIGHT_DEFAULT_SAT 255
// #define RGBLIGHT_DEFAULT_VAL 128
// #undef RGB_DI_PIN
// #define RGB_DI_PIN NO_PIN // Just as a test to ensure no conflict

// reduce the firmware size
// using up to 9 layers
#define LAYER_STATE_8BIT


// audio
#define AUDIO_PIN 12
#define AUDIO_PWM_DRIVER PWMD6
#define AUDIO_PWM_CHANNEL RP2040_PWM_CHANNEL_A
// Optional: If you want a startup melody
#define AUDIO_INIT_DELAY
// #define STARTUP_SONG SONG(QMK_STARTUP_SOUND)
#define AUDIO_CLICKY

#ifdef AUDIO_ENABLE
  #define STARTUP_SONG SONG(E1M1_DOOM);
#endif
