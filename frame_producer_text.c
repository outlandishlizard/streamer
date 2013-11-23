#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "circular_buffer.h"
#include "server.h"

#define POOL_SIZE 10
typedef struct {
    pthread_t thread;

    enum {IDLE,WORKING,KILLME,UNINITIALIZED} state;
    int name;
    int sockfd;
    int resource_fd;   
    pthread_cond_t *buffer_cond;
    pthread_cond_t *buffer_empty_cond;
    pthread_cond_t *tcb_cond;

    pthread_mutex_t *buffer_lock;
    pthread_mutex_t *tcb_lock;
    circBuff *buffer;
} tcb;

typedef struct {
    int priority;
    char* text;
} text_frame;

struct {
    tcb **workers;
    int size;
    pthread_mutex_t lock;
    void* task;
} worker_pool;


typedef struct {
    void ** from_buff;
    int length;
} flat_buffer;
/*
typedef struct {
    circBuff *buffer;
    void*   producer_func;
    pthread_mutex_t *buffer_lock;
    pthread_mutex_t *tcb_lock;
    pthread_cond_t  *buffer_cond;
    pthread_cond_t  *buffer_empty_cond;
} global_data

static global_data globals;
*/
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
            printf("State != WORKING, attempting to grab worker_pool.lock\n");
            pthread_mutex_lock(&worker_pool.lock);
            printf("State != WORKING, grabbed worker_pool.lock\n");
             
            int err;
            printf("About to wait on cond at:%p\n",&block->tcb_cond);        
            if (err = pthread_cond_wait(&block->tcb_cond, &worker_pool.lock))
            {
                printf("pthread_cond_wait failed in text_producer, error code:%d",err);
                return 0;
            }
            pthread_mutex_unlock(&worker_pool.lock);
        }

        //Begin actual text production.
        printf("started working!\n"); 
        char* text_string = (char*)calloc(512,sizeof(char));
        snprintf(text_string, (size_t)512,"Text Producer %d:%d\0",block->name,framenum);
       
        text_frame *frame = (text_frame*)calloc(1,sizeof(text_frame));
        frame->priority = block->name;
        frame->text = text_string;
        pthread_mutex_lock(&circular_buffer.lock);
        printf("In worker, got circbuff lock, waiting on cond\n");
        while (circBuff_push(circular_buffer.cb,frame))
        {
            //The buffer is full, or some other error state, we sleep until it isn't.
            pthread_cond_wait(&circular_buffer.producer_cond, &circular_buffer.lock);
        }
        if (circBuff_isOne(block->buffer))
        {
            printf("signalling consumer_cond\n");
            pthread_cond_signal(&circular_buffer.consumer_cond);
        }
        framenum++;
        pthread_mutex_unlock(&circular_buffer.lock);
        printf("In worker, released circbuff lock\n");
    }   
    return framenum;

}
int consume(circBuff* buffer)
{
    char* pop;
    pop = circBuff_pop(buffer);
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
    
    pthread_create(control->thread,NULL,worker_pool.task,(void*)control);
    printf("Signalling to start worker thread at cond: %p\n",control->tcb_cond);
    pthread_cond_signal(control->tcb_cond);
    return 1;
}
/*
worker_pool *create_worker_pool(int size)
{
    //TODO refactor initialize_workers into this and an actual initializer function so we can use intialize_workers after we dynamically get more workers.
    worker_pool *pool   = calloc(1,sizeof(worker_pool));
    pool->workers       = calloc(size,sizeof(tcb**));
    pool->size          = size;
    pool->tcb_lock      = tcb_lock;
    int i=0;
    for (i=0;i<size;i++)
    {
        tcb* worker         = calloc(1,sizeof(tcb));
        (pool->workers)[i]  = worker;
        worker->state       = UNINITIALIZED;
    }
    return pool;
}*/
int create_worker_pool(void *task, int size)
{
    worker_pool.workers = calloc(size,sizeof(tcb**));
    worker_pool.size    = size;
    worker_pool.task    = task;
    //pthread_mutex_init(&worker_pool.lock, NULL);
    int i =0;
    for (i=0;i<size;i++)
    {
        tcb* worker = calloc(1,sizeof(tcb));
        worker_pool.workers[i] = worker;

        worker->state        = UNINITIALIZED;

    //    pthread_cond_init(&worker->tcb_cond, NULL);
      //  pthread_create(&worker->thread, NULL, task, (void *)worker);
    }
    return 0;
}
void initialize_workers()
{
    int i =0;
    tcb* worker;
    for (i=0;i<worker_pool.size;i++)
    {
        worker = (worker_pool.workers)[i];
        if(worker->state == UNINITIALIZED)
        {
        worker->state               = IDLE;
        worker->tcb_lock            = &worker_pool.lock;
        worker->buffer_lock         = &circular_buffer.lock;
        worker->buffer_cond         = &circular_buffer.producer_cond;
        worker->buffer_empty_cond   = &circular_buffer.consumer_cond;

        worker->buffer       = circular_buffer.cb;

        worker->tcb_cond     = calloc(1,sizeof(pthread_cond_t));
        worker->thread       = calloc(1,sizeof(pthread_t));

        pthread_cond_init(worker->tcb_cond,NULL);
        }
    }
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
    int newmem;
    if (newmem = realloc(worker_pool.workers, (2 * worker_pool.size * sizeof(tcb *))))
    {
        worker_pool.workers= newmem;
        int i=0;
        for (i=worker_pool.size;i<worker_pool.size*2;i++)
        {
            worker_pool.workers[i]->state = UNINITIALIZED;
        }
        worker_pool.size *= 2;
        initialize_workers();
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

flat_buffer* dispatcher_copybuffer()
{
while (circBuff_isEmpty(circular_buffer.cb))
        {
            printf("in copybuffer, buffer empty, waiting on consumer_cond\n");
            int err;        
            if (err = pthread_cond_wait(&circular_buffer.consumer_cond,&circular_buffer.lock))
            {
                printf("pthread_cond_wait failed in dispatcher, error code:%d",err);
                return 0;
            }
        }
        printf("in copybuffer, finished waiting.\n");
  int count = 0;
  void** from_buff = (void**)calloc((circular_buffer.cb)->size, sizeof(void**));
  while ((from_buff[count] = circBuff_pop(circular_buffer.cb)) != 0){
      count++;
  }
  flat_buffer* flat = (flat_buffer*)calloc(1,sizeof(flat_buffer));
  flat->length = count;
  flat->from_buff=from_buff;
  return flat;

}

int dispatcher_thread(void* arg)//ARG is unused.
{
    while(1)
    {
        printf("in dispatcher,trying to acquire circular_buffer.lock\n");
        pthread_mutex_lock(&circular_buffer.lock);
        printf("in dispatcher,got circular_buffer.lock\n");

        //BEGIN CRITICAL 
        flat_buffer* flat_buff = dispatcher_copybuffer();
        pthread_cond_broadcast(&circular_buffer.producer_cond);
        pthread_mutex_unlock(&circular_buffer.lock);
        printf("In dispatcher, released lock\n");
        //END CRITICAL
       //Perform noncritical section
        dispatcher_transmit(flat_buff);
    }
}

void dispatcher_transmit(flat_buffer* flat_buff) {
    //Note here that length is NOT nescessarily the full size of the buffer, as we may have dispatched without a full buffer.
  // Now send them all
  void **from_buff = flat_buff->from_buff; 
  for (int k=0; k < flat_buff->length; k++) {
    printf("text:%s\n",((text_frame*)from_buff[k])->text);
    fflush(stdout);  
    //write(i->socket, i->text, strlen(i->text));
    free_text_frame(from_buff[k]);
  }
  free(from_buff);
  free(flat_buff);
}

int server_thread(void* args) {
  //8080 is the default port, the user can change this at runtime though
  int PORT = (int)args;
  int MAX_CLIENTS=100;
  int REQUEST_SIZE=1024; 
  int server_socket = make_server_socket(PORT);
  
  // Start accepting clients, forking for each new one
  int i;
  for(i=0; i<MAX_CLIENTS; i++) {
    
    int client_socket = get_client(server_socket);
    if (client_socket < 0) {
      printf("ERROR adding client\n");
      continue;
    }
    
    printf("Another client accepted for a total of %d\n", i+1);
    
    // Fork a process for handling the request
    // Will later change this to giving to a thread
    // Parent just continues the loop, wainting for another client
    
    if(fork() == 0) {
      // Read a request from the client
      char buffer[REQUEST_SIZE];
      bzero(buffer, REQUEST_SIZE);
      
      int n = read(client_socket, buffer, REQUEST_SIZE-1);
      if (n < 0)
        error("ERROR reading from socket\n");
      
      // Print the request
      printf("From client %d:\n%s\n", i, buffer);
      
      // DO SHIT
      // This part of it be difficult
      // Get the workers, try to find a new one
      // If there is one, give it the task of this user
      // If there not be one, make a new one
      // If there are too many, kill idle ones
      
      return 0;
    }
    // Close socket?
  }
  // The main loop is over, so close the socket and finish up
  close(server_socket);
  return 0;
}



int main(int argc,char** argv)
{
    
    //Begin initializing global locks and conds
/*
    pthread_mutex_t buffer_lock;
    pthread_mutex_init(&buffer_lock,NULL);
    
    pthread_cond_t  buffer_cond;
    pthread_cond_init(&buffer_cond,NULL);

    pthread_cond_t buffer_empty_cond;
    pthread_cond_init(&buffer_empty_cond,NULL);

    circBuff* buffer;
    buffer = circBuff_init(100);

    pthread_mutex_t tcb_lock;
    pthread_mutex_init(&tcb_lock,NULL);
    
    worker_pool* pool; 
*/ 
    pthread_mutex_init(&circular_buffer.lock,NULL);
    pthread_cond_init(&circular_buffer.producer_cond, NULL);
    pthread_cond_init(&circular_buffer.consumer_cond, NULL);
    circular_buffer.cb = circBuff_init(100);

//    pthread_t producers[POOL_SIZE];
//    tcb worker_pool[POOL_SIZE];

    pthread_mutex_init(&worker_pool.lock,NULL);
    pthread_mutex_lock(&worker_pool.lock);
    create_worker_pool(text_producer,POOL_SIZE);
    initialize_workers();
    pthread_mutex_unlock(&worker_pool.lock);



/*    int i;
    
    for (i = 0; i < POOL_SIZE; i++)
    {
        assign_worker(i,0,0);
    }
*//*
        dispatch_arg *disp_args = calloc(1,sizeof(dispatch_arg));
        disp_args->buffer = buffer;
        disp_args->buffer_lock  = &buffer_lock;
        disp_args->buffer_cond  = &buffer_cond;
        disp_args->buffer_empty_cond = &buffer_empty_cond;
*/
        pthread_t disp_thread;
        pthread_create(&disp_thread,NULL,dispatcher_thread,(void*)0);
        while(1)
        {
            sleep(1000);
        }
}
