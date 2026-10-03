/* Stand-in for Apple's <dispatch/dispatch.h>: the surface task_queue.c
 * uses, enough to syntax-check its HAVE_GCD arm on a non-Apple host
 * with clang -fblocks.  Not a runtime. */
#ifndef DISPATCH_DISPATCH_H_STUB
#define DISPATCH_DISPATCH_H_STUB
#include <stdint.h>
typedef uint64_t dispatch_time_t;
typedef struct dispatch_queue_s *dispatch_queue_t;
typedef void (^dispatch_block_t)(void);
#define DISPATCH_TIME_NOW ((dispatch_time_t)0)
#define QOS_CLASS_USER_INITIATED 0x19
dispatch_queue_t dispatch_get_global_queue(long identifier, unsigned long flags);
void dispatch_async(dispatch_queue_t queue, dispatch_block_t block);
dispatch_time_t dispatch_time(dispatch_time_t when, int64_t delta);
void dispatch_after(dispatch_time_t when, dispatch_queue_t queue, dispatch_block_t block);
#endif
