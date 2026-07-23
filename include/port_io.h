#ifndef PORT_IO_H
#define PORT_IO_H
// pic  reamapping + 내일 마무리 할 것(스케줄링, fs, processing, ai에게 물어보고 할 것 etc)
#include <stdint.h>

/* 1바이트 */
void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

/* 2바이트 */
void outw(uint16_t port, uint16_t val);
uint16_t inw(uint16_t port);

/* 4바이트 */
void outl(uint16_t port, uint32_t val);
uint32_t inl(uint16_t port);

/* 포트 I/O 간 딜레이용 더미 write (관례적으로 0x80 포트 사용) */
void io_wait(void);

#endif /* PORT_IO_H */