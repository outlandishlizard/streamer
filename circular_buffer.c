#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <circular_buffer.h>


typedef struct
{
	int * base;
	int size;
	int read;
	int write;

} circBuff;

circBuff circBuff_init(int size)
{	

	circBuff buffer;
	buffer.base 	= calloc(size,sizeof(char*));
	buffer.read	= 0;
	buffer.write	= 0;
	buffer.size	= size;
	return buffer;
}
int circBuff_isEmpty(circBuff* buffer)
{
    return buffer->read == buffer->write;
}
int circBuff_push(circBuff* buffer, void* item)
{
	//Invariant: write should never be equal to read if we're full-- it will be if we're EMPTY!
	if ((buffer->write + 1) % (buffer->size) != buffer->read)
	{
		(buffer->base)[buffer->write] = item;
		buffer->write = ((buffer->write)+1) % (buffer->size);
		return 1;
	}

	else
	{
		return 0;
	}
}

int* circBuff_pop(circBuff* buffer)
{
	if ((buffer->write) == (buffer->read))
	{
		//We are empty, fail!
		return 0;
	}
	else
	{
		int * ret;
		ret = (buffer->base)[buffer->read];
		buffer->read = ((buffer->read)+1 % (buffer->size));
		return ret;
	}
	
}

int main (int argc, char** argv)
{
	circBuff buffer = circBuff_init(100);
	int i = 0;
	int array[10] = {1,2,3,4,5,6,7,8,9,10};	
	for (i=1;i<100;i++)
	{
		int ret =0;
		ret = circBuff_push(&buffer,array+(i%10));
		printf("%d,%x,%x\n",ret,array+(i%10),buffer.write);
	}

	for (i=0;i<100;i++)
	{
		int* x;
		int y;
		x = (circBuff_pop(&buffer));
		y = buffer.read;
		printf("Pop:%x,%x,%x\n",x,y,array);
	}
	exit(1);
}
