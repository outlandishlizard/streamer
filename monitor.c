#include <pthread.h>
#include <stdlib.h>
#include "monitor.h"

struct monitor {
  pthread_mutex_t lock;
  pthread_cond_t queue; /* associated with lock */
  int active; /* locked by lock */
};

struct monitor_cond {
  pthread_cond_t cond; /* associated with monitor's lock */
  int waiting; /* locked by monitor's lock */
  struct monitor *monitor;
};

struct monitor *monitor_create (void) {
  struct monitor *returnee = (struct monitor *) malloc(sizeof(struct monitor));
  pthread_mutex_init(&returnee->lock, NULL);
  pthread_cond_init(&returnee->queue, NULL);
  returnee->active = 0;
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
                     monitor_func f,
                     void *user_data) {
  /* Grab lock, and enter monitor */
  pthread_mutex_lock(&m->lock);
  m->active++;
  if (m->active > 1) {
    /* There's another thread that's trying to execute now; defer to it to
       avoid starvation. */
    pthread_cond_wait(&m->queue, &m->lock);
  }
  /* Else, no threads trying to execute now; skip the wait to avoid
     deadlock */
  f(user_data);
  /* Leave monitor, and signal another waiting thread to execute */
  m->active--;
  pthread_cond_signal(&m->queue);
  pthread_mutex_unlock(&m->lock);
}

/* Caller must own monitor mutex */
void monitor_cond_wait (struct monitor_cond *c) {
  /* Shift thread from 'active' to 'waiting on c' */
  c->monitor->active--;
  c->waiting++;
  /* Queue up another thread, and wait */
  pthread_cond_signal(&c->monitor->queue);
  pthread_cond_wait(&c->cond, &c->monitor->lock);
  /* Shift thread from 'waiting on c' to 'active' */
  c->waiting--;
  c->monitor->active++;
}

/* Caller must own monitor mutex */
void monitor_cond_signal (struct monitor_cond *c) {
  if (c->waiting) {
    /* Shift thread from 'active' to 'inactive' */
    c->monitor->active--;
    /* Queue up another thread, and wait */
    pthread_cond_signal(&c->cond);
    pthread_cond_wait(&c->monitor->queue, &c->monitor->lock);
    /* Shift thread from 'inactive' to 'active */
    c->monitor->active++;
  }
  /* If no threads waiting on c, do nothing at all. */
}
