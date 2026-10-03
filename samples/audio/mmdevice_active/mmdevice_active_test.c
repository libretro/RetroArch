/* The table of open endpoints, set from two audio threads and matched
 * from a notification thread at once, every one of them first to touch
 * it. Built under ThreadSanitizer by the check target: a guard that
 * lets two threads in at once shows up as a race, a lost or doubled
 * free under ASan. */
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../../../audio/common/mmdevice_active.h"

#define ROUNDS 20000

static mmdevice_active_t active;
static retro_atomic_int_t go;
static const wchar_t *ids[2][2] = {
   { L"{0.0.0.00000000}.{render-a}",  L"{0.0.0.00000000}.{render-b}"  },
   { L"{0.0.1.00000000}.{capture-a}", L"{0.0.1.00000000}.{capture-b}" }
};
static int flows[2] = { 0, 1 };

static void setter(void *data)
{
   int flow = *(int*)data;
   int i;
   while (!retro_atomic_load_acquire_int(&go)) { }
   for (i = 0; i < ROUNDS; i++)
      mmdevice_active_set(&active, (unsigned)flow,
            (i % 5 == 4) ? NULL : ids[flow][i & 1]);
   mmdevice_active_set(&active, (unsigned)flow, ids[flow][0]);
}

static void matcher(void *data)
{
   int i;
   (void)data;
   while (!retro_atomic_load_acquire_int(&go)) { }
   for (i = 0; i < ROUNDS; i++)
   {
      mmdevice_active_match(&active, ids[i & 1][(i >> 1) & 1]);
      mmdevice_active_match(&active, L"{0.0.0.00000000}.{unrelated}");
   }
}

int main(void)
{
   int fails = 0;
   sthread_t *t[3];

   /* nothing recorded: every notification counts */
   if (!mmdevice_active_match(&active, L"x"))
   { printf("FAIL  an empty table matches nothing\n"); fails++; }

   t[0] = sthread_create(setter,  &flows[0]);
   t[1] = sthread_create(setter,  &flows[1]);
   t[2] = sthread_create(matcher, NULL);
   retro_atomic_store_release_int(&go, 1);
   sthread_join(t[0]);
   sthread_join(t[1]);
   sthread_join(t[2]);

   if (     !mmdevice_active_match(&active, ids[0][0])
         || !mmdevice_active_match(&active, ids[1][0]))
   { printf("FAIL  an open endpoint does not match\n"); fails++; }
   if (mmdevice_active_match(&active, ids[0][1]))
   { printf("FAIL  a closed endpoint matches\n"); fails++; }
   if (mmdevice_active_match(&active, L"{0.0.0.00000000}.{unrelated}"))
   { printf("FAIL  an unrelated endpoint matches\n"); fails++; }
   if (!mmdevice_active_match(&active, NULL))
   { printf("FAIL  a notification with no id is ignored\n"); fails++; }

   mmdevice_active_set(&active, 2, ids[0][1]);   /* no such flow */
   if (mmdevice_active_match(&active, ids[0][1]))
   { printf("FAIL  an out-of-range flow was recorded\n"); fails++; }

   mmdevice_active_set(&active, 0, NULL);
   mmdevice_active_set(&active, 1, NULL);
   if (!fails)
      printf("ok    endpoints set from two threads, matched from a third\n");
   return fails ? 1 : 0;
}
