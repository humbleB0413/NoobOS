#include "linux/console.h"
#include "../driver/keyboard.h"
#include "../driver/serial.h"
#include "../mm/process.h"

int console_poll_char(void) {
  uint8_t key = keyboard_get_key();
  if (key) return key;

  char c = serial_getchar();
  if (c == '\r') return '\n';
  if (c == 0x7F) return '\b';
  return (uint8_t)c;
}

int console_getchar(void) {
  int c;
  while (!(c = console_poll_char())) {
    process_sleep(10);
  }
  return c;
}
