# First Step에서의 변경점 (interaction을 위해)

### 기존 스택 방식

![첫스텝_부분설명1](/uploads/afdf548ff8a15f0fcdab614cd015229c/첫스텝_부분설명1.png)


* Interaction시, 많은 문제점이 존재하는 방식이였습니다

>
1. sz가 힙 영역이 생기는 걸 고려하지 않았기 때문에, 힙 영역이 생기는 즉시 해당 방식은 사용할 수 없게 됩니다.
2. 연쇄적인 종료방식 역시 사이에 힙 영역이 들어가 버린다면 사용할 수 없게 됩니다.


### 새로운 스택 방식 

![새로운스택](/uploads/81e0c3b4aa800cb89b102eefb125c33d/새로운스택.png)

>
* 힙 영역과의 interaction을 위해 위에서부터 스택을 할당시켜주는 방식입니다.
* 연쇄적인 종료를 하기 때문에, KERNBASE를 기준으로, EMPTY 페이지 1 + 최대 쓰레드 개수(63) * 2
 = 127 페이지보다 아래로 스택이 할당되지 않습니다.



### proc 구조체
```c
  int is_worker_thread ;
```
>
*해당 변수를 추가하였습니다.
* 값이 0인 경우 master thread를 나타내고, 값이 1인 경우 worker thread임을 나타냅니다.
* allocproc함수에서 기본적으로 0으로 초기화를 해줍니다.
* 만약 thread_create함수가 호출된다면, 1로 값을 변경해줍니다.


### alloc_shard_proc 함수

```c
np -> is_worker_thread = 1;
```
>
*  master와 worker를 구분하기 위해 is_worker_thread의 값 변경이 이루어집니다.

 

### thread_create 함수
```c
  if( (temp = allocuvm(np->pgdir, total_thread_stack_end -2*PGSIZE, total_thread_stack_end)) ==0 )
  {
    release(&ptable.lock);
    return -2;
  }
```
>
* 스택을 위에서 아래로 쌓아가는 방식입니다.


```c
  np->pass = curproc->pass;
  np->is_run_mlfq = curproc->is_run_mlfq;
  if(np->is_run_mlfq == 1){
    np->priority = 0;
    np->time_quantum = 1;
    np->time_allotment = 5;
  }
  set_stride();
```
>
* 제가 만든 스케줄러와의 호환성을 위해 스케줄러와 관련된 값 변경을 해줍니다.
* 제가 만든 스케줄러에서 fork를 해줄 때 어떻게 스케줄했는지에 대한 원리를 그대로 따라갑니다. 

### thread_dealloc 함수

* 기존의 thread_join 함수에서의 메모리 해제 루틴을 재사용성을 위해 따로 만들었습니다.

```c
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
```
>
* first step에서는 쓰레드를 정리할 때, 각 쓰레드별로 file 관련된 처리를 하지 않았었는데 이 함수에서는 파일들 관련 처리도 해주게 됩니다.
* 나머지 부분은 first step과 동일한 방식입니다.

### thread_join 함수
* 메모리관련 해제 루틴을 thread_dealloc으로 빼준 것 이외에는 first step과 동일합니다.


# Interaction with other services in xv6 

### exit 함수

##### 기본 디자인
1. 어떤 쓰레드에서 호출하던 같은 Pid를 갖는 프로세스는 모두 종료돼야한다.
2. 메모리 관련 정리는 호출한 쓰레드가 아닌 무조건 master 쓰레드가 행한다.

##### 코드를 통한 구현 방식 설명

```c
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
    release(&ptable.lock);
    while(1) {
      acquire(&ptable.lock);
      sched();
      release(&ptable.lock);

    }
  }



```
1. 해당 프로세스의 master 쓰레드를 찾습니다. (master라는 변수에 저장)
2. worker 쓰레드에서 호출된 경우 master 쓰레드가 SLEEPING 상태일 수 있으므로, RUNNABLE로 바꾸어줍니다. 
3. master의 killed값을 non-zero로 만들어줍니다. (마스터 쓰레드가 RUN될 때 exit 루틴으로 분기할 수 있게하기 위해 
4. worker 쓰레드에서 sched()를 호출해 스케줄되는 쓰레드를 변경시켜줍니다. 결국, 다음 번에 메인 쓰레드가 실행될 때 모든 정리가 이루어집니다.  

<br>
*여기부터는 mater thread만 실행할 수 있는 부분입니다.

```c
  for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++)
    if(p->pid == master->pid && p->is_worker_thread == 1){
      p->state = ZOMBIE;
      thread_dealloc(p);
    }
```
5.master 쓰레드에서 worker 쓰레드들의 상태를 ZOMBIE로 바꾸어준 후 ( 연쇄 종료가 ZOMBIE인 경우만 취급하기 때문에) <br>
6.worker 쓰레드의 메모리를 정리해줍니다. <br>
7.나머지 부분은 원래의 exit루틴과 동일하기 때문에, 따로 언급하지 않겠습니다. <br>

### fork 함수

##### 시도하려 했던 방식 (불가능함)

![불가능fork](/uploads/b2c30e3d08773899a7a376b9d21ba641/불가능fork.png)

>
* 설계 이유 : 위쪽 영역을 모두 지움으로써, 더 깔끔하게 메모리를 사용할 수 있을거라 생각했습니다.
* 실패 이유 : MASTER STACK 영역으로 복사를 한다면 결국 함수의 리턴 콜 역시 복사가 되는데, 이 경우 리턴 어드레스는 아직도 STACK 1 의 주소영역을 가리키기 때문에, VALIE하지 않은 페이지를 참조하게 됩니다. 즉, 오류가 발생합니다.


##### 구현한 방식

![사용한_fork](/uploads/1732d375af7aaf5a97b0880130171838/사용한_fork.png)

>
* 원래 영역의 가상 주소값을 그대로 사용함으로써 위의 문제점을 해결했습니다.
* 대신 아래의 원래 스택 영역은 사용하지 않게되기 때문에 페이지 하나의 메모리 leak가 발생하게 됩니다.

##### 코드를 통한 구현 방식 설명

```c
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
```
1. master thread는 원래 fork와 동일하게 동작합니다.
2. work thread는 원래 fork의 동작에 현재 쓰레드의 스택 영역을 복사하는 기능이 추가된 copyuvm2함수를 이용합니다. 나머지는 원래의 fork와 동일합니다.


### exec 함수

* for_exec이라는 함수 루틴을 추가한 것 외에는 변경 사항이 없습니다.

##### for_exec 함수 코드
```c
void for_exec(struct proc * curproc)
{
  struct proc * p;
  int fd;

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
```
>
* 현재 쓰레드를 제외한 다른 쓰레드를 UNUSED 상태와 같게 만들어줍니다.
* 파일 관련한 것들은 모두 정리해줍니다.
* 메모리를 여기서 직접 정리하지 않는 이유는 exec 루틴의 마지막에 freevm을 호출함으로써 모든 영역에 대한 메모리 해제가 일어나기 때문입니다.
* 현재 쓰레드가 마스터 쓰레드인 것처럼 값 변경을 해줍니다. (exec은 결국 다른 프로그램으로 만드는 일이기 때문에 해당 쓰레드가 결국 마스터 쓰레드가 됩니다.)

### sbrk 함수

* 직접적인 sbrk 함수는 존재하지 않기 때문에 sys_sbrk 함수와 growproc 함수의 값 변경이 이루어졌습니다.

##### sys_sbrk 함수
```c
  if(p->is_worker_thread ==0)
    addr = p->sz;
  else
  {
    addr = p->parent->sz;
  } 
```
 * sys_sbrk의 리턴 값은 항상 늘어나기 직전의 sz값을 반환해야되기 때문에, 힙에 대한 sz는 마스터 쓰레드의 sz와 같으므로 마스터 쓰레드의 sz를 반환하도록 합니다.

##### growproc 함수
```c
  sz = curproc->sz;
  if(curproc->is_worker_thread == 1)
    sz = curproc ->parent ->sz;

...

  if(curproc->is_worker_thread == 1)
    curproc->parent->sz =sz;
  else
    curproc->sz = sz;
```
* 위에서 언급했다시피 마스터 쓰레드의 sz를 기준으로 힙 영역이 생성, 제거돼야 하기 때문에 그에 대한 sz 처리를 해주는 과정입니다.

### kill 함수

* exit 설계 방식에 따라, 쓰레드들 중 하나라도 exit 루틴에 들어온다면 결국 모든 쓰레드가 종료되기 때문에, 간단하게 모든 쓰레드의 killed값을 non-zero로 만들어주는 과정을 추가했습니다.

```c
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
```
### pipe 함수, sleep 함수

* 파일 관련한 모든 처리를 쓰레드 생성, 종료 등에 적용시켜 놨기 때문에, pipe함수에 관련한 추가적인 변경은 없었습니다.

* sleep 함수역시 쓰레드 생성 시 기본적으로 되기 때문에, 추가적인 변경은 없었습니다. 


### 결과 분석

##### 기본 테스트 케이스 결과
![두번째결과0_3](/uploads/3fb12f5634e30891c7d1aeb962068e94/두번째결과0_3.png)
![두번째결과4_6](/uploads/82e8ae40011eb0952387e04eb5dffedf/두번째결과4_6.png)
![두번째결과7](/uploads/2950490c769cf528bfd3673f234eef2d/두번째결과7.png)
![두번째결과8](/uploads/8a0d6fc3afba02dad36b9f46a2c9f5a0/두번째결과8.png)
![두번째결과9_12](/uploads/1a7b1dae313f1e1ef36a06fd3775f8e1/두번째결과9_12.png)
![두번째결과13](/uploads/96b15219f8ca983a77294b2a9fee5675/두번째결과13.png)

* 모든 경우에 정상 동작함을 볼 수 있다.
* stridetest의 경우 1:1의 protion을 갖는 걸 볼 수 있는데, 그 이유는 cpu_share를 만들 당시 모든 프로세스의 합이 20을 넘지 않게 짰기 때문에, 그걸 지켜주기위해 쓰레드에서도 fork와 같은 원리로 만들었기 때문입니다. ( cpu_share는 초기화하고, 자신이 속한 스케줄러만 지켜주는 방식)

#### 추가적인 결과
![sbrk결과](/uploads/32adb391889eae267f1715144ea48144/sbrk결과.png)

* 모든 thread가 동시에 sbrk를 호출해도 정확히 1000씩 10번 증가하는 것을 확인할 수 있습니다