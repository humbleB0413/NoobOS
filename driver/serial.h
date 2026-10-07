#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define SERIAL_COM1 0x3F8

/* COM1 레지스터 오프셋 (DLAB=0 기준) */
#define SERIAL_DATA          0  /* RX/TX 버퍼 (DLAB=1 이면 divisor low) */
#define SERIAL_INT_ENABLE    1  /* 인터럽트 enable (DLAB=1 이면 divisor high) */
#define SERIAL_FIFO_CTRL     2
#define SERIAL_LINE_CTRL     3
#define SERIAL_MODEM_CTRL    4
#define SERIAL_LINE_STATUS   5

#define SERIAL_LSR_DATA_READY 0x01
#define SERIAL_LSR_THR_EMPTY  0x20

int init_serial(void);
void serial_putchar(char c);
int serial_received(void);
char serial_getchar(void);

#endif
