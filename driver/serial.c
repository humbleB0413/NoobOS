#include "serial.h"
#include "port_io.h"
#include "linux/vga.h"

static int serial_ready = 0;

static void serial_console_putchar(char c) {
  if (c == '\b') {
    /* 터미널 에뮬레이터에서 실제로 글자를 지우려면 "뒤로-공백-뒤로" 가 필요 */
    serial_putchar('\b');
    serial_putchar(' ');
    serial_putchar('\b');
    return;
  }
  if (c == '\n') serial_putchar('\r');
  serial_putchar(c);
}

/*
 * 38400 baud, 8N1, FIFO 사용으로 COM1 을 초기화한다.
 * loopback 모드에서 보낸 바이트가 그대로 돌아오는지로 포트 존재 여부를 확인하고,
 * 성공하면 VGA 콘솔 출력을 시리얼로도 복제(mirror)한다 — `make run` 의 -serial stdio 로 커널 로그를 볼 수 있다.
 */
int init_serial(void) {
  outb(SERIAL_COM1 + SERIAL_INT_ENABLE, 0x00);   /* 인터럽트 끔(폴링) */
  outb(SERIAL_COM1 + SERIAL_LINE_CTRL, 0x80);    /* DLAB=1 */
  outb(SERIAL_COM1 + SERIAL_DATA, 0x03);         /* divisor = 3 -> 38400 baud */
  outb(SERIAL_COM1 + SERIAL_INT_ENABLE, 0x00);
  outb(SERIAL_COM1 + SERIAL_LINE_CTRL, 0x03);    /* 8 bit, no parity, 1 stop, DLAB=0 */
  outb(SERIAL_COM1 + SERIAL_FIFO_CTRL, 0xC7);    /* FIFO enable/clear, 14 byte threshold */
  outb(SERIAL_COM1 + SERIAL_MODEM_CTRL, 0x1E);   /* loopback 테스트 모드 */

  outb(SERIAL_COM1 + SERIAL_DATA, 0xAE);
  if (inb(SERIAL_COM1 + SERIAL_DATA) != 0xAE) {
    return -1;
  }

  outb(SERIAL_COM1 + SERIAL_MODEM_CTRL, 0x0F);   /* 정상 모드: DTR/RTS/OUT1/OUT2 */
  serial_ready = 1;
  terminal_set_mirror(serial_console_putchar);
  return 0;
}

void serial_putchar(char c) {
  if (!serial_ready) return;
  while (!(inb(SERIAL_COM1 + SERIAL_LINE_STATUS) & SERIAL_LSR_THR_EMPTY));
  outb(SERIAL_COM1 + SERIAL_DATA, (uint8_t)c);
}

int serial_received(void) {
  return serial_ready && (inb(SERIAL_COM1 + SERIAL_LINE_STATUS) & SERIAL_LSR_DATA_READY);
}

/* 받은 바이트가 없으면 0 을 돌려준다(논블로킹) */
char serial_getchar(void) {
  if (!serial_received()) return '\0';
  return (char)inb(SERIAL_COM1 + SERIAL_DATA);
}
