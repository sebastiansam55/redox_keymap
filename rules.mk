SRC += hid_clipboard.c

DYNAMIC_MACRO_ENABLE = yes
DIGITIZER_ENABLE = yes
PROGRAMMABLE_BUTTON_ENABLE = yes
AUTOCORRECT_ENABLE = yes
# SEND_STRING_ENABLE = yes
CONVERT_TO = elite_pi
RAW_ENABLE = yes

# for troubleshooting enable and then run `qmk console`
# may also have to disable autocorrect/dynamic to have enough firmware room
CONSOLE_ENABLE = yes


# LTO_ENABLE = yes
SPACE_CADET_ENABLE = no
GRAVE_ESC_ENABLE = no
MAGIC_ENABLE = no
MUSIC_ENABLE = yes

AUDIO_ENABLE = yes
AUDIO_DRIVER = pwm_hardware

