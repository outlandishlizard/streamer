#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "monitor.h"


struct monitor m_str;
struct monitor m_out;

struct monitor_cond empty;
struct monitor_cond full;

void *monitor_print (void *arg) {
  fprintf(stdout, "%s", (char *) arg);
  return NULL;
}

char *str;

void *monitor_put (void *arg) {
  if (str) {
    monitor_cond_wait(&empty);
  }
  str = (char *) arg;
  monitor_cond_signal(&full);
  return NULL;
}

void *monitor_get (void __attribute__((unused)) *arg) {
  char *returnee;
  if (!str) {
    monitor_cond_wait(&full);
  }
  returnee = str;
  str = NULL;
  monitor_cond_signal(&empty);
  return returnee;
}


void *putter (void __attribute__((unused)) *a) {
  while (1) {
    monitor_run_fn(&m_str, monitor_put, "ONE  ");
    monitor_run_fn(&m_out, monitor_print, "thread one sleeping\n");
    sleep(1);
    monitor_run_fn(&m_str, monitor_put, "TWO  ");
    monitor_run_fn(&m_str, monitor_put, "THREE");
    monitor_run_fn(&m_str, monitor_put, "FOUR ");
  }
}

void *getter (void __attribute__((unused)) *a) {
  while (1) {
    monitor_run_fn(&m_out, monitor_print, monitor_run_fn(&m_str, monitor_get, NULL));
    monitor_run_fn(&m_out, monitor_print, "one  \n");
    monitor_run_fn(&m_out, monitor_print, monitor_run_fn(&m_str, monitor_get, NULL));
    monitor_run_fn(&m_out, monitor_print, "two  \n");
    monitor_run_fn(&m_out, monitor_print, "thread two sleeping\n");
    sleep(1);
    monitor_run_fn(&m_out, monitor_print, monitor_run_fn(&m_str, monitor_get, NULL));
    monitor_run_fn(&m_out, monitor_print, "three\n");
  }
}

#if 0
void *f (int index) {
  while (1) {
    monitor_run_fn(&m, monitor_print, "Hey 1 there\n");
    monitor_run_fn(&m, monitor_print, "Hey 2 ewf\n");
    monitor_run_fn(&m, monitor_print, "Hey 3 twfere\n");
    monitor_run_fn(&m, monitor_print, "Hey 4 therwefewfe\n");
    monitor_run_fn(&m, monitor_print, "Hey 5 therwegwee\n");
    sleep(index);
  }
}
#endif


int main (void) {
  monitor_init(&m_str);
  monitor_init(&m_out);

  monitor_cond_init(&full, &m_str);
  monitor_cond_init(&empty, &m_str);
  pthread_t f2_thread, f1_thread;
  pthread_create(&f1_thread, NULL, getter, NULL);
  pthread_create(&f2_thread, NULL, putter, NULL);
  pthread_join(f1_thread, NULL);
  pthread_join(f2_thread, NULL);

}
