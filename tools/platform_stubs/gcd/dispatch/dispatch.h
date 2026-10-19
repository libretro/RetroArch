/* Stand-in for Apple's <dispatch/dispatch.h>: the surface task_queue.c
 * uses, enough to syntax-check its HAVE_GCD arm on a non-Apple host
 * with clang -fblocks.  Not a runtime. */
#ifndef DISPATCH_DISPATCH_H_STUB
#define DISPATCH_DISPATCH_H_STUB
#include <stdint.h>
typedef uint64_t dispatch_time_t;
typedef long dispatch_once_t;
typedef void (*dispatch_function_t)(void *);
typedef struct dispatch_queue_attr_s *dispatch_queue_attr_t;
#define DISPATCH_QUEUE_SERIAL ((dispatch_queue_attr_t)0)
typedef struct dispatch_queue_s *dispatch_queue_t;
typedef void (^dispatch_block_t)(void);
#define DISPATCH_TIME_NOW ((dispatch_time_t)0)
#define QOS_CLASS_USER_INITIATED 0x19
#define DISPATCH_QUEUE_PRIORITY_DEFAULT 0
dispatch_queue_t dispatch_get_global_queue(long identifier, unsigned long flags);
void dispatch_async(dispatch_queue_t queue, dispatch_block_t block);
dispatch_queue_t dispatch_queue_create(const char *, dispatch_queue_attr_t);
void dispatch_async_f(dispatch_queue_t, void *, dispatch_function_t);
void dispatch_once_f(dispatch_once_t *, void *, dispatch_function_t);
dispatch_time_t dispatch_time(dispatch_time_t when, int64_t delta);
void dispatch_after(dispatch_time_t when, dispatch_queue_t queue, dispatch_block_t block);
#endif
