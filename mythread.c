#include "mythread.h"
#include "syscall_config.h"

#define _GNU_SOURCE /* Necessary for forward-decalartion of 'syscall' */
#include "unistd.h" /* For 'syscall' function */

int mythread_mutex_init (mythread_mutex_t *m, void __attribute__((unused)) *props) {
  return syscall(SYSCALL_HOLE, MYTHREAD_MUTEX_INIT, m, NULL);
}

int mythread_mutex_destroy (mythread_mutex_t *m) {
  return syscall(SYSCALL_HOLE, MYTHREAD_MUTEX_DESTROY, m, NULL);
}

int mythread_mutex_lock (mythread_mutex_t *m) {
  return syscall(SYSCALL_HOLE, MYTHREAD_MUTEX_LOCK, m, NULL);
}

int mythread_mutex_trylock (mythread_mutex_t *m) {
  return syscall(SYSCALL_HOLE, MYTHREAD_MUTEX_TRYLOCK, m, NULL);
}

int mythread_mutex_unlock (mythread_mutex_t *m) {
  return syscall(SYSCALL_HOLE, MYTHREAD_MUTEX_UNLOCK, m, NULL);
}

int mythread_cond_init (mythread_cond_t *c, void __attribute__((unused)) *props) {
  return syscall(SYSCALL_HOLE, MYTHREAD_COND_INIT, NULL, c);
}

int mythread_cond_destroy (mythread_cond_t *c) {
  return syscall(SYSCALL_HOLE, MYTHREAD_COND_DESTROY, NULL, c);
}

int mythread_cond_wait (mythread_cond_t *c, mythread_mutex_t *m) {
  return syscall(SYSCALL_HOLE, MYTHREAD_COND_WAIT, m, c);
}

int mythread_cond_signal (mythread_cond_t *c) {
  return syscall(SYSCALL_HOLE, MYTHREAD_COND_SIGNAL, NULL, c);
}

int mythread_cond_broadcast (mythread_cond_t *c) {
  return syscall(SYSCALL_HOLE, MYTHREAD_COND_BROADCAST, NULL, c);
}
