/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtime.c).
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

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_assert.h>
#include <stdlib.h>
#endif

#include <string.h>
#include <time/rtime.h>

#ifdef HAVE_THREADS
/* TODO/FIXME - global */
slock_t *rtime_localtime_lock = NULL;
#endif

/* Must be called before using rtime_localtime() */
void rtime_init(void)
{
   rtime_deinit();
#ifdef HAVE_THREADS
   if (!rtime_localtime_lock)
      rtime_localtime_lock = slock_new();

   retro_assert(rtime_localtime_lock);
#endif
}

/* Must be called upon program termination */
void rtime_deinit(void)
{
#ifdef HAVE_THREADS
   if (rtime_localtime_lock)
   {
      slock_free(rtime_localtime_lock);
      rtime_localtime_lock = NULL;
   }
#endif
}

/* Thread-safe wrapper for localtime() */
struct tm *rtime_localtime(const time_t *timep, struct tm *result)
{
   struct tm *time_info = NULL;

   /* Lock mutex */
#ifdef HAVE_THREADS
   slock_lock(rtime_localtime_lock);
#endif

   time_info = localtime(timep);
   if (time_info)
      memcpy(result, time_info, sizeof(struct tm));

   /* Unlock mutex */
#ifdef HAVE_THREADS
   slock_unlock(rtime_localtime_lock);
#endif

   return result;
}
