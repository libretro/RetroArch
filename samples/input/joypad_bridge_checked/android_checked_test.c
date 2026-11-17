/* The Android joypad driver, held to what the snapshot bridge assumes
 * of a driver it serves (see bridge_view.h). The driver is built into
 * griffin after android_input.c, whose key state and keycode bound it
 * reads, and against the platform's android_app; both are stood in for
 * here, with no JNI behind them, and filled by hand. */
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
   int16_t analog_state[DEFAULT_MAX_PADS][MAX_AXIS];
   int8_t hat_state[DEFAULT_MAX_PADS][2];
   uint16_t rumble_last_strength_strong[DEFAULT_MAX_PADS];
   uint16_t rumble_last_strength_weak[DEFAULT_MAX_PADS];
   uint16_t rumble_last_strength[DEFAULT_MAX_PADS];
   int id[DEFAULT_MAX_PADS];
};

static struct android_app test_app;
static struct android_app *g_android = &test_app;

/* android_input.c's rows of key state, one a pad */
static uint8_t test_key_state[DEFAULT_MAX_PADS + 1][(LAST_KEYCODE + 7) / 8];
uint8_t *android_keyboard_state_get(unsigned port)
{
   return test_key_state[port];
}

#include "../../../input/drivers_joypad/android_joypad.c"

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      unsigned pad = n % 3;

      for (i = 0; i < sizeof(test_key_state[pad]); i++)
         test_key_state[pad][i] = (uint8_t)bv_rand();
      /* past LAST_KEYCODE in the row's last byte: never set by the
       * frontend, never read by button() */
      test_key_state[pad][sizeof(test_key_state[pad]) - 1] |= 0xf8;
      for (i = 0; i < 2; i++)
         test_app.hat_state[pad][i] = (int8_t)((int)(bv_rand() % 3) - 1);
      for (i = 0; i < MAX_AXIS; i++)
      {
         unsigned r = bv_rand() % 8;
         test_app.analog_state[pad][i] = (r == 0) ? 0
                                       : (r == 1) ? 0x7fff
                                       : (r == 2) ? -0x7fff
                                       : (r == 3) ? -0x8000
                                       : (int16_t)bv_rand();
      }
      bad += bv_compare(&android_joypad, pad);
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
