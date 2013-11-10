#ifndef __MONITOR_H
#define __MONITOR_H

struct monitor;
struct monitor_cond;

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
