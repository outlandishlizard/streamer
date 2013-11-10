#ifndef __MONITOR_H
#define __MONITOR_H

struct monitor;
struct monitor_cond;

/*
 * This is an implementation of Hoare-style blocking monitors.
 *
 * The workflow is as follows: Create a new monitor with
 * 'monitor_create'. Create any needed condition variables associated with the
 * monitor with 'monitor_cond_create'. At this point, threads may call
 * 'monitor_run_fn' on a work function.  At some point when no threads are in
 * the monitor, you may destroy the monitor and condition variables with
 * 'monitor_destroy' and 'monitor_cond_destroy'.
 *
 * Work functions should have these properties: They should not attempt to
 * call 'monitor_run_fn' themselves.  They may wait on and signal conditions
 * associated with the monitor, with 'monitor_cond_wait' and
 * 'monitor_cond_signal'.
 *
 * It is unsafe to manipulate condition variables from outside the monitor.
 */

struct monitor *monitor_create (void);
struct monitor_cond *monitor_cond_create (struct monitor *monitor);
void monitor_destroy (struct monitor *m);
void monitor_cond_destroy (struct monitor_cond *c);
void monitor_run_fn (struct monitor *m,
                     void (*f)(void *),
                     void *user_data);
void monitor_cond_wait (struct monitor_cond *c);
void monitor_cond_signal (struct monitor_cond *c);

#endif
