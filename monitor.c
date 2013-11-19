#include "global_config.h"
#ifdef USE_MYTHREADS
# include "mythread.h"
#else
# include <pthread.h>
#endif
#include <stdlib.h>
#include "monitor.h"

/* All variables in the monitor, as well as in any associated monitor_cond,
   may be modified ONLY by a thread holding the monitor's 'lock' mutex. */

struct monitor {
#ifdef USE_MYTHREADS
  mythread_mutex_t lock;
  mythread_cond_t queue;
#else
  pthread_mutex_t lock;
  pthread_cond_t queue;
#endif
  int waiting;
};

struct monitor_cond {
#ifdef USE_MYTHREADS
  mythread_cond_t cond;
#else
  pthread_cond_t cond;
#endif
  int waiting;
  struct monitor *monitor;
};

struct monitor *monitor_create (void) {
  struct monitor *returnee = (struct monitor *) malloc(sizeof(struct monitor));
#ifdef USE_MYTHREADS
  mythread_mutex_init(&returnee->lock, NULL);
  mythread_cond_init(&returnee->queue, NULL);
#else
  pthread_mutex_init(&returnee->lock, NULL);
  pthread_cond_init(&returnee->queue, NULL);
#endif
  returnee->waiting = 0;
  return returnee;
}

struct monitor_cond *monitor_cond_create (struct monitor *monitor) {
  struct monitor_cond *returnee = (struct monitor_cond *) malloc(sizeof(struct monitor_cond));
#ifdef USE_MYTHREADS
  mythread_cond_init(&returnee->cond, NULL);
#else
  pthread_cond_init(&returnee->cond, NULL);
#endif
  returnee->waiting = 0;
  returnee->monitor = monitor;
  return returnee;
}

void monitor_destroy (struct monitor *m) {
#ifdef USE_MYTHREADS
  mythread_mutex_destroy(&m->lock);
  mythread_cond_destroy(&m->queue);
#else
  pthread_mutex_destroy(&m->lock);
  pthread_cond_destroy(&m->queue);
#endif
  free(m);
}

void monitor_cond_destroy (struct monitor_cond *c) {
#ifdef USE_MYTHREADS
  mythread_cond_destroy(&c->cond);
#else
  pthread_cond_destroy(&c->cond);
#endif
  free(c);
}

void monitor_run_fn (struct monitor *m,
                     void (*f)(void *),
                     void *user_data) {
  /* Grab lock, and enter monitor */
#ifdef USE_MYTHREADS
  mythread_mutex_lock(&m->lock);
#else
  pthread_mutex_lock(&m->lock);
#endif
  if (m->waiting) {
    /* There's another thread that's trying to execute now; defer to it to
       avoid starvation. */
    m->waiting++;
#ifdef USE_MYTHREADS
    mythread_cond_wait(&m->queue, &m->lock);
#else
    pthread_cond_wait(&m->queue, &m->lock);
#endif
    m->waiting--;
  }
  /* Else, no threads trying to execute now; skip the wait to avoid
     deadlock */
  f(user_data);
  /* Leave monitor, and signal another waiting thread to execute */
#ifdef USE_MYTHREADS
  mythread_cond_signal(&m->queue);
  mythread_mutex_unlock(&m->lock);
#else
  pthread_cond_signal(&m->queue);
  pthread_mutex_unlock(&m->lock);
#endif
}

/* Caller must own monitor mutex */
void monitor_cond_wait (struct monitor_cond *c) {
  /* Queue up another thread */
#ifdef USE_MYTHREADS
  mythread_cond_signal(&c->monitor->queue);
#else
  pthread_cond_signal(&c->monitor->queue);
#endif
  /* Wait */
  c->waiting++;
#ifdef USE_MYTHREADS
  mythread_cond_wait(&c->cond, &c->monitor->lock);
#else
  pthread_cond_wait(&c->cond, &c->monitor->lock);
#endif
  c->waiting--;
}

/* Caller must own monitor mutex */
void monitor_cond_signal (struct monitor_cond *c) {
  if (c->waiting) {
    /* Queue up another thread */
#ifdef USE_MYTHREADS
    mythread_cond_signal(&c->cond);
#else
    pthread_cond_signal(&c->cond);
#endif
    /* Wait */
    c->monitor->waiting++;
#ifdef USE_MYTHREADS
    mythread_cond_wait(&c->monitor->queue, &c->monitor->lock);
#else
    pthread_cond_wait(&c->monitor->queue, &c->monitor->lock);
#endif
    c->monitor->waiting--;
  }
  /* If no threads waiting on c, do nothing at all. */
}
