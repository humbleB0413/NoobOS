#ifndef TIMER_H
#define TIMER_H

#include "port_io.h"

#define PIT_CH0_DATA_PORT 0x40
#define PIT_CH1_DATA_PORT 0x41
#define PIT_CH2_DATA_PORT 0x42
#define PIT_CMD_MODE_PORT 0x43
#define PIT_BASE_FREQUENCY 1193182
#define PIT_FREQUENCY 1000
#define PIT_CMD_SELECT_CH0 (0 << 6)
#define PIT_CMD_SELECT_CH1 (1 << 6)
#define PIT_CMD_SELECT_CH2 (2 << 6)
#define PIT_CMD_SELECT_LOW_BYTE_ONLY (1 << 4)
#define PIT_CMD_SELECT_HIGH_BYTE_ONLY (2 << 4)
#define PIT_CMD_SELECT_LOW_HIGH_BYTE (3 << 4)
#define PIT_CMD_SELECT_PRD_MODE (0b010 << 1)
#define PIT_CMD_SELECT_SQR_WAVE_MODE (0b011 << 1)
#define PIT_CMD_SELECT_BINARY_ENC 0
#define PIT_CMD_SELECT_BCD_ENC 1

void init_timer();
uint64_t get_ticks();
void sleep(uint64_t s);
void msleep(uint64_t ms);

#endif