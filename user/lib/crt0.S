/*
 * 유저 프로그램 진입점. 커널(exec_user)이 만든 초기 스택: [esp]=argc, [esp+4]=argv
 * main 의 반환값으로 exit 한다.
 */
#include "uapi/syscall_nr.h"

.section .text.start
.global _start
_start:
    mov (%esp), %eax
    mov 4(%esp), %ebx
    push %ebx
    push %eax
    call main
    mov %eax, %ebx
    mov $SYS_EXIT, %eax
    int $0x80
1:  jmp 1b
