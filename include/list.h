#pragma once

#include <stddef.h>

/*
 * 리눅스 커널식 intrusive 이중 연결 리스트.
 * 노드(list_node_t)를 자료구조 안에 직접 박아 넣고 list_entry() 로 바깥 구조체를 되찾으므로,
 * 리스트 자체는 메모리를 할당하지 않는다 — kmalloc 없이 정적 배열(예: 프로세스 테이블)에도 쓸 수 있다.
 * 머리(head)는 값이 없는 sentinel 이며, 빈 리스트는 head->next == head 이다.
 */
typedef struct list_node {
  struct list_node *prev, *next;
} list_node_t;

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define list_entry(node, type, member) \
  ((type *)((char *)(node) - offsetof(type, member)))

#define list_for_each(pos, head) \
  for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/* 순회 중 현재 노드를 지워도 안전한 버전 */
#define list_for_each_safe(pos, tmp, head)                  \
  for ((pos) = (head)->next, (tmp) = (pos)->next;           \
       (pos) != (head); (pos) = (tmp), (tmp) = (pos)->next)

static inline void list_init(list_node_t *head) {
  head->prev = head;
  head->next = head;
}

static inline int list_empty(const list_node_t *head) {
  return head->next == head;
}

static inline void list_insert_between(list_node_t *node, list_node_t *prev, list_node_t *next) {
  node->prev = prev;
  node->next = next;
  prev->next = node;
  next->prev = node;
}

static inline void list_push_front(list_node_t *head, list_node_t *node) {
  list_insert_between(node, head, head->next);
}

static inline void list_push_back(list_node_t *head, list_node_t *node) {
  list_insert_between(node, head->prev, head);
}

static inline void list_remove(list_node_t *node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = node;
  node->next = node;
}

/* 비어 있으면 NULL */
static inline list_node_t *list_pop_front(list_node_t *head) {
  if (list_empty(head)) return (list_node_t *)0;
  list_node_t *node = head->next;
  list_remove(node);
  return node;
}

static inline size_t list_length(const list_node_t *head) {
  size_t n = 0;
  for (const list_node_t *p = head->next; p != head; p = p->next) n++;
  return n;
}
