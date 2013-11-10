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
  pthread_mutex_lock(&m->lock);
  m->active++;
  if (m->active > 1) {
    pthread_cond_wait(&m->queue, &m->lock);
  } /* If not, don't wait, because no one will signal us. */
  f(user_data);
  m->active--;
  pthread_cond_signal(&m->queue);
  pthread_mutex_unlock(&m->lock);
}

/* Caller must own monitor mutex */
void monitor_cond_wait (struct monitor_cond *c) {
  c->monitor->active--;
  pthread_cond_signal(&c->monitor->queue);
  c->waiting++;
  pthread_cond_wait(&c->cond, &c->monitor->lock);
  c->waiting--;
  c->monitor->active++;
}

/* Caller must own monitor mutex */
void monitor_cond_signal (struct monitor_cond *c) {
  if (c->waiting) {
    pthread_cond_signal(&c->cond);
    c->monitor->active--;
    pthread_cond_wait(&c->monitor->queue, &c->monitor->lock);
    c->monitor->active++;
  }
}
