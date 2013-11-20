#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "circular_buffer.h"

#define POOL_SIZE 10
typedef struct {
    pthread_t *thread;

    enum {IDLE,WORKING,KILLME} state;
    int name;
    int sockfd;
 
    int resource_fd;   
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

typedef struct {
    tcb** workers;
    int size;
    pthread_mutex_t* tcb_lock;
} worker_pool;

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
        snprintf(text_string, (size_t)512,"Text Producer %d:%d\0",block->name,framenum);
       
        text_frame *frame = (text_frame*)calloc(1,sizeof(text_frame));
        frame->priority = block->name;
        frame->text = text_string;
        pthread_mutex_lock(block->buffer_lock);
        while (circBuff_push(block->buffer,frame))
        {
            //The buffer is full, or some other error state, we sleep until it isn't.
            pthread_cond_wait(block->buffer_cond,block->buffer_lock);
        }
        framenum++;
        pthread_mutex_unlock(block->buffer_lock);
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

int dispatch(tcb* control,int name,int sockfd,int resource_fd)
{
    if (control->state == WORKING)
    {
        return 0;
    }
    control->name   = name;
    control->sockfd = sockfd;
    control->state  = WORKING;
    control->resource_fd = resource_fd;
    //signal for wakeup on semaphore
    // TODO
    printf("tcb:%p,%d,%d,%d\n",control,name,sockfd,resource_fd);
    printf("cond:%p\n",control->tcb_cond);
    printf("buffer:%p\n",control->buffer);
    pthread_cond_signal(control->tcb_cond);
    printf("Got past signal\n");
    fflush(stdout);    
    return 1;
}
worker_pool *initialize_workers(void *task,int size,circBuff* buffer,pthread_mutex_t *buffer_lock, pthread_mutex_t *tcb_lock, pthread_cond_t *buffer_cond)
{
    worker_pool *pool   = calloc(1,sizeof(worker_pool));
    pool->workers       = calloc(size,sizeof(tcb**));
    pool->size          = size;
    pool->tcb_lock      = tcb_lock;
    int i =0;
    for (i=0;i<size;i++)
    {
        tcb* worker = calloc(1,sizeof(tcb));
        (pool->workers)[i] = worker;

        worker->state        = IDLE;
        worker->tcb_lock     = tcb_lock;
        worker->buffer_lock  = buffer_lock;
        worker->buffer_cond  = buffer_cond;

        worker->buffer       = buffer;

        worker->tcb_cond     = calloc(1,sizeof(pthread_cond_t));
        worker->thread       = calloc(1,sizeof(pthread_t));

        pthread_cond_init(worker->tcb_cond,NULL);
        pthread_create(worker->thread,NULL,task,(void*)worker);
    }
    return pool;
}
int assign_worker(worker_pool *pool, int name, int sockfd, int resource_fd)
{
    //Assigns a worker to the job specified by the arguments from pool; assumes
    //pool is not full. From a synchronization standpoint, it would also be
    //reasonable to have this entire call be wrapped in lock acquisitions,
    //rather than just acquiring the lock before dispatching, if we're
    //concerned about changes to the thread pool state occurring during
    //our attempt.

    int i = 0;
    for (i = 0 ; i< pool->size; i++)
    {
        if ((pool->workers)[i]->state == IDLE)
        {
            break;
        }
    }
    
    if (i == pool->size)
    {
        return 1;
    }
   // pthread_mutex_lock(pool->tcb_lock);
    dispatch(&((pool->workers)[i]),name,sockfd,resource_fd);
   // pthread_mutex_unlock(pool->tcb_lock);
    return 0;
}
int pool_grow(worker_pool *pool)
{
    int newmem;
    if (newmem = realloc(pool->workers,(2 * pool->size * sizeof(tcb*))))
    {
        pool->size *= 2;
        pool->workers= newmem;
    }
    else
    {
        return 1;
    }
    return 0;
}
worker_pool *pool_shrink(worker_pool *pool)
{
    //Not implemented yet, if ever.
    return 1;
}

void dispatcher(circBuff* buffer,pthread_mutex_t *buffer_lock,pthread_cond_t *buffer_cond) {
    // Steal the lock on buffer
  pthread_mutex_lock(buffer_lock);
  // Now empty the buffer into an array for sorting
  // Currently the buffer is of size 100
  int count = 0;
  void** from_buff = (void**)calloc(buffer->size, sizeof(void**));
  while ((from_buff[count] = circBuff_pop(buffer)) != 0){
     // printf("structure:%p\n",((text_frame*)(from_buff[count])));
     // printf("txt:%p\n",((text_frame*)(from_buff[count]))->text);
     // printf("priority:%d\n",((text_frame*)(from_buff[count]))->priority);
      count++;
  }
  printf("Count:%d\n",count); 
  // Release the lock
  pthread_cond_broadcast(buffer_cond);
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
int main(int argc,char** argv)
{
    
    pthread_mutex_t buffer_lock;
    pthread_mutex_init(&buffer_lock,NULL);
    
    pthread_cond_t  buffer_cond;
    pthread_cond_init(&buffer_cond,NULL);


    circBuff* buffer;
    buffer = circBuff_init(100);


//    pthread_t producers[POOL_SIZE];
//    tcb worker_pool[POOL_SIZE];

    pthread_mutex_t tcb_lock;
    pthread_mutex_init(&tcb_lock,NULL);
    
    worker_pool* pool; 
    
    pthread_mutex_lock(&tcb_lock);
    pool = initialize_workers(text_producer,POOL_SIZE,buffer, &buffer_lock, &tcb_lock,&buffer_cond);
    pthread_mutex_unlock(&tcb_lock);
    int i;
    for (i =0;i<POOL_SIZE;i++)
    {
        printf("field:%p\n",((pool->workers)[i])->buffer);
        assign_worker(pool,i,0,0);
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
        dispatcher(buffer,&buffer_lock,&buffer_cond);
        sleep(1);
    }
}
