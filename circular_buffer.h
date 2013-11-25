struct circBuff_t
{
	void ** base;
	int size;
	int read;
	int write;
};
typedef struct circBuff_t circBuff;

circBuff* circBuff_init(int size);
int circBuff_push(circBuff* buffer, void* item);
int circBuff_isEmpty(circBuff* buffer);
int circBuff_isFull(circBuff* buffer);
int* circBuff_pop(circBuff* buffer);
