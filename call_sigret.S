#include "syscall.h"
#include "traps.h"

.globl call_sigret_start
.globl call_sigret_end

call_sigret_start:
	mov $SYS_sigret, %eax
	int $T_SYSCALL
call_sigret_end: