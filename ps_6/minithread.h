/*
 * minithread.h:
 *  Definitions for minithreads.
 *
 *  Your assignment is to implement the functions defined in this file.
 *  You must use the names for types and procedures that are used here.
 *  Your procedures must take the exact arguments that are specified
 *  in the comments.  See minithread.c for prototypes.
 */

#ifndef __MINITHREAD_H__
#define __MINITHREAD_H__

#include "machineprimitives.h"
#include "multilevel_queue.h"
#include "disk.h"
#define RUNNABLE 0
#define BLOCKED 1
#define RUNNING 2
#define DEAD -1

extern disk_t* disk;

//simple getter function for priority of thread
int minithread_priority();

//simple getter function for system time
int minithread_time();

/*
 * struct minithread:
 *  This is the key data structure for the thread management package.
 *  You must define the thread control block as a struct minithread.
 */

typedef struct minithread *minithread_t;

/*
 * minithread_t
 * minithread_fork(proc_t proc, arg_t arg)
 *  Create and schedule a new thread of control so
 *  that it starts executing inside proc_t with
 *  initial argument arg.
 */ 
extern minithread_t minithread_fork(proc_t proc, arg_t arg);

/*
 * minithread_enqueue_and_schedule(queue_t q)
 * puts current_thread on q and runs the next available thread
 */
extern void minithread_enqueue_and_schedule(queue_t q);

/*
 *  minithread_dequeue_and_schedule(queue_t q)
 *  dequeues the first element of q, puts on runnable queue
 */
extern void minithread_dequeue_and_run(queue_t q);

/*
 * minithread_t
 * minithread_create(proc_t proc, arg_t arg)
 *  Like minithread_fork, only returned thread is not scheduled
 *  for execution.
 */
extern minithread_t minithread_create(proc_t proc, arg_t arg);



/*
 * minithread_t minithread_self():
 *  Return identity (minithread_t) of caller thread.
 */
extern minithread_t minithread_self();


/*
 * int minithread_id():
 *      Return thread identifier of caller thread, for debugging.
 *
 */
extern int minithread_id();

/*
 * Gets the current directory of this thread
 * */
extern char* minithread_get_curr_dir();

/*
 * Sets the current directory of this thread to arg
 * Returns the old current directory
 * Caller is responsible for freeing the old char*
 * */
extern char* minithread_set_curr_dir(char* arg);


/*
 * minithread_stop()
 *  Block the calling thread.
 */
extern void minithread_stop();

/*
 * minithread_start(minithread_t t)
 *  Make t runnable.
 */
extern void minithread_start(minithread_t t);

/*
 * minithread_yield()
 *  Forces the caller to relinquish the processor and be put to the end of
 *  the ready queue.  Allows another thread to run.
 */
extern void minithread_yield();

/*
 * minithread_system_initialize(proc_t mainproc, arg_t mainarg)
 *  Initialize the system to run the first minithread at
 *  mainproc(mainarg).  This procedure should be called from your
 *  main program with the callback procedure and argument specified
 *  as arguments.
 */
extern void minithread_system_initialize(proc_t mainproc, arg_t mainarg);


/*
 * minithread_sleep_with_timeout(int delay)
 *      Put the current thread to sleep for [delay] milliseconds
 */
extern void minithread_sleep_with_timeout(int delay);


#endif /*__MINITHREAD_H__*/

