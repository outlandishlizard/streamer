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
  struct monitor *monitor;
};

struct monitor *monitor_create (void) {
  struct monitor *returnee = (struct monitor *) malloc(sizeof(struct monitor));
  if (THREAD(mutex_init)(&returnee->lock, NULL)) {
    free(returnee);
    return NULL;
  }
  if (THREAD(cond_init)(&returnee->queue, NULL)) {
    THREAD(mutex_destroy)(&returnee->lock);
    free(returnee);
    return NULL;
  }
  returnee->waiting = 0;
  return returnee;
}

struct monitor_cond *monitor_cond_create (struct monitor *monitor) {
  struct monitor_cond *returnee = (struct monitor_cond *) malloc(sizeof(struct monitor_cond));
  if (THREAD(cond_init)(&returnee->cond, NULL)) {
    free(returnee);
    return NULL;
  }
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

void *monitor_run_fn (struct monitor *m,
                      void *(*f)(void *),
                      void *user_data) {
  void *returnee;
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
  returnee = f(user_data);
  /* Leave monitor, and signal another waiting thread to execute */
  THREAD(cond_signal)(&m->queue);
  THREAD(mutex_unlock)(&m->lock);
  return returnee;
}

/* Caller must own monitor mutex */
void monitor_cond_wait (struct monitor_cond *c) {
  /* Queue up another thread */
  THREAD(cond_signal)(&c->monitor->queue);
  /* Wait */
  THREAD(cond_wait)(&c->cond, &c->monitor->lock);
}

/* Caller need not own monitor mutex */
void monitor_cond_signal (struct monitor_cond *c) {
  THREAD(cond_signal)(&c->cond);
}

/* Caller need not own monitor mutex */
void monitor_cond_broadcast (struct monitor_cond *c) {
  THREAD(cond_broadcast)(&c->cond);
}
