#pragma once

/*
 * int 0x80 시스템 콜 번호 — 커널과 유저 프로그램(user/)이 함께 include 한다.
 * 호출 규약: eax = 번호, ebx/ecx/edx = 인자 1~3, 반환값은 eax (음수면 오류).
 */
#define SYS_EXIT    0   /* exit(code)                  — 돌아오지 않음 */
#define SYS_WRITE   1   /* write(fd, buf, len)          — fd 1/2 = 콘솔 */
#define SYS_READ    2   /* read(fd, buf, len)           — fd 0 = 콘솔(한 글자 이상 올 때까지 대기) */
#define SYS_GETPID  3   /* getpid()                    */
#define SYS_SLEEP   4   /* sleep(ms)                   */
#define SYS_YIELD   5   /* yield()                     */
#define SYS_UPTIME  6   /* uptime()                    — 부팅 후 ms */
#define SYS_OPEN    7   /* open(path)                  — 읽기 전용, fd(3 이상) 반환 */
#define SYS_CLOSE   8   /* close(fd)                   */
#define SYS_READDIR 9   /* readdir(index, buf, len)    — index 번째 항목 경로를 buf 에, 반환: 1=파일 2=디렉터리, 끝이면 0 */

#define SYSCALL_VECTOR 0x80

#define E_FAULT  (-14)  /* 잘못된 유저 포인터 */
#define E_BADF   (-9)   /* 잘못된 파일 디스크립터 */
#define E_NOSYS  (-38)  /* 없는 시스템 콜 */
#define E_INVAL  (-22)
#define E_NOENT  (-2)   /* 없는 파일 */
#define E_MFILE  (-24)  /* 열 수 있는 파일 수 초과 */
