
#include <linux/module.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <linux/linkage.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include "../syscall_config.h"

MODULE_LICENSE("GPL");

static short debug = 0;
module_param(debug, short, 0000);
MODULE_PARM_DESC(debug, "Whether to print debug messages");

#define DEBUG(x) do { if (debug) { printk(KERN_DEBUG "mythread:" x "\n"); } } while (0);

/*
 * Implementation of pthreads-compatible mutices and condition variables.
 *
 * There are several spinlocks in this code: one spinlock for each possible
 * mutex and one each for each possible cond.  There is exactly one case where
 * a function will hold two spinlocks: mythread_cond_wait will call
 * mythread_mutex_lock and mythread_mutex_unlock on the associated mutex.
 * This cannot cause a deadlock.
 */

struct mythread_mutex {
  enum {
    MUTEX_NEXIST,
    MUTEX_EXIST,
    MUTEX_DESTROYING,
  } state;
  int locked;
  spinlock_t sl;
};

struct mythread_cond {
  enum {
    COND_NEXIST,
    COND_EXIST
  } state;
  wait_queue_head_t queue;
  mythread_mutex_t mutex;
  spinlock_t sl;
};

/* Arbitrary limits so I don't have to manage memory */
#define NUM_MUTICES 32
#define NUM_CONDS 256

static struct mythread_driver_t {
  struct mythread_mutex mutices[NUM_MUTICES];
  struct mythread_cond conds[NUM_CONDS];
  void *old_syscall;
} mythread_driver;

/* These two functions globally disable and enable the processor's write
   protection.  This is necessary because the sys_call_table is in write-only
   memory for obvious security reasons.  But since we are to insert a new
   syscall, we have to bypass this. */

static void disable_wp (void) {
  unsigned int cr0_value;
  __asm__ volatile ("movl %%cr0, %0" : "=r" (cr0_value));
  /* Disable WP */
  cr0_value &= ~(1 << 16);
  __asm__ volatile ("movl %0, %%cr0" :: "r" (cr0_value));
}

static void enable_wp (void) {
  unsigned int cr0_value;
  __asm__ volatile ("movl %%cr0, %0" : "=r" (cr0_value));
  /* Enable WP */
  cr0_value |= (1 << 16);
  __asm__ volatile ("movl %0, %%cr0" :: "r" (cr0_value));
}

/* Synchronication primitives */

mythread_mutex_t mythread_mutex_init (void) {
  long m;
  struct mythread_mutex *mutex;
  /* Choose a mutex struct to initialize. */
  for (m = 0; m < NUM_MUTICES; m++) {
    mutex = &mythread_driver.mutices[m];
    spin_lock(&mutex->sl);
    if (mutex->state != MUTEX_NEXIST) {
      /* Not available; try next */
      spin_unlock(&mutex->sl);
    } else {
      /* Initialize and return */
      mutex->state = MUTEX_EXIST;
      mutex->locked = 0;
      spin_unlock(&mutex->sl);
      DEBUG("mutex_init: Success");
      return m;
    }
  }
  /* Out of mutices. */
  printk(KERN_ERR "mythread: mutex_init: No room for new mutex\n");
  return -EAGAIN;
}

long mythread_mutex_lock (mythread_mutex_t mutex) {
  struct mythread_mutex *m = &mythread_driver.mutices[mutex];
  spin_lock(&m->sl);
  /* Check that lock still exists */
  if (m->state != MUTEX_EXIST) {
    spin_unlock(&m->sl);
    DEBUG("mutex_lock: No such mutex");
    return -EINVAL;
  }
  /* Wait until unlocked */
  while (m->locked) {
    spin_unlock(&m->sl);
    DEBUG("mutex_lock: Lock already taken.  Waiting...");
    schedule();
    spin_lock(&m->sl);
    /* Check that lock still exists */
    if (m->state != MUTEX_EXIST) {
      spin_unlock(&m->sl);
      DEBUG("mutex_lock: Disappeared");
      return -EINVAL;
    }
  }
  /* Grab lock */
  m->locked = 1;
  spin_unlock(&m->sl);
  DEBUG("mutex_lock: Success");
  return 0;
}

/* This currently lets people unlock OTHER peoples mutices.  This is
   clearly bad. */
long mythread_mutex_unlock (mythread_mutex_t mutex) {
  struct mythread_mutex *m = &mythread_driver.mutices[mutex];
  spin_lock(&m->sl);
  /* Check that lock exists */
  if (m->state != MUTEX_EXIST) {
    spin_unlock(&m->sl);
    DEBUG("mutex_unlock: No such mutex");
    return -EINVAL;
  }
  /* Check that it's really locked */
  if (!m->locked) {
    spin_unlock(&m->sl);
    DEBUG("mutex_unlock: Not locked");
    return -EPERM;
  }
  /* Unlock it */
  m->locked = 0;
  spin_unlock(&m->sl);
  DEBUG("mutex_unlock: Success");
  return 0;
}

/* This immediately marks the mutex for destruction, and then if it really can
   destroy it, marks it as destroyed.  In the case where the user tries to
   destroy a mutex, and then does some operation on it, that operation may
   fail needlessly.  This could be avoided by having operations on mutices
   sleep when operating on a mutex in the process of destruction. */
long mythread_mutex_destroy (mythread_mutex_t mutex) {
  struct mythread_mutex *m = &mythread_driver.mutices[mutex];
  long cond;
  DEBUG("Attempting to destroy mutex...");
  spin_lock(&m->sl);
  /* Check that mutex exists */
  if (m->state != MUTEX_EXIST) {
    spin_unlock(&m->sl);
    DEBUG("mutex_destroy: No such mutex");
    return -EINVAL;
  }
  /* Check that mutex is not locked */
  if (m->locked) {
    spin_unlock(&m->sl);
    DEBUG("mutex_destroy: Mutex locked");
    return -EBUSY;
  }
  /* Mark mutex for destruction */
  m->state = MUTEX_DESTROYING;
  spin_unlock(&m->sl);
  /* Check that no conds are waiting on it */
  for (cond = 0; cond < NUM_CONDS; cond++) {
    struct mythread_cond *c = &mythread_driver.conds[cond];
    spin_lock(&c->sl);
    if (c->state == COND_EXIST &&
        c->mutex == mutex &&
        waitqueue_active(&c->queue)) {
      spin_unlock(&c->sl);
      /* Found an active cond---don't destroy after all. */
      spin_lock(&m->sl);
      m->state = MUTEX_EXIST;
      spin_unlock(&m->sl);
      DEBUG("mutex_destroy: Mutex being waited upon");
      return -EBUSY;
    } else {
      spin_unlock(&c->sl);
    }
  }
  /* Succeeded---destroyed mutex */
  spin_lock(&m->sl);
  m->state = MUTEX_NEXIST;
  spin_unlock(&m->sl);
  DEBUG("mutex_destroy: Success");
  return 0;
}

mythread_cond_t mythread_cond_init (void) {
  long c;
  struct mythread_cond *cond;
  for (c = 0; c < NUM_CONDS; c++) {
    cond = &mythread_driver.conds[c];
    spin_lock(&cond->sl);
    if (cond->state == COND_EXIST) {
      spin_unlock(&cond->sl);
    } else {
      /* Initialize and return */
      cond->state = COND_EXIST;
      cond->mutex = -1;
      spin_unlock(&cond->sl);
      DEBUG("cond_init: Success");
      return c;
    }
  }
  /* Out of conds. */
  printk(KERN_ERR "mythread: cond_init: No room for new cond\n");
  return -EAGAIN;
}

long mythread_cond_wait (mythread_cond_t cond, mythread_mutex_t mutex) {
  struct mythread_cond *c = &mythread_driver.conds[cond];
  DEFINE_WAIT(__wait);
  spin_lock(&c->sl);
  /* Check that cond exists */
  if (c->state != COND_EXIST) {
    spin_unlock(&c->sl);
    DEBUG("cond_wait: No such cond");
    return -EINVAL;
  }
  if (c->mutex == -1) {
    /* No mutex associated yet; associate this one */
    DEBUG("cond_wait: Associating cond");
    c->mutex = mutex;
  }
  if (c->mutex != mutex) {
    /* Trying to use a new mutex when one associated */
    spin_unlock(&c->sl);
    DEBUG("cond_wait: Refusing to re-associate cond");
    return -EINVAL;
  }
  if (mythread_mutex_unlock(mutex)) {
    /* Error while unlocking; user is doing SOMETHING wrong */
    spin_unlock(&c->sl);
    DEBUG("cond_wait: Something wrong");
    return -EINVAL;
  }
  /* Now wait until signalled. */
  /* TODO FIXME: Check ordering of the following function calls.  Where should
     I be locking and unlocking the spinlock? */
  prepare_to_wait(&c->queue, &__wait, TASK_INTERRUPTIBLE);
  spin_unlock(&c->sl);
  DEBUG("cond_wait: Waiting...");
  schedule();
  spin_lock(&c->sl);
  finish_wait(&c->queue, &__wait);
  /* We've been woken up: take lock and return */
  if (mythread_mutex_lock(mutex)) {
    spin_unlock(&c->sl);
    printk(KERN_CRIT "mythread: Weird condition when finishing cond wait\n");
    return 10; /* This really shouldn't happen. */
  }
  spin_unlock(&c->sl);
  DEBUG("cond_wait: Success");
  return 0;
}

long mythread_cond_signal (mythread_cond_t cond) {
  struct mythread_cond *c = &mythread_driver.conds[cond];
  spin_lock(&c->sl);
  /* Check that cond exists */
  if (c->state != COND_EXIST) {
    spin_unlock(&c->sl);
    DEBUG("cond_signal: No such cond");
    return -EINVAL;
  }
  /* Lock exists: wake up a queued-up task */
  wake_up_interruptible(&c->queue);
  spin_unlock(&c->sl);
  DEBUG("cond_signal: Success");
  return 0;
}

long mythread_cond_destroy (mythread_cond_t cond) {
  struct mythread_cond *c = &mythread_driver.conds[cond];
  spin_lock(&c->sl);
  /* Check that cond exists */
  if (c->state != COND_EXIST) {
    spin_unlock(&c->sl);
    DEBUG("cond_destroy: No such cond");
    return -EINVAL;
  }
  /* Check that no one is waiting on cond */
  if (waitqueue_active(&c->queue)) {
    spin_unlock(&c->sl);
    DEBUG("cond_destroy: Cond being waited on");
    return -EBUSY;
  }
  /* Destroy */
  c->state = COND_NEXIST;
  c->mutex = -1;
  spin_unlock(&c->sl);
  DEBUG("cond_destroy: Success");
  return 0;
}

/* Syscall function; dispatches to the various methods above, depending on
   first argument to syscall */
asmlinkage long mythread_syscall (enum mythread_op op,
                                  mythread_mutex_t *m,
                                  mythread_cond_t *c) {
  long r; /* possible return value */
  mythread_mutex_t mutex;
  mythread_cond_t cond;

  switch (op) {
  case MYTHREAD_MUTEX_INIT:
    r = mythread_mutex_init();
    if (r < 0) {
      return r; /* Error code */
    } else {
      mutex = (mythread_mutex_t) r;
      if (copy_to_user(m, &mutex, sizeof(mythread_mutex_t))) {
        return -EINVAL;
      }
      return 0;
    }
  case MYTHREAD_MUTEX_LOCK:
    if (copy_from_user(&mutex, m, sizeof(mythread_mutex_t))) {
      return -EINVAL;
    }
    return mythread_mutex_lock(mutex);
  case MYTHREAD_MUTEX_UNLOCK:
    if (copy_from_user(&mutex, m, sizeof(mythread_mutex_t))) {
      return -EINVAL;
    }
    return mythread_mutex_unlock(mutex);
  case MYTHREAD_MUTEX_DESTROY:
    if (copy_from_user(&mutex, m, sizeof(mythread_mutex_t))) {
      return -EINVAL;
    }
    return mythread_mutex_destroy(mutex);
  case MYTHREAD_COND_INIT:
    r = mythread_cond_init();
    if (r < 0) {
      return r; /* Error code */
    } else {
      cond = (mythread_cond_t) r;
      if (copy_to_user(c, &cond, sizeof(mythread_cond_t))) {
        return -EINVAL;
      }
      return 0;
    }
  case MYTHREAD_COND_WAIT:
    if (copy_from_user(&mutex, m, sizeof(mythread_mutex_t))) {
      return -EINVAL;
    }
    if (copy_from_user(&cond, c, sizeof(mythread_cond_t))) {
      return -EINVAL;
    }
    return mythread_cond_wait(cond, mutex);
  case MYTHREAD_COND_SIGNAL:
    if (copy_from_user(&cond, c, sizeof(mythread_cond_t))) {
      return -EINVAL;
    }
    return mythread_cond_signal(cond);
  case MYTHREAD_COND_DESTROY:
    if (copy_from_user(&cond, c, sizeof(mythread_cond_t))) {
      return -EINVAL;
    }
    return mythread_cond_destroy(cond);
  default:
    return -ENOSYS; /* Unknown method */
  }
}

static int __init init_function (void) {
  void **sys_call_table = (void **) SYSCALL_TABLE;
  mythread_mutex_t m;
  mythread_cond_t c;
  printk(KERN_INFO "mythread: Hello, World!\n");
  /* Initialize driver state */
  printk(KERN_INFO "mythread: Initializing internal structures...\n");
  for (m = 0; m < NUM_MUTICES; m++) {
    struct mythread_mutex *mutex = &mythread_driver.mutices[m];
    spin_lock_init(&mutex->sl);
    mutex->state = MUTEX_NEXIST;
    mutex->locked = 0;
  }
  for (c = 0; c < NUM_CONDS; c++) {
    struct mythread_cond *cond = &mythread_driver.conds[c];
    spin_lock_init(&cond->sl);
    cond->state = COND_NEXIST;
    init_waitqueue_head(&cond->queue);
    cond->mutex = -1;
  }
  /* Insert our system call into the syscall table */
  printk(KERN_INFO "mythread: Inserting syscall...\n");
  disable_wp();
  mythread_driver.old_syscall = sys_call_table[SYSCALL_HOLE];
  sys_call_table[SYSCALL_HOLE] = mythread_syscall;
  enable_wp();
  return 0;
}

static void __exit cleanup_function (void) {
  void **sys_call_table = (void **) SYSCALL_TABLE;
  /* Replace our syscall with the 'not-implemented' syscall */
  printk(KERN_INFO "mythread: Removing syscall...\n");
  disable_wp();
  sys_call_table[SYSCALL_HOLE] = mythread_driver.old_syscall;
  enable_wp();
  printk(KERN_INFO "mythread: Goodbye, cruel world.\n");
}


module_init(init_function);
module_exit(cleanup_function);
