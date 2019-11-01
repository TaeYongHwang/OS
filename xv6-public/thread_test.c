#include "types.h"
#include "stat.h"
#include "user.h"
/*
void* t_function(void * data)
{

  int pid ;
  pid = fork();
  
  if (pid ==0){

  printf(1,"t_function pid = %d \n\n", pid);

  printf(1,"child!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  exit();
  }
 
  if( pid >0){
    printf(1,"pid >0\n");
     wait();
  }
  
 
// char * a = "ls";

// printf(1,"t_function sbrk : %d\n", sbrk(10000));

// printf(1,"t_function\n");


 //exec(a,&a);


 thread_exit(0);

 return 0;
}


void* q_function(void *data)
{

  int i;

 printf(1,"q_function tid : %d\n", getpid());

 
 for(i = 0; i<5; i++){

  printf(1,"afsdsa %d\n",i);
  //yield(); 
  sleep(100);

  }
 


 thread_exit(0); 
//  kill(getpid());  
  exit();
//  thread_exit(0);
return 0;
}

int main()
{
 //int pid;

//  char *a ="ls";
  thread_t p_thread[2];
         int thr_id;
        int status;
        char p1[] = "thread_1";
   //     char p2[] = "thread_2";


// pid = fork();  
        thr_id = thread_create(&p_thread[0], t_function, (void *) p1);
     //   printf(1,"gettid : %d\n", getpid());

      //  printf(1,"when is terminated?\n");
        if(thr_id <0)
        {
                printf(1,"thread create error : ");
        }

//        thr_id = thread_create(&p_thread[1],t_function, (void *) p2);
        if(thr_id <0)
        {
                printf(1,"thread create error : ");
        }


        thread_join ( p_thread[0], (void **)&status);
  //      thread_join ( p_thread[1], (void **) &status);


//	printf(1,"%d %d\n", p_thread[0], p_thread[1]);


// if(pid >0)
 //  wait();

//  for(;;){}
// sleep(700);


//  sbrk(100);
 // printf(1,"addr = %d\n", sbrk(1));

//  exec(a,&a);


  exit();
}
*/





#define NUM_THREAD 10
#define NTEST 5

// Show race condition
int racingtest(void);

// Test basic usage of thread_create, thread_join, thread_exit
int basictest(void);
int jointest1(void);
int jointest2(void);

// Test whether a process can reuse the thread stack
int stresstest(void);

int gcnt;
int gpipe[2];

int (*testfunc[NTEST])(void) = {
  racingtest,
  basictest,
  jointest1,
  jointest2,
  stresstest,
};
char *testname[NTEST] = {
  "racingtest",
  "basictest",
  "jointest1",
  "jointest2",
  "stresstest",
};

int
main(int argc, char *argv[])
{
  int i;
  int ret;
  int pid;
  int start = 0;
  int end = NTEST-1;
  if (argc >= 2)
    start = atoi(argv[1]);
  if (argc >= 3)
    end = atoi(argv[2]);

 

  for (i = start; i <= end; i++){
    printf(1,"%d. %s start\n", i, testname[i]);
    if (pipe(gpipe) < 0){
      printf(1,"pipe panic\n");
      exit();
    }
    ret = 0;

    if ((pid = fork()) < 0){
      printf(1,"fork panic\n");
      exit();
    }
    if (pid == 0){
      close(gpipe[0]);
      ret = testfunc[i]();
      write(gpipe[1], (char*)&ret, sizeof(ret));
      close(gpipe[1]);
      exit();
    } else{
      close(gpipe[1]);
      if (wait() == -1 || read(gpipe[0], (char*)&ret, sizeof(ret)) == -1 || ret != 0){
        printf(1,"%d. %s panic\n", i, testname[i]);
        exit();
      }
      close(gpipe[0]);
    }
    printf(1,"%d. %s finish\n", i, testname[i]);
    sleep(100);
  }
  exit();
}

// ============================================================================
void nop(){ }

void*
racingthreadmain(void *arg)
{
  int tid = (int) arg;
  int i;
  //int j;
  int tmp;
  
  for (i = 0; i < 10000000; i++){
    tmp = gcnt;
    tmp++;
    nop();
    gcnt = tmp;
  }
  thread_exit((void *)(tid+1));

  return 0;
}

int
racingtest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;
  gcnt = 0;
  
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], racingthreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0 || (int)retval != i+1){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1,"%d\n", gcnt);
  return 0;
}

// ============================================================================
void*
basicthreadmain(void *arg)
{
  int tid = (int) arg;
  int i;
  for (i = 0; i < 100000000; i++){
    if (i % 20000000 == 0){
      printf(1, "%d", tid);
    }
  }
  thread_exit((void *)(tid+1));

  return 0;
}

int
basictest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;
  
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], basicthreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0 || (int)retval != i+1){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1,"\n");
  return 0;
}

// ============================================================================

void*
jointhreadmain(void *arg)
{
  int val = (int)arg;
  sleep(200);
  printf(1, "thread_exit...\n");
  thread_exit((void *)(val*2));
  return 0;
}

int
jointest1(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;
  
  for (i = 1; i <= NUM_THREAD; i++){
    if (thread_create(&threads[i-1], jointhreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  printf(1, "thread_join!!!\n");
  for (i = 1; i <= NUM_THREAD; i++){
    if (thread_join(threads[i-1], &retval) != 0 || (int)retval != i * 2 ){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1,"\n");
  return 0;
}

int
jointest2(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;
  
  for (i = 1; i <= NUM_THREAD; i++){
    if (thread_create(&threads[i-1], jointhreadmain, (void*)(i)) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  sleep(500);
  printf(1, "thread_join!!!\n");
  for (i = 1; i <= NUM_THREAD; i++){
    if (thread_join(threads[i-1], &retval) != 0 || (int)retval != i * 2 ){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1,"\n");
  return 0;
}

// ============================================================================

void*
stressthreadmain(void *arg)
{
  thread_exit(0);
  return 0;
}

int
stresstest(void)
{
  const int nstress = 35000;
  thread_t threads[NUM_THREAD];
  int i, n;
  void *retval;


  for (n = 1; n <= nstress; n++){
    if (n % 1000 == 0)
      printf(1, "%d\n", n);
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_create(&threads[i], stressthreadmain, (void*)i) != 0){
        printf(1, "panic at thread_create\n");
        return -1;
      }
    }
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_join(threads[i], &retval) != 0){
        printf(1, "panic at thread_join\n");
        return -1;
      }
    }
  }
  printf(1, "\n");
  return 0;
}

// ============================================================================
