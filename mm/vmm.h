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

/*
 * 가상 주소 배치 (모든 주소 공간 공통)
 *   0x00000000 - 0x003FFFFF  커널 이미지 + 정적 자료구조, identity 매핑 (커널 전용)
 *   0x08000000 - 0x3FFFFFFF  유저 영역 (코드는 ELF 기본값 0x08048000, 스택은 위에서 아래로)
 *   0x40000000 - 0x463FFFFF  커널 힙 100MB (커널 전용)
 * 커널 힙이 1GB 에 고정돼 있어 유저 영역은 그 아래로 제한된다.
 */
#define USER_SPACE_BASE 0x08000000u
#define USER_SPACE_TOP  KERNEL_HEAP_VIRT_BASE
#define USER_STACK_TOP  USER_SPACE_TOP
/* 처음에 미리 매핑하는 스택 페이지 수. 그 아래는 page fault 때 필요한 만큼 붙인다(demand paging) */
#define USER_STACK_PAGES 4
/* 스택이 자랄 수 있는 최대 크기(1MB). ELF 세그먼트는 이 예약 구간과 겹칠 수 없다 */
#define USER_STACK_MAX_PAGES 256
#define USER_STACK_LIMIT (USER_STACK_TOP - USER_STACK_MAX_PAGES * 4096)
/* esp 보다 이만큼 아래까지의 접근은 정상적인 스택 확장으로 본다 (sub esp 후 아래쪽부터 쓰는 큰 지역 배열 등) */
#define USER_STACK_SLACK 65536

/*
 * 프로세스별 페이지 디렉터리. 커널 PDE(identity 4MB, 힙)는 kernel_pdt 의 것을 그대로 복사해
 * 같은 페이지 테이블을 공유하므로, 커널 매핑은 어느 주소 공간에서나 동일하게 보인다.
 * 유저 페이지 테이블은 힙(가상 1GB~)에 있어서 물리 주소와 커널 가상 주소를 함께 기억해야 한다.
 */
typedef struct {
  page_directory_t *pd;      /* 커널 가상 주소 */
  uint32_t pd_phys;          /* CR3 에 넣을 물리 주소 */
  page_t *pt[1024];          /* PDE 별 유저 페이지 테이블의 커널 가상 주소 (없으면 NULL) */
  uint32_t mapped_pages;
} address_space_t;

void init_vmm();
/* 첫 4MB identity 매핑 안의 페이지 하나를 비활성화한다 — 커널 스택 가드 페이지용 */
void vmm_unmap_identity_page(uint32_t addr);
void *kmalloc(uint32_t size);
void kfree(void *address);
/* 페이지 정렬된 힙 페이지 1개 — 페이지 디렉터리/테이블처럼 물리 주소가 필요한 곳에 쓴다 */
void *kmalloc_page(uint32_t *phys_out);
void kfree_page(void *page);

/* 이미 존재하는 물리 메모리(부트 모듈 등)를 커널 힙 가상 주소에 매핑한다. 해제하지 않는다 */
void *vmm_map_physical(uint32_t phys, uint32_t size);
uint32_t vmm_kernel_cr3(void);
address_space_t *vmm_create_address_space(void);
void vmm_destroy_address_space(address_space_t *as);
/* [vaddr, vaddr+size) 를 덮는 유저 페이지를 새 프레임으로 매핑하고 0 으로 채운다. 실패 시 -1 */
int vmm_map_user_range(address_space_t *as, uint32_t vaddr, uint32_t size, int writable);
/* 커널 메모리 src 를 as 의 유저 주소 dst 로 복사 (잠깐 as 의 CR3 로 전환해서 쓴다) */
void vmm_copy_to_user(address_space_t *as, uint32_t dst, const void *src, uint32_t len);
/* [addr, addr+len) 이 전부 as 에 유저 접근 가능하게 매핑돼 있는지 (writable 이면 쓰기 가능까지) */
int vmm_user_range_ok(address_space_t *as, uint32_t addr, uint32_t len, int writable);
void vmm_switch(uint32_t cr3);
/* fault 주소가 스택 예약 구간 안이고 esp 근처면 그 페이지를 매핑해 0 을 돌려준다(복구 가능한 fault). 아니면 -1 */
int vmm_grow_user_stack(address_space_t *as, uint32_t addr, uint32_t user_esp);

#ifdef DEBUG
void kmalloc_selftest();
#endif

#endif