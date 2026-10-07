#pragma once

#include "idt.h"
#include "uapi/syscall_nr.h"

void init_syscall(void);
void syscall_dispatch(pt_regs *regs);
