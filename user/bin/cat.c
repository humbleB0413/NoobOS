#include "ulib.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: cat <file>...");
    return 1;
  }
  int status = 0;
  char buf[256];
  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i]);
    if (fd < 0) {
      printf("cat: %s: %s\n", argv[i], strerror(fd));
      status = 1;
      continue;
    }
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) write(1, buf, n);
    close(fd);
  }
  return status;
}
