/*
  WARNING: the overhead of POSIX ucontext is very high,
  assembly versions of libco or libco_sjlj should be much faster

  this library only exists for two reasons:
  1: as an initial test for the viability of a ucontext implementation
  2: to demonstrate the power and speed of libco over existing implementations,
     such as pth (which defaults to wrapping ucontext on unix targets)

  use this library only as a *last resort*
*/

#define LIBCO_C
#include "libco.h"
#include "settings.h"

#define _BSD_SOURCE
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

static thread_local ucontext_t co_primary;
static thread_local ucontext_t* co_running = 0;

cothread_t co_active() {
  if(!co_running) co_running = &co_primary;
  return (cothread_t)co_running;
}

cothread_t co_derive(void* memory, unsigned int heapsize, void (*coentry)(void)) {
  if(!co_running) co_running = &co_primary;
  ucontext_t* thread = (ucontext_t*)memory;
  memory = (unsigned char*)memory + sizeof(ucontext_t);
  heapsize -= sizeof(ucontext_t);
  if(thread) {
    if((!getcontext(thread) && !(thread->uc_stack.ss_sp = 0)) && (thread->uc_stack.ss_sp = memory)) {
      thread->uc_link = co_running;
      thread->uc_stack.ss_size = heapsize;
      makecontext(thread, coentry, 0);
    } else {
      thread = 0;
    }
  }
  return (cothread_t)thread;
}

cothread_t co_create(unsigned int heapsize, void (*coentry)(void)) {
  if(!co_running) co_running = &co_primary;
  ucontext_t* thread = (ucontext_t*)malloc(sizeof(ucontext_t));
  if(thread) {
    if((!getcontext(thread) && !(thread->uc_stack.ss_sp = 0)) && (thread->uc_stack.ss_sp = malloc(heapsize))) {
      thread->uc_link = co_running;
      thread->uc_stack.ss_size = heapsize;
      makecontext(thread, coentry, 0);
    } else {
      co_delete((cothread_t)thread);
      thread = 0;
    }
  }
  return (cothread_t)thread;
}

void co_delete(cothread_t cothread) {
  if(cothread) {
    if(((ucontext_t*)cothread)->uc_stack.ss_sp) { free(((ucontext_t*)cothread)->uc_stack.ss_sp); }
    free(cothread);
  }
}

void co_switch(cothread_t cothread) {
  ucontext_t* old_thread = co_running;
  co_running = (ucontext_t*)cothread;
  swapcontext(old_thread, co_running);
}

int co_serializable() {
  return 0;
}

#ifdef __cplusplus
}
#endif
