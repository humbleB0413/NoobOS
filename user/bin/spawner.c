#include "ulib.h"

/* 유저 프로세스가 다른 프로그램을 spawn/wait 하는 예: counter 두 개를 동시에 돌리고 종료 코드를 모은다 */
int main(void) {
  int a = spawn("/bin/counter", "counter 3 200");
  int b = spawn("/bin/counter", "counter 2 300");
  if (a < 0 || b < 0) {
    printf("spawner: spawn failed (%s)\n", strerror(a < 0 ? a : b));
    return 1;
  }
  printf("spawner: started pid %d and %d\n", a, b);
  int ra = wait(a);
  int rb = wait(b);
  printf("spawner: pid %d exited %d, pid %d exited %d\n", a, ra, b, rb);
  return 0;
}
