#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "circular_buffer.h"
#include "server.h"

#define POOL_SIZE 10

typedef struct {
    pthread_t *thread;

    enum {IDLE,WORKING,SHINITAI,UNINITIALIZED} state;
    int name;
    int sockfd;
    int resource_fd;
    char* path;   
    pthread_cond_t tcb_cond;
    circBuff *buffer;
} tcb;


struct {
    tcb **workers;
    int size;
    pthread_mutex_t lock;
    void* task;
} worker_pool;

struct {
  circBuff *cb;
  pthread_cond_t producer_cond;
  pthread_cond_t consumer_cond;
  pthread_mutex_t lock;
} circular_buffer;


typedef struct {
    int priority;
    tcb* owner;
    char* text;
    int length;
} text_frame;

typedef struct {
    void ** from_buff;
    int length;
} flat_buffer;
typedef struct {
    char* text;
    int length;
} rbuff_ret;
void tcb_cleanup(tcb* block)
{
    block->name         = 0;
    block->sockfd       = 0;
    block->resource_fd  = 0;
    block->state        = IDLE;
    return;
}

void free_text_frame(text_frame *tf)
{
    free(tf->text);
    free(tf);
}

rbuff_ret* get_jpeg_data(char* path,int index)
{
//    printf("In get_jpeg_data\n");
    char* filename = calloc(256,sizeof(char));
    snprintf(filename,(size_t)256,"%s/frame-%d.jpeg",path,index);
    int fd=0;
    if((fd = open(filename,O_RDONLY)) < 0)
    {
        free(filename);
        if (index == 0)
        {
        printf("Failed to open %s\n",filename);
        return 0;
        }
        else
          return (rbuff_ret *)-1;
    }
//    printf("Opened file in jpeg, fd:%d,path:%s\n",fd,filename);
    int rsize = 2048;
    int chunk = 2048;
    int sizecount = 0;
    char* rbuff = calloc(2048,sizeof(char));
//    printf("About to enter loop\n");
    int amount=0;
    while((amount = read(fd,(rbuff),(size_t)chunk)) > 0)
    {
        sizecount+=amount;
        printf("Looping in jpeg, read:%d from %s\n",amount,filename);
        rbuff = realloc(rbuff,rsize+chunk);
        rsize+=amount;
    }
    free(filename);
    printf("in jpeg, returning frame of length:~%d\n",sizecount);
    rbuff_ret *ret = calloc(1,sizeof(rbuff_ret));
    ret->text = rbuff;
    ret->length=sizecount;
    return ret;
}

struct text_helper_struct {
  text_frame *frame;
  tcb *block;
  int *framenumber;
};

int text_helper (struct text_helper_struct *s) {
        printf("In worker, got circbuff lock, waiting on cond\n");
        while (circBuff_push(circular_buffer.cb, s->frame))
        {
            //The buffer is full, or some other error state, we sleep until it isn't.
            pthread_cond_wait(&circular_buffer.producer_cond, &circular_buffer.lock);
        }
        if (circBuff_isFull(s->block->buffer))
        {
            printf("signalling consumer_cond\n");
            pthread_cond_signal(&circular_buffer.consumer_cond);
        }
        (*(s->framenumber))++;
        return 0;
}

int text_producer(void* _block)
{
    /* Logic:
     *  1. Block on tcb_cond in our tcb until someone moves us into WORKING and unblocks tcb_cond
     *  2. Try to acquire buffer_cond, block until we do (ie, grab lock, 4
     *  3. Add a data block to the circular buffer
     *  4. Release buffer_cond
     *  5. Loop, unless state == SHINITAI, in which case, cleanup. Note that sync problems are possible here (ie, we may have to go through an extra iteration before noticing we're supposed to die in some cases, however, this will at worst cause minor control latency issues, nothing more.
     * */

    tcb* block = (tcb*)_block;
    int framenum = 0;
    int paused = 0;
    while(1)
    {
        if (block->state == SHINITAI)
        {
            tcb_cleanup(block);
        }
        //Check that we're in a state that we should be in, block until we are.
        while (block->state != WORKING)
        {
            int err;
            pthread_mutex_lock(&worker_pool.lock);
            err = pthread_cond_wait(&block->tcb_cond, &worker_pool.lock);
            pthread_mutex_unlock(&worker_pool.lock);
            if (err)
            {
                printf("pthread_cond_wait failed in text_producer, error code:%d",err);
                return 0;
            }
        }

        //Begin actual text production.
        printf("started working!\n");
	// Check if there is a msg from the client
	int command = -1;
	int jump = 0;
    if (!paused)
	recv(block->sockfd, &command, sizeof(int) , MSG_DONTWAIT);
    else
	recv(block->sockfd, &command, sizeof(int) , 0);
	command = ntohl(command);
	printf("Command:%d\n",command);
    switch(command) {
	case -1:
		break;
	case 0:
	        recv(block->sockfd, &jump, sizeof(int), 0);
		jump = ntohl(jump);
		framenum = jump;
                printf("SKIPPING TO %d\n", jump);
		break;
	case 1:
        printf("PAUSING\n");
		paused ^= 1;
		break;
	case 2:
                printf("DYING\n");
		block->state = SHINITAI;
		break;
	default:
                printf("NOPE. GOT %d\n", command);
		break;
	}
	if (paused) {
		continue;
	}
        // Else build the frame for writing
    //    char* text_string = (char*)calloc(512,sizeof(char));
      //  snprintf(text_string, (size_t)512,"Text Producer %d:%d", block->name, framenum);
        sleep(1);
        text_frame *frame = calloc(1,sizeof(text_frame));	
    //    sleep(1);
        rbuff_ret *image_data = get_jpeg_data(block->path,framenum); 
        if (image_data == (rbuff_ret *)-1)
        {
            framenum=0;
            image_data = get_jpeg_data(block->path,framenum);
            if (image_data == 0)
            {
               block->state = SHINITAI; 
               continue;
            }
        }
        frame->priority = block->name;
        frame->owner = block;
        frame->text = image_data->text;
        frame->length= image_data->length;
        free(image_data);
        struct text_helper_struct t = {
          .frame = frame,
          .block = block,
          .framenumber = &framenum,
        };
        pthread_mutex_lock(&circular_buffer.lock);
        text_helper(&t);
        pthread_mutex_unlock(&circular_buffer.lock);
    }   
    return framenum;

}

struct dispatch_struct {
  tcb *control;
  int name;
  int sockfd;
  int resource_fd;
  char *resname;
};

int dispatch(struct dispatch_struct *d)
{
    if (d->control->state == WORKING)
    {
        return 1;
    }
    d->control->name   = d->name;
    d->control->sockfd = d->sockfd;
    d->control->state  = WORKING;
    d->control->resource_fd = d->resource_fd;
    d->control->path = d->resname;
    //signal for wakeup on semaphore
    
    pthread_create(d->control->thread,NULL,worker_pool.task,(void*)d->control);
//    printf("Signalling to start worker thread at cond: %p\n",control->tcb_cond);
    pthread_cond_signal(&d->control->tcb_cond);
    return 1;
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
        worker->buffer       = circular_buffer.cb;
        worker->thread       = calloc(1,sizeof(pthread_t));
        pthread_cond_init(&worker->tcb_cond,NULL);
        }
    }
}

struct create_worker_pool_struct {
  void *task;
  int size;
};

int create_worker_pool(struct create_worker_pool_struct *c)
{
    worker_pool.workers = calloc(c->size,sizeof(tcb**));
    worker_pool.size    = c->size;
    worker_pool.task    = c->task;
    int i;
    for (i = 0; i < c->size; i++)
    {
        tcb* worker = calloc(1,sizeof(tcb));
        worker_pool.workers[i] = worker;

        worker->state        = UNINITIALIZED;
    }
    initialize_workers();
    return 0;
}


int pool_grow (void __attribute__((unused)) *a)
{
    tcb **newmem = realloc(worker_pool.workers, (2 * worker_pool.size * sizeof(tcb *)));
    if (newmem)
    {
        worker_pool.workers= newmem;
        int i=0;
        for (i=worker_pool.size;i<worker_pool.size*2;i++)
        {
            printf("growing:%d\n",i);
            worker_pool.workers[i] = calloc(1,sizeof(tcb));
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

int assign_worker (int name, int sockfd, int resource_fd,char* resname)
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
        pthread_mutex_lock(&worker_pool.lock);
        pool_grow(NULL);
        pthread_mutex_unlock(&worker_pool.lock);
        return assign_worker(name,sockfd,resource_fd,resname);
    }
    struct dispatch_struct d = {
      .control = worker_pool.workers[i],
      .name = name,
      .sockfd = sockfd,
      .resource_fd = resource_fd,
      .resname = resname,
    };
    pthread_mutex_lock(&worker_pool.lock);
    dispatch(&d);
    pthread_mutex_unlock(&worker_pool.lock);
    return 0;
}


flat_buffer* dispatcher_copybuffer (void __attribute__((unused)) *a)
{
  while (circBuff_isEmpty(circular_buffer.cb)) {
    printf("dispatcher in copybuffer, buffer empty, waiting on consumer_cond\n");
    int err = pthread_cond_wait(&circular_buffer.consumer_cond,&circular_buffer.lock);
    if (err) {
      printf("pthread_cond_wait failed in dispatcher, error code:%d",err);
      return 0;
    }
  }
  printf("dispatcher in copybuffer, finished waiting.\n");
  int count = 0;
  void** from_buff = (void**)calloc((circular_buffer.cb)->size, sizeof(void**));
  while ((from_buff[count] = circBuff_pop(circular_buffer.cb)) != 0) {
      count++;
  }
  flat_buffer* flat = (flat_buffer*)calloc(1,sizeof(flat_buffer));
  flat->length = count;
  flat->from_buff=from_buff;
  return flat;

}

void *shinitai (text_frame *t) {
  t->owner->state = SHINITAI;
  return NULL;
}

void dispatcher_transmit(flat_buffer* flat_buff) {
    //Note here that length is NOT nescessarily the full size of the buffer, as we may have dispatched without a full buffer.
  // Now send them all
    printf("In dispatcher_transmit\n");
    text_frame **from_buff = (text_frame**)(flat_buff->from_buff); 
  for (int k=0; k < flat_buff->length; k++) {
    int frame_sockfd = (from_buff[k])->owner->sockfd;
    char *frame_text = (from_buff[k])->text;
    int len = (from_buff[k])->length;
    printf("dispatcher Text:%s\n",frame_text);
    printf("dispatcher packet length:%d\n",len);
    if(send(frame_sockfd, &len, sizeof(int),MSG_DONTWAIT|MSG_NOSIGNAL)==-1)
    {
        printf("dispatcher Send failed length!\n");
        pthread_mutex_lock(&worker_pool.lock);
        shinitai(from_buff[k]);
        pthread_mutex_unlock(&worker_pool.lock);
    }
    
    if(send(frame_sockfd, frame_text, len,MSG_DONTWAIT|MSG_NOSIGNAL)==-1)
    {
        printf("dispatcher Send failed!\n");
        pthread_mutex_lock(&worker_pool.lock);
        shinitai(from_buff[k]);
        pthread_mutex_unlock(&worker_pool.lock);
    }
    free_text_frame(from_buff[k]);
  }
  free(from_buff);
  free(flat_buff);
}

void *dispatcher_thread(void __attribute__ ((unused)) *arg)
{
    while(1)
    {
        printf("dispatcher About to enter copybuffer\n");
        pthread_mutex_lock(&circular_buffer.lock);
        flat_buffer* flat_buff = dispatcher_copybuffer(NULL);
        pthread_mutex_unlock(&circular_buffer.lock);
        printf("dispatcher Finished copybuffer\n");
        pthread_cond_broadcast(&circular_buffer.producer_cond);
        printf("In dispatcher, released lock\n");
        //END CRITICAL
       //Perform noncritical section
        dispatcher_transmit(flat_buff);
    }
}


void *server_thread(void* args) {
  //8080 is the default port, the user can change this at runtime though
  int PORT = *(int*)args;
  int server_socket = make_server_socket(PORT);
  
  // Start accepting clients, forking for each new one
  int i=0;
  while(1) {
    
    int client_socket = get_client(server_socket);
    if (client_socket < 0) {
      printf("ERROR adding client\n");
      continue;
    }
    
    printf("Another client accepted for a total of %d\n", ++i);
    
    // Fork a process for handling the request
    // Will later change this to giving to a thread
    // Parent just continues the loop, wainting for another client
    char* requestname;
    int requestsize=0;
	recv(client_socket, &requestsize, sizeof(int) , 0);
    requestname = calloc(requestsize,sizeof(char));
    recv(client_socket, requestname, requestsize,0);
    assign_worker(i,client_socket,0,requestname);
  }
  // The main loop is over, so close the socket and finish up
  close(server_socket);
  return 0;
}



int main (void)
{
    
    //Begin initializing global locks and conds
    pthread_mutex_init(&circular_buffer.lock,NULL);
    pthread_cond_init(&circular_buffer.producer_cond, NULL);
    pthread_cond_init(&circular_buffer.consumer_cond, NULL);
    circular_buffer.cb = circBuff_init(100);

    pthread_mutex_init(&worker_pool.lock,NULL);
    struct create_worker_pool_struct c = {
      .task = text_producer,
      .size = 1,
    };
    pthread_mutex_lock(&worker_pool.lock);
    create_worker_pool(&c);
    pthread_mutex_unlock(&worker_pool.lock);

    pthread_t disp_thread;
    pthread_t serv_thread;

    int port = 8080;

    pthread_create(&disp_thread,NULL,dispatcher_thread,(void*)0);
    pthread_create(&serv_thread,NULL,server_thread,(void*)&port);

    while (1) {
        sleep(1000);
    }
}
