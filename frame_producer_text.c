#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "circular_buffer.h"

#define POOL_SIZE 10
typedef struct {
    pthread_t thread;

    enum {IDLE,WORKING,KILLME} state;
    int name;
    int sockfd;
    int resource_fd;   

    pthread_cond_t tcb_cond;
} tcb;

typedef struct {
    int priority;
    char* text;
} text_frame;

struct {
    tcb **workers;
    int size;
    pthread_mutex_t lock;
} worker_pool;

struct {
  circBuff *cb;
  pthread_cond_t producer_cond;
  pthread_cond_t consumer_cond;
  pthread_mutex_t lock;
} circular_buffer;

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
            pthread_mutex_lock(&worker_pool.lock);
            
            int err = pthread_cond_wait(&block->tcb_cond, &worker_pool.lock);
            if (err)
            {
                printf("pthread_cond_wait failed in text_producer, error code:%d",err);
                return 0;
            }
            pthread_mutex_unlock(&worker_pool.lock);
        }

        //Begin actual text production.
       
        char* text_string = (char*)calloc(512,sizeof(char));
        snprintf(text_string, (size_t)512,"Text Producer %d:%d",block->name,framenum);
       
        text_frame *frame = (text_frame*)calloc(1,sizeof(text_frame));
        frame->priority = block->name;
        frame->text = text_string;
        pthread_mutex_lock(&circular_buffer.lock);
        while (circBuff_push(circular_buffer.cb,frame))
        {
            //The buffer is full, or some other error state, we sleep until it isn't.
            pthread_cond_wait(&circular_buffer.producer_cond, &circular_buffer.lock);
        }
        framenum++;
        pthread_mutex_unlock(&circular_buffer.lock);
    }   
    return framenum;

}
/*
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
        return 0;
}
*/
int dispatch(tcb* control,int name,int sockfd,int resource_fd)
{
    if (control->state == WORKING)
    {
        return 1;
    }
    control->name   = name;
    control->sockfd = sockfd;
    control->state  = WORKING;
    control->resource_fd = resource_fd;
    //signal for wakeup on semaphore
    // TODO
  //  printf("dtcb:%p,%d,%d,%d\n",control,name,sockfd,resource_fd);
  //  printf("dcond:%p\n",control->tcb_cond);
  //  printf("dbuffer:%p\n",control->buffer);
    pthread_cond_signal(&control->tcb_cond);
  //  printf("Got past signal\n");
  //  fflush(stdout);    
    return 0;
}
int initialize_worker_pool(void *task, int size)
{
    worker_pool.workers   = calloc(size,sizeof(tcb**));
    worker_pool.size      = size;
    pthread_mutex_init(&worker_pool.lock, NULL);
    int i =0;
    for (i=0;i<size;i++)
    {
        tcb* worker = calloc(1,sizeof(tcb));
        worker_pool.workers[i] = worker;

        worker->state        = IDLE;

        pthread_cond_init(&worker->tcb_cond, NULL);
        pthread_create(&worker->thread, NULL, task, (void *)worker);
    }
    return 0;
}
int assign_worker (int name, int sockfd, int resource_fd)
{
    //Assigns a worker to the job specified by the arguments from pool; assumes
    //pool is not full. From a synchronization standpoint, it would also be
    //reasonable to have this entire call be wrapped in lock acquisitions,
    //rather than just acquiring the lock before dispatching, if we're
    //concerned about changes to the thread pool state occurring during
    //our attempt.

    int i = 0;
    for (i = 0 ; i< worker_pool.size; i++)
    {
        if (worker_pool.workers[i]->state == IDLE)
        {
            break;
        }
    }
    
    if (i == worker_pool.size)
    {
        return 1;
    }
    pthread_mutex_lock(&worker_pool.lock);
    dispatch(worker_pool.workers[i], name, sockfd, resource_fd);
    pthread_mutex_unlock(&worker_pool.lock);
    return 0;
}
int pool_grow (void)
{
    tcb **newmem = realloc(worker_pool.workers, (2 * worker_pool.size * sizeof(tcb *)));
    if (newmem)
    {
        worker_pool.size *= 2;
        worker_pool.workers = newmem;
    }
    else
    {
        return 1;
    }
    return 0;
}
int pool_shrink (void)
{
    //Not implemented yet, if ever.
    return 1;
}

void dispatcher (void) {
    // Steal the lock on buffer
  pthread_mutex_lock(&circular_buffer.lock);
  // Now empty the buffer into an array for sorting
  // Currently the buffer is of size 100
  int count = 0;
  void **from_buff = (void **) calloc(circular_buffer.cb->size, sizeof(void **));
  while ((from_buff[count] = circBuff_pop(circular_buffer.cb)) != 0){
     // printf("structure:%p\n",((text_frame*)(from_buff[count])));
     // printf("txt:%p\n",((text_frame*)(from_buff[count]))->text);
     // printf("priority:%d\n",((text_frame*)(from_buff[count]))->priority);
      count++;
  }
  printf("Count:%d\n", count);
  // Release the lock
  pthread_cond_broadcast(&circular_buffer.producer_cond);
  pthread_mutex_unlock(&circular_buffer.lock);
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
 
  // Now send them all
  for (int k=0; k < count; k++) {
   // printf("text location:%x",((text_frame*)from_buff[k])->text);
    printf("text:%s\n",((text_frame*)from_buff[k])->text);
    fflush(stdout);  
    //write(i->socket, i->text, strlen(i->text));
    free_text_frame(from_buff[k]);
  }
  free(from_buff);
  printf("Done dispatching\n");
}
/*tcb** init_worker_pool(int size)
{
    int i = 0;
    tcb * worker_pool = calloc(size,sizeof(tcb));
    pthread_cond_t  tcb_conds[size];
   
    for (i=0;i<size;i++)
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
        dispatch(&worker_pool[i],i,0,0);
    }

}*/
int main (void)
{
    
    pthread_mutex_init(&circular_buffer.lock,NULL);
    pthread_cond_init(&circular_buffer.producer_cond, NULL);
    pthread_cond_init(&circular_buffer.consumer_cond, NULL);
    circular_buffer.cb = circBuff_init(100);

//    pthread_t producers[POOL_SIZE];
//    tcb worker_pool[POOL_SIZE];

    pthread_mutex_init(&worker_pool.lock,NULL);
    pthread_mutex_lock(&worker_pool.lock);
    initialize_worker_pool(text_producer,POOL_SIZE);
    pthread_mutex_unlock(&worker_pool.lock);

    int i;
    for (i = 0; i < POOL_SIZE; i++)
    {
       // printf("tcb out:%p\n",((pool->workers)[i]));
       // printf("buffer:%p\n",((pool->workers)[i])->buffer);
       // printf("buffercond::%p\n",((pool->workers)[i])->buffer_cond);
       // printf("tcblock:%p\n",((pool->workers)[i])->tcb_lock);
       // printf("tcbcond:%p\n",((pool->workers)[i])->tcb_cond);
        assign_worker(i, 0, 0);
    }

    /*    pthread_cond_t  tcb_conds[POOL_SIZE];

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
        dispatch(&worker_pool[i],i,0,0);
    }*/
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
        dispatcher();
        sleep(1);
    }
}
