#include "ulib.h"

/* 재귀 깊이 depth 만큼 1KB 씩 스택을 쓴다 */
static int recurse(int depth) {
  volatile char frame[1024];
  frame[0] = (char)depth;
  frame[sizeof(frame) - 1] = (char)depth;
  return depth ? recurse(depth - 1) + frame[0] : 0;
}

/* fault <null|kernel|cli|div|stack N> — 일부러 잘못된 동작을 해서 커널이 이 프로세스만 종료하는지 보여준다 */
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
  } else if (strcmp(kind, "stack") == 0) {
    /* 처음 매핑된 16KB 를 넘어 N KB 까지 스택을 쓴다: 1MB 한도 안이면 커널이 페이지를 붙여 살아남고, 넘으면 종료 */
    int kb = argc > 2 ? atoi(argv[2]) : 512;
    recurse(kb);
    printf("fault: used ~%d KB of stack and survived\n", kb);
    return 0;
  } else {
    puts("usage: fault <null|kernel|cli|div|stack [KB]>");
    return 1;
  }
  puts("fault: still alive?!");
  return 0;
}
