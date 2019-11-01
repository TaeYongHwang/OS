#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_sync(void){
  return sync();
}
int
sys_get_log_num(void){
  return get_log_num();
}




int
sys_thread_create(void)
{
  thread_t *thread;
  void * start_routine;
  void * arg;
  

  if(argptr(0, (void*) &thread, sizeof(*thread)) <0)
    return -1;

  if(argint(1, (int*)&start_routine) <0)
    return -1;

  if(argptr(2, (void*)&arg, sizeof(*arg)) < 0)
    return -1;
 
  return thread_create(thread, start_routine, arg);   

}

int
sys_thread_exit(void)
{
  void* retval;
  
  if(argptr(0, (void*)&retval, sizeof(*retval)) < 0)
    return -1;
  
  thread_exit(retval );
  return 0;
}

int
sys_thread_join(void)
{
  thread_t thread;
  void ** retval;
 

  if(argint(0, (int*)&thread) <0)
    return -1;

  if(argint(1, (int*)&retval) <0)
    return -1;

  return thread_join((thread_t)thread,retval);
}






int
sys_set_stride(void)
{
 set_stride();
 return 0;
}

int
sys_cpu_share(void)
{
 int percent;
 
 if(argint(0, &percent) <0)
	return -1;

 cpu_share(percent);
 return 0;
}


int
sys_get_min_pass(void)
{
 return get_min_pass();
}

int
sys_getlev(void)
{
 return getlev();
}

int
sys_run_MLFQ(void)
{
 run_MLFQ();
 return 0;
}

int
sys_mlfq_scheduler(void)
{
 mlfq_scheduler();
 return 0;
}








int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_getppid(void)
{
 return myproc()->parent->pid;
}

int
sys_yield(void)
{
  yield();
  return 1;
} 

int
sys_sbrk(void)
{
  int addr;
  int n;
  struct proc * p = myproc();

  if(argint(0, &n) < 0)
    return -1;

  if(p->is_worker_thread ==0)
    addr = p->sz;
  else
  {
    addr = p->parent->sz;
  }


 
  //addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
