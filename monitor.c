#include <stdlib.h>
#include "global_config.h"
#include "monitor.h"

/* All variables in the monitor, as well as in any associated monitor_cond,
   may be modified ONLY by a thread holding the monitor's 'lock' mutex. */

int monitor_init (struct monitor *m) {
  if (THREAD(mutex_init)(&m->lock, NULL)) {
    return -1;
  }
  if (THREAD(cond_init)(&m->queue, NULL)) {
    THREAD(mutex_destroy)(&m->lock);
    return -1;
  }
  m->waiting = 0;
  return 0;
}

int monitor_cond_init (struct monitor_cond *c, struct monitor *monitor) {
  if (THREAD(cond_init)(&c->cond, NULL)) {
    return -1;
  }
  c->monitor = monitor;
  return 0;
}

void *monitor_destroy (struct monitor *m) {
  THREAD(mutex_destroy)(&m->lock);
  THREAD(cond_destroy)(&m->queue);
  free(m);
  return NULL;
}

void *monitor_cond_destroy (struct monitor_cond *c) {
  THREAD(cond_destroy)(&c->cond);
  free(c);
  return NULL;
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
void *monitor_cond_wait (struct monitor_cond *c) {
  /* Queue up another thread */
  THREAD(cond_signal)(&c->monitor->queue);
  /* Wait */
  THREAD(cond_wait)(&c->cond, &c->monitor->lock);
  return NULL;
}

/* Caller need not own monitor mutex */
void *monitor_cond_signal (struct monitor_cond *c) {
  THREAD(cond_signal)(&c->cond);
  return NULL;
}

/* Caller need not own monitor mutex */
void *monitor_cond_broadcast (struct monitor_cond *c) {
  THREAD(cond_broadcast)(&c->cond);
  return NULL;
}
