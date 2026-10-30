/* A stand-in for <dispatch/dispatch.h> on rthreads: the part of the
 * API task_queue.c's GCD runner calls, so its samples run that runner
 * on a host without libdispatch (GCD=standin).  Queues, async calls,
 * suspend and resume, and timer sources with GCD's ordering: a serial
 * queue runs one call at a time in order, a source's cancel handler
 * runs after any event handler already on its way, and no event
 * handler starts after a cancel.  Test infrastructure, not a port: it
 * keeps its lists under one lock. */
#ifndef DISPATCH_STANDIN_H
#define DISPATCH_STANDIN_H

#include <stdint.h>

typedef uint64_t dispatch_time_t;
typedef void (*dispatch_function_t)(void *);
typedef struct dispatch_standin_queue  *dispatch_queue_t;
typedef struct dispatch_standin_source *dispatch_source_t;
typedef const struct dispatch_standin_attr *dispatch_queue_attr_t;
typedef const void *dispatch_source_type_t;

#define DISPATCH_QUEUE_SERIAL      ((dispatch_queue_attr_t)0)
#define DISPATCH_QUEUE_CONCURRENT  ((dispatch_queue_attr_t)1)
#define DISPATCH_SOURCE_TYPE_TIMER ((dispatch_source_type_t)1)
#define DISPATCH_TIME_NOW          ((dispatch_time_t)0)
#define DISPATCH_TIME_FOREVER      (~(dispatch_time_t)0)
#define NSEC_PER_USEC              ((uint64_t)1000)
#define QOS_CLASS_USER_INITIATED   0x19

dispatch_queue_t  dispatch_get_global_queue(long identifier,
      unsigned long flags);
dispatch_queue_t  dispatch_queue_create(const char *label,
      dispatch_queue_attr_t attr);
void dispatch_async_f(dispatch_queue_t queue, void *context,
      dispatch_function_t work);
dispatch_source_t dispatch_source_create(dispatch_source_type_t type,
      uintptr_t handle, unsigned long mask, dispatch_queue_t queue);
void dispatch_source_set_event_handler_f(dispatch_source_t source,
      dispatch_function_t handler);
void dispatch_source_set_cancel_handler_f(dispatch_source_t source,
      dispatch_function_t handler);
void dispatch_source_set_timer(dispatch_source_t source,
      dispatch_time_t start, uint64_t interval, uint64_t leeway);
void dispatch_source_cancel(dispatch_source_t source);
dispatch_time_t dispatch_time(dispatch_time_t when, int64_t delta);

/* The calls on dispatch_object_t, for either kind of object. */
void dispatch_standin_set_target_queue(void *object, dispatch_queue_t q);
void dispatch_standin_set_context(void *object, void *context);
void dispatch_standin_suspend(void *object);
void dispatch_standin_resume(void *object);
void dispatch_standin_release(void *object);
#define dispatch_set_target_queue(o, q) \
   dispatch_standin_set_target_queue((void*)(o), (q))
#define dispatch_set_context(o, c) \
   dispatch_standin_set_context((void*)(o), (c))
#define dispatch_suspend(o) dispatch_standin_suspend((void*)(o))
#define dispatch_resume(o)  dispatch_standin_resume((void*)(o))
#define dispatch_release(o) dispatch_standin_release((void*)(o))

/* How many calls the stand-in has run, for a sample to check that
 * the runner went through it. */
unsigned long dispatch_standin_calls(void);

#endif
