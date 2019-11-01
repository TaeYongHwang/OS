# Stride Scheduling Design

### 1.  stride scheduling mechanism

##### 임의의 큰 수 : T,  현재 프로세스의 개수 : N 이라고 둔 후

- 프로세스 생성 시 <br>

> 스케줄러 안에 이미 존재하는 프로세스와 새로 생성된 프로세스의 stride 값을 재설정 해준 후, 원래 존재하는 프로세스 중 가장 작은 pass값을 새로 생성된 프로세스의 패스값에 할당시켜준다.<br>
 위의 방식이 아니라 패스값을 0으로 할당시킨 후 스케줄러에 넣게된다면, 다른 프로세스의 starvation을 야기할 수 있기 때문이다.

- 프로세스 종료 시 <br>

> 프로세스 생성과 마찬가지로 남아있는 프로세스들의 stride를 재설정해준다.<br>
   기본은 T/N으로 stride 재설정을 해주면 되지만, 종료된 프로세스가 cpu_share() 시스템 콜을 호출한 프로세스이거나 run_MLFQ()를 호출한 프로세스라면, stride 재설정에 주의를 기울여야 한다. <br>
 - run_MLFQ() 내부에 존재하는 2개 이상의 프로세스가 존재할 때, 그 중 하나가 종료되더라도, MLFQ자체의 cpu_portion이  20% 고정이기 때문에 stride 변화가 없다. <br>
 - run_MLFQ()의 마지막 프로세스가 종료되는 경우 MLFQ에 할당된 cpu_portion 20%를 나머지 프로세스가 가져가야 하기 때문에, stride의 재설정이 필요하다.
 - 마찬가지로, cpu_share()를 호출한 프로세스가 반환할 경우도 stride의 재설정이 필요하다.

- 기본적인 동작 상태일 때

> 1. 스케줄러에 있는 모든 프로세스 중, pass값이 가장 작은 값을 갖는 프로세스를 실행시킨다.<br>
2. 타이머 인터럽트를 만나면(할당된 시간이 끝나면) 패스값+= stride를 해준 후 cpu를 yield()한다.<br>
3. 1,2번을 반복한다.  <br>


### 2. 시스템 콜 호출
- 시스템 콜이 호출되지 않은 경우 <br>

> 모든 프로세스가 T/N 만큼의 stride를 갖는다. -> 결국 실행시간에는 RR방식과 동일하게 동작한다.

- cpu_share() system call이 호출될 경우

> - 만약 5%만큼을 cpu_share()를 통해 호출한다면, 나머지 95를 모든 프로세스가 동일하게 갖고, 시스템콜을 호출한 프로세스에 5%만큼을 더 배정시킨다. ( stride가 작을수록 cpu의 할당량이 커지는 것을 이용해서) -> stride의 재설정이 필요하다 <br>
- 모든 프로세스에서 cpu_share()를 통해 추가적으로 배정받은 값이 20%을 넘어가는 경우 exception handling을 해준다. <br>

- rum_MLFQ() system call이 호출될 경우

> - MLFQ scheduler 프로세스를 생성한 후, 해당 스케줄러에서 run_MLFQ()를 호출한 프로세스를 스케줄링하게끔 한다.<br>
- MLFQ의 cpu_portion이 20% 고정이기 때문에, 몇 개의 프로세스가 run_MLFQ를 호출하던 MLFQ 자체는 20% 만큼만 cpu를 할당받게된다. <br>
- run_MLFQ를 호출한 프로세스가 모두 종료된 후에는 MLFQ 스케줄러 프로세스 자체도 종료시켜 사용 중인  20%의 cpu_portion을 반납시킨다.
- stride의 재설정이 필요하다.


# MLFQ Scheduling Design

### MLFQ Scheduling Mechanism

- MLFQ의 구조

> 
- 3-Levl feedback queue로 만든다.
- 프로세스가 처음 스케줄러에 배정될 때, 최상위 큐에 배정된다.
- time allotment를 소진한 프로세스는 한 단계 한위 큐로 이동한다.
- 각 큐는 모두 RR 방식으로 동작한다.<br>
프로세스 A가 B보다 상위 레벨 큐에 있다면, A가 실행되고, 같은 레벨 큐에 존재하는 경우에는 RR방식으로
- 프로세스의 starvation을 예방하기 위한 priority boost가 존재
- 각 큐는 각기 다른 time quantum, time allotment를 갖는다.
- 20%의 고정된 cpu_portion을 갖는다.

- Time quantum

> 
- 하위 큐로 갈수록 cpu 사용량이 많은 프로세스이기 때문에 하위 큐의 time quantum을 상위 큐보다 크게 잡는 것이 효율이 좋다.
- 타이머 인터럽트의 배수로 time quantum을 설정해야 한다. (정확히 동기화 시점을 맞추기 위해서)



- Time allotment

> - Time quantum과 동일한 이유로, 하위 큐로 갈수록 크게 잡아준다.

- Priority boost

> starvation을 방지하기 위한 방법이므로, 적절한 N ticks를 찾아 N만큼의 ticks가 발생하면 두번 째, 세번 째 큐에 존재하는 모든 프로세스는 최상위 큐로 올려준다.

- MLFQ 동작 방식

>
1. 최상위 큐에서 시작해서, 주어진 Time allotment가 다 할 때까지 해당 큐의 time quantum만큼 씩 프로세스를 RR방식으로 실행시킨다.
2. Time allotment가 다 하면 한 단계 아래 큐로 가서 동일한 행동을 반복한다. 
3. 최하위 큐에서 Time allotment를 소진하면 최상위 큐로 다시 올라간다.
4. 위의 과정을 반복하다가, 처음에 설정한  N ticks에 도달하면 Priority boost를 실행시킨다.
5. MLFQ 스케줄러에 존재하는 모든 프로세스가 종료할 때까지 해당 과정들을 반복한다. 
6. 모든 프로세스가 종료되면 MLFQ 스케줄러 프로세스 자체도 종료시켜 20%를 다시 stride scheduler에 있는 다른 프로세스를 이 사용할 수 있게 한다.

 







