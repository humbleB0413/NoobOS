#include "ulib.h"

int main(int argc, char **argv) {
  printf("Hello from ring 3! I am pid %d, started at %u ms.\n", getpid(), uptime_ms());
  for (int i = 0; i < argc; i++) printf("  argv[%d] = \"%s\"\n", i, argv[i]);
  return 0;
}
