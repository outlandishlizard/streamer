#ifndef __SYSCALL_CONFIG_H
#define __SYSCALL_CONFIG_H

#define SYSCALL_HOLE 223 /* Hole on at least some 32-bit systems */

/* To find the following constant, grep /boot/System.map (followed by kernel
   version) for sys_call_table. */
#define SYSCALL_TABLE 0xC1303EE0

#include "mythread_types.h"

enum mythread_op {
  MYTHREAD_MUTEX_INIT,
  MYTHREAD_MUTEX_LOCK,
  MYTHREAD_MUTEX_UNLOCK,
  MYTHREAD_MUTEX_DESTROY,
  MYTHREAD_COND_INIT,
  MYTHREAD_COND_WAIT,
  MYTHREAD_COND_SIGNAL,
  MYTHREAD_COND_BROADCAST,
  MYTHREAD_COND_DESTROY,
};

#endif
