#ifndef VT_VP9_MOCK_DISPATCH_H
#define VT_VP9_MOCK_DISPATCH_H
#include <stdint.h>
#include <stddef.h>
typedef struct mock_dispatch *dispatch_queue_t;
typedef struct mock_dispatch *dispatch_source_t;
typedef uint64_t dispatch_time_t;
#define DISPATCH_QUEUE_SERIAL NULL
#define DISPATCH_SOURCE_TYPE_DATA_ADD 1
#define DISPATCH_SOURCE_TYPE_TIMER 2
#define DISPATCH_TIME_NOW 0
#define DISPATCH_TIME_FOREVER ((uint64_t) - 1)
#define NSEC_PER_MSEC 1000000
dispatch_queue_t dispatch_queue_create(const char *, void *);
dispatch_source_t dispatch_source_create(
      int, uintptr_t, unsigned long, dispatch_queue_t);
void dispatch_retain(void *);
void dispatch_release(void *);
void dispatch_set_context(void *, void *);
void dispatch_source_set_event_handler_f(dispatch_source_t, void (*)(void *));
void dispatch_source_set_cancel_handler_f(dispatch_source_t, void (*)(void *));
void dispatch_source_set_timer(
      dispatch_source_t, dispatch_time_t, uint64_t, uint64_t);
void dispatch_resume(void *);
void dispatch_source_cancel(dispatch_source_t);
void dispatch_source_merge_data(dispatch_source_t, unsigned long);
dispatch_time_t dispatch_time(dispatch_time_t, int64_t);
#endif
