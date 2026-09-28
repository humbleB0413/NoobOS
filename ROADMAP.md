# NoobOS 구현 로드맵

x86(i386) 실습용 프리스탠딩 커널의 향후 구현 목표 목록. 현재 코드베이스(`kernel.c`, `kernel/`, `driver/`, `include/`) 상태를 기준으로 정리했다.

## 현재 구현된 것

- [x] Multiboot 부팅 (`boot/boot.S`) + 16KB 초기 스택
- [x] GDT + TSS (`kernel/gdt.c`, `kernel/gdt_asm.S`) — 커널/유저 코드·데이터 세그먼트, TSS 셀렉터까지 구성됨(유저 모드 진입은 아직 없음)
- [x] IDT + Intel 예외(0~31) 핸들러 (`kernel/idt.c`, `kernel/trap.c`, `kernel/isr_*`)
- [x] 8259 PIC 리매핑 + IRQ(32~47) 스텁 연결, EOI 처리 (`driver/pic.c`, `kernel/irq.c`, `kernel/irq_stubs.S`)
  - `irq_handler`는 여전히 EOI만 보내는 공용 폴백. IRQ별 디스패치 테이블 대신, 디바이스 드라이버가 자기 벡터의 IDT 엔트리를 전용 스텁으로 덮어쓰는 방식(아래 타이머 참고)으로 진행 중
- [x] PIT 타이머 (IRQ0) (`driver/timer.c`, `driver/timer.h`, `driver/timer_stubs.S`) — 채널 0을 1000Hz 구형파 모드로 초기화하고, `irq_system_timer` 전용 스텁이 IDT의 IRQ0 엔트리를 덮어써 `irq_pic_timer`에서 tick 카운트. `get_ticks()`/`sleep()`/`msleep()` 제공, `kernel_main`에서 `init_timer()` 호출 후 메인 루프가 매초 tick을 출력
- [x] VGA 텍스트 모드 드라이버 + `kprintf` (`kernel/vga.c`)
- [x] 프리스탠딩 문자열 라이브러리 헤더 (`include/string.h`)
- [x] 포트 I/O 래퍼 (`include/port_io.h`, `kernel/port_io.c`)
- [x] PS/2 키보드 드라이버 (IRQ1) (`driver/keyboard.c`, `driver/keyboard.h`, `driver/keyboard_stubs.S`) — `init_keyboard()`가 장치에 `KEYBOARD_CMD_RESET`/`KEYBOARD_CMD_ENABLE_SCANNING`을 보내고 ACK(`0xFA`) 확인 후 재시도, IDT의 IRQ1 엔트리를 `asm_irq_keyboard` 전용 스텁으로 덮어씀. `irq_keyboard`가 0x60에서 스캔코드를 읽어 make/break를 구분하고 `scancode_to_ascii()`(Scan Code Set 1 switch 테이블, Shift 조합 문자 지원)로 변환, `head`/`tail` 링 버퍼(`keyboard_get_key()`)로 FIFO 순서 조회. `kernel_main`에서 `init_keyboard()` 호출 후 메인 루프가 키 입력을 출력하며 build/link 확인됨(`make`)
  - `memset` 인자 순서 버그, 스택(LIFO) 순서 반전, `keyboard_clear()` 범위 밖 접근 문제는 모두 수정됨
  - 링 버퍼 wraparound 문제: 근본적인 원형 큐(modulo 기반 head/tail 분리 추적)로의 전환은 아니지만, `KEYBOARD_INPUT_BUFFER_SIZE`를 256 → 3000으로 늘려 실사용 빈도에서 가득 차는 경우 자체를 희박하게 만드는 완화책으로 마무리(근본 해결 아님, 이론상 여전히 재현 가능)
  - `0xE0` 확장 스캔코드 처리 완료: `scancode_to_ascii(code, shift, extended)`로 시그니처 확장, `irq_keyboard`가 저장만 하고 버리던 `extended` 플래그를 실제로 전달. 방향키/Home/End/PageUp/PageDown/Insert/Delete를 `driver/keyboard.h`의 `KEY_UP`~`KEY_DELETE`(0x80~0x89, ASCII 반환값과 안 겹치는 예약 구간)로, 키패드 Enter/Divide는 `'\n'`/`'/'`로 매핑. Print Screen/Pause 같은 멀티바이트(E0/E1 다중 시퀀스) 및 좌우 Ctrl/Alt 구분은 범위 밖으로 남겨둠. `keyboard_get_key()` 소비 측(현재 `kernel_main`의 테스트 루프)에서 0x80대 값을 실제로 해석하는 로직은 아직 없음
- [x] 물리 메모리 관리자 PMM (`mm/pmm.c`, `mm/pmm.h`) — `init_pmm()`이 Multiboot 메모리 맵(`mmap_addr`/`mmap_length`)을 순회해 프레임(4KB) 단위 비트맵을 구성하고, `_end` 심볼 기준으로 커널 자신의 이미지(`1MB~_end`)를 다시 사용 중으로 재보호. VGA 텍스트 메모리(`0xA0000~0xFFFFF`)도 같은 방식으로 명시적으로 재보호해, BIOS/에뮬레이터의 mmap 보고 여부에 의존하지 않음. `alloc_frame()`/`free_frame()`으로 프레임 단위 할당·해제, `pmm_free_count()`/`print_frame_state()`로 상태 조회. `kernel_main`에서 `init_pmm(addr)` 호출 후 키보드 입력(`a`=상태 출력/`s`=할당/`d`=해제)으로 수동 테스트 가능. 서브시스템 헤더들은 신규 `kernel.h`로 한데 모음
- [x] 실제 페이징 + 커널 힙 `kmalloc`/`kfree` (`mm/vmm.c`, `mm/vmm.h`, `mm/vmm_asm.S`) — `KMALLOC_PAGING_REDESIGN.md`에서 검토했던 "가상주소는 연속, 물리 프레임은 흩어져도 됨" 설계가 실제로 구현되어 `kernel_main`에서 `init_vmm()`이 호출되고 페이징이 켜진 채로 부팅됨(이전 항목의 "단순한 전역 커널 힙(물리=가상)" 설계는 폐기됨)
  - `init_vmm()`: `set_pdt_entry`/`set_pt_entry`로 첫 4MB를 identity mapping(`non_touchable_pt`)하고, 커널 힙 가상주소 구간(`KERNEL_HEAP_VIRT_BASE`=1GB ~ +100MB, PDE #256~280)을 `kernel_heap[25600]` 배열 위에 PDE로 미리 연결한 뒤 `init_paging_asm()`으로 `CR3` 로드 + `CR0.PG` on
  - `kmalloc(size)`/`kfree(ptr)`: `heap_find_free_run()`이 `kernel_heap[]`의 PTE `PRESENT` 비트를 직접 스캔해 힙의 사용/미사용을 추적(별도 비트맵 없음), 페이지마다 `alloc_frame(1, ...)`으로 물리 프레임을 개별 확보해 매핑(더 이상 물리적으로 연속일 필요 없음 → 예전 512프레임/2MB 상한 사라짐), 반환 포인터 바로 앞 `-4` 오프셋에 `page_count`를 심어 O(1)로 해제
  - Page Fault(ISR14)도 타이머/키보드와 같은 "디바이스별 IDT 오버라이드" 패턴으로 처리됨: `init_vmm()`이 `KERNEL_IDT[ISR_PAGE_FAULT]`를 `asm_isr_page_fault`(`mm/vmm_asm.S`) → `isr_page_fault()`(`mm/vmm.c`)로 덮어써서, `trap.c`의 범용 "Page Fault\n" 메시지 대신 `CR2`(fault 주소)와 에러코드(protection/not-present, write/read, user/kernel)를 디코드해 출력함. 다만 아직 **복구 로직은 없음** — 출력 후 `while(1)`로 영구 정지
  - `kmalloc_selftest()`(DEBUG 빌드, 키 `k`)로 zero-size/멀티페이지/멀티 동시 할당/600페이지(구 512프레임 한계 우회 확인)/100MB 힙 한도 초과/99×1MB 벌크 alloc→free→realloc까지 실행, `pmm_free_count()` 전후 비교로 프레임 완전 반환 검증 — 2026-09-08 QEMU 빌드+`sendkey`/`screendump`로 전체 PASS 확인
  - `alloc_frame_contiguous()`(PMM)는 이제 `kmalloc`이 쓰지 않지만, 범용 PMM 기능으로 남겨둠(나중 DMA 버퍼 등 대비)
  - **주의**: 이 기능을 커밋되지 않은 상태로 작업하던 중 `mm/vmm_asm.S`의 어셈 레이블이 `.global asm_isr_page_fault` 선언과 다르게 `isr_page_fault:`로 붙어 있어 `undefined reference to 'asm_isr_page_fault'` 링크 에러로 **빌드가 깨져 있었음** — 레이블을 `asm_isr_page_fault:`로 수정해 해결(2026-09-08). 커밋 전에는 항상 `make`로 링크까지 확인할 것

`lib/` 디렉터리는 Makefile 소스 탐색 대상에 포함돼 있지만 현재 비어 있음.

## 다음 구현 목록 (우선순위 순)

### 1. Page Fault 핸들러를 실제 복구 가능하게 확장
- [ ] 지금은 `isr_page_fault()`(`mm/vmm.c`)가 `CR2`/에러코드를 진단 출력만 하고 `while(1)`로 영구 정지함 — 커널 패닉 매크로(아래 8번)와 연계해 최소한 "패닉 메시지 + 정지"를 명확한 형태로 통일하거나, 유저 모드가 생기면 해당 프로세스만 종료하도록 확장 필요
- [ ] 커널 힙 성장(demand paging)이나 스택 가드 페이지 등, page fault를 "복구 가능한 이벤트"로 활용하려면 여기서부터 손대야 함

### 2. 표준 유틸리티 라이브러리 채우기 (`lib/`)
- [ ] `string.h`의 인라인 어셈 구현을 검증하는 유닛/스모크 테스트
- [ ] `kprintf` 보완: 64비트 정수 지원(현재 `%lld/%llu`는 32비트로 잘림), 패딩/폭 지정자
- [ ] 공용 자료구조: 링크드 리스트, ring buffer 등 — 이제 `kmalloc`/`kfree`가 있으니 동적 컨테이너 구현이 가능해짐

### 3. 유저 모드 진입
- [ ] 최소한의 유저 프로세스 이미지 로드 후 `iret`으로 ring3 진입 (GDT의 유저 세그먼트·TSS는 이미 준비됨)
- [ ] `int 0x80` 또는 `syscall` 기반 시스템 콜 게이트
- [ ] 커널/유저 스택 분리, TSS의 `esp0` 갱신 로직
- [ ] 유저 프로세스별 주소 공간 분리(현재 `kernel_pdt`는 전역 단일 페이지 디렉터리) — 프로세스마다 별도 PDT를 두고 `CR3` 전환할지, 커널 힙처럼 공유 영역과 유저 전용 영역을 어떻게 나눌지 설계 필요

### 4. 프로세스/스케줄러
- [ ] PCB(Process Control Block) 구조체
- [ ] 컨텍스트 스위칭 (레지스터 저장/복원 어셈블리 루틴)
- [ ] 타이머 IRQ 기반 라운드로빈 스케줄러

### 5. 파일 시스템
- [ ] VFS 추상 계층
- [ ] initrd 또는 간단한 read-only FS로 시작 (FAT12/16 등)
- [ ] ATA/IDE PIO 드라이버 (`driver/ata.c`)

### 6. 셸 / 사용자 프로그램
- [ ] 커널 내장 간단한 셸 (키보드 입력 + VGA 출력 기반)
- [ ] ELF 로더로 유저 바이너리 실행

### 7. 기타 드라이버/인프라
- [ ] 시리얼(COM1) 드라이버 — `make run`이 이미 `-serial stdio`를 사용하므로 커널 로그를 시리얼로 이중 출력하면 디버깅에 유용
- [ ] RTC 드라이버 (날짜/시간)
- [ ] 스핀락/인터럽트 마스킹 기반 동기화 프리미티브

### 8. 테스트/디버깅 인프라
- [ ] GDB 스크립트 보강 (`make debug-qemu`/`make gdb` 활용한 브레이크포인트 헬퍼)
- [ ] 커널 패닉/어서션 매크로 (`kpanic`, `KASSERT`) — page fault 핸들러(1번)와 공유할 수 있는 공통 정지/메시지 경로로 설계
- [ ] QEMU `-d int` 등을 활용한 인터럽트 디버깅 문서화
- [ ] 링커 스크립트가 만드는 `.o`를 항상 링크까지 확인하는 습관화 — 이번에 어셈 레이블 오타(`mm/vmm_asm.S`)가 한동안 빌드를 깨뜨린 채 방치된 전례가 있으므로, 커밋 전 `make`(양쪽 MODE) 통과를 체크리스트화할 가치가 있음

### 9. 남은 키보드 개선 (낮은 우선순위)
- [ ] 링 버퍼를 진짜 modulo 기반 원형 큐로 전환 — 현재는 버퍼 크기를 3000으로 늘려 완화했을 뿐, `tail`이 끝에 닿으면 `head`를 무조건 0으로 되돌리는 구조적 문제 자체는 남아있음(이론상 여전히 재현 가능)
- [ ] `KEY_UP` 등 확장 키 코드(0x80~0x89)를 실제로 소비하는 쪽 — 현재 `kernel_main`의 테스트 루프는 이 값을 해석하지 않음
- [ ] Print Screen/Pause 등 멀티바이트(E0/E1 다중 시퀀스) 및 좌우 Ctrl/Alt 구분

## 진행 순서 제안

페이징과 커널 힙(`kmalloc`/`kfree`)에 이어 키보드 드라이버의 확장 스캔코드 처리까지 마무리됨(2026-09-29) — 이제 스케줄러·유저모드·파일시스템으로 확장할 기반이 마련된 상태다. 다음 우선순위는 **Page Fault 핸들러 정비(1) → 유저 모드 진입(3)** 순을 권장한다. 유저 모드 진입 시 프로세스별 주소 공간 분리를 함께 설계해야 스케줄러(4)로 자연스럽게 이어진다.
