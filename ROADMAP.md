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
- [x] PS/2 키보드 드라이버 (IRQ1) (`driver/keyboard.c`, `driver/keyboard.h`, `driver/keyboard_stubs.S`) — `init_keyboard()`가 장치에 `KEYBOARD_CMD_RESET`/`KEYBOARD_CMD_ENABLE_SCANNING`을 보내고 ACK(`0xFA`) 확인 후 재시도, IDT의 IRQ1 엔트리를 `asm_irq_keyboard` 전용 스텁으로 덮어씀. `irq_keyboard`가 0x60에서 스캔코드를 읽어 make/break를 구분하고 `scancode_to_ascii()`(Scan Code Set 1 switch 테이블)로 변환, `head`/`tail` 링 버퍼(`keyboard_get_key()`)로 FIFO 순서 조회. `kernel_main`에서 `init_keyboard()` 호출 후 메인 루프가 키 입력을 출력하며 build/link 확인됨(`make`)
  - `memset` 인자 순서 버그, 스택(LIFO) 순서 반전, `keyboard_clear()` 범위 밖 접근 문제는 모두 수정됨
  - 남은 이슈 2가지, 아래 "다음 구현 목록" 1번 참고

`mm/`, `lib/` 디렉터리는 Makefile 소스 탐색 대상에 포함돼 있지만 현재 비어 있음.

## 다음 구현 목록 (우선순위 순)

### 1. 키보드 드라이버 다듬기
- [ ] `irq_keyboard`의 링 버퍼 wraparound가 완전한 모듈러 순환이 아님 — `tail`이 버퍼 끝(255)에 도달하면 `tail=-1`과 함께 `head`도 강제로 0으로 리셋하는데, 리더가 아직 다 소비하지 못한 상태에서 이 리셋이 발생하면 이미 읽은 구간을 다시 읽게 되어 순서가 꼬일 수 있음
- [ ] `0xE0` 확장 스캔코드 처리 완성 — `extended` 플래그가 설정만 되고 실제로 쓰이지 않음(컴파일러 경고 `set but not used`), 방향키 등 확장 키 시퀀스가 아직 구분되지 않음

### 2. 물리 메모리 관리자 (PMM)
- [ ] Multiboot 메모리 맵(`multiboot_info.mmap_*`) 파싱
- [ ] 비트맵 기반 프레임 할당자 (`mm/pmm.c`) — `_end` 심볼 이후 영역부터 관리
- [ ] `alloc_frame()` / `free_frame()`

### 3. 페이징 (가상 메모리)
- [ ] `mm/paging.c` — 페이지 디렉터리/테이블 설정, 커널을 상위 절반 또는 identity mapping
- [ ] Page Fault(ISR14) 핸들러를 `kernel/trap.c`에서 실제로 처리하도록 확장 (현재는 메시지만 출력)
- [ ] `cr3` 전환, `paging_enable()`

### 4. 커널 힙
- [ ] `mm/kmalloc.c` — PMM+페이징 위에 간단한 힙 할당자 (bump allocator → free-list로 발전)
- [ ] `kmalloc`/`kfree` 제공

### 5. 표준 유틸리티 라이브러리 채우기 (`lib/`)
- [ ] `string.h`의 인라인 어셈 구현을 검증하는 유닛/스모크 테스트
- [ ] `kprintf` 보완: 64비트 정수 지원(현재 `%lld/%llu`는 32비트로 잘림), 패딩/폭 지정자
- [ ] 공용 자료구조: 링크드 리스트, ring buffer 등

### 6. 유저 모드 진입
- [ ] 최소한의 유저 프로세스 이미지 로드 후 `iret`으로 ring3 진입 (GDT의 유저 세그먼트·TSS는 이미 준비됨)
- [ ] `int 0x80` 또는 `syscall` 기반 시스템 콜 게이트
- [ ] 커널/유저 스택 분리, TSS의 `esp0` 갱신 로직

### 7. 프로세스/스케줄러
- [ ] PCB(Process Control Block) 구조체
- [ ] 컨텍스트 스위칭 (레지스터 저장/복원 어셈블리 루틴)
- [ ] 타이머 IRQ 기반 라운드로빈 스케줄러

### 8. 파일 시스템
- [ ] VFS 추상 계층
- [ ] initrd 또는 간단한 read-only FS로 시작 (FAT12/16 등)
- [ ] ATA/IDE PIO 드라이버 (`driver/ata.c`)

### 9. 셸 / 사용자 프로그램
- [ ] 커널 내장 간단한 셸 (키보드 입력 + VGA 출력 기반)
- [ ] ELF 로더로 유저 바이너리 실행

### 10. 기타 드라이버/인프라
- [ ] 시리얼(COM1) 드라이버 — `make run`이 이미 `-serial stdio`를 사용하므로 커널 로그를 시리얼로 이중 출력하면 디버깅에 유용
- [ ] RTC 드라이버 (날짜/시간)
- [ ] 스풀락/인터럽트 마스킹 기반 동기화 프리미티브

### 11. 테스트/디버깅 인프라
- [ ] GDB 스크립트 보강 (`make debug-qemu`/`make gdb` 활용한 브레이크포인트 헬퍼)
- [ ] 커널 패닉/어서션 매크로 (`kpanic`, `KASSERT`)
- [ ] QEMU `-d int` 등을 활용한 인터럽트 디버깅 문서화

## 진행 순서 제안

메모리 관리(2~4) 없이는 스케줄러/유저모드/파일시스템을 붙이기 어려우므로, **키보드 드라이버 다듬기 → PMM → 페이징 → 힙** 순으로 먼저 기반을 다진 뒤 유저 모드·스케줄러·파일시스템으로 확장하는 것을 권장한다.
