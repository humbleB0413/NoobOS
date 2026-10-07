#ifdef DEBUG
#include "kstd.h"
#include "list.h"
#include "string.h"
#include "linux/vga.h"
#include "../mm/vmm.h"

static int failures;

static void check(const char *name, int condition) {
  if (!condition) {
    failures++;
    kprintf("  [FAIL] %s\n", name);
  }
}

typedef struct {
  int value;
  list_node_t node;
} item_t;

/*
 * include/string.h 와 lib/ 유틸리티의 스모크 테스트. 실패한 항목만 출력하고 실패 개수를 돌려준다.
 * string.h 는 Linux 0.01 의 inline asm 을 C 로 옮긴 것이라 경계 조건(빈 문자열, 겹치는 memmove 등)을 위주로 본다.
 */
int lib_selftest(void) {
  char buf[32];
  failures = 0;

  check("strlen empty", strlen("") == 0);
  check("strlen", strlen("noob") == 4);
  check("strcmp equal", strcmp("abc", "abc") == 0);
  check("strcmp order", strcmp("abc", "abd") < 0 && strcmp("b", "a") > 0);
  check("strcmp prefix", strcmp("ab", "abc") < 0);
  check("strncmp", strncmp("abcX", "abcY", 3) == 0 && strncmp("abcX", "abcY", 4) < 0);
  strcpy(buf, "foo");
  strcat(buf, "bar");
  check("strcpy+strcat", strcmp(buf, "foobar") == 0);
  strncpy(buf, "xy", 5);
  check("strncpy pads with NUL", buf[0] == 'x' && buf[2] == '\0' && buf[4] == '\0');
  { const char *h = "hello"; check("strchr hit", strchr(h, 'l') == h + 2); }
  check("strchr miss", strchr("hello", 'z') == 0);
  check("strchr NUL", strchr("hi", '\0') != 0);
  check("strrchr", *(strrchr("a/b/c", '/') + 1) == 'c');
  check("strstr hit", strstr("kernel panic", "panic") != 0);
  check("strstr tail miss", strstr("abc", "cd") == 0);
  check("strstr empty needle", strstr("abc", "") != 0);
  check("strspn/strcspn", strspn("aab", "a") == 2 && strcspn("ab:c", ":") == 2);

  memset(buf, 0, sizeof(buf));
  memcpy(buf, "0123456789", 10);
  memmove(buf + 2, buf, 5);
  check("memmove overlap forward", memcmp(buf, "0101234789", 10) == 0);
  memcpy(buf, "0123456789", 10);
  memmove(buf, buf + 2, 5);
  check("memmove overlap backward", memcmp(buf, "2345656789", 10) == 0);
  check("memchr", memchr("abc", 'c', 3) != 0 && memchr("abc", 'd', 3) == 0);
  memset(buf, 'z', 4);
  check("memset", buf[0] == 'z' && buf[3] == 'z');

  check("atoi", atoi("  -42") == -42 && atoi("17abc") == 17);
  check("strtoul hex", strtoul("0x1F", 0, 0) == 31 && strtoul("ff", 0, 16) == 255);

  list_node_t head = LIST_HEAD_INIT(head);
  item_t items[4];
  check("list empty", list_empty(&head));
  for (int i = 0; i < 4; i++) {
    items[i].value = i;
    list_push_back(&head, &items[i].node);
  }
  check("list length", list_length(&head) == 4);
  list_remove(&items[1].node);
  list_node_t *pos;
  int order = 0, expected[] = {0, 2, 3};
  list_for_each(pos, &head) {
    check("list order", list_entry(pos, item_t, node)->value == expected[order++]);
  }
  check("list pop", list_entry(list_pop_front(&head), item_t, node)->value == 0);
  list_node_t *tmp;
  list_for_each_safe(pos, tmp, &head) list_remove(pos);
  check("list drained", list_empty(&head) && list_pop_front(&head) == 0);

  char *dup = kstrdup("heap");
  check("kstrdup", dup && strcmp(dup, "heap") == 0);
  kfree(dup);

  kprintf("lib selftest: %s (%d failures)\n", failures ? "FAIL" : "ALL PASS", failures);
  return failures;
}
#endif
