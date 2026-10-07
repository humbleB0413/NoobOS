#ifndef RTC_H
#define RTC_H

#include <stdint.h>

#define CMOS_ADDRESS_PORT 0x70
#define CMOS_DATA_PORT    0x71

/* CMOS RTC 레지스터 번호 */
#define RTC_REG_SECONDS   0x00
#define RTC_REG_MINUTES   0x02
#define RTC_REG_HOURS     0x04
#define RTC_REG_DAY       0x07
#define RTC_REG_MONTH     0x08
#define RTC_REG_YEAR      0x09
#define RTC_REG_STATUS_A  0x0A
#define RTC_REG_STATUS_B  0x0B

#define RTC_STATUS_A_UPDATING 0x80  /* 1 이면 RTC 가 값을 갱신 중이라 읽으면 찢어진 값이 나올 수 있음 */
#define RTC_STATUS_B_24HOUR   0x02
#define RTC_STATUS_B_BINARY   0x04  /* 0 이면 BCD */
#define RTC_HOUR_PM           0x80  /* 12시간제일 때 시(hour) 레지스터의 PM 비트 */

typedef struct {
  uint16_t year;
  uint8_t month, day;
  uint8_t hour, minute, second;
} rtc_time_t;

/* QEMU 기본값 기준 UTC */
void rtc_read(rtc_time_t *out);

#endif
