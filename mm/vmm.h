#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define PAGE_PRESENT 0x001  // bit 0: present
#define PAGE_RW 0x002       // bit 1: read/write (0=read-only, 1=read/write)
#define PAGE_US 0x004  // bit 2: user/supervisor (0=커널만, 1=유저 접근 가능)
#define PAGE_PWT 0x008       // bit 3: page-level write-through
#define PAGE_PCD 0x010       // bit 4: page-level cache disable
#define PAGE_ACCESSED 0x020  // bit 5: accessed
#define PAGE_DIRTY 0x040     // bit 6: dirty (PTE 전용, PDE에선 예약)
#define PAGE_SIZE_4MB \
  0x080  // bit 7: PDE 전용 — 1이면 4MB 페이지, 0이면 페이지 테이블을 가리킴
#define PAGE_PAT 0x080  // bit 7: PTE 전용 — Page Attribute Table 인덱스
#define PAGE_GLOBAL \
  0x100  // bit 8: PTE 전용 — global(cr3 리로드 시 TLB 무효화 예외), CR4.PGE
         // 필요
// bit 9~11: OS 자유 사용(AVL), 아키텍처상 의미 없음
#define PAGE_FRAME_MASK 0xFFFFF000u  // bit 12~31: 4KB 정렬 물리 프레임 주소
#define MAX_PAGES_COUNT 25600

#define KERNEL_HEAP_VIRT_BASE 0x40000000u          // 1GB
#define KERNEL_HEAP_PAGE_COUNT MAX_PAGES_COUNT      // 25600 pages = 100MB
#define KERNEL_HEAP_PDE_START (KERNEL_HEAP_VIRT_BASE / 0x400000u)  // PDE #256

typedef struct __attribute__((packed)) {
  uint8_t attribute;
  uint8_t avl_and_low_address;
  uint16_t high_address;
} page_directory_t;

typedef struct __attribute__((packed)) {
  uint8_t attribute;
  uint8_t avl_and_low_address;
  uint16_t high_address;
} page_t;

typedef struct {
  __attribute__((aligned(4096))) page_directory_t kernel_pdt[1024];
  __attribute__((aligned(4096))) page_t non_touchable_pt[1024];
  __attribute__((aligned(4096))) page_t kernel_heap[MAX_PAGES_COUNT]; // 100MB
} pmm_t;

void init_vmm();
/* 첫 4MB identity 매핑 안의 페이지 하나를 비활성화한다 — 커널 스택 가드 페이지용 */
void vmm_unmap_identity_page(uint32_t addr);
void *kmalloc(uint32_t size);
void kfree(void *address);

#ifdef DEBUG
void kmalloc_selftest();
#endif

#endif