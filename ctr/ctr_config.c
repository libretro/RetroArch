

#ifndef CTR_STACK_SIZE
#define CTR_STACK_SIZE         0x100000
#endif

#ifndef CTR_LINEAR_HEAP_SIZE
#define CTR_LINEAR_HEAP_SIZE   0x600000
#endif

#ifndef CTR_MAX_HEAP_SIZE
#define CTR_MAX_HEAP_SIZE     0x6000000
#endif


int          __stacksize__      = CTR_STACK_SIZE;
unsigned int __linear_heap_size = CTR_LINEAR_HEAP_SIZE;
unsigned int __heap_size        = CTR_MAX_HEAP_SIZE + CTR_STACK_SIZE;
