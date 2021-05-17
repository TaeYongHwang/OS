# 코드 설명

### alloc_shard_proc 함수

```c
struct proc* alloc_shard_proc ( struct proc * curproc)
{
  struct proc * np;
  int i;

  if( (np = allocproc()) == 0)
    return 0;
  
  acquire(&ptable.lock);
  np -> sz = curproc ->sz; 
  np -> pgdir = curproc -> pgdir;
  np -> pid = curproc -> pid;
  np ->parent = curproc; 
  *np ->tf = *curproc -> tf;
  np -> chan = 0;
  np -> killed = curproc->killed;
  
  for(i = 0 ; i<NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np -> cwd = idup(curproc-> cwd);
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  
  release(&ptable.lock);
  return np;
}
```
>
* thread_create 함수에서 사용되는 함수입니다.
* 유저 영역 스택 할당을 제외한 대부분의 할당이 여기서 일어납니다.
* 쓰레드는 메모리를 공유하기 때문에, 대부분의 영역을 새로 할당하는 것이 아닌 주소값을 그대로 할당받아 같은 메모리 영역의 사용을 보장합니다.


### thread_create 함수

```c
  sz = PGROUNDUP(np->sz);
  if((sz = allocuvm(np->pgdir , sz, sz + 2*PGSIZE)) == 0)
    return -2;
  clearpteu(np->pgdir, (char*)(sz - 2*PGSIZE));
```
>
* 유저 스택용 페이지를 2개 할당받는다.
* 하나의 페이지는 guard page로써, 서로 다른 쓰레드의 스택에 간섭하지 않게 보장해준다.

```c
  sp = sz;
  sp = sp -4;
  *((uint*)sp) = (uint)arg;
  sp -= 4;
  *((uint*)sp) = 0xffffffff;
  for(p = ptable.proc; p<&ptable.proc[NPROC]; p++) {
    if(p->pid == np->pid)
      p -> sz= sz;
  }
  np -> tf ->eip = (uint)start_routine;
  np -> tf -> esp = sp;
  np -> state = RUNNABLE;
```
>
* 할당받은 스택에 argument와 fake pc값을 넣어준다. (커널 스택 영역에서 유저 스택 영역으로 나가기 때문에, argint같은 방식으로는 argument를 전달할 수 없다.)
* 같은 주소 공간을 사용하는 쓰레드들 (같은 프로세스에서 파생된 쓰레드들)의 sz값을 동일하게 업데이트 시켜준다.
* 트랩 프레임의 eip 즉, 다음 실행할 주소값에 start_routine 함수의 주소값을 넣어주고, 동일하게 esp값 역시 새로 할당한 쓰레드 전용 스택을 가리키게 한다.
* 마지막으로, 만들어진 쓰레드의 tid를 thread parameter에 넣어줌으로서,  thread_create를 호출한 원래 프로세스에서 join할 수 있게 해준다.


### thread_exit 함수
```c
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
```
>
* 그저 쓰레드의 루틴이 끝났을 때, 공유 영역에 리턴값을 채운 후(join에서 받아간다.) 상태를 ZOMBIE로 바꾼 후 sched를 호출해 다시 스케줄링되지 않게 한다.
* 만약 해당 함수가 실행되기 전에 thread_join이 먼저 호출된다면 wakeup1을 통해 master thread를 깨워주게 된다.

### thread_join 함수
```c
  for(p = ptable.proc; p< &ptable.proc[NPROC]; p++){
    if(p->tid == thread)
      break;
  }
  if( p == &ptable.proc[NPROC]){
    release(&ptable.lock);
    return -1;
  }

```
>
* 현재 thread_join으로 기다릴 thread를 찾는다.
* 존재하지 않는 경우 -1을 리턴하게 된다.

```c
  while(p->state !=ZOMBIE)
    sleep((void*)p, &ptable.lock);
  *retval = p->thread_ret;
  p->thread_ret = 0;
```
>
* thread_exit가 호출되어 종료된 경우 리턴값을 가져온다.
* thread_exit가 아직 호출되지 않은 경우 채널을 해당 쓰레드로 걸고 값이 들어올 때까지 SLEEPING 상태로 기다리게 된다.

```c
  for(;;) {
    sz = p->sz;
    if( sz == PGROUNDUP(p->tf->esp))
    {
      p-> state = UNUSED;
      kfree(p->kstack);
      p->kstack = 0;
      p ->parent = 0;
      p -> name[0] = 0;
      p -> killed = 0;

      sz = deallocuvm(p->pgdir, sz, sz-2*PGSIZE);

      for(q = ptable.proc; q<&ptable.proc[NPROC]; q++) {
        if(q->pid == p->pid)
          q -> sz= sz;
      }

      p -> pid = 0;
      p -> pgdir = 0;
      p -> sz = 0;

      for(p = ptable.proc; p< &ptable.proc[NPROC]; p++)
        if(sz == PGROUNDUP(p->tf->esp) && p->state == ZOMBIE)
          break;


      if( p == &ptable.proc[NPROC])
        break;

      continue;
    }
    break;
  }
```


![첫스텝_부분설명1](/uploads/849a7914202b4e93b9fcf289279e2afa/첫스텝_부분설명1.png)

>
* 해당 부분은 유저 스택 영역에서, 드문드문 free되는 경우를 방지시켜준다.
* thread_join을 호출한 쓰레드의 유저 스택이 가장 위에 호출돼있는 경우 즉,sz == PGROUNDUP(p->tf->esp)인 경우에만 ZOMBIE thread를 처리한다.
* 연속적인 ZOMBIE thread를 갖는 경우, ZOMBIE thread가 아닌 부분까지 동시에 처리해준다.
* 쓰레드들 중 맨 위에 스택이 존재하지 않는 쓰레드들은 join함수 호출 시에도 메모리의 정리가 바로 되지 않기 때문에, 약간의 메모리 낭비가 존재하게 된다. (xv6에서는 스케줄링 할 수 있는 쓰레드의 개수가 64개이기 때문에, 약간이라고 표현하였다.)

# Test Case 결과
![첫스텝결과1](/uploads/dacb9b308a255f5f1affc751aa2114a0/첫스텝결과1.png)

![첫스텝결과2](/uploads/3e72c8f3e92a22844874ba8165bf55b4/첫스텝결과2.png)

*
> 모든 경우에 출력값의 순서 변화만 있을 뿐 정확하게 동작하고 있는 것을 확인할 수 있다.


# Solved Problem

* 처음에 디자인 했을 때, thread_exit의 리턴 값을 thread_join에 넣어주기 위해 따로 만들어 둔 공간을 master thread에 둬서, 하나의 쓰레드씩 접근하고 나머지 쓰레드의 thread_exit에서는 스핀락을 해둔 채로 기다리는 형식이였다. 이렇게 했더니  master thread에서 2번 쓰레드를 join할 때, 1번 쓰레드의 값이 채워지는 경우, 교착 상태가 발생하였다. -> 해당 공간을 각 worker thread마다 넣어주는 형식으로 바꾸었더니 교착 상태가 발생하지 않고, 스핀락을 따로 걸어주지 않아도 돼 성능 면에서도 향상된 것 같다.
<br><br>























































































