#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/*port*/
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_CMD_PORT 0x64
/*command*/
#define KEYBOARD_CMD_SET_LEDS                       0xED /* Set LEDs */
#define KEYBOARD_CMD_ECHO                           0xEE /* Echo (diagnostic / device removal detection) */
#define KEYBOARD_CMD_SCAN_CODE_SET                  0xF0 /* Get/set current scan code set */
#define KEYBOARD_CMD_IDENTIFY                       0xF2 /* Identify keyboard */
#define KEYBOARD_CMD_SET_TYPEMATIC                  0xF3 /* Set typematic rate and delay */
#define KEYBOARD_CMD_ENABLE_SCANNING                0xF4 /* Enable scanning (keyboard will send scan codes) */
#define KEYBOARD_CMD_DISABLE_SCANNING                0xF5 /* Disable scanning (may also restore default parameters) */
#define KEYBOARD_CMD_SET_DEFAULT_PARAMS             0xF6 /* Set default parameters */
#define KEYBOARD_CMD_SET_ALL_TYPEMATIC               0xF7 /* Set all keys to typematic/autorepeat only (scan code set 3 only) */
#define KEYBOARD_CMD_SET_ALL_MAKE_RELEASE            0xF8 /* Set all keys to make/release (scan code set 3 only) */
#define KEYBOARD_CMD_SET_ALL_MAKE_ONLY               0xF9 /* Set all keys to make only (scan code set 3 only) */
#define KEYBOARD_CMD_SET_ALL_TYPEMATIC_MAKE_RELEASE  0xFA /* Set all keys to typematic/autorepeat/make/release (scan code set 3 only) */
#define KEYBOARD_CMD_SET_KEY_TYPEMATIC                0xFB /* Set specific key to typematic/autorepeat only (scan code set 3 only) */
#define KEYBOARD_CMD_SET_KEY_MAKE_RELEASE            0xFC /* Set specific key to make/release (scan code set 3 only) */
#define KEYBOARD_CMD_SET_KEY_MAKE_ONLY               0xFD /* Set specific key to make only (scan code set 3 only) */
#define KEYBOARD_CMD_RESEND                          0xFE /* Resend last byte */
#define KEYBOARD_CMD_RESET                           0xFF /* Reset and start self-test */
/*special data bytes*/
#define KEYBOARD_RESPONSE_ERROR 0x00
#define KEYBOARD_RESPONSE_SELFTEST_PASSED 0xAA
#define KEYBOARD_RESPONSE_ECHO 0xEE
#define KEYBOARD_RESPONSE_ACK 0xFA
#define KEYBOARD_RESPONSE_SELFTEST_FAILED_1 0xFC
#define KEYBOARD_RESPONSE_SELFTEST_FAILED_2 0xFD
#define KEYBOARD_RESPONSE_RESEND 0xFE
#define KEYBOARD_RESPONSE_BUFFER_OVERRUN 0xFF

#define KEYBOARD_INPUT_BUFFER_SIZE 3000

/* scancode_to_ascii()의 일반 반환값은 ASCII(0x00~0x7F) 범위이므로,
   ASCII로 표현할 수 없는 확장(0xE0 접두) 키들은 0x80 이상을 예약해 구분한다. */
#define KEY_UP        0x80
#define KEY_DOWN      0x81
#define KEY_LEFT      0x82
#define KEY_RIGHT     0x83
#define KEY_HOME      0x84
#define KEY_END       0x85
#define KEY_PAGE_UP   0x86
#define KEY_PAGE_DOWN 0x87
#define KEY_INSERT    0x88
#define KEY_DELETE    0x89

typedef struct{
    int head;
    int tail;
    uint8_t input[KEYBOARD_INPUT_BUFFER_SIZE];
} keyboard_t;

void init_keyboard();
uint8_t keyboard_get_key();
void keyboard_clear();


#endif