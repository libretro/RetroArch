/* The Android joypad driver, held to what the snapshot bridge assumes
 * of a driver it serves (see bridge_view.h). The driver is built into
 * griffin after android_input.c, whose pad views it lends, and against
 * the platform's android_app; both are stood in for here, with no JNI
 * behind them, and the views filled as android_input.c fills them: a
 * pad's keys set bit by bit in the bytes of its buttons, as its row of
 * keys is written. */
#include <stddef.h>
#include "../../../input/input_driver.h"

/* android_input.c's */
#define LAST_KEYCODE     219
#define MAX_AXIS         10

/* JNI as the rumble path names it; never reached here */
typedef void JNIEnv;
typedef int jint;
typedef unsigned char jboolean;
#define JNI_FALSE 0
#define CALL_BOOLEAN_METHOD_PARAM(env, var, obj, mid, ...) ((void)(var = JNI_FALSE))
#define CALL_VOID_METHOD_PARAM(env, obj, mid, ...) ((void)0)
static void *jni_thread_getenv(void) { return NULL; }

struct android_test_activity { void *clazz; };

/* the frontend's; asked by the rumble path alone */
bool input_config_get_device_vibration(void)
{ return false; }

/* the fields of frontend/drivers/platform_unix.h's the driver reads */
struct android_app
{
   struct android_test_activity *activity;
   void *doVibrate;
   void *doVibrateUSB;
   void *doVibrateJoypad;
   int8_t hat_state[DEFAULT_MAX_PADS][2];
   uint16_t rumble_last_strength_strong[DEFAULT_MAX_PADS];
   uint16_t rumble_last_strength_weak[DEFAULT_MAX_PADS];
   uint16_t rumble_last_strength[DEFAULT_MAX_PADS];
   int id[DEFAULT_MAX_PADS];
};

static struct android_app test_app;
static struct android_app *g_android = &test_app;

/* android_input.c's pad views */
static input_pad_view_t test_views[DEFAULT_MAX_PADS];
struct input_pad_view *android_input_pad_view(unsigned port)
{
   return (port < DEFAULT_MAX_PADS) ? &test_views[port] : NULL;
}

#include "../../../input/drivers_joypad/android_joypad.c"

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      unsigned pad = n % 3;

      {
         /* the row of keys, written as android_key_state_write() does:
          * bit by bit into the bytes, below LAST_KEYCODE */
         uint8_t *row = (uint8_t*)test_views[pad].buttons.data;
         memset(&test_views[pad], 0, sizeof(test_views[pad]));
         for (i = 0; i < LAST_KEYCODE; i++)
            if (bv_rand() & 1)
               BIT_SET(row, i);
         for (i = 0; i < LAST_KEYCODE; i++)
            if (     (android_joypad.button(pad, (uint16_t)i) != 0)
                  != (BIT_GET(row, i) != 0))
               bad++;
      }
      test_views[pad].hats = (uint16_t)(bv_rand() & 0xf);
      for (i = 0; i < MAX_AXIS; i++)
         test_views[pad].axes[i] = bv_rand_axis();
      bad += bv_compare(&android_joypad, pad);
      bad += bv_compare_view(&android_joypad, pad);
   }

   /* a port past the pads the driver keeps has no buttons */
   {
      input_bits_t bits;
      memset(&bits, 0xff, sizeof(bits));
      android_joypad_get_buttons(DEFAULT_MAX_PADS, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
   }

   if (bad)
   {
      printf("FAIL android_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS android_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
