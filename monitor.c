#include "global_config.h"
#ifdef USE_MYTHREADS
# include "mythread.h"
# define THREAD(ID)  mythread_ ## ID
#else
# include <pthread.h>
# define THREAD(ID) pthread_ ## ID
#endif
#include <stdlib.h>
#include "monitor.h"

/* All variables in the monitor, as well as in any associated monitor_cond,
   may be modified ONLY by a thread holding the monitor's 'lock' mutex. */

struct monitor {
  THREAD(mutex_t) lock;
  THREAD(cond_t) queue;
  int waiting;
};

struct monitor_cond {
  THREAD(cond_t) cond;
  int waiting;
  struct monitor *monitor;
};

struct monitor *monitor_create (void) {
  struct monitor *returnee = (struct monitor *) malloc(sizeof(struct monitor));
  THREAD(mutex_init)(&returnee->lock, NULL);
  THREAD(cond_init)(&returnee->queue, NULL);
  returnee->waiting = 0;
  return returnee;
}

struct monitor_cond *monitor_cond_create (struct monitor *monitor) {
  struct monitor_cond *returnee = (struct monitor_cond *) malloc(sizeof(struct monitor_cond));
  THREAD(cond_init)(&returnee->cond, NULL);
  returnee->waiting = 0;
  returnee->monitor = monitor;
  return returnee;
}

void monitor_destroy (struct monitor *m) {
  THREAD(mutex_destroy)(&m->lock);
  THREAD(cond_destroy)(&m->queue);
  free(m);
}

void monitor_cond_destroy (struct monitor_cond *c) {
  THREAD(cond_destroy)(&c->cond);
  free(c);
}

void monitor_run_fn (struct monitor *m,
                     void (*f)(void *),
                     void *user_data) {
  /* Grab lock, and enter monitor */
  THREAD(mutex_lock)(&m->lock);
  if (m->waiting) {
    /* There's another thread that's trying to execute now; defer to it to
       avoid starvation. */
    m->waiting++;
    THREAD(cond_wait)(&m->queue, &m->lock);
    m->waiting--;
  }
  /* Else, no threads trying to execute now; skip the wait to avoid
     deadlock */
  f(user_data);
  /* Leave monitor, and signal another waiting thread to execute */
  THREAD(cond_signal)(&m->queue);
  THREAD(mutex_unlock)(&m->lock);
}

/* Caller must own monitor mutex */
void monitor_cond_wait (struct monitor_cond *c) {
  /* Queue up another thread */
  THREAD(cond_signal)(&c->monitor->queue);
  /* Wait */
  c->waiting++;
  THREAD(cond_wait)(&c->cond, &c->monitor->lock);
  c->waiting--;
}

/* Caller need not own monitor mutex */
void monitor_cond_signal (struct monitor_cond *c) {
  THREAD(cond_signal)(&c->cond);
}

/* Caller need not own monitor mutex */
void monitor_cond_broadcast (struct monitor_cond *c) {
  THREAD(cond_broadcast)(&c->cond);
}
