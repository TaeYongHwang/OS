#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"



struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;


static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);


//-----------declare
void set_stride(void);
void cpu_share(int percent);
void run_MLFQ(void);
void mlfq_scheduler(void);
int get_min_pass(void);
int getlev(void);

void thread_dealloc(struct proc * p);  //current thread memory dealloc
void for_exec(struct proc * curproc);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
int nexttid =1;
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;


 ////---------------------------------------------------------------------------------------------------------------------------------------------------------------
 p->pass = get_min_pass();
 acquire(&ptable.lock);
 p->cpu_share = 0;
 p->is_run_mlfq = 0;

 p->tid = nexttid++;
 p->thread_ret = 0; 
 p->is_worker_thread = 0;
 set_stride();
 release(&ptable.lock);
 //cprintf("\n p->stride : %d\n ", p->stride);



  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();


  sz = curproc->sz;
  if(curproc->is_worker_thread == 1)
    sz = curproc ->parent ->sz;
  

  

  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }

  if(curproc->is_worker_thread == 1)
    curproc->parent->sz =sz;
  else
    curproc->sz = sz;

  
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }
  if(curproc->is_worker_thread == 0) { //master thread execution

    if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
    np->sz = curproc->sz;
    np->parent = curproc;
    *np->tf = *curproc->tf;
  } else                             //worker thread execution
  {
    if((np->pgdir = copyuvm2(curproc->pgdir, curproc->parent->sz , curproc)) ==0) {
      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
    np->sz = curproc->sz;
    np->parent = curproc;
    *np->tf = *curproc->tf;

   
  }



  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));


  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;


 //----------------------------------------------------------------------------------


  np->pass = curproc->pass;
  np->is_run_mlfq = curproc->is_run_mlfq;
  if(np->is_run_mlfq == 1){
    np->priority = 0;
    np->time_quantum = 1;
    np->time_allotment = 5;
  }



 

   set_stride();

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *master;
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");


  //first. make all worker thread ZOMBIE, and find master thread. 
  if(curproc->is_worker_thread == 0)
    master = curproc;

  

  else if(curproc->is_worker_thread ==1){

      acquire(&ptable.lock);///////////////
      master = curproc->parent;
      master->state =RUNNABLE;


    for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++) { 
      if(p->pid == master->pid && p->is_worker_thread == 1) {
        p->state = ZOMBIE;
      }
    }
    master->killed = 1; 
      
    release(&ptable.lock);//////////////////////
    

    while(1) {
      acquire(&ptable.lock);
      sched();
      release(&ptable.lock);
  
    }  
  }

  //-------------only master thread execution------------------------------------------------
  //second. free thread indepenent area and make UNUSED

  for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++) 
    if(p->pid == master->pid && p->is_worker_thread == 1){
      p->state = ZOMBIE;
      thread_dealloc(p);    
    }
      
  //third. execute origin exec routine.-----------------------------------

  curproc = master;
  
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;



//  cprintf("pid %d \n", curproc->parent->pid);

  


  acquire(&ptable.lock);


  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);
  

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;

  if(curproc -> cpu_share != 0 && curproc->priority != 4){  //priority 4 == mlfq scheduler
	cpu_share(- (curproc->cpu_share));
  }
  set_stride();
  sched();
  panic("zombie exit");

}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
 
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
      
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;

      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }
   
   

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
/*
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}
*/

//stride scheduler design

void 
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  int min_pass = 999999999;
  struct proc * min_pass_proc;
  c->proc = 0;

  for(;;) {
    sti();
    acquire(&ptable.lock);
	
    for (p = ptable.proc; p< &ptable.proc[NPROC] ; p++) {
      if(p->state != RUNNABLE ) 
        continue;
	//RUNNABLE State Process execute
	//find min_pass proc
      if (( p->is_run_mlfq ==0 ||p->priority ==4 )&&  p->pass < min_pass) {
        min_pass = p->pass;
	min_pass_proc = p;	
      }
    }

    c->proc = min_pass_proc;
    switchuvm(min_pass_proc);
    min_pass_proc->state = RUNNING;
    min_pass_proc->pass += min_pass_proc->stride;

    swtch(&(c->scheduler) , min_pass_proc -> context);
    switchkvm();		
    c->proc = 0;
    min_pass = 999999999;
    release(&ptable.lock);

  }
}

int get_min_pass(void)
{
  struct proc * p;
  int min_pass = 999999999;

  for( p = ptable.proc; p < &ptable.proc[NPROC] ; p++){
    if(( p->state == RUNNABLE||p->state == RUNNING )&& p->is_run_mlfq == 0 ) {
      if(min_pass > p->pass)
        min_pass = p->pass;
    }
  }
  if(min_pass == 999999999) {//nothing changed (there is no runnable process!)
    return 0;
  } else {
      return min_pass; 
  }
}

//only RUNNING, RUNNABLE state set
//just stride 
void
set_stride(void)
{
  const int BIG_VALUE = 10000;
  int pair_cpu = 100;
  int pair_share_proc_num = 0;
  int pair_stride ;
  int pair_ticket;
  struct proc * p;

 //adjust cpu percentage 
  for(p = ptable.proc; p<&ptable.proc[NPROC] ; p++){
    if(p->state == RUNNABLE){
      if(p->cpu_share ==0 && p->is_run_mlfq ==0)
        pair_share_proc_num ++;
        if(p->cpu_share !=0){  //called cpu_share or mlfq_scheduler
          pair_cpu -= p->cpu_share;
        }  
      }
  }
 //calculate stride cpu 1 % per 10 tickets
 //update stride value
  if(pair_share_proc_num !=0)
    pair_ticket = (1+(pair_cpu / pair_share_proc_num)) * 10 ;

  else
    pair_ticket =1;




  pair_stride = BIG_VALUE/pair_ticket;

  for(p = ptable.proc; p<&ptable.proc[NPROC] ; p++){
    if(p->state == RUNNABLE){  
      if(p->cpu_share == 0 &&p->is_run_mlfq == 0){
        p->stride = pair_stride;
      }
      if(p->cpu_share != 0){ //cpu_share() called process or mlfq_schesuler 
        p->stride  = BIG_VALUE/(10 * p->cpu_share);	
      }
	
    }
  }
}




//excephandling when it is exceeded 20%
//else set total_percent and call set_stride function
void
cpu_share(int percent)
{
  static int total_percent = 0; 
  struct proc * p = myproc();

  if(percent>0){
    int exceed_check = total_percent + percent;	
    if( exceed_check  > 20){  //exception
      cprintf("cpu_share is exeeded 20%\n");
      cprintf("nothing changed\n"); 
    } else {
        if(p->is_run_mlfq == 1)
        p->is_run_mlfq =0;  //execute from mlfq to stride scheduler
        p->cpu_share = percent;
        total_percent += percent;
        set_stride();
       }
  } else{
    total_percent += percent;
  }
}










// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{

  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
 
   


  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;


  set_stride();
  // Tidy up.


  sched();

  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }

}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      p->pass = get_min_pass();
      set_stride(); //////////////////

  }

}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
  
 // set_stride();
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;
  struct proc *master = 0; 

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->is_worker_thread==0   ){
      master = p;
    }
    else if (p->pid == pid && p->is_worker_thread == 1) {
        p->killed = 1;
    }
  }

  if(master !=0) {
    master->killed = 1;
  // Wake process from sleep if necessary.
    if(master->state == SLEEPING)
      master->state = RUNNABLE;
    release(&ptable.lock);
    return 0;

  }



  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}





int run_mlfq_onoff = 0;
void run_MLFQ(void)
{
  struct proc * p;

  p = myproc();

  if(p->cpu_share != 0)
    cpu_share(- (p->cpu_share));

  p->pass = 0;
  p-> cpu_share = 0;
  p-> priority = 0;
  p-> time_quantum = 1;
  p -> is_run_mlfq = 1; 
  p-> time_allotment = 5;
  if(run_mlfq_onoff ==0){
    run_mlfq_onoff = 1;
    fork();
//execute parent
    p->is_run_mlfq = 0;
    mlfq_scheduler();
  }


 
}

int getlev(void)
{
  int lev;
  struct proc *cur_proc = myproc();
  lev = cur_proc-> priority;
  return lev;
}


void
mlfq_scheduler(void)
{
  struct proc *p;
  struct proc * curproc;
  struct cpu *c;
  struct proc *choice =0;
  int onoff;
  int priority_boost = 100;
  int highest_priority = 3;

  pushcli();
  c = mycpu();
  popcli();
 // mlfq procss initialzing
  curproc = myproc();
  curproc->cpu_share = 20;
  curproc->pass = get_min_pass();
  curproc->priority = 4;
  set_stride();
  for(;;){ //if exist run_mlfq called process, never return
    onoff=0;
    highest_priority = 3;	
    sti();
    acquire(&ptable.lock);
    for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
        
	//about process in MLFQ, find process that is executing  
      if(p->is_run_mlfq == 1){
        onoff ++;//exist process in mlfq                
 	//if exist executing process,
	if(p->priority ==1 && p->time_quantum <2){
	  choice = p;
	  break;
	} else if (p-> priority ==2 && p->time_quantum <4) {
	    choice = p;
	    break;		
	}			
	
	
	//if have higher priority
      if(p->priority < highest_priority){
        choice = p;
	highest_priority = p->priority;

       } else if(p->priority == choice->priority) {
          if(p->time_allotment > choice->time_allotment){
	    choice = p;
	   }
         }

      }

    }
		
    if(onoff ==0){
      release(&ptable.lock);
      wait();
      run_mlfq_onoff = 0;
      exit();
    }
    c->proc = choice;
    curproc->state = RUNNABLE;

    switchuvm(choice);
    choice->state = RUNNING;

    choice->time_quantum--;
    choice->time_allotment--;

    if(choice->time_allotment == 0){
      choice->priority++;

      if(choice->priority ==1){
        choice->time_quantum = 2;
        choice->time_allotment = 10;
      } else if(choice->priority ==2){
          choice->time_quantum = 4;
          choice->time_allotment = 110; //priority boost : per 100 ticks      
      }

    } else {  //just burn out time_quantum (do not priority down)
        if(choice->time_quantum ==0){

          if(choice->priority ==0) {
            choice->time_quantum = 1;
          } else if(choice->priority ==1) {
            choice->time_quantum = 2;
          } else {
            choice->time_quantum = 4;
          }

       }  
    }

    swtch(&curproc ->context, choice ->context);
    switchkvm();
	

    priority_boost --;
   //priority boost exec
    if(priority_boost == 0) {
      priority_boost = 100;
      for(p = ptable.proc ;  p < &ptable.proc[NPROC] ; p++){
        if(p->is_run_mlfq == 1 &&p->priority != 0){
          p->priority = 0;
          p->time_quantum = 1;
          p->time_allotment = 5;
        }
      }

    }


    release(&ptable.lock);
  }
}



///////for thread/////////////////////////////////////////////////////////////////////////////////////

uint total_thread_stack_end = KERNBASE - PGSIZE;

//before RUNNABLE
struct proc* alloc_shard_proc ( struct proc * curproc)
{
  struct proc * np;
  int i;  

  if( (np = allocproc()) == 0)
    return 0;

  acquire(&ptable.lock);
  np -> sz = total_thread_stack_end;


  np -> is_run_mlfq  = 0;
  np -> cpu_share= 0;
  np -> pgdir = curproc -> pgdir;
  np -> pid = curproc -> pid;
  np ->parent = curproc;
  *np ->tf = *curproc -> tf;
  np -> chan = 0;
  np -> killed = curproc->killed;
  np -> is_worker_thread = 1;  

 
  for(i = 0 ; i<NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np -> cwd = idup(curproc-> cwd);
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  set_stride();
  release(&ptable.lock);

  
  return np;
}






int thread_create(thread_t * thread, void * (*start_routine)(void *), void *arg)
{
  

  struct proc *np;
  struct proc *curproc = myproc();
  uint  sz, sp;
  int temp;
  np = alloc_shard_proc(curproc);

  //make thread routine
  acquire(&ptable.lock);
  sz = total_thread_stack_end;
  temp = sz - 2*PGSIZE;
  
   
  if( (temp = allocuvm(np->pgdir, total_thread_stack_end -2*PGSIZE, total_thread_stack_end)) ==0 )
  {
    release(&ptable.lock);
    return -2;
  }


  total_thread_stack_end -= 2*PGSIZE;
  clearpteu(np->pgdir, (char*)(total_thread_stack_end));


  sp = sz; 
 
  sp = sp -4;

  *((uint*)sp) = (uint)arg;

  sp -= 4;
  *((uint*)sp) = 0xffffffff;


  np -> tf ->eip = (uint)start_routine;
  np -> tf -> esp = sp;

  np -> state = RUNNABLE;

  *thread = np->tid;

  np->pass = curproc->pass;
  np->is_run_mlfq = curproc->is_run_mlfq;
  if(np->is_run_mlfq == 1){
    np->priority = 0;
    np->time_quantum = 1;
    np->time_allotment = 5;
  }
  set_stride();
  //np->pass = get_min_pass();
 
  release(&ptable.lock);
  
 
  return 0; //success 
}






void thread_exit(void * retval)   //execute in thread
{
  struct proc *curproc = myproc();
 
  acquire(&ptable.lock);
  curproc->thread_ret = retval;
  wakeup1((void*)curproc);
  curproc-> state = ZOMBIE;


  sched();
 
  release(&ptable.lock);  
}





void thread_dealloc(struct proc * p)  //current thread memory dealloc
{
  int sz;
  int fd;

  for(;;) {
    sz = total_thread_stack_end;
    if( sz == (PGROUNDDOWN(p->tf->esp) -PGSIZE))
    {

  
      acquire(&ptable.lock);

      p-> state = UNUSED;
      kfree(p->kstack);
      p->kstack = 0;
      p ->parent = 0;
      p -> name[0] = 0;
      p -> killed = 0;
      p -> tid = 0;
      release(&ptable.lock);
 

      for(fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd]){
          fileclose(p->ofile[fd]);
          p->ofile[fd] = 0;
        }
      }
      begin_op();
      iput(p->cwd);
      end_op();
      p->cwd = 0;



      acquire(&ptable.lock);
      total_thread_stack_end =deallocuvm(p->pgdir, sz+2*PGSIZE, sz);

      total_thread_stack_end += 2*PGSIZE;

      p -> pid = 0;
      p -> pgdir = 0;
      p -> sz = 0;
      p -> tid = 0;
      release(&ptable.lock);
      for(p = ptable.proc; p< &ptable.proc[NPROC]; p++)
        if(total_thread_stack_end == (PGROUNDDOWN(p->tf->esp) - PGSIZE) && p->state == ZOMBIE)
          break;


      if( p == &ptable.proc[NPROC])
        break;

      continue;
    }
    break;
  }

}

 

int thread_join(thread_t thread, void **retval)  //execute in master thread
{

  struct proc *p; 
  //int sz;

  acquire(&ptable.lock);

  for(p = ptable.proc; p< &ptable.proc[NPROC]; p++){
    if(p->tid == thread)
      break;
  }

  if( p == &ptable.proc[NPROC]){
    release(&ptable.lock);
    return -1;
  }


  while(p->state !=ZOMBIE)
    sleep((void*)p, &ptable.lock);

  *retval = p->thread_ret;
  p->thread_ret = 0;


  release(&ptable.lock);
  thread_dealloc(p);

  set_stride();
  return 0;
}



void for_exec(struct proc * curproc)
{
  struct proc * p;
  int fd;
  //except cur thread, make all thread UNUSED.
  acquire(&ptable.lock);
  for( p = ptable.proc; p< &ptable.proc[NPROC]; p++) {
    if(p != curproc && p->pid == curproc->pid) {
      p->state = UNUSED;
      p->kstack = 0;
      p->parent = 0;
      p->name[0] = 0;
      p-> killed = 0;
      p->pid =0 ;
      p->pgdir = 0;
      p-> sz = 0;
  release(&ptable.lock);
  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      fileclose(p->ofile[fd]);
      p->ofile[fd] = 0;
    }
  }
  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;
  acquire(&ptable.lock);
    }
  }

  for( p = ptable.proc; p< &ptable.proc[NPROC]; p++) 
    if(p->parent->pid == 2)
      break;



  if(curproc->is_worker_thread !=0)
  {
   // cprintf("for exec %d\n", p->pid);
    curproc->parent = p;
    curproc->is_worker_thread = 0;
  }

  release(&ptable.lock);





}






























