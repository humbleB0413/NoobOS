# NoobOS

C와 x86 어셈블리로 작성한 i386(32비트 보호 모드) 학습용 커널입니다. Multiboot로 부팅하며 QEMU에서 실행합니다.

---

## 먼저 밝혀 둘 것: 이 프로젝트는 실패했습니다

**처음 목표는 OS의 주요 부분을 직접 구현하면서 배우는 것이었습니다. 그 목표는 달성하지 못했습니다.**

직접 구현한 부분은 부팅부터 페이징·커널 힙까지입니다. **스케줄링부터는 AI(Claude Code)가 구현했습니다.** 지금 저장소에 있는 기능의 상당 부분은 직접 만든 것이 아닙니다.

### 직접 구현한 범위 (스케줄러 이전)

| 영역 | 파일 |
|---|---|
| Multiboot 부팅, 16KB 초기 스택 | `boot/boot.S`, `linker.ld` |
| GDT + TSS, IDT, Intel 예외(0~31) 스텁 | `kernel/gdt*.{c,S}`, `kernel/idt.c`, `kernel/trap.c`, `kernel/isr_*.S` |
| 8259 PIC 리매핑, IRQ 스텁, 디바이스별 IDT 덮어쓰기 패턴 | `driver/pic.c`, `kernel/irq*.{c,S}` |
| PIT 타이머(IRQ0), PS/2 키보드(IRQ1) | `driver/timer*`, `driver/keyboard*` |
| VGA 텍스트 모드 + `kprintf`, 문자열 함수, 포트 I/O | `kernel/vga.c`, `include/string.h`, `kernel/port_io.c` |
| 물리 메모리 관리자(비트맵) | `mm/pmm.c` |
| 페이징 + 커널 힙 `kmalloc`/`kfree` | `mm/vmm.c`, `mm/vmm_asm.S` |

git 기록에는 초기 커밋부터 `Co-Authored-By: Claude` 트레일러가 붙어 있습니다. 어디까지가 직접 작성이고 어디부터가 AI 구현인지는 위 표와 아래 커밋 목록을 기준으로 봐 주세요.

### AI가 구현한 범위 (스케줄링 이후)

커밋 `381469f`(타이머 기반 라운드 로빈 스케줄러)부터 이후 모든 커밋이 여기에 해당합니다. 스케줄러, 동기화, 유저 모드, 시스템 콜, 파일 시스템, ELF 로더, 셸, 드라이버 추가, 테스트 인프라, 그리고 이 README까지 포함됩니다. 전체 목록은 [커밋 기록](#커밋-기록)에 있습니다.

### 실패한 원인

**1. 처음부터 전체 메모리 레이아웃을 설계하지 않았습니다.**

기능을 하나 붙일 때마다 그 기능에 필요한 만큼만 주소를 정했습니다. 그래서 뒤에서 앞의 결정을 계속 다시 고쳐야 했습니다. 이 저장소에 실제로 남은 예는 다음과 같습니다.

- 처음 `kmalloc`은 "물리 주소 = 가상 주소"를 전제로 설계했습니다. 연속된 물리 프레임이 없으면 2MB 이상을 할당할 수 없었고, 결국 페이징 기반으로 처음부터 다시 설계했습니다.
- 커널 힙을 가상 1GB(`0x40000000`)에 고정해 두었습니다. 유저 모드를 붙이면서 관례적인 유저 영역(`0x08000000`~`0xC0000000`)을 잡자마자 힙과 겹쳐 page fault가 났습니다. 그래서 유저 영역을 1GB 아래로 줄였습니다.
- identity 매핑이 첫 4MB뿐입니다. initrd 같은 부트 모듈은 위치를 장담할 수 없어서, 커널 힙의 가상 주소에 따로 매핑하는 우회로를 만들어야 했습니다.
- PMM이 Multiboot 정보 구조체와 부트 모듈이 있는 프레임을 보호하지 않았습니다. 부팅 후에도 이 정보를 읽어야 한다는 사실을 나중에야 고려했습니다.
- 커널이 하위 주소에 identity 매핑으로 올라가 있습니다(higher-half 커널이 아님). 그래서 커널과 유저 영역을 깔끔하게 나누기 어렵습니다.

**2. 한꺼번에 너무 많은 기능을 구현하려다 우선순위가 무너졌습니다.**

여러 서브시스템을 동시에 벌여 놓는 동안 기본적인 버그와 마무리 작업이 계속 밀렸습니다.

- `include/port_io.h`에는 "내일 마무리 할 것(스케줄링, fs, processing, …)"이라는 메모가 그대로 남아 있습니다.
- 키보드 링 버퍼의 wraparound 버그를 근본적으로 고치지 않고, 버퍼를 256에서 3000으로 늘려 증상만 가렸습니다.
- VGA `'\n'`이 화면 아래 끝에서 스크롤하지 않는 버그를 방치했습니다. 그래서 25줄이 넘는 selftest가 끝났는데도 "멈춘 것처럼" 보였습니다.
- page fault 스텁이 에러 코드를 두 번 push하는 버그, 그리고 어셈 레이블 오타로 빌드가 깨진 상태가 커밋되지 않은 채 한동안 남아 있었습니다(`ROADMAP.md`에 기록).

돌아보면 순서가 반대였어야 했습니다.
1. 메모리 배치(커널/유저/힙/부트 모듈)를 먼저 종이에 그려서 고정합니다.
2. 기능은 한 번에 하나씩, 테스트로 끝을 확인한 뒤에 다음으로 넘어갑니다.

---

## 현재 동작하는 것

AI가 구현한 부분을 포함한 전체 목록입니다. 모두 QEMU에서 확인했고, `make test`가 자동으로 검증합니다.

- **부팅과 CPU**
  - Multiboot, GDT/TSS, IDT
  - 예외가 나면 `kpanic`이 레지스터를 덤프합니다.
  - double fault는 별도 TSS(task gate)에서 처리합니다.
- **메모리**
  - 비트맵 PMM, 4KB 페이징, 100MB 커널 힙
  - 프로세스별 페이지 디렉터리
  - 커널 스택 가드 페이지
  - 유저 스택 demand paging(최대 1MB)
  - 0번 페이지를 매핑하지 않아 NULL 역참조를 잡습니다.
- **프로세스**
  - 타이머 선점형 라운드 로빈(10ms time slice), sleep 큐, idle 태스크
  - kill/wait, 종료 코드
- **유저 모드**
  - ring 3 진입, `int 0x80` 시스템 콜 12개
  - 유저 포인터 검증
  - 유저 예외(#PF, #GP, #DE 등)가 나면 그 프로세스만 종료합니다.
- **실행 파일**
  - 정적 ELF32 로더, argv 전달
  - `user/`의 미니 libc와 프로그램 7개: `hello`, `echo`, `cat`, `ls`, `counter`, `fault`, `spawner`
- **파일 시스템**
  - 최소 VFS, 그 위에 읽기 전용 tar initrd(Multiboot 모듈)
- **드라이버**
  - VGA 텍스트(스크롤, 커서), COM1 시리얼(콘솔 미러, 입력)
  - PS/2 키보드(Shift/Ctrl/Alt/CapsLock, 확장 키), PIT, CMOS RTC
  - ATA PIO 디스크(primary master, 섹터 읽기/쓰기)
- **셸**: 커널 프로세스로 동작합니다.
  - 줄 편집, ↑/↓ 히스토리
  - Ctrl+C로 포그라운드 프로그램 종료, `&`로 백그라운드 실행
  - `ps`, `kill`, `ls`, `cat`, `mem`, `date`, `disk`, `selftest` 등
- **동기화**: 인터럽트 차단 기반 spinlock(단일 CPU)

## 메모리 레이아웃

모든 주소 공간이 커널 부분을 공유합니다.

```
가상 주소                    내용
0x00000000 - 0x00000FFF     매핑 안 함 (NULL 역참조 → #PF)
0x00001000 - 0x003FFFFF     커널 이미지·정적 자료구조, identity 매핑 (커널 전용)
                            └ 0x100000 커널 로드 주소, 프로세스 테이블(가드 페이지 + 8KB 커널 스택)
0x08000000 - 0x3FEFFFFF     유저 코드/데이터 (ELF 기본 0x08048000)
0x3FF00000 - 0x3FFFFFFF     유저 스택 1MB 예약 (처음 16KB만 매핑, 나머지는 fault 때 확장)
0x40000000 - 0x463FFFFF     커널 힙 100MB (커널 전용, 부트 모듈도 여기에 매핑)
```

## 빌드와 실행

`i686-elf` 크로스 컴파일러(`i686-elf-gcc`, `i686-elf-ld`), `qemu-system-i386`, `python3`(테스트용)이 필요합니다.

```bash
make                    # debug 빌드 → build/debug/kernel.elf + initrd.tar(rootfs/ + user 프로그램)
make MODE=release       # 최적화 빌드
make run                # QEMU 실행 (시리얼 콘솔이 터미널에도 나옴, 터미널에서 입력 가능)
make run DISK=disk.img  # raw 디스크 이미지를 ATA primary master로 연결
make test               # 화면 없이 부팅해 시리얼로 셸을 조작하는 자동 스모크 테스트 (18개 항목)
make debug-qemu         # GDB 대기 상태로 QEMU 시작
make gdb                # 다른 터미널에서 접속 (tools/gdbinit 자동 로드)
make clean
```

부팅하면 셸이 뜹니다.

```
noob> help
noob> ls /bin
noob> counter 5 300 &
noob> ps
noob> fault null          # 유저 프로그램만 죽고 커널은 계속 동작
noob> selftest            # (debug 빌드) lib / user mode / kmalloc 커널 내 테스트
```

## 디렉터리 구조

```
boot/      Multiboot 헤더와 진입점
kernel/    GDT/IDT/예외, IRQ, 패닉, spinlock, 시스템 콜, ELF 로더, 셸, 콘솔, 유저 모드 selftest
driver/    PIC, PIT, PS/2 키보드, COM1 시리얼, RTC, ATA
mm/        PMM, VMM(페이징·힙·주소 공간), 프로세스/스케줄러
fs/        VFS, tar initrd
lib/       공용 유틸리티(문자열→숫자, lib selftest)
include/   공용 헤더 (uapi/ 는 커널과 유저 프로그램이 함께 쓰는 시스템 콜 번호)
user/      유저 프로그램용 crt0, 미니 libc, /bin 프로그램, 링커 스크립트
rootfs/    initrd에 들어갈 파일 (예: /etc/motd)
tools/     smoke_test.py, gdbinit
```

## 디버깅

- **커널 패닉**: 원인과 레지스터 덤프가 VGA와 시리얼에 함께 출력됩니다. GDB에서는 `kpanic`에 브레이크포인트가 미리 걸려 있습니다.
- **GDB 도우미**(`tools/gdbinit`)
  - `ps`: 프로세스 테이블
  - `regs <pt_regs*>`: 인터럽트 프레임
  - `pte <vaddr>`: 현재 CR3 기준 페이지 테이블 탐색
- **QEMU 인터럽트 로그**: 예외와 인터럽트가 어떤 순서로 났는지 추적할 때 씁니다.
  ```bash
  qemu-system-i386 -kernel build/debug/kernel.elf -initrd build/debug/initrd.tar \
      -no-reboot -d int,cpu_reset -D qemu.log
  ```
  `-d int`는 모든 인터럽트·예외를 `v=` 벡터 번호, `e=` 에러 코드, `CR2`와 함께 기록합니다. triple fault로 리셋되는 경우는 `-no-reboot`로 QEMU를 멈추고, `cpu_reset` 로그에서 마지막 예외를 확인합니다.

## 한계와 미구현

- 디스크 파일 시스템이 없습니다. ATA 드라이버는 섹터 단위 읽기/쓰기만 합니다. 파일 시스템은 읽기 전용 initrd뿐입니다.
- VFS는 평평한 노드 표(최대 64개)라서 디렉터리 계층을 실제로 다루지는 않습니다.
- 단일 CPU만 지원합니다. spinlock은 사실상 인터럽트 차단입니다.
- 프로세스는 최대 8개(정적 테이블)입니다. `fork`/copy-on-write, 시그널, 유저 힙(`brk`/`mmap`)이 없습니다.
- `kmalloc`은 요청이 작아도 최소 한 페이지를 씁니다. PMM과 힙 탐색은 선형 스캔입니다.
- 커널이 higher-half가 아니고, 유저 영역이 커널 힙 아래 약 1GB로 제한됩니다.
- ATA는 인터럽트 없이 폴링(PIO)으로만 동작합니다. 모든 기능은 QEMU에서만 시험했고, 실제 하드웨어에서는 시험하지 않았습니다.
- 셸은 유저 프로그램이 아니라 커널 프로세스입니다.

## 커밋 기록

| 커밋 | 내용 | 구현 |
|---|---|---|
| `7d82129` | 초기 커널: 부팅, GDT/IDT, 예외, PIC/IRQ, 타이머, 키보드, VGA | 직접 |
| `cad450f` | 물리 메모리 관리자 | 직접 |
| `4323c82` | 페이징 + kmalloc/kfree | 직접 |
| `5c2a459` | 키보드 확장 스캔코드, 버퍼 확대 | 직접 |
| `95b0f0e` | page fault 스텁 수정 (작업 중이던 변경을 커밋) | 직접 |
| `381469f` | 타이머 기반 라운드 로빈 스케줄러 | **AI** |
| `89b142d` ~ `09b2861` | VGA 스크롤, 시리얼, 패닉, spinlock, kprintf 64비트, lib, 키보드 링 버퍼, sleep/idle/kill, 가드 페이지, RTC, 유저 모드, VFS/initrd, ELF 로더, NULL 페이지, 셸, ATA, 키보드 수식 키, 부트 헤더, `make test`, 스택 demand paging | **AI** |

각 커밋 메시지에 변경 이유와 QEMU에서 확인한 내용을 적어 두었습니다.
