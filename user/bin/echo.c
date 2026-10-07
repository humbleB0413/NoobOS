#include "ulib.h"

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) printf(i + 1 < argc ? "%s " : "%s", argv[i]);
  putchar('\n');
  return 0;
}
