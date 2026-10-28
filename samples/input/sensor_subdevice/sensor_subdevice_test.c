/* A core's sensor calls with a sub-device index (RETRO_SENSOR_SUBDEVICE),
 * through the frontend's wrappers as they ship (extracted from
 * input_driver.c between their markers). The controller's own sensors
 * behave as before; a sub-device's are passed on to the drivers with
 * the index kept, read only once enabled, and refused past the
 * sub-devices the frontend tracks. */
#include <stdio.h>
#include <string.h>

#include <boolean.h>
#include <libretro.h>

#include "input/input_driver.h"

static input_driver_state_t input_driver_st;

/* the driver side, stood in for: what it was asked, and its answer */
static unsigned last_action, last_id, set_calls, get_calls;
static bool     driver_has = true;

bool input_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   (void)port; (void)rate;
   last_action = (unsigned)action;
   set_calls++;
   return driver_has;
}

float input_get_sensor_state(unsigned port, unsigned id)
{
   (void)port;
   last_id = id;
   get_calls++;
   return 0.5f;
}

#include "core_sensor_fragment.c"

static unsigned failures;
#define CHECK(cond, msg) do { if (!(cond)) { \
   printf("   FAIL %s\n", msg); failures++; } } while (0)

#define SUB(i, x) ((enum retro_sensor_action)RETRO_SENSOR_SUBDEVICE((i), (x)))

int main(void)
{
   /* the controller's own: as before */
   CHECK(RETRO_SENSOR_SUBDEVICE(0, RETRO_SENSOR_GYROSCOPE_Y) == RETRO_SENSOR_GYROSCOPE_Y,
         "index 0 is not the value in use");
   CHECK(input_core_get_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_X) == 0.0f,
         "the accelerometer read before it was enabled");
   CHECK(input_core_set_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_ENABLE, 60)
         && input_driver_st.core_accel_rate == 60, "the accelerometer enable");
   CHECK(input_core_get_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_X) == 0.5f
         && last_id == RETRO_SENSOR_ACCELEROMETER_X, "the accelerometer read");
   printf("   ok   the controller's own sensors behave as before\n");

   /* the Nunchuk's accelerometer: refused while not enabled, passed
    * on with its index once it is */
   get_calls = 0;
   CHECK(input_core_get_sensor_state(0,
            RETRO_SENSOR_SUBDEVICE(1, RETRO_SENSOR_ACCELEROMETER_X)) == 0.0f
         && get_calls == 0, "sub-device 1 read before it was enabled");
   CHECK(input_core_set_sensor_state(0, SUB(1, RETRO_SENSOR_ACCELEROMETER_ENABLE), 100)
         && last_action == RETRO_SENSOR_SUBDEVICE(1, RETRO_SENSOR_ACCELEROMETER_ENABLE),
         "sub-device 1's enable was not passed on with its index");
   CHECK(input_driver_st.core_accel_rate == 60,
         "sub-device 1's enable changed the controller's rate");
   CHECK(input_core_get_sensor_state(0,
            RETRO_SENSOR_SUBDEVICE(1, RETRO_SENSOR_ACCELEROMETER_Z)) == 0.5f
         && last_id == RETRO_SENSOR_SUBDEVICE(1, RETRO_SENSOR_ACCELEROMETER_Z),
         "sub-device 1's read was not passed on with its index");
   get_calls = 0;
   CHECK(input_core_get_sensor_state(0,
            RETRO_SENSOR_SUBDEVICE(1, RETRO_SENSOR_GYROSCOPE_X)) == 0.0f
         && get_calls == 0, "sub-device 1's gyroscope read without being enabled");
   printf("   ok   a sub-device's sensor is passed on with its index, once enabled\n");

   /* disabled: no more reads */
   CHECK(input_core_set_sensor_state(0, SUB(1, RETRO_SENSOR_ACCELEROMETER_DISABLE), 0),
         "sub-device 1's disable");
   get_calls = 0;
   CHECK(input_core_get_sensor_state(0,
            RETRO_SENSOR_SUBDEVICE(1, RETRO_SENSOR_ACCELEROMETER_X)) == 0.0f
         && get_calls == 0, "sub-device 1 read after it was disabled");

   /* a frontend driver without it: the enable says so, and nothing reads */
   driver_has = false;
   CHECK(!input_core_set_sensor_state(0, SUB(1, RETRO_SENSOR_GYROSCOPE_ENABLE), 100),
         "a sub-device the drivers do not have was enabled");
   get_calls = 0;
   CHECK(input_core_get_sensor_state(0,
            RETRO_SENSOR_SUBDEVICE(1, RETRO_SENSOR_GYROSCOPE_X)) == 0.0f
         && get_calls == 0, "a sub-device that failed to enable was read");
   driver_has = true;

   /* past what is tracked, and actions with no sensor behind them */
   set_calls = 0;
   CHECK(!input_core_set_sensor_state(0,
            SUB(INPUT_SENSOR_SUBDEVICES + 1, RETRO_SENSOR_ACCELEROMETER_ENABLE), 100)
         && set_calls == 0, "a sub-device past the tracked ones was passed on");
   CHECK(!input_core_set_sensor_state(0, SUB(1, RETRO_SENSOR_ILLUMINANCE_ENABLE), 100)
         && set_calls == 0, "a sub-device's light sensor was passed on");
   CHECK(input_core_get_sensor_state(0,
            RETRO_SENSOR_SUBDEVICE(INPUT_SENSOR_SUBDEVICES + 1,
               RETRO_SENSOR_ACCELEROMETER_X)) == 0.0f,
         "a sub-device past the tracked ones read");
   printf("   ok   disabled, missing and untracked sub-devices read 0\n");

   if (failures)
   {
      printf("FAIL sensor_subdevice_test: %u\n", failures);
      return 1;
   }
   printf("PASS sensor_subdevice_test\n");
   return 0;
}
