#pragma once

#include <stdint.h>

#define EXEC_MAX_ARGS 16
#define EXEC_ARGLINE_MAX 256

/*
 * VFS 의 ELF32 실행 파일을 새 주소 공간에 올려 유저 프로세스로 시작한다.
 * argline 은 공백으로 나눈 인자 문자열(argv[0] 포함, 예: "cat /etc/motd").
 * 성공하면 pid, 실패하면 음수(E_NOENT: 파일 없음, E_INVAL: ELF 가 아님/잘못된 세그먼트, -12: 메모리 부족).
 */
int exec_user(const char *path, const char *argline);
