#ifndef __ALARM_H__
#define __ALARM_H__ 1

/*
 * This is the alarm interface. You should implement the functions for these
 * prototypes, though you may have to modify some other files to do so.
 */


/* An alarm_handler_t is a function that will run within the interrupt handler.
 * It must not block, and it must not perform I/O or any other long-running
 * computations.
 */
typedef void (*alarm_handler_t)(void*);
typedef void *alarm_id;

typedef struct alarm *alarm_t;
typedef struct alarm_node *alarm_node_t;
typedef struct alarm_list *alarm_list_t;

int alarm_list_len(alarm_list_t a_list);

/*
 * wrapper function for setting alarm with system time information.
 */
alarm_id set_alarm(int delay, alarm_handler_t func, void *arg, int sys_time);

/* register an alarm to go off in "delay" milliseconds.  Returns a handle to
 * the alarm.
 */
alarm_id register_alarm(int delay, alarm_handler_t func, void *arg);

/* unregister an alarm.  Returns 0 if the alarm had not been executed, 1
 * otherwise.
 */
int deregister_alarm(alarm_id id);

void execute_alarms();

alarm_list_t init_alarm();

#endif
