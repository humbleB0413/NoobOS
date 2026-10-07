#pragma once

#include <stdint.h>

#define EFLAGS_IF 0x200

/*
 * 단일 CPU 커널이므로 "락" 은 곧 인터럽트 차단이다. 타이머 IRQ 가 yield() 로
 * 다른 프로세스에 CPU 를 넘길 수 있게 된 뒤로는, 공유 자료구조를 건드리는 구간을
 * 인터럽트 없이 실행해야 경쟁 상태를 막을 수 있다.
 * 이전 IF 상태를 저장했다가 복원하므로 이미 cli 상태인 문맥(IRQ 핸들러 등)에서 써도 안전하다.
 */
static inline uint32_t irq_save(void) {
  uint32_t flags;
  __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");
  return flags;
}

static inline void irq_restore(uint32_t flags) {
  if (flags & EFLAGS_IF) {
    __asm__ volatile("sti" : : : "memory");
  }
}

typedef struct {
  volatile uint32_t locked;
  uint32_t flags;
  const char *name;
} spinlock_t;

#define SPINLOCK_INIT(n) { 0, 0, (n) }

void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
