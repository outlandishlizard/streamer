#ifndef __MYTHREAD_H
#define __MYTHREAD_H

#include "mythread_types.h"

int mythread_mutex_init (mythread_mutex_t *, void *);
int mythread_mutex_destroy (mythread_mutex_t *);
int mythread_mutex_lock (mythread_mutex_t *);
int mythread_mutex_trylock (mythread_mutex_t *);
int mythread_mutex_unlock (mythread_mutex_t *);

int mythread_cond_init (mythread_cond_t *, void *);
int mythread_cond_destroy (mythread_cond_t *);
int mythread_cond_wait (mythread_cond_t *, mythread_mutex_t *);
int mythread_cond_signal (mythread_cond_t *);
int mythread_cond_broadcast (mythread_cond_t *);

#endif
