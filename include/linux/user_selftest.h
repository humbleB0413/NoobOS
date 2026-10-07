#pragma once

#ifdef DEBUG
/* ring3 진입·시스템 콜·유저 예외 격리를 위치 독립 코드 조각으로 검증. 모두 통과하면 1 */
int usermode_selftest(void);
#endif
