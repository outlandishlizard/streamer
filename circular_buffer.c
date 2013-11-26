#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "circular_buffer.h"

circBuff* circBuff_init(int size)
{	
	circBuff* buffer= (circBuff*)(calloc(1,sizeof(circBuff)));	
    buffer->base 	= (void**)(calloc(size,sizeof(void*)));
    buffer->read	= 0;
	buffer->write	= 0;
	buffer->size	= size;
    return buffer;
}

/* Return 1 if empty */
int circBuff_isEmpty(circBuff *buffer)
{
    return buffer->read == buffer->write;
}

/* Return 1 if full */
int circBuff_isFull(circBuff *buffer)
{
    return (buffer->read+1)%(buffer->size) == buffer->write % buffer->size;
}

int circBuff_push(circBuff *buffer, void *item)
{
	//Invariant: write should never be equal to read if we're full-- it will be if we're EMPTY!
    if (((buffer->write + 1) % buffer->size) != buffer->read)
	{
		buffer->base[buffer->write] = item;
		buffer->write = (buffer->write + 1) % buffer->size;
		return 0;
	}

	else
	{
		return 1;
	}
}

int* circBuff_pop(circBuff *buffer)
{
	if ((buffer->write % (buffer->size)) == (buffer->read % (buffer->size)))
	{
		//We are empty, fail!
		return 0;
	}
	else
	{
        fflush(stdout);
        int *ret;
		ret = buffer->base[buffer->read];
		buffer->read = (((buffer->read)+1) % (buffer->size));
		return ret;
	}
	
}

