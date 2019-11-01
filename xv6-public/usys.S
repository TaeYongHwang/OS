#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(getppid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(my_syscall)
SYSCALL(yield)
SYSCALL(set_stride)
SYSCALL(cpu_share)
SYSCALL(get_min_pass)
SYSCALL(getlev)
SYSCALL(run_MLFQ)
SYSCALL(mlfq_scheduler)
SYSCALL(thread_create)
SYSCALL(thread_exit)
SYSCALL(thread_join)
SYSCALL(sync)
SYSCALL(get_log_num);
