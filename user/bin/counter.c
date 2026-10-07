#include "ulib.h"

/* counter [횟수] [간격ms] — 여러 개를 동시에 띄워 선점형 스케줄링을 눈으로 확인하는 용도 */
int main(int argc, char **argv) {
  int count = argc > 1 ? atoi(argv[1]) : 5;
  int interval = argc > 2 ? atoi(argv[2]) : 500;
  int pid = getpid();
  for (int i = 1; i <= count; i++) {
    printf("[counter pid %d] %d/%d\n", pid, i, count);
    sleep_ms(interval);
  }
  return count;
}
