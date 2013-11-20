#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "circular_buffer.h"

#define POOL_SIZE 10
typedef struct {
    enum {IDLE,WORKING,KILLME} state;
    int name;
    int sockfd;
    
    pthread_cond_t* buffer_cond;
    pthread_cond_t* tcb_cond;

    pthread_mutex_t* buffer_lock;
    pthread_mutex_t* tcb_lock;

    circBuff* buffer;
} tcb;
typedef struct {
    int priority;
    char* text;
} text_frame;
void free_text_frame(text_frame *tf)
{
    free(tf->text);
    free(tf);
}
int text_producer(void* _block)
{
    /* Logic:
     *  1. Block on tcb_cond in our tcb until someone moves us into WORKING and unblocks tcb_cond
     *  2. Try to acquire buffer_cond, block until we do (ie, grab lock, 4
     *  3. Add a data block to the circular buffer
     *  4. Release buffer_cond
     *  5. Loop, unless state == KILLME, in which case, cleanup. Note that sync problems are possible here (ie, we may have to go through an extra iteration before noticing we're supposed to die in some cases, however, this will at worst cause minor control latency issues, nothing more.
     * */

    tcb* block = (tcb*)_block;
    int framenum = 0;
    while(block->state != KILLME)
    {
        //Check that we're in a state that we should be in, block until we are.
        while (block->state != WORKING)
        {
            pthread_mutex_lock(block->tcb_lock);
            
            int err;        
            if (err = pthread_cond_wait(block->tcb_cond,block->tcb_lock))
            {
                printf("pthread_cond_wait failed in text_producer, error code:%d",err);
                return 0;
            }
            pthread_mutex_unlock(block->tcb_lock);
        }

        //Begin actual text production.
       
        char* text_string = (char*)calloc(512,sizeof(char));
        snprintf(text_string, (size_t)512,"Text Producer %d:%d",block->name,framenum);
       
        text_frame* frame = (text_frame*)calloc(1,sizeof(text_frame));
        frame->priority = block->name;
        frame->text = text_string;
        pthread_mutex_lock(block->buffer_lock);
        printf("worker acquired mutex\n"); 
        fflush(stdout);
        while (circBuff_push(block->buffer,text_string) == 0)
        {
            //The buffer is full, or some other error state, we sleep until it isn't.
            pthread_cond_wait(block->buffer_cond,block->buffer_lock);
        }
        circBuff_push(block->buffer,text_string);
        framenum++;
        printf("pthread attempting to release mutex\n");
        fflush(stdout);
        pthread_mutex_unlock(block->buffer_lock);
        printf("pthread released mutex\n");
        fflush(stdout);
    }   
    return framenum;

}
int consume(circBuff* buffer)
{
    char* pop;
    pop = circBuff_pop(buffer);
    printf("Pop addr:%x\n",pop);
    if (pop)
    {
        printf("Popped:%s\n",pop);
    }
    else
    {
        printf("Pop failed\n");
    }
        fflush(stdout);
}

int dispatch(tcb* control,int name,int sockfd)
{
    if (control->state == WORKING)
    {
        return 0;
    }
    control->name   = name;
    control->sockfd = sockfd;
    control->state  = WORKING;
    //signal for wakeup on semaphore
    // TODO

    return 1;
}

void dispatcher(circBuff* buffer,pthread_mutex_t *buffer_lock) {
  printf("In dispatcher\n");
    // Steal the lock on buffer
  pthread_mutex_lock(buffer_lock);
  printf("Dispatcher has lock\n");
 fflush(stdout); 
  // Now empty the buffer into an array for sorting
  // Currently the buffer is of size 100
  int count = 0;
  void** from_buff = (void**)calloc(buffer->size, sizeof(void*));
  while ((from_buff[count] = circBuff_pop(buffer)) != 0){count++;};
  
  printf("Count:%d\n",count);
  fflush(stdout);
  // Release the lock
  pthread_mutex_unlock(buffer_lock);
  // Now sort it
  // Bubblesort FTW
  for (int c=0 ; c < count; c++) {
    for (int d=0 ; d < count - c - 1; d++) {
      if (((text_frame*)from_buff[d])->priority > ((text_frame*)from_buff[d+1])->priority) {
        //from_buff[d] = (int)from_buff[d] ^ (int)from_buff[d+1];
        //from_buff[d+1] = (int)from_buff[d] ^ (int)from_buff[d+1];
        //from_buff[d] = (int)from_buff[d] ^ (int)from_buff[d+1];
        void* swap = from_buff[d];
        from_buff[d] = from_buff[d+1];
        from_buff[d+1] = swap;
      }
    }
  }
 
  printf("Past sorted\n");
  fflush(stdout);
  // Now send them all
  for (int k=0; k < count; k++) {
    printf("text:%s",((text_frame*)from_buff[k])->text);
    fflush(stdin);  
    //write(i->socket, i->text, strlen(i->text));
    free_text_frame(from_buff[k]);
  }
  free(from_buff);
  printf("Done dispatching\n");
  fflush(stdout);
}

int main(int argc,char** argv)
{
    
    pthread_mutex_t buffer_lock;
    pthread_cond_t  buffer_cond;
    pthread_cond_init(&buffer_cond,NULL);


    circBuff* buffer;
    buffer = circBuff_init(100);


    pthread_t producers[POOL_SIZE];
    tcb worker_pool[POOL_SIZE];

    pthread_mutex_t tcb_lock;
    pthread_cond_t  tcb_conds[POOL_SIZE];

    int i = 0;
	for (i=0;i<POOL_SIZE;i++)
	{
        pthread_cond_init(&tcb_conds[i],NULL);
        worker_pool[i].state    = IDLE;
        worker_pool[i].name     = i;
        worker_pool[i].sockfd   = 0;

        worker_pool[i].buffer_cond = &buffer_cond;
        worker_pool[i].buffer_lock = &buffer_lock;
        worker_pool[i].tcb_cond = &tcb_conds[i];
        worker_pool[i].tcb_lock = &tcb_lock;
        worker_pool[i].buffer   = buffer;
        
		
        pthread_create(producers+i,NULL,text_producer, (void*)(&worker_pool[i]));
        dispatch(&worker_pool[i],i,0);
    }
    //Dispatcher stub code
    //
    while(1)
    {
    /*    pthread_mutex_lock(&buffer_lock);
        if (!circBuff_isEmpty(buffer))
        {
            consume(buffer);        
        }
        else
        {
                pthread_cond_broadcast(&buffer_cond); 
        }
        pthread_mutex_unlock(&buffer_lock);
   */
        dispatcher(buffer,&buffer_lock);
    }
}
