struct circBuff;
circBuff circBuff_init(int size);
int circBuff_push(circBuff* buffer, void* item);
int* circBuff_pop(circBuff* buffer);
