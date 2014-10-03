/*
 * minithread.c:
 *      This file provides a few function headers for the procedures that
 *      you are required to implement for the minithread assignment.
 *
 *      EXCEPT WHERE NOTED YOUR IMPLEMENTATION MUST CONFORM TO THE
 *      NAMING AND TYPING OF THESE PROCEDURES.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "interrupts.h"
#include "minithread.h"
#include "synch.h"
#include <assert.h>
#include "unistd.h"
#include "alarm.h"

#define LOWEST_PRIORITY 3

typedef struct minithread {
  int id;
  int priority;
  int rem_quanta;
  stack_pointer_t stackbase;
  stack_pointer_t stacktop;
  int status;
} minithread;

int current_id = 0; // the next thread id to be assigned
minithread_t current_thread = NULL;
multilevel_queue_t runnable_q = NULL;
queue_t blocked_q = NULL;
queue_t dead_q = NULL;
semaphore_t dead_sem = NULL;
int sys_time = 0;
const int TIME_QUANTA = 100 * MILLISECOND;

//getter for priority
int minithread_priority(){
  return current_thread->priority;
}

int minithread_time(){
  return sys_time;
}

int random_number(int max_num) {
  srand(time(NULL));
  return rand() % max_num;
}

int choose_priority_level() {
  int num;
  num = random_number(100);

  if (num < 50) return 0;
  else if (num < 75) return 1;
  else if (num < 90) return 2;
  else return 3;
}

int clean_up(){
  interrupt_level_t l;

  minithread_t dead = NULL;
  while (1){
    semaphore_P(dead_sem);
    l = set_interrupt_level(DISABLED);
    if (queue_dequeue(dead_q, (void**)(&dead)) == -1){
      set_interrupt_level(l);
      return -1;
    }
    minithread_free_stack(dead->stackbase);
    free(dead);
    set_interrupt_level(l);
  }
  return -1;
} 

int scheduler() {
  int next_priority = 0;
  minithread_t next = NULL;
  minithread_t temp = NULL;
 
  while (1) {
    set_interrupt_level(DISABLED); 
    //dequeue from runnable threads
    next_priority = choose_priority_level();
    if (multilevel_queue_dequeue(runnable_q,
        next_priority,(void**)(&next)) != -1) {
      temp = current_thread;
      current_thread = next;
      minithread_switch(&(temp->stacktop),&(next->stacktop));
      return 0;
    }
    set_interrupt_level(ENABLED);
    //if dead/runnable queue is empty, do nothing (idle thread)
  }
  return 0;
}

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */


int
minithread_exit(minithread_t completed) {
  interrupt_level_t l;

  current_thread->status = DEAD;
  
  l = set_interrupt_level(DISABLED);
  queue_append(dead_q, current_thread);
  set_interrupt_level(l);

  semaphore_V(dead_sem);
  scheduler();
  while(1);
  return 0;
}
 
minithread_t
minithread_fork(proc_t proc, arg_t arg) {
  interrupt_level_t l;

  minithread_t new_thread = minithread_create(proc,arg);
  
  l = set_interrupt_level(DISABLED);
  multilevel_queue_enqueue(runnable_q,
    new_thread->priority,new_thread); //add to queue
  set_interrupt_level(l);

  return new_thread;
}

minithread_t
minithread_create(proc_t proc, arg_t arg) {
  interrupt_level_t l;

  minithread_t new_thread = (minithread_t)malloc(sizeof(minithread));
  if (new_thread == NULL){
    return NULL;
  }

  l = set_interrupt_level(DISABLED);
  new_thread->id = current_id++;
  set_interrupt_level(l);

  new_thread->priority = 0;
  new_thread->rem_quanta = 1;
  new_thread->stackbase = NULL;
  new_thread->stacktop =  NULL;
  new_thread->status = RUNNABLE;
  minithread_allocate_stack(&(new_thread->stackbase), &(new_thread->stacktop) );
  minithread_initialize_stack(&(new_thread->stacktop), proc, arg,
                              (proc_t)minithread_exit, NULL);
  return new_thread; 
}

minithread_t
minithread_self() {
  return current_thread;
}

int
minithread_id() {
  return current_thread->id;
}

void
minithread_stop() { minithread_enqueue_and_schedule(blocked_q); }

void
minithread_start(minithread_t t) {
  interrupt_level_t l;

  t->status = RUNNABLE;
  
  t->priority = 0;
  t->rem_quanta = 1;
  l = set_interrupt_level(DISABLED);
  multilevel_queue_enqueue(runnable_q,t->priority,t);
  set_interrupt_level(l); 
}

void
minithread_enqueue_and_schedule(queue_t q) {
  interrupt_level_t l;
 
  current_thread->status = BLOCKED;
  
  l = set_interrupt_level(DISABLED);
  queue_append(q, current_thread);
  set_interrupt_level(l);

  scheduler();
}

void
minithread_dequeue_and_run(queue_t q) {
  interrupt_level_t l;
  minithread_t blocked_thread = NULL;

  l = set_interrupt_level(DISABLED);
  queue_dequeue(q, (void**)(&blocked_thread) );
  set_interrupt_level(l);

  if (blocked_thread->status != BLOCKED) {
    printf("thread %d should have status BLOCKED\n", minithread_id());
  }
  minithread_start(blocked_thread);
}

void
minithread_demote_priority() {
  interrupt_level_t l;

  if (current_thread->priority == LOWEST_PRIORITY);
  else current_thread->priority++;
  current_thread->rem_quanta = 1 << current_thread->priority;
  l = set_interrupt_level(DISABLED);
  multilevel_queue_enqueue(runnable_q,
      current_thread->priority,current_thread);
  set_interrupt_level(l);
  
  scheduler();
}

void
minithread_yield() {
  interrupt_level_t l;
  //put current thread at end of runnable
  
  l = set_interrupt_level(DISABLED);
  multilevel_queue_enqueue(runnable_q,
      current_thread->priority,current_thread);
  set_interrupt_level(l);
  
  //call scheduler here
  scheduler();
}

/*
 * This is the clock interrupt handling routine.
 * You have to call minithread_clock_init with this
 * function as parameter in minithread_system_initialize
 */
void 
clock_handler(void* arg) {
  sys_time += 1;
  printf("systime is %i\n", sys_time);
  execute_alarms(sys_time);
  if (--(current_thread->rem_quanta) == 0) {
    minithread_demote_priority();
  }
}

void
wake_up(void* sem){
  semaphore_V((semaphore_t)sem);
}

/*
 * sleep with timeout in milliseconds
 */
void 
minithread_sleep_with_timeout(int delay){
  semaphore_t thread_sem;
  int num_cycles;

  if (delay % TIME_QUANTA == 0){
    num_cycles = delay / TIME_QUANTA;
  } else {
    num_cycles = (delay / TIME_QUANTA) + 1;
  }
  thread_sem = semaphore_create();
  set_alarm(num_cycles, wake_up, (void*)thread_sem, sys_time);
  semaphore_P(thread_sem);
  semaphore_destroy(thread_sem);
}

/*
 * Initialization.
 *
 *      minithread_system_initialize:
 *       This procedure should be called from your C main procedure
 *       to turn a single threaded UNIX process into a multithreaded
 *       program.
 *
 *       Initialize any private data structures.
 *       Create the idle thread.
 *       Fork the thread which should call mainproc(mainarg)
 *       Start scheduling.
 *
 */
void
minithread_system_initialize(proc_t mainproc, arg_t mainarg) {
  minithread_t clean_up_thread = NULL;
  int a = 0;
  void* dummy_ptr = NULL;
  minithread_t tmp = NULL;
  tmp = NULL;
  dummy_ptr = (void*)&a;
  current_id = 0; // the next thread id to be assigned
  
  runnable_q = multilevel_queue_new(4);
  blocked_q = queue_new();
  dead_q = queue_new();

  dead_sem = semaphore_create();
  semaphore_initialize(dead_sem,0);    
  clean_up_thread = minithread_create(clean_up, NULL);
  multilevel_queue_enqueue(runnable_q,
    clean_up_thread->priority,clean_up_thread);

  minithread_clock_init(TIME_QUANTA, (interrupt_handler_t)clock_handler);
  init_alarm();
  current_thread = minithread_create(mainproc, mainarg);
  minithread_switch(&dummy_ptr, &(current_thread->stacktop));
  return;
}

