#pragma once

#include "idt.h"

/* 복구 불가능한 커널 오류: 메시지를 빨간 글씨로 출력하고 인터럽트를 끈 채 영구 정지한다 */
__attribute__((noreturn)) void kpanic(const char *fmt, ...);
/* 예외/인터럽트 문맥에서 호출: 메시지 + 레지스터 덤프 후 정지 */
__attribute__((noreturn)) void kpanic_regs(pt_regs *regs, const char *fmt, ...);

#define KASSERT(cond)                                                        \
  do {                                                                       \
    if (!(cond))                                                             \
      kpanic("assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__);     \
  } while (0)
