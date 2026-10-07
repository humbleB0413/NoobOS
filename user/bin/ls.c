#include "ulib.h"

/* 파일 시스템이 평평한 노드 표라서, 접두어(디렉터리)가 맞는 항목을 전부 보여준다 */
int main(int argc, char **argv) {
  const char *prefix = argc > 1 ? argv[1] : "";
  while (*prefix == '/') prefix++;
  size_t plen = strlen(prefix);
  char path[128];
  int type;
  for (unsigned i = 0; (type = readdir(i, path, sizeof(path))) > 0; i++) {
    int match = 1;
    for (size_t k = 0; k < plen; k++) {
      if (path[k] != prefix[k]) { match = 0; break; }
    }
    if (match) printf("%s /%s\n", type == 2 ? "d" : "-", path);
  }
  return 0;
}
