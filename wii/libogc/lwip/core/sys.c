/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/memp.h"

#if (NO_SYS == 0)

struct sswt_cb
{
    s16_t timeflag;
    sys_sem_t *psem;
};

void
sys_mbox_fetch(sys_mbox_t mbox, void **msg)
{
  u32_t time;
  struct sys_timeouts *timeouts;
  struct sys_timeout *tmptimeout;
  sys_timeout_handler h;
  void *arg;

 again:
  timeouts = sys_arch_timeouts();

  if (!timeouts || !timeouts->next) {
    sys_arch_mbox_fetch(mbox, msg, 0);
  } else {
    if (timeouts->next->time > 0) {
      time = sys_arch_mbox_fetch(mbox, msg, timeouts->next->time);
    } else {
      time = SYS_ARCH_TIMEOUT;
    }

    if (time == SYS_ARCH_TIMEOUT) {
      /* If time == SYS_ARCH_TIMEOUT, a timeout occured before a message
   could be fetched. We should now call the timeout handler and
   deallocate the memory allocated for the timeout. */
      tmptimeout = timeouts->next;
      timeouts->next = tmptimeout->next;
      h = tmptimeout->h;
      arg = tmptimeout->arg;
      memp_free(MEMP_SYS_TIMEOUT, tmptimeout);
      if (h != NULL) {
        LWIP_DEBUGF(SYS_DEBUG, ("smf calling h=%p(%p)\n", (void *)h, (void *)arg));
      	h(arg);
      }

      /* We try again to fetch a message from the mbox. */
      goto again;
    } else {
      /* If time != SYS_ARCH_TIMEOUT, a message was received before the timeout
   occured. The time variable is set to the number of
   milliseconds we waited for the message. */
      if (time <= timeouts->next->time) {
  timeouts->next->time -= time;
      } else {
  timeouts->next->time = 0;
      }
    }

  }
}

void
sys_sem_wait(sys_sem_t sem)
{
  u32_t time;
  struct sys_timeouts *timeouts;
  struct sys_timeout *tmptimeout;
  sys_timeout_handler h;
  void *arg;

  /*  while (sys_arch_sem_wait(sem, 1000) == 0);
      return;*/

 again:

  timeouts = sys_arch_timeouts();

  if (!timeouts || !timeouts->next) {
    sys_arch_sem_wait(sem, 0);
  } else {
    if (timeouts->next->time > 0) {
      time = sys_arch_sem_wait(sem, timeouts->next->time);
    } else {
      time = SYS_ARCH_TIMEOUT;
    }

    if (time == SYS_ARCH_TIMEOUT) {
      /* If time == SYS_ARCH_TIMEOUT, a timeout occured before a message
   could be fetched. We should now call the timeout handler and
   deallocate the memory allocated for the timeout. */
      tmptimeout = timeouts->next;
      timeouts->next = tmptimeout->next;
      h = tmptimeout->h;
      arg = tmptimeout->arg;
      memp_free(MEMP_SYS_TIMEOUT, tmptimeout);
      if (h != NULL) {
        LWIP_DEBUGF(SYS_DEBUG, ("ssw h=%p(%p)\n", (void *)h, (void *)arg));
        h(arg);
      }

      /* We try again to fetch a message from the mbox. */
      goto again;
    } else {
      /* If time != SYS_ARCH_TIMEOUT, a message was received before the timeout
   occured. The time variable is set to the number of
   milliseconds we waited for the message. */
      if (time <= timeouts->next->time) {
  timeouts->next->time -= time;
      } else {
  timeouts->next->time = 0;
      }
    }

  }
}

void
sys_timeout(u32_t msecs, sys_timeout_handler h, void *arg)
{
  struct sys_timeouts *timeouts;
  struct sys_timeout *timeout, *t;

  timeout = memp_malloc(MEMP_SYS_TIMEOUT);
  if (timeout == NULL) {
    return;
  }
  timeout->next = NULL;
  timeout->h = h;
  timeout->arg = arg;
  timeout->time = msecs;

  timeouts = sys_arch_timeouts();

  LWIP_DEBUGF(SYS_DEBUG, ("sys_timeout: %p msecs=%"U32_F" h=%p arg=%p\n",
    (void *)timeout, msecs, (void *)h, (void *)arg));

  LWIP_ASSERT("sys_timeout: timeouts != NULL", timeouts != NULL);

  if (timeouts->next == NULL) {
    timeouts->next = timeout;
    return;
  }

  if (timeouts->next->time > msecs) {
    timeouts->next->time -= msecs;
    timeout->next = timeouts->next;
    timeouts->next = timeout;
  } else {
    for(t = timeouts->next; t != NULL; t = t->next) {
      timeout->time -= t->time;
      if (t->next == NULL || t->next->time > timeout->time) {
        if (t->next != NULL) {
          t->next->time -= timeout->time;
        }
        timeout->next = t->next;
        t->next = timeout;
        break;
      }
    }
  }

}

/* Go through timeout list (for this task only) and remove the first matching entry,
   even though the timeout has not triggered yet.
*/

void
sys_untimeout(sys_timeout_handler h, void *arg)
{
    struct sys_timeouts *timeouts;
    struct sys_timeout *prev_t, *t;

    timeouts = sys_arch_timeouts();

    if (timeouts->next == NULL)
        return;

    for (t = timeouts->next, prev_t = NULL; t != NULL; prev_t = t, t = t->next)
    {
        if ((t->h == h) && (t->arg == arg))
        {
            /* We have a match */
            /* Unlink from previous in list */
            if (prev_t == NULL)
                timeouts->next = t->next;
            else
                prev_t->next = t->next;
            /* If not the last one, add time of this one back to next */
            if (t->next != NULL)
                t->next->time += t->time;
            memp_free(MEMP_SYS_TIMEOUT, t);
            return;
        }
    }
    return;
}

static void
sswt_handler(void *arg)
{
    struct sswt_cb *sswt_cb = (struct sswt_cb *) arg;

    /* Timeout. Set flag to TRUE and signal semaphore */
    sswt_cb->timeflag = 1;
    sys_sem_signal(*(sswt_cb->psem));
}

/* Wait for a semaphore with timeout (specified in ms) */
/* timeout = 0: wait forever */
/* Returns 0 on timeout. 1 otherwise */

int
sys_sem_wait_timeout(sys_sem_t sem, u32_t timeout)
{
    struct sswt_cb sswt_cb;

    sswt_cb.psem = &sem;
    sswt_cb.timeflag = 0;

    /* If timeout is zero, then just wait forever */
    if (timeout > 0)
        /* Create a timer and pass it the address of our flag */
        sys_timeout(timeout, sswt_handler, &sswt_cb);
    sys_sem_wait(sem);
    /* Was it a timeout? */
    if (sswt_cb.timeflag)
    {
        /* timeout */
        return 0;
    } else {
        /* Not a timeout. Remove timeout entry */
        sys_untimeout(sswt_handler, &sswt_cb);
        return 1;
    }

}

void
sys_msleep(u32_t ms)
{
  sys_sem_t delaysem = sys_sem_new(0);

  sys_sem_wait_timeout(delaysem, ms);

  sys_sem_free(delaysem);
}

#endif /* NO_SYS */
