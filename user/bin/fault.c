#include "ulib.h"

/* fault <null|kernel|cli|div> — 일부러 잘못된 동작을 해서 커널이 이 프로세스만 종료하는지 보여준다 */
int main(int argc, char **argv) {
  const char *kind = argc > 1 ? argv[1] : "null";
  printf("fault: triggering '%s'...\n", kind);
  if (strcmp(kind, "null") == 0) {
    volatile int *p = 0;
    *p = 1;
  } else if (strcmp(kind, "kernel") == 0) {
    volatile int *p = (int *)0x100000;  /* 커널 이미지 */
    *p = 1;
  } else if (strcmp(kind, "cli") == 0) {
    __asm__ volatile("cli");
  } else if (strcmp(kind, "div") == 0) {
    /* 1/x 는 GCC 가 idiv 없이 비교식으로 바꿔 버리므로 피제수도 volatile 로 둔다 */
    volatile int num = 7, zero = 0;
    printf("%d\n", num / zero);
  } else {
    puts("usage: fault <null|kernel|cli|div>");
    return 1;
  }
  puts("fault: still alive?!");
  return 0;
}
