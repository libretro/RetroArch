#include "switch_pthread.h"

#include <errno.h>

//extern unsigned cpu_features_get_core_amount(void);

// Access is safe by safe_double_thread_launch Mutex
static uint32_t threadCounter = 1;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    u32 prio = 0;
    Thread new_switch_thread;

    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

    //start_routine_jump = start_routine;
    //if (threadCounter == cpu_features_get_core_amount())
    //    threadCounter = 1;

    //int rc = threadCreate(&new_switch_thread, switch_thread_launcher, arg, STACKSIZE, prio - 1, 1);
    int rc = threadCreate(&new_switch_thread, (void (*)(void *))start_routine, arg, STACKSIZE, prio - 1, 1);

    if (R_FAILED(rc))
    {
        return EAGAIN;
    }

    printf("[Threading]: Starting Thread(#%i)\n", threadCounter);
    if (R_FAILED(threadStart(&new_switch_thread)))
    {
        threadClose(&new_switch_thread);
        return -1;
    }

    *thread = new_switch_thread;

    return 0;
}

void pthread_exit(void *retval)
{
    (void)retval;
    printf("[Threading]: Exiting Thread\n");
    svcExitThread();
}
