#ifndef VFS_H
#define VFS_H

#include <stdint.h>

#define VFS_NAME_MAX  100   /* ustar 의 name 필드 길이 */
#define VFS_MAX_NODES 64

#define VFS_FILE 1
#define VFS_DIR  2

struct vfs_node;

/* 파일 시스템 구현이 채우는 연산 표 — 지금은 initrd 하나뿐이지만 디스크 FS 를 붙일 자리 */
typedef struct {
  int (*read)(struct vfs_node *node, uint32_t offset, void *buf, uint32_t len);
} vfs_ops_t;

typedef struct vfs_node {
  char path[VFS_NAME_MAX + 1];  /* 앞의 '/' 없이 루트 기준 전체 경로, 예: "bin/hello" */
  uint32_t type;
  uint32_t size;
  const vfs_ops_t *ops;
  void *fs_data;                /* 구현별 데이터 (initrd: 파일 내용 시작 주소) */
} vfs_node_t;

/* 경로 앞의 "/" 나 "./" 는 무시한다. 없으면 NULL */
vfs_node_t *vfs_lookup(const char *path);
/* offset 부터 최대 len 바이트를 읽고 읽은 바이트 수를 돌려준다(파일 끝이면 0) */
int vfs_read(vfs_node_t *node, uint32_t offset, void *buf, uint32_t len);
uint32_t vfs_count(void);
vfs_node_t *vfs_entry(uint32_t index);
/* 구현이 노드를 등록할 때 쓴다 */
vfs_node_t *vfs_add_node(const char *path, uint32_t type, uint32_t size, const vfs_ops_t *ops, void *fs_data);
/* "./a/b/" -> "a/b" 처럼 비교용 정규 형태로 바꾼다 */
void vfs_normalize(char *dst, const char *src, uint32_t max);

/* Multiboot 모듈(ustar 아카이브)을 루트 파일 시스템으로 올린다. 등록한 파일 수, 모듈이 없으면 -1 */
int init_initrd(unsigned long mbi_address);

#endif
