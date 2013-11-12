#ifndef __SYSCALL_CONFIG_H
#define __SYSCALL_CONFIG_H

#define SYSCALL_HOLE 223 /* Hole on at least some 32-bit systems */

typedef long mythread_mutex_t;
typedef long mythread_cond_t;

enum mythread_op {
  MYTHREAD_MUTEX_INIT,
  MYTHREAD_MUTEX_LOCK,
  MYTHREAD_MUTEX_UNLOCK,
  MYTHREAD_MUTEX_DESTROY,
  MYTHREAD_COND_INIT,
  MYTHREAD_COND_WAIT,
  MYTHREAD_COND_SIGNAL,
  MYTHREAD_COND_DESTROY,
};

#endif
