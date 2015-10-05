

#ifndef CTR_STACK_SIZE
#define CTR_STACK_SIZE         0x100000
#endif

#ifndef CTR_LINEAR_HEAP_SIZE
#define CTR_LINEAR_HEAP_SIZE   0x600000
#endif

int          __stacksize__    = CTR_STACK_SIZE;
unsigned int linear_heap_size = CTR_LINEAR_HEAP_SIZE;
