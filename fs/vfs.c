#include "vfs.h"
#include "string.h"

static vfs_node_t nodes[VFS_MAX_NODES];
static uint32_t node_count = 0;

void vfs_normalize(char *dst, const char *src, uint32_t max) {
  while (*src == '/' || (src[0] == '.' && src[1] == '/')) {
    src += (*src == '/') ? 1 : 2;
  }
  uint32_t n = 0;
  while (*src && n + 1 < max) dst[n++] = *src++;
  while (n > 0 && dst[n - 1] == '/') n--;
  dst[n] = '\0';
}

vfs_node_t *vfs_add_node(const char *path, uint32_t type, uint32_t size, const vfs_ops_t *ops, void *fs_data) {
  if (node_count >= VFS_MAX_NODES) return 0;
  vfs_node_t *node = &nodes[node_count];
  vfs_normalize(node->path, path, sizeof(node->path));
  if (node->path[0] == '\0') return 0;  /* 루트 디렉터리 자체 */
  node->type = type;
  node->size = size;
  node->ops = ops;
  node->fs_data = fs_data;
  node_count++;
  return node;
}

vfs_node_t *vfs_lookup(const char *path) {
  char wanted[VFS_NAME_MAX + 1];
  vfs_normalize(wanted, path, sizeof(wanted));
  for (uint32_t i = 0; i < node_count; i++) {
    if (strcmp(nodes[i].path, wanted) == 0) return &nodes[i];
  }
  return 0;
}

int vfs_read(vfs_node_t *node, uint32_t offset, void *buf, uint32_t len) {
  if (!node || node->type != VFS_FILE || !node->ops || !node->ops->read) return -1;
  if (offset >= node->size) return 0;
  if (len > node->size - offset) len = node->size - offset;
  return node->ops->read(node, offset, buf, len);
}

uint32_t vfs_count(void) {
  return node_count;
}

vfs_node_t *vfs_entry(uint32_t index) {
  return index < node_count ? &nodes[index] : 0;
}
