#include "linux/console.h"
#include "../driver/keyboard.h"
#include "../driver/serial.h"
#include "../mm/process.h"

/* console_take_interrupt 가 들여다본 뒤 되돌려 놓은 한 글자 */
static int pushed_back = 0;

int console_poll_char(void) {
  if (pushed_back) {
    int c = pushed_back;
    pushed_back = 0;
    return c;
  }
  uint8_t key = keyboard_get_key();
  if (key) return key;

  char c = serial_getchar();
  if (c == '\r') return '\n';
  if (c == 0x7F) return '\b';
  return (uint8_t)c;
}

int console_take_interrupt(void) {
  int c = console_poll_char();
  if (c == KEY_CTRL_C) return 1;
  pushed_back = c;
  return 0;
}

int console_getchar(void) {
  int c;
  while (!(c = console_poll_char())) {
    process_sleep(10);
  }
  return c;
}
