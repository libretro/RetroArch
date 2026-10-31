/* set_rotation_deferred_test.c -- video_driver_set_rotation() with
 * and without a live driver instance.
 *
 * Cores issue RETRO_ENVIRONMENT_SET_ROTATION from retro_load_game,
 * which runs before drivers_init: the vtable is installed but the
 * instance is not. The request is latched and applied at init, so
 * the call has to report success there; a false sends cores such as
 * mame2003-plus to their internal rotation fallback (issue #19957).
 * The instance must still not be touched while it is absent. */

#include <stdio.h>
#include <stddef.h>

#include <boolean.h>

typedef struct video_driver
{
   void (*set_rotation)(void *data, unsigned rotation);
} video_driver_t;

typedef struct
{
   const video_driver_t *current_video;
   void *data;
} video_driver_state_t;

static video_driver_state_t video_driver_st;
static unsigned calls;
static void    *last_data;
static unsigned last_rotation;

static void video_driver_publish_vp_params(void) { }

static void stub_set_rotation(void *data, unsigned rotation)
{
   calls++;
   last_data     = data;
   last_rotation = rotation;
}

#include "set_rotation_driver.h"

static int failures;

static void expect(int cond, const char *what)
{
   if (!cond)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

int main(void)
{
   static const video_driver_t with_rot = { stub_set_rotation };
   static const video_driver_t no_rot   = { NULL };
   int instance;

   /* Vtable installed, instance not created yet */
   video_driver_st.current_video = &with_rot;
   video_driver_st.data          = NULL;
   calls                         = 0;
   expect(video_driver_set_rotation(1),
         "no instance: request reported as failed");
   expect(calls == 0, "no instance: driver called with NULL data");

   /* Live instance */
   video_driver_st.data = &instance;
   calls                = 0;
   expect(video_driver_set_rotation(3), "live instance: returned false");
   expect(calls == 1 && last_data == &instance && last_rotation == 3,
         "live instance: driver not called with its handle");

   /* Driver without rotation support */
   video_driver_st.current_video = &no_rot;
   expect(!video_driver_set_rotation(1), "no set_rotation: returned true");
   video_driver_st.data = NULL;
   expect(!video_driver_set_rotation(1),
         "no set_rotation, no instance: returned true");

   /* No driver at all */
   video_driver_st.current_video = NULL;
   expect(!video_driver_set_rotation(1), "no driver: returned true");

   if (failures)
      return 1;
   printf("set_rotation_deferred: OK\n");
   return 0;
}
