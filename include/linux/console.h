#pragma once

/*
 * 콘솔 입력: PS/2 키보드와 COM1 시리얼을 한데 모은다. 시리얼 쪽은 터미널이 보내는
 * '\r' / DEL(0x7F) 을 키보드와 같은 '\n' / '\b' 로 맞춰 준다.
 */
/* 입력이 없으면 0 */
int console_poll_char(void);
/* 입력이 올 때까지 잠들며(10ms 간격) 기다린다 */
int console_getchar(void);
