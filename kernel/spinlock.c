#include "linux/spinlock.h"
#include "linux/panic.h"

void spin_lock(spinlock_t *lock) {
  uint32_t flags = irq_save();
  /* CPU 가 하나뿐이라 이미 잡힌 락을 다시 잡으려 하면 영원히 풀리지 않는다 — 재귀 진입 버그를 즉시 드러낸다 */
  if (lock->locked) {
    kpanic("spinlock '%s' acquired recursively", lock->name);
  }
  lock->locked = 1;
  lock->flags = flags;
}

void spin_unlock(spinlock_t *lock) {
  uint32_t flags = lock->flags;
  lock->locked = 0;
  irq_restore(flags);
}
