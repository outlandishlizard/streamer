#include <pthread.h>
#include <stdlib.h>
#include "monitor.h"

/* All variables in the monitor, as well as in any associated monitor_cond,
   may be modified ONLY by a thread holding the monitor's 'lock' mutex. */

struct monitor {
  pthread_mutex_t lock;
  pthread_cond_t queue;
  int waiting;
};

struct monitor_cond {
  pthread_cond_t cond;
  int waiting;
  struct monitor *monitor;
};

struct monitor *monitor_create (void) {
  struct monitor *returnee = (struct monitor *) malloc(sizeof(struct monitor));
  pthread_mutex_init(&returnee->lock, NULL);
  pthread_cond_init(&returnee->queue, NULL);
  returnee->waiting = 0;
  return returnee;
}

struct monitor_cond *monitor_cond_create (struct monitor *monitor) {
  struct monitor_cond *returnee = (struct monitor_cond *) malloc(sizeof(struct monitor_cond));
  pthread_cond_init(&returnee->cond, NULL);
  returnee->waiting = 0;
  returnee->monitor = monitor;
  return returnee;
}

void monitor_destroy (struct monitor *m) {
  pthread_mutex_destroy(&m->lock);
  pthread_cond_destroy(&m->queue);
  free(m);
}

void monitor_cond_destroy (struct monitor_cond *c) {
  pthread_cond_destroy(&c->cond);
  free(c);
}

void monitor_run_fn (struct monitor *m,
                     void (*f)(void *),
                     void *user_data) {
  /* Grab lock, and enter monitor */
  pthread_mutex_lock(&m->lock);
  if (m->waiting) {
    /* There's another thread that's trying to execute now; defer to it to
       avoid starvation. */
    m->waiting++;
    pthread_cond_wait(&m->queue, &m->lock);
    m->waiting--;
  }
  /* Else, no threads trying to execute now; skip the wait to avoid
     deadlock */
  f(user_data);
  /* Leave monitor, and signal another waiting thread to execute */
  pthread_cond_signal(&m->queue);
  pthread_mutex_unlock(&m->lock);
}

/* Caller must own monitor mutex */
void monitor_cond_wait (struct monitor_cond *c) {
  /* Queue up another thread */
  pthread_cond_signal(&c->monitor->queue);
  /* Wait */
  c->waiting++;
  pthread_cond_wait(&c->cond, &c->monitor->lock);
  c->waiting--;
}

/* Caller must own monitor mutex */
void monitor_cond_signal (struct monitor_cond *c) {
  if (c->waiting) {
    /* Queue up another thread */
    pthread_cond_signal(&c->cond);
    /* Wait */
    c->monitor->waiting++;
    pthread_cond_wait(&c->monitor->queue, &c->monitor->lock);
    c->monitor->waiting--;
  }
  /* If no threads waiting on c, do nothing at all. */
}
