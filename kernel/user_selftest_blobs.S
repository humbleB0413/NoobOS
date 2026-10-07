/*
 * usermode_selftest() 가 유저 주소 공간에 그대로 복사해 ring3 에서 실행하는 위치 독립 코드 조각들.
 * ELF 로더 없이도 유저 모드 진입/시스템 콜/권한 위반 처리를 검증하기 위한 것 (DEBUG 빌드 전용).
 * call/pop 으로 자기 위치를 구해 문자열을 상대 주소로 찾는다.
 */
#ifdef DEBUG
#include "uapi/syscall_nr.h"

.section .rodata

/* 1) write -> getpid -> sleep(30) -> write -> exit(7) */
.global ublob_hello, ublob_hello_end
ublob_hello:
    call 1f
1:  pop %ebp
    mov $SYS_WRITE, %eax
    mov $1, %ebx
    lea (hello_msg - 1b)(%ebp), %ecx
    mov $(hello_msg_end - hello_msg), %edx
    int $0x80
    mov $SYS_GETPID, %eax
    int $0x80
    mov $SYS_SLEEP, %eax
    mov $30, %ebx
    int $0x80
    mov $SYS_WRITE, %eax
    mov $1, %ebx
    lea (bye_msg - 1b)(%ebp), %ecx
    mov $(bye_msg_end - bye_msg), %edx
    int $0x80
    mov $SYS_EXIT, %eax
    mov $7, %ebx
    int $0x80
hello_msg: .ascii "  ring3: hello via int 0x80\n"
hello_msg_end:
bye_msg: .ascii "  ring3: woke up, exiting with 7\n"
bye_msg_end:
ublob_hello_end:

/* 2) 특권 명령 cli -> #GP 로 이 프로세스만 종료돼야 함 */
.global ublob_cli, ublob_cli_end
ublob_cli:
    cli
    jmp ublob_cli
ublob_cli_end:

/* 3) 커널 메모리(0x100000, 커널 이미지) 쓰기 -> 보호 위반 #PF 로 종료 */
.global ublob_kwrite, ublob_kwrite_end
ublob_kwrite:
    movl $0xdead, 0x100000
    jmp ublob_kwrite
ublob_kwrite_end:

/* 4) 커널 주소를 write 에 넘김 -> E_FAULT 를 돌려받아 그 값을 종료 코드로 */
.global ublob_badptr, ublob_badptr_end
ublob_badptr:
    mov $SYS_WRITE, %eax
    mov $1, %ebx
    mov $0x100000, %ecx
    mov $16, %edx
    int $0x80
    mov %eax, %ebx
    mov $SYS_EXIT, %eax
    int $0x80
ublob_badptr_end:

/* 6) open("/etc/motd") -> read -> write(stdout) -> close -> exit(읽은 바이트 수) */
.global ublob_cat, ublob_cat_end
ublob_cat:
    call 1f
1:  pop %ebp
    sub $128, %esp
    mov $SYS_OPEN, %eax
    lea (motd_path - 1b)(%ebp), %ebx
    int $0x80
    mov %eax, %esi              /* fd */
    mov $SYS_READ, %eax
    mov %esi, %ebx
    mov %esp, %ecx
    mov $128, %edx
    int $0x80
    mov %eax, %edi              /* 읽은 길이 */
    mov $SYS_WRITE, %eax
    mov $1, %ebx
    mov %esp, %ecx
    mov %edi, %edx
    int $0x80
    mov $SYS_CLOSE, %eax
    mov %esi, %ebx
    int $0x80
    mov $SYS_EXIT, %eax
    mov %edi, %ebx
    int $0x80
motd_path: .asciz "/etc/motd"
ublob_cat_end:

/* 7) 없는 파일 open -> E_NOENT 를 종료 코드로 */
.global ublob_noent, ublob_noent_end
ublob_noent:
    call 1f
1:  pop %ebp
    mov $SYS_OPEN, %eax
    lea (noent_path - 1b)(%ebp), %ebx
    int $0x80
    mov %eax, %ebx
    mov $SYS_EXIT, %eax
    int $0x80
noent_path: .asciz "/no/such/file"
ublob_noent_end:

/* 5) 무한 루프 — 타이머 선점으로 다른 프로세스가 계속 돌고, kill 로 끝낼 수 있어야 함 */
.global ublob_spin, ublob_spin_end
ublob_spin:
    jmp ublob_spin
ublob_spin_end:
#endif
