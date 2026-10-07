#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "linux/irq.h"
#include "linux/trap.h"
#include "linux/panic.h"
#include "linux/spinlock.h"

#define PMM_ERROR_CODE 0xFFFFFFFF

pmm_t pmm;
static spinlock_t heap_lock = SPINLOCK_INIT("kernel_heap");

extern void init_paging_asm(uint32_t address);
extern void asm_isr_page_fault(void);
extern __attribute__((aligned(0x10))) idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
static void set_pdt_entry(page_directory_t* pdt, uint32_t address, uint32_t attribute);
static void set_pt_entry(page_t* pdt, uint32_t address, uint32_t attribute);


void init_vmm(){

    memset(&pmm, 0, sizeof(pmm));

    for(int i = 0; i < 1024; i++){
        uint32_t phys_address = i * PAGE_SIZE;
        set_pt_entry(&(pmm.non_touchable_pt[i]), (uint32_t)phys_address, PAGE_PRESENT | PAGE_RW);
    }

    set_pdt_entry(&(pmm.kernel_pdt[0]), (uint32_t)pmm.non_touchable_pt, PAGE_PRESENT | PAGE_RW);
    // 첫 4MB에 대해서만 identity paging

    // 커널 힙(1GB~1GB+100MB)용 페이지 테이블 25개를 kernel_heap 배열 위에 얹는다.
    // page_t가 4바이트라 1024개(=4KB)씩 잘라 쓰면 그 자체로 유효한 페이지 테이블이 됨.
    for(int k = 0; k < KERNEL_HEAP_PAGE_COUNT / 1024; k++){
        set_pdt_entry(&(pmm.kernel_pdt[KERNEL_HEAP_PDE_START + k]),
                      (uint32_t)&(pmm.kernel_heap[k * 1024]), PAGE_PRESENT | PAGE_RW);
    }

    /*Init paging default*/
    __asm__ volatile ("cli");
    set_idt_descriptor(KERNEL_IDT+ISR_PAGE_FAULT, asm_isr_page_fault, 0x08, IDT_P | IDT_DPL_KERNEL | IDT_GATE_TYPE_32BIT_INTERRUPT);
    __asm__ volatile ("sti");

    init_paging_asm((uint32_t)(pmm.kernel_pdt));

    return;
}

/* kmalloc이 반환하는 포인터 바로 앞(고정 -4 오프셋)에는 항상 이 할당의
 * page_count만 저장한다. page_count를 먼저 읽으면 그 앞에 몇 바이트짜리
 * 가상주소 배열이 있었는지(= 헤더 시작 vaddr_base) 역산할 수 있어서, 헤더
 * 크기가 요청마다 달라져도 kfree가 헤더를 O(1)로 찾을 수 있다.
 *
 * 레이아웃(vaddr_base는 힙 페이지 경계에 정렬됨):
 *   [ page_vaddr[0..page_count-1] ]  page_count * 4 bytes
 *   [ page_count ]                   4 bytes
 *   [ 사용자 데이터 ]                 <- kmalloc이 반환하는 포인터
 */

static uint32_t heap_page_phys(page_t *pt) {
  uint32_t value = ((uint32_t)pt->high_address << 16) |
                    ((uint32_t)pt->avl_and_low_address << 8) | pt->attribute;
  return value & PAGE_FRAME_MASK;
}

static int heap_page_present(uint32_t index) {
  return pmm.kernel_heap[index].attribute & PAGE_PRESENT;
}

static void invlpg(uint32_t vaddr) {
  __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* 힙 PTE 배열에서 page_count개 연속된 미사용 가상 페이지 구간을 찾는다. */
static uint32_t heap_find_free_run(uint32_t page_count) {
  uint32_t run_start = 0, run_len = 0;
  for (uint32_t i = 0; i < KERNEL_HEAP_PAGE_COUNT; i++) {
    if (heap_page_present(i)) {
      run_len = 0;
      continue;
    }
    if (run_len == 0) run_start = i;
    if (++run_len == page_count) return run_start;
  }
  return PMM_ERROR_CODE;
}

/* 힙 가상 페이지 하나를 물리 프레임 반환 + PTE 해제까지 되돌린다. */
static void heap_release_page(uint32_t index) {
  allocated_frame_info_t frame = {0};
  frame.count = 1;
  frame.frames[0] = heap_page_phys(&pmm.kernel_heap[index]) / PAGE_SIZE;
  free_frame(&frame);

  pmm.kernel_heap[index].attribute = 0;
  pmm.kernel_heap[index].avl_and_low_address = 0;
  pmm.kernel_heap[index].high_address = 0;
  invlpg(KERNEL_HEAP_VIRT_BASE + index * PAGE_SIZE);
}

static void *kmalloc_unlocked(uint32_t size) {

  /* 헤더 크기(4 + page_count*4)가 page_count에 의존하므로, 안정될 때까지
   * 반복 계산한다. 반복마다 4바이트/페이지씩만 자라서 몇 번 안에 수렴함. */
  uint32_t page_count = 1;
  for (;;) {
    uint32_t total = size + sizeof(uint32_t) + page_count * sizeof(uint32_t);
    uint32_t needed = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (needed == page_count) break;
    page_count = needed;
  }

  if (page_count > KERNEL_HEAP_PAGE_COUNT) return 0;

  uint32_t heap_start = heap_find_free_run(page_count);
  if (heap_start == PMM_ERROR_CODE) return 0;

  for (uint32_t i = 0; i < page_count; i++) {
    allocated_frame_info_t frame;
    if (alloc_frame(1, &frame) == PMM_ERROR_CODE) {
      for (uint32_t j = 0; j < i; j++) heap_release_page(heap_start + j);
      return 0;
    }
    set_pt_entry(&pmm.kernel_heap[heap_start + i], frame.frames[0] * PAGE_SIZE,
                 PAGE_PRESENT | PAGE_RW);
    invlpg(KERNEL_HEAP_VIRT_BASE + (heap_start + i) * PAGE_SIZE);
  }

  uint32_t vaddr_base = KERNEL_HEAP_VIRT_BASE + heap_start * PAGE_SIZE;
  uint32_t *page_vaddr = (uint32_t *)vaddr_base;
  for (uint32_t i = 0; i < page_count; i++) {
    page_vaddr[i] = vaddr_base + i * PAGE_SIZE;
  }
  uint32_t *page_count_slot = page_vaddr + page_count;
  *page_count_slot = page_count;

  return (void *)(page_count_slot + 1);
}

static void kfree_unlocked(void *address) {

  uint32_t page_count = *((uint32_t *)address - 1);
  uint32_t vaddr_base =
      (uint32_t)address - sizeof(uint32_t) - page_count * sizeof(uint32_t);
  uint32_t heap_start = (vaddr_base - KERNEL_HEAP_VIRT_BASE) / PAGE_SIZE;

  for (uint32_t i = 0; i < page_count; i++) {
    heap_release_page(heap_start + i);
  }
}

void vmm_unmap_identity_page(uint32_t addr) {
  KASSERT(addr < 1024 * PAGE_SIZE && (addr & (PAGE_SIZE - 1)) == 0);
  set_pt_entry(&pmm.non_touchable_pt[addr / PAGE_SIZE], 0, 0);
  invlpg(addr);
}

void *kmalloc(uint32_t size) {
  if (size == 0) return 0;
  spin_lock(&heap_lock);
  void *ptr = kmalloc_unlocked(size);
  spin_unlock(&heap_lock);
  return ptr;
}

void kfree(void *address) {
  if (!address) return;
  spin_lock(&heap_lock);
  kfree_unlocked(address);
  spin_unlock(&heap_lock);
}

static void set_pdt_entry(page_directory_t* pdt, uint32_t address, uint32_t attribute){
    uint32_t value = (address & PAGE_FRAME_MASK) | (attribute & 0x0FFF);
    pdt->attribute = value & 0xFF;
    pdt->avl_and_low_address = (value >> 8) & 0xFF;
    pdt->high_address = (value >> 16) & 0xFFFF;
    return;
}

static void set_pt_entry(page_t* pt, uint32_t address, uint32_t attribute){
    uint32_t value = (address & PAGE_FRAME_MASK) | (attribute & 0x0FFF);
    pt->attribute = value & 0xFF;
    pt->avl_and_low_address = (value >> 8) & 0xFF;
    pt->high_address = (value >> 16) & 0xFFFF;
    return;
}

extern const char* process_guard_owner(uint32_t addr);

void isr_page_fault(pt_regs* pt){
  uint32_t fault_addr;
  __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
  const char* owner = process_guard_owner(fault_addr);
  if (owner) {
    kpanic_regs(pt, "kernel stack overflow in %s (guard page 0x%x touched)", owner, fault_addr);
  }
  kpanic_regs(pt, "Page Fault at 0x%x (%s, %s, %s)",
        fault_addr,
        (pt->err_code & 0x1) ? "protection" : "not-present",
        (pt->err_code & 0x2) ? "write" : "read",
        (pt->err_code & 0x4) ? "user" : "kernel");
}

#ifdef DEBUG
#include "../include/linux/vga.h"

static int vmm_test_check(const char *name, int condition) {
  kprintf("  [%s] %s\n", condition ? "PASS" : "FAIL", name);
  return condition;
}

/* 이제 virtual != physical이므로, 시작 페이지의 PTE를 직접 조회해 실제
 * 물리주소를 출력한다. */
static void vmm_test_print_range(const char *label, void *ptr, uint32_t size) {
  if (!ptr) {
    kprintf("  %s: alloc failed (NULL)\n", label);
    return;
  }
  uint32_t start = (uint32_t)ptr;
  uint32_t end = start + size - 1;
  uint32_t heap_index = (start - KERNEL_HEAP_VIRT_BASE) / PAGE_SIZE;
  uint32_t phys_start = heap_page_phys(&pmm.kernel_heap[heap_index]);
  kprintf("  %s: virtual [0x%x - 0x%x] physical start [0x%x] (%u bytes)\n",
          label, start, end, phys_start, size);
}

void kmalloc_selftest() {
  int pass = 1;

  kprintf("=== kmalloc/kfree selftest ===\n");

  /* 0. size=0은 실패(NULL)해야 함 */
  void *p0 = kmalloc(0);
  pass &= vmm_test_check("kmalloc(0) returns NULL", p0 == 0);

  /* 1. 기본 할당 - 프레임 소모 전/후 카운트 비교용 기준점 */
  uint32_t free_before = pmm_free_count();
  void *p1 = kmalloc(16);
  pass &= vmm_test_check("kmalloc(16) returns non-null", p1 != 0);
  vmm_test_print_range("p1", p1, 16);

  /* 2. 쓰기/읽기 무결성 */
  if (p1) {
    memset(p1, 0xAB, 16);
    uint8_t *bytes = (uint8_t *)p1;
    int ok = 1;
    for (int i = 0; i < 16; i++) {
      if (bytes[i] != 0xAB) ok = 0;
    }
    pass &= vmm_test_check("written bytes read back correctly", ok);
  }

  /* 3. 여러 페이지에 걸친 할당 - 헤더/프레임 경계 오프셋 계산이 맞는지 확인 */
  void *p2 = kmalloc(PAGE_SIZE * 3);
  pass &= vmm_test_check("multi-page kmalloc(3*PAGE_SIZE) returns non-null", p2 != 0);
  vmm_test_print_range("p2", p2, PAGE_SIZE * 3);
  if (p2) {
    memset(p2, 0xCD, PAGE_SIZE * 3);
    uint8_t *bytes = (uint8_t *)p2;
    int ok = 1;
    for (uint32_t i = 0; i < PAGE_SIZE * 3; i += 512) {
      if (bytes[i] != 0xCD) ok = 0;
    }
    if (bytes[PAGE_SIZE * 3 - 1] != 0xCD) ok = 0;
    pass &= vmm_test_check("multi-page write/read across page boundaries", ok);
  }

  /* 4. kfree가 프레임을 실제로 PMM에 반환하는지 확인 */
  kfree(p1);
  kfree(p2);
  uint32_t free_after = pmm_free_count();
  pass &= vmm_test_check("kfree returns all frames to PMM (free_count restored)", free_after == free_before);

  /* 5. NULL은 안전하게 무시되어야 함 */
  kfree(0);
  pass &= vmm_test_check("kfree(NULL) does not crash", 1);

  /* 6. 동시에 살아있는 두 할당이 서로 겹치면 안 됨 */
  void *p3 = kmalloc(64);
  void *p4 = kmalloc(64);
  pass &= vmm_test_check("two live kmallocs return distinct addresses", p3 != 0 && p4 != 0 && p3 != p4);
  vmm_test_print_range("p3", p3, 64);
  vmm_test_print_range("p4", p4, 64);
  kfree(p3);
  kfree(p4);

  /* 7. 물리 프레임은 더 이상 연속일 필요가 없으므로(개별 페이지 매핑),
   * 예전엔 실패하던 600페이지 요청도 이제는 정상적으로 성공해야 함 */
  void *p5 = kmalloc(600 * PAGE_SIZE);
  pass &= vmm_test_check("kmalloc(600*PAGE_SIZE) succeeds now that frames need not be contiguous", p5 != 0);
  vmm_test_print_range("p5", p5, 600 * PAGE_SIZE);
  kfree(p5);

  /* 7b. 힙 가상주소 한도(KERNEL_HEAP_PAGE_COUNT=100MB)를 넘는 요청은 실패해야 함 */
  void *p6 = kmalloc((KERNEL_HEAP_PAGE_COUNT + 1) * PAGE_SIZE);
  pass &= vmm_test_check("kmalloc exceeding the 100MB heap correctly fails (returns NULL)", p6 == 0);

  /* 8. 최대할당량(100MB) 반복 할당 -> 추가 할당 -> Free -> 재할당
   * 1MB 청크 100개(=100MB)를 채울 수 있는 만큼 채우고, 그 뒤 한 번 더
   * 요청해서 한계 근처 동작을 확인한다. 이후 모두 Free하고 다시 1MB를
   * 할당해서 대량 free가 실제로 프레임을 되돌려줬는지 검증한다. */
#define SELFTEST_CHUNK_BYTES (1u * 1024 * 1024) /* 1MB */
#define SELFTEST_TARGET_MB 100

  void *chunks[SELFTEST_TARGET_MB];
  uint32_t n;
  uint32_t free_before_bulk = pmm_free_count();

  for (n = 0; n < SELFTEST_TARGET_MB; n++) {
    chunks[n] = kmalloc(SELFTEST_CHUNK_BYTES);
    if (!chunks[n]) break;
  }

  kprintf("  bulk alloc: %u/%u MB chunks succeeded\n", n, (uint32_t)SELFTEST_TARGET_MB);
  if (n > 0) {
    vmm_test_print_range("bulk[first]", chunks[0], SELFTEST_CHUNK_BYTES);
    vmm_test_print_range("bulk[last]", chunks[n - 1], SELFTEST_CHUNK_BYTES);
  }
  pass &= vmm_test_check("bulk allocation makes progress (at least 1 chunk)", n > 0);

  int reached_target = (n == SELFTEST_TARGET_MB);
  void *extra = kmalloc(SELFTEST_CHUNK_BYTES);
  vmm_test_print_range("extra", extra, SELFTEST_CHUNK_BYTES);
  if (!reached_target) {
    /* 목표 도달 전에 이미 물리 메모리가 바닥나서 멈췄다면, 같은 이유로
     * 추가 요청도 또 실패해야 정상(우연히 성공하면 비트맵/헤더가 깨진 것) */
    pass &= vmm_test_check("extra alloc after real OOM still fails safely", extra == 0);
  } else {
    /* 힙 가상주소 한도는 이제 100MB로 고정돼 있으므로(물리 메모리 양과
     * 무관), 100MB를 다 채운 뒤의 추가 요청은 항상 실패해야 함 */
    pass &= vmm_test_check("extra alloc after filling the fixed 100MB heap fails", extra == 0);
  }

  for (uint32_t i = 0; i < n; i++) {
    kfree(chunks[i]);
  }
  kfree(extra);

  uint32_t free_after_bulk = pmm_free_count();
  pass &= vmm_test_check("bulk free returns all frames to PMM", free_after_bulk == free_before_bulk);

  void *realloc_p = kmalloc(SELFTEST_CHUNK_BYTES);
  pass &= vmm_test_check("re-allocation after bulk free succeeds", realloc_p != 0);
  vmm_test_print_range("realloc", realloc_p, SELFTEST_CHUNK_BYTES);
  kfree(realloc_p);

#undef SELFTEST_CHUNK_BYTES
#undef SELFTEST_TARGET_MB

  kprintf("=== result: %s ===\n\n", pass ? "ALL PASS" : "SOME FAILED");
}
#endif