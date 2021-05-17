# 실수
> Makefile에 삭제된 연습용 자료 mlfq.c가 등록돼있어 현재 실행이 안되는 상태입니다. 죄송하지만 그것만 지워주시고 실행해 주시면 감사하겠습니다. 


# 추가된 디자인 

## Stride scheduler와 MLFQ scheduler 연동

![기본](/uploads/43ae14ebe7e6f3bf391af0d936a6f845/기본.png)

>

* MLFQ 스케줄러는 Stride 스케줄러에서 스케줄됩니다.
* run_MLFQ를 호출한 프로세스들은 Stride의 통제에서 벗어나 MLFQ 스케줄러의 스케줄을 받게 됩니다.
<br> -> 결국 두 번의 스케줄러를 거치게 되기 때문에 context 스위치 오버헤드가 증가하게 됩니다.
* run_MLFQ()가 처음 호출될 경우 (MLFQ에서 스케줄되는 프로세스가 없는 경우) fork()를 통해 부모 프로세스가 MLFQ 스케줄러로 역할을 하게 되고, 자식 프로세스가 원래 부모 프로세스가 돌던 내용을 마무리하게 됩니다.



## cpu_share function

![cpu_share예외](/uploads/12315fc360bd2dc7680685ed2cb70a7e/cpu_share예외.png)






> 
* 요청한 CPU portion이 20%를 넘어가는 경우 해당 프로세스들은 원래 스케줄되던 스케줄러에서 계속 스케줄됩니다.
* MLFQ에서 cpu_share를 호출했을 때 , Exception에 걸리지 않는다면, MLFQ를 빠져 나와 Stride 스케줄러에서 요청한 CPU portion으로 스케줄됩니다.
* cpu_share를 호출한 프로세스가 종료 시 exit 함수 내부에서 호출한 값과 동일한 음수값으로 cpu_share를 재호출 시킴으로써 정확한 exception handling이 되게끔 유지시켰습니다.

## set_stride function,          get_min_pass function
>
프로세스 생성, 종료, Sleeping, Awake 시에 해당 함수들을 호출해 stride scheduler의 fairness를 유지시켰습니다.








# 코드 설명

###  void scheduler (void)

``` c
    for (p = ptable.proc; p< &ptable.proc[NPROC] ; p++) {
      if(p->state != RUNNABLE )
        continue;
      if (( p->is_run_mlfq ==0 ||p->priority ==4 )&&  p->pass < min_pass) {
        min_pass = p->pass;
        min_pass_proc = p;
      }
    }
```
* RUNNABLE 프로세스 중에서 패스 값이 가장 작은 프로세스를 선택하는 과정입니다.


``` c
    c->proc = min_pass_proc;
    switchuvm(min_pass_proc);
    min_pass_proc->state = RUNNING;
    min_pass_proc->pass += min_pass_proc->stride;
    swtch(&(c->scheduler) , min_pass_proc -> context);
    switchkvm();
    c->proc = 0;
    min_pass = 999999999;
```
* 찾은 프로세스에 CPU를 할당 (스위치)시키는 부분입니다. 
* fair한 비율을 위해, 패스값에 stride를 더해서 저장합니다.


### int get_min_pass (void)
``` c
int get_min_pass(void)
{
  struct proc * p;
  int min_pass = 999999999;
  for( p = ptable.proc; p < &ptable.proc[NPROC] ; p++){
    if(p->state == RUNNABLE && p->is_run_mlfq == 0 ) {
      if(min_pass > p->pass)
        min_pass = p->pass;
    }
  }
  if(min_pass == 999999999) {
    return 0;
  } else {
      return min_pass;
  }
}
```
* stride 스케줄러에서 스케줄되는 RUNNABLE 프로세스 중에서 가장 작은 패스값을 찾아 반환해줍니다.

### void set_stride(void)

``` c
  for(p = ptable.proc; p<&ptable.proc[NPROC] ; p++){
    if(p->state == RUNNABLE){ //|| p->state == RUNNING || p->state == EMBRYO){
      if(p->cpu_share ==0 && p->is_run_mlfq ==0)
        pair_share_proc_num ++;
        if(p->cpu_share !=0){  //called cpu_share or mlfq_scheduler
          pair_cpu -= p->cpu_share;
        }
      }
  }
```
* stride scheduling의 원활한 실행을 위해 시스템 콜을 호출하지 않은 프로세스들이 정확한 CPU portion을 갖고 실행할 수 있게 도와주는 코드입니다.

``` c
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
      if(p->cpu_share != 0){ 
        p->stride  = BIG_VALUE/(10 * p->cpu_share);
      }     
    }
  }
```
* MLFQ scheduler가 존재하면 20% 만큼의 CPU portion을 할당합니다.
* cpu_share system call을 호출한 프로세스가 존재하면 각각 요청한 CPU portion을 할당합니다.
* 남은 CPU portion을 남은 프로세스들이 N등분해서 나눠가지게 됩니다.
* pair_ticket을 구할 때, 1을 더해주는 이유는 버려지는 소수점 부분이 있을 경우, stride를 조금이라도 더 잘 맞추기 위해서 해주었습니다.

### void cpu_share(int percent)

``` c
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
```
*  프로세스에서 요청하는 percent는 무조건 >=0이라고 가정하고 만들었습니다.
*  모든 프로세스에서 요청한 cpu_share가 20%를 넘어가지 않는 경우에만 CPU portion을 할당합니다.
* 해당 프로세스가 cpu_share를 호출할 때, 20%가 넘어간다면, 이 프로세스는 CPU portion을 할당받지 않고 원래 돌던 scheduler 내에서 계속 스케줄링 되게 합니다.
* MLFQ 스케줄러에서 수행되던 프로세스가 cpu_share를 호출하는 경우, MLFQ를 빠져나와 stride 스케줄러에 할당하고 요청한 CPU portion을 할당합니다.
* percent < 0인 경우 역시 만들었는데, 이 경우는 exit()함수 내부에 선언되어, 종료 시 total_percent를 유지하게 해줍니다.



### void run_MLFQ(void)

``` c
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
```
* run_mlfq_onoff 변수를 통해 MLFQ 스케줄러가 존재하지 않는 경우 (run_mlfq_onoff == 0), fork를 통해서 부모 프로세스를 MLFQ 스케줄러로 만들어준다.
* MLFQ에서 돌게끔 프로세싱 해주기 위해 프로세스의 값을 재설정 해줍니다.  
* cpu_share를 호출한 프로세스가 run_MLFQ를 호출하면, 할당된 CPU portion을 반납하고 난 후 MLFQ 스케줄러로 분기하게 됩니다. 


>
 fork()한 후, 자식 프로세스인지 부모 프로세스인지 확인하는 과정이 별도로 없지만, fork()를 호출한 이후에는 무조건 부모 프로세스만 존재하고, 자식 프로세스는 함수 밖으로 분기하게 됩니다.<br/>
왜냐하면, run_MLFQ() 자체가 시스템 콜이기 떄문에, fork 함수로는 run_MLFQ 내부에서 어디까지 실행했는지를 복사하지 않기 때문입니다.  <br/>
 

### int getlev(void)

```c
  int lev;
  struct proc *cur_proc = myproc();
  lev = cur_proc-> priority;
  return lev;
```
* 현재 실행 중인 프로세스가 도는 MLFQ 큐의 레벨을 출력시킵니다.

### void mlfq_scheduler(void)

``` c
  pushcli();
  c = mycpu();
  popcli();
```
* 시스템 콜에서 mycpu를 호출하기 위해서는 pushcli()를 통해 인터럽트를 block시켜놓고 실행해야 합니다.

``` c
    for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;  
      if(p->is_run_mlfq == 1){
        onoff ++;               
        if(p->priority ==1 && p->time_quantum <2){
          choice = p;
          break;
        } else if (p-> priority ==2 && p->time_quantum <4) {
            choice = p;
            break;
        }
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
```
* RUNNABLE 프로세스 중에서 MLFQ에서 실행해야되는 프로세스 중 실행되어야 하는 프로세스를 선택해줍니다.
* 실행 중인 프로세스가 time quantum을 다 사용하지 않았다면 그 프로세스 먼저 실행되게 합니다.
* 실행 중인 프로세스가 없을 때, 즉 time quantum을 다 써서 다른 프로세스를 선택해야 할 때, 가장 우선순위가 높고, time allotment가 많이 남아있는 것을 뽑아 각 큐에서는 RR 방식으로, 높은 우선순위를 갖는 큐에서 실행하게 합니다.

``` c
if(onoff ==0){
      release(&ptable.lock);
      wait();
      run_mlfq_onoff = 0;
      exit();
    }
```

* 만약 MLFQ에서 수행되는 프로세스들이 모두 종료된 상태라면, MLFQ 자체도 종료시켜 CPU portion 20%를 반납하게 합니다.
* 나중에 다시 run_MLFQ를 호출할 프로세스가 있을 수 있으므로, 전역변수 run_mlfq_onoff를 0으로 만들어 나중에 다시 스케줄러를 만들 수 있도록 합니다.
* wait는 해당 프로세스가 만든 프로세스 ( 스케줄러로 분기하기 전에 실행하던 코드를 마저 실행시키는 프로세스)가 좀비 프로세스가 되지 않게하기 위해 선언했습니다.

``` c
    c->proc = choice;
    curproc->state = RUNNABLE;
    switchuvm(choice);
    choice->state = RUNNING;
```

>
MLFQ 스케줄러 자체의 state를 RUNNABLE로 왜 직접 바꿔줘야 했는지? <br/>
해당 프로세스는 스케줄러 역할을 하지만, 디폴트 스케줄러가 아니기 때문에 yield를 호출하지 않습니다. 그래서 직접 RUNNABLE로 바꿔주지 않으면 stride 스케줄러에서 MLFQ가 동작하지 않게 됩니다.
<br/> 
그 후 선택한 프로세스의 state를 RUNNING으로 바꿔줍니다. (선택된 프로세스는 추후에 타이머 인터럽트를 맞으면 자동으로 yield를 호출하기 때문에, RUNNABLE로 바뀌게 됩니다.)

``` c
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
```

* 선택한 프로세스의 MLFQ 내부에서의 상태를조정시키고, 해당 프로세스로 분기합니다.


``` c
    priority_boost --;
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
```
* 100 tick이 끝나면 priority boost를 시행시켜, 최상위 큐를 제외한 나머지 큐에 존재하는 프로세스들을 최상위 큐로 올려줍니다.


# 기타 추가한 사항들

``` c
void sleep( ...)
static void wakeup1(...)
void exit()
int fork()
static struct proc* allocproc(void)
```
* 공통적으로, 원래 선언된 함수 안에 set_stride()함수를 넣어줌으로써 RUNNABLE한 프로세스의 개수가 변할 때마다 stride를 재설정 해주었습니다. 



``` c
static struct proc* allocproc()
...
 p->pass = get_min_pass();
 p->cpu_share = 0;
 p->is_run_mlfq = 0;
...


void fork()
...
  np->pass = curproc->pass;
  np->is_run_mlfq = curproc->is_run_mlfq;
  if(np->is_run_mlfq == 1){
    np->priority = 0;
    np->time_quantum = 1;
    np->time_allotment = 5;
  }
...
```
* 위의 부분을 추가해서 fork 후에 자식 프로세스는 어떤 상태를 갖게 되는지
1. 부모 프로세스가  stride 스케줄러에서 동작한다면, 자식 프로세스는 그대로 stride 스케줄러에서 동작하게 해줍니다.

2. 부모 프로세스가 mlfq 스케줄러에서 동작한다면, 자식 프로세스 역시 mlfq 스케줄러에서 동작하게 됩니다.

3. cpu_share를 호출한 부모 프로세스의 자식 프로세스는 동일한 CPU portion을 할당받는 것이 아닌 default 프로세스처럼 (아무 시스템 콜을 호출하지 않은 프로세스) 동작하게 됩니다.




# 결과 분석

![이미지_001](/uploads/984d9a12ade2a9641d3210076cf7ba57/이미지_001.png)


*  cpu_share (5)를 한 프로세스를 기준으로
1. cpu_share (15)를 한 프로세스는 약 3배로 거의 딱 맞는 CPU potion을 갖습니다.
2. MLFQ에서 동작하는 두개의 프로세스의 합이 약 3.7배로, 기준의 4배 보다는 약간 낮은 값을 갖습니다.
3. DEFAULT 값은 약 8배의 값을 갖습니다.

>
cpu_share (5)가 전체 cpu의 5%를 점유하고 있다고 가정할 떄, 모든 프로세스의 합이 80%가 되지 않습니다. <br>

실제 비율로는 cpu_share (5)가 약 6%, cpu_share (15)가 약 18%,  MLFQ는 총 23%,  Default는 50%의 비율을 갖고 있습니다.

<br>
해당 결과로 미루어 볼 때, Default를 제외한 다른 프로세스들이 Default의 CPU portion을 조금을 가져가는 것 같습니다. <br>
set_stride에서 stride를 계산할 떄, 사라지는 소수점 값이 이정도의 오차를 발생시는 것 같습니다.


<br><br>
![5디폴트](/uploads/e469fccde62ac74bad73569dc84119ef/5디폴트.png)

>
모든 프로세스가 디폴트인 경우, 동일한 CPU portion을 할당받는 것을 확인할 수 있습니다.

<br><br>

![5_15_디폴트_5_디폴트](/uploads/94edf53c25c7dfa96d6cf25fbe07de55/5_15_디폴트_5_디폴트.png)

>
workload의 순서를 cpu_share(5), cpu_share(15),  dafault, cpu_share(5), default 순서로 한 경우입니다.
<br>
cpu_share를 통해 20%가 넘는 CPU portion을 요청했기 때문에, 세 번째로 들어온 cpu_shar(5)는 오류 메세지를 출력하고, default 프로세스처럼 동작하게 됩니다.

















































































