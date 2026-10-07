#include "rtc.h"
#include "port_io.h"
#include "string.h"
#include "linux/spinlock.h"

static uint8_t cmos_read(uint8_t reg) {
  /* bit 7 은 NMI disable 비트 — 0 으로 두어 NMI 를 막지 않는다 */
  outb(CMOS_ADDRESS_PORT, reg);
  return inb(CMOS_DATA_PORT);
}

static int rtc_updating(void) {
  return cmos_read(RTC_REG_STATUS_A) & RTC_STATUS_A_UPDATING;
}

static void rtc_read_raw(rtc_time_t *t) {
  while (rtc_updating());
  t->second = cmos_read(RTC_REG_SECONDS);
  t->minute = cmos_read(RTC_REG_MINUTES);
  t->hour = cmos_read(RTC_REG_HOURS);
  t->day = cmos_read(RTC_REG_DAY);
  t->month = cmos_read(RTC_REG_MONTH);
  t->year = cmos_read(RTC_REG_YEAR);
}

static uint8_t bcd_to_bin(uint8_t v) {
  return (v & 0x0F) + (v >> 4) * 10;
}

/*
 * 갱신 중 플래그를 확인한 뒤에도 읽는 사이에 초가 넘어갈 수 있으므로,
 * 같은 값이 두 번 연속 나올 때까지 다시 읽는다(OSDev 권장 방식).
 */
void rtc_read(rtc_time_t *out) {
  rtc_time_t a, b;
  /* 0x70 에 레지스터 번호를 쓰고 0x71 을 읽는 두 단계 사이에 끼어들면 안 된다 */
  uint32_t flags = irq_save();

  rtc_read_raw(&a);
  do {
    b = a;
    rtc_read_raw(&a);
  } while (memcmp(&a, &b, sizeof(a)) != 0);

  uint8_t status_b = cmos_read(RTC_REG_STATUS_B);
  irq_restore(flags);

  uint8_t pm = a.hour & RTC_HOUR_PM;
  a.hour &= ~RTC_HOUR_PM;
  if (!(status_b & RTC_STATUS_B_BINARY)) {
    a.second = bcd_to_bin(a.second);
    a.minute = bcd_to_bin(a.minute);
    a.hour = bcd_to_bin(a.hour);
    a.day = bcd_to_bin(a.day);
    a.month = bcd_to_bin(a.month);
    a.year = bcd_to_bin((uint8_t)a.year);
  }
  if (!(status_b & RTC_STATUS_B_24HOUR) && pm) {
    a.hour = (a.hour + 12) % 24;
  }
  /* century 레지스터는 ACPI FADT 를 봐야 위치를 알 수 있어 2000년대로 가정한다 */
  a.year += 2000;
  *out = a;
}
