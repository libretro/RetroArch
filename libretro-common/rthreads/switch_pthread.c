/* Copyright  (C) 2018 - M4xw <m4x@m4xw.net>, RetroArch Team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (switch_pthread.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "switch_pthread.h"

#define STACKSIZE 1000000 * 12 // 12 MB
static uint32_t threadCounter = 1;
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
   u32 prio = 0;

   Thread new_switch_thread;

   svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

   // Launch threads on Core 1
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
