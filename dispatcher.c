
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define buff_size 100

typedef struct {
        int socket;
        int priority;
        char* text;

} text_producer_job;

typedef struct {
        int* base;
        int size;
        int read;
        int write;

} circBuff;


void dispatcher(circBuff* buffer) {
  // Steal the lock on buffer
  pthread_mutex_lock(buffer_lock);
  
  // Now empty the buffer into an array for sorting
  // Currently the buffer is of size 100
  int count = 0;
  void* from_buff = calloc(buff_size, sizeof(void*));
  while ((from_buff[count++] = circBuff_pop(buffer)) != 0);
  
  // Release the lock
  pthread_mutex_unlock(buffer_lock);
  
  // Now sort it
  // Bubblesort FTW
  for (int c=0 ; c < count; c++) {
    for (int d=0 ; d < count - c - 1; d++) {
      if (from_buff[d]->priority > from_buff[d+1]->priority) {
        swap = from_buff[d];
        from_buff[d] = from_buff[d+1];
        from_buff[d+1] = swap;
      }
    }
  }
  
  // Now send them all
  for (int k=0; k < count; k++) {
    write(i->socket, i->text, strlen(i->text));
  }
  free(from_buff);
}


