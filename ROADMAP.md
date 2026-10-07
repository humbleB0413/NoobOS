# NoobOS 구현 로드맵

x86(i386) 실습용 프리스탠딩 커널의 구현 현황과 남은 목표를 정리한 문서다.

> **주의**: 이 프로젝트의 초기 목표(OS 주요 부분 직접 구현)는 달성하지 못했다. 아래 "직접 구현" 구간만 직접 작성했고, 스케줄러(`381469f`)부터는 AI(Claude Code)가 구현했다. 경위와 원인은 `README.md`에 정리했다.

## 직접 구현 (스케줄러 이전)

- [x] Multiboot 부팅 (`boot/boot.S`) + 16KB 초기 스택
- [x] GDT + TSS (`kernel/gdt.c`, `kernel/gdt_asm.S`)
- [x] IDT + Intel 예외(0~31) 스텁 (`kernel/idt.c`, `kernel/trap.c`, `kernel/isr_*`)
- [x] 8259 PIC 리매핑 + IRQ(32~47) 스텁, 디바이스별 IDT 덮어쓰기 패턴 (`driver/pic.c`, `kernel/irq.c`)
- [x] PIT 타이머 IRQ0 (`driver/timer.c`) — 1000Hz
- [x] PS/2 키보드 IRQ1 (`driver/keyboard.c`) — Scan Code Set 1, Shift, 0xE0 확장 키
- [x] VGA 텍스트 모드 + `kprintf` (`kernel/vga.c`), 문자열 헤더 (`include/string.h`), 포트 I/O
- [x] 물리 메모리 관리자 PMM (`mm/pmm.c`) — Multiboot 메모리 맵 기반 프레임 비트맵
- [x] 페이징 + 커널 힙 `kmalloc`/`kfree` (`mm/vmm.c`) — 첫 4MB identity, 1GB 지점 100MB 힙, 페이지 단위 개별 매핑
  - 처음엔 물리=가상 설계였다가 연속 프레임 한계(2MB) 때문에 페이징 기반으로 재설계됨

## AI 구현 (스케줄러 이후)

각 항목 옆은 커밋 해시. 모두 QEMU에서 확인했고 대부분 `make test`가 회귀 검증한다.

- [x] 타이머 기반 라운드 로빈 스케줄러, 컨텍스트 스위칭 (`381469f`)
- [x] VGA 스크롤/백스페이스/하드웨어 커서 — 25줄 넘는 출력이 화면 밖에 써지던 버그 수정 (`89b142d`)
- [x] COM1 시리얼 드라이버 + 콘솔 미러링 (`4ac181e`)
- [x] `kpanic`/`KASSERT`, 예외를 공통 정지 경로로 (`187c0c6`)
- [x] 인터럽트 차단 기반 spinlock — `kprintf`/`kmalloc`/PMM 보호 (`eb9c5d1`)
- [x] `kprintf` 64비트 정수, 폭/패딩 지정자 (libgcc 링크) (`d11f1cb`)
- [x] `lib/`: intrusive 연결 리스트, `strtoul`/`atoi`/`kstrdup`, `string.h` 스모크 테스트 (`5ffb04f`)
- [x] 키보드 버퍼를 진짜 원형 큐로 (wraparound 시 미처리 입력 유실 수정) (`67b1838`)
- [x] sleep 큐, idle 태스크, 10ms time slice, kill/wait/getpid, CPU 시간 집계 (`87eaf5d`)
- [x] 커널 스택 가드 페이지 + double fault 전용 TSS(task gate) (`3bfd423`)
- [x] CMOS RTC 드라이버 (`1557944`)
- [x] 유저 모드: 프로세스별 페이지 디렉터리, ring 3 진입, TSS `esp0` 갱신, `int 0x80` 시스템 콜, 유저 예외 시 해당 프로세스만 종료 (`ba49683`)
- [x] VFS + tar initrd(Multiboot 모듈), open/read/close/readdir 시스템 콜 (`5a5c1c4`)
- [x] ELF32 로더, argv, spawn/wait, 유저 미니 libc와 `/bin` 프로그램 7개 (`1372bc3`)
- [x] 0번 페이지 매핑 해제로 NULL 역참조 검출 (`89951c9`)
- [x] 커널 셸 — 히스토리, Ctrl+C, 백그라운드 실행, 내장 명령 (`134d61f`)
- [x] ATA PIO 디스크 드라이버(primary master 읽기/쓰기) (`4547819`)
- [x] 키보드 Ctrl/Alt/CapsLock, 좌우 Shift 분리, E1(Pause)·가짜 Shift 시퀀스 무시 (`d581fd7`)
- [x] Multiboot 헤더의 불필요한 그래픽 모드 요청 제거 (`8e6ae08`)
- [x] `make test` 시리얼 스모크 테스트, GDB 도우미 스크립트 (`ff9cb86`)
- [x] 유저 스택 demand paging — 첫 "복구 가능한" page fault (`09b2861`)

## 남은 목표

### 메모리
- [ ] higher-half 커널(예: 커널을 3GB 위로) — 지금은 커널이 하위 identity 매핑이라 유저 영역이 힙 아래 ~1GB로 제한됨
- [ ] 작은 객체용 슬랩/버디 할당기 — 지금 `kmalloc`은 최소 1페이지
- [ ] 유저 힙(`brk` 또는 `mmap`)
- [ ] copy-on-write `fork`

### 프로세스
- [ ] 정적 8개 프로세스 테이블을 동적 할당으로
- [ ] 시그널(최소한 SIGINT/SIGKILL 개념), 부모-자식 관계와 좀비 회수
- [ ] 키보드 입력을 기다리는 프로세스를 sleep 폴링 대신 wait queue로 깨우기

### 파일 시스템 / 저장 장치
- [ ] ATA 위의 디스크 파일 시스템(FAT12/16 읽기부터)
- [ ] 디렉터리 계층을 실제로 다루는 VFS (지금은 평평한 노드 표)
- [ ] 쓰기 가능한 파일 시스템, 유저 `write`가 파일에도 동작하도록
- [ ] ATA IRQ(14) 기반 비동기 I/O

### 기타
- [ ] 셸을 유저 프로그램으로 옮기기 (지금은 커널 프로세스)
- [ ] SMP / 진짜 spinlock
- [ ] 실제 하드웨어 또는 GRUB 부팅 이미지(ISO)에서 검증

## 진행 순서 제안

남은 목표 중에서는 **메모리 레이아웃 재정비(higher-half 커널)**를 가장 먼저 하는 것을 권장한다. 이 프로젝트가 막힌 근본 원인이 메모리 배치를 처음에 정하지 않은 것이었고, 디스크 파일 시스템이나 유저 힙 같은 다음 기능도 모두 주소 공간 배치 위에 올라가기 때문이다. 기능은 한 번에 하나씩, `make test`에 항목을 추가해 끝을 확인한 뒤 다음으로 넘어간다.
