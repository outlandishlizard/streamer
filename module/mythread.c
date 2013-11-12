
#include <linux/module.h>
#include <linux/init.h>

#include <asm/syscall.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/linkage.h>
#include <linux/kallsyms.h>

#include "../syscall_config.h"

MODULE_LICENSE("GPL");

/*
 *
 */

/* This struct contains all non-constant state for the kernel module.  It is
 * also used as the driver pointer in the IRQ handler request. */

struct mythread_mutex {
  int extant;
  int locked;
  spinlock_t sl;
};

struct mythread_cond {
  int extant;
  wait_queue_head_t queue;
  mythread_mutex_t mutex;
  spinlock_t sl;
};

#define NUM_MUTICES 32
#define NUM_CONDS 256

static struct mythread_driver_t {
  spinlock_t sl;
  struct mythread_mutex mutices[NUM_MUTICES];
  long num_mutices;
  struct mythread_cond conds[NUM_CONDS];
  long num_conds;
} mythread_driver;



mythread_mutex_t mythread_mutex_create (void) {
  long mutex;
  struct mythread_mutex *m;
  spin_lock(&mythread_driver.sl);
  mutex = mythread_driver.num_mutices++;
  if (mutex >= NUM_MUTICES) {
    return -EAGAIN;
  }
  m = &mythread_driver.mutices[mutex];
  m->locked = 0;
  spin_lock_init(&m->sl);
  spin_lock(&m->sl);
  m->extant = 0;
  spin_unlock(&m->sl);
  spin_unlock(&mythread_driver.sl);
  return mutex;
}

long mythread_mutex_lock (mythread_mutex_t mutex) {
  struct mythread_mutex *m = &mythread_driver.mutices[mutex];
  spin_lock(&m->sl);
  if (!m->extant) {
    spin_unlock(&m->sl);
    return -EINVAL;
  }
  while (m->locked) {
    spin_unlock(&m->sl);
    schedule();
    spin_lock(&m->sl);
    if (m->extant) {
      spin_unlock(&m->sl);
    } else {
      spin_unlock(&m->sl);
      return -EINVAL;
    }
  }

  /* Grab lock */
  m->locked = 1;
  spin_unlock(&m->sl);
  return 0;
}

long mythread_mutex_unlock (mythread_mutex_t mutex) {
  struct mythread_mutex *m = &mythread_driver.mutices[mutex];
  spin_lock(&m->sl);
  if (!m->extant) {
    spin_unlock(&m->sl);
    return -EINVAL;
  }
  if (!m->locked) {
    /* This currently lets people unlock OTHER peoples mutices.  This is
       clearly bad. */
    spin_unlock(&m->sl);
    return -EPERM;
  }
  m->locked = 0;
  spin_unlock(&m->sl);
  return 0;
}

long mythread_mutex_destroy (mythread_mutex_t mutex) {
  struct mythread_mutex *m = &mythread_driver.mutices[mutex];
  long cond;
  spin_lock(&mythread_driver.sl);
  /* Check if active---if so, mark as destroyed */
  spin_lock(&m->sl);
  if (m->extant) {
    m->extant = 0;
    spin_unlock(&m->sl);
  } else {
    spin_unlock(&m->sl);
    return -EINVAL;
  }
  /* Now check for conds waiting on it */
  for (cond = 0; cond < mythread_driver.num_conds; cond++) {
    struct mythread_cond *c = &mythread_driver.conds[cond];
    spin_lock(&c->sl);
    if (c->extant && c->mutex == mutex && waitqueue_active(&c->queue)) {
      spin_unlock(&c->sl);
      spin_unlock(&mythread_driver.sl);
      /* Found an active cond--- don't destroy after all. */
      spin_lock(&m->sl);
      m->extant = 0;
      spin_unlock(&m->sl);
      return -EBUSY;
    }
  }
  /* Succeeded---destroyed mutex */
  spin_unlock(&mythread_driver.sl);
  return 0;
}

mythread_cond_t mythread_cond_create (void) {
  long cond;
  struct mythread_cond *c;
  spin_lock(&mythread_driver.sl);
  cond = mythread_driver.num_conds++;
  if (cond >= NUM_CONDS) {
    return -EAGAIN;
  }
  c = &mythread_driver.conds[cond];
  spin_lock_init(&c->sl);
  spin_lock(&c->sl);
  init_waitqueue_head(&c->queue);
  c->extant = 0;
  c->mutex = -1;
  spin_unlock(&c->sl);
  spin_unlock(&mythread_driver.sl);
  return cond;
}

long mythread_cond_wait (mythread_cond_t cond, mythread_mutex_t mutex) {
  struct mythread_cond *c = &mythread_driver.conds[cond];
  DEFINE_WAIT(__wait);

  spin_lock(&c->sl);
  if (c->extant) {
    if (c->mutex == -1) {
      c->mutex = mutex;
    }
    if (c->mutex != mutex) {
      spin_unlock(&c->sl);
      return -EINVAL;
    }

    if (mythread_mutex_unlock(mutex)) {
      spin_unlock(&c->sl);
      return -EINVAL;
    }

    prepare_to_wait(&c->queue, &__wait, TASK_INTERRUPTIBLE);
    spin_unlock(&c->sl);
    schedule();
    spin_lock(&c->sl);
    finish_wait(&c->queue, &__wait);
    if (mythread_mutex_lock(mutex)) {
      spin_unlock(&c->sl);
      printk("<1>mythread: Weird condition when finishing cond wait\n");
      return 10; /* This really shouldn't happen. */
    }
    spin_unlock(&c->sl);
    return 0;
  } else {
    spin_unlock(&c->sl);
    return -EINVAL;
  }
}

long mythread_cond_signal (mythread_cond_t cond) {
  struct mythread_cond *c = &mythread_driver.conds[cond];
  spin_lock(&c->sl);
  if (c->extant) {
    wake_up_interruptible(&c->queue);
    spin_unlock(&c->sl);
    return 0;
  } else {
    spin_unlock(&c->sl);
    return -EINVAL;
  }
}

long mythread_cond_destroy (mythread_cond_t cond) {
  struct mythread_cond *c = &mythread_driver.conds[cond];
  spin_lock(&c->sl);
  if (c->extant) {
    if (waitqueue_active(&c->queue)) {
      spin_unlock(&c->sl);
      return -EBUSY;
    } else {
      c->extant = 0;
      spin_unlock(&c->sl);
      return 0;
    }
  } else {
    spin_unlock(&c->sl);
    return -EINVAL;
  }

}

asmlinkage long mythread_syscall (enum mythread_op op,
                                  mythread_mutex_t *m,
                                  mythread_cond_t *c) {
  long r; /* possible return value */
  mythread_mutex_t mutex;
  mythread_cond_t cond;

  switch (op) {
  case MYTHREAD_MUTEX_CREATE:
    r = mythread_mutex_create();
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
  case MYTHREAD_COND_CREATE:
    r = mythread_cond_create();
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
  void **sys_call_table;
  printk("<1>Hello, World!\n");
  printk("<1>Loading George's Module\n");
  printk("<1>Inserting syscall...\n");
  if (!kallsyms_lookup_name("sys_ni_syscall")) {
    printk("<1> Couldn't find symbol sys_ni_syscall; won't be able to unload correctly.\n");
    return -1;
  }
  sys_call_table = (void **) kallsyms_lookup_name("sys_call_table");
  if (!sys_call_table) {
    printk("<1> Couldn't find symbol sys_call_table; can't load correctly.\n");
    return -1;
  }
  spin_lock_init(&mythread_driver.sl);
  mythread_driver.num_mutices = 0;
  mythread_driver.num_conds = 0;
  sys_call_table[SYSCALL_HOLE] = mythread_syscall;
  return 0;
}

static void __exit cleanup_function (void) {
  void *sys_ni_syscall = (void *) kallsyms_lookup_name("sys_ni_syscall");
  void **sys_call_table = (void **) kallsyms_lookup_name("sys_call_table");
  if (!sys_ni_syscall) {
    printk("<1> Couldn't find symbol sys_ni_syscall; can't unload correctly.\n");
  }
  if (!sys_call_table) {
    printk("<1> Couldn't find symbol sys_call_table; can't unload correctly.\n");
  }
  printk("<1>Removing syscall...\n");
  sys_call_table[SYSCALL_HOLE] = sys_ni_syscall;
  printk("<1>Goodbye, cruel world.\n");
}


module_init(init_function);
module_exit(cleanup_function);
