#include <pthread.h>
#include <circular_buffer.h>

#define POOL_SIZE 10

typedef struct{
    typedef enum {IDLE,WORKING,KILLME} state;
    int name;
    int sockfd;
    
    pthread_cond_t* buffer_cond;
    pthread_cond_t* tcb_cond;

    pthread_mutex_t* buffer_lock;
    pthread_mutex_t* tcb_lock;
} tcb;

int text_producer(void* name)
{
    /* Logic:
     *  1. Block on tcb_cond in our tcb until someone moves us into WORKING and unblocks tcb_cond
     *  2. Try to acquire buffer_cond, block until we do (ie, grab lock, 4
     *  3. Add a data block to the circular buffer
     *  4. Release buffer_cond
     *  5. Loop, unless state == KILLME, in which case, cleanup. Note that sync problems are possible here (ie, we may have to go through an extra iteration before noticing we're supposed to die in some cases, however, this will at worst cause minor control latency issues, nothing more.
     * */
    

}
int consume(circBuff* buffer)
{
    printf("Popped:%s",circBuff_pop(buffer));
}

int dispatch(tcb* control,int name,int sockfd)
{
    if (tcb->state == WORKING)
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
int main(int argc,char** argv)
{
	pthread_mutex_t tcb_lock;
	pthread_mutex_t buffer_lock;
   
    pthread_cond_t  buffer_cond;
    pthread_cond_t  tcb_cond;

    pthread_cond_init(buffer_cond,NULL);
    pthread_cond_init(tcb_cond,NULL);

    circBuff buffer;
	buffer = circBuff_init(100);
	pthread_t producers[POOL_SIZE];
    tcb worker_pool[POOL_SIZE]
	int i = 0;
	for (i=0;i<POOL_SIZE;i++)
	{
        worker_pool[i].state    = IDLE;
        worker_pool[i].name     = i;
        worker_pool[i].sockfd   = 0;
        worker_pool[i].buffer_cond = buffer_cond;
        worker_pool[i].buffer_lock = buffer_lock;
        worker_pool[i].tcb_cond = tcb_cond;

		pthread_create(producers+i,NULL,text_producer, (void*)i);
	}
    
}
