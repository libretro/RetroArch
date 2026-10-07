/* The motion sensors of the DualShock 4 and the DualSense, read out of
 * their input reports: input/common/sony_pad_motion.h, on its own.
 *
 * No pad is read. What is held is what can be held without one: that
 * the six values are taken from where each pad's report has them,
 * low byte first and signed; that a report without them - another
 * id, or the short one a pad sends over Bluetooth - is refused; and
 * that a pad lying flat and still reads one g on libretro's Z and
 * nothing else, and a turn reads in radians a second on the axis and
 * with the sign the SDL joypad driver gives the same pad. */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../../input/common/sony_pad_motion.h"

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static void put16(uint8_t *p, int v)
{
   p[0] = (uint8_t)(v & 0xFF);
   p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static bool near(float a, float b) { return fabsf(a - b) < 0.0005f; }

int main(void)
{
   uint8_t r[64];
   sony_pad_motion_t m;
   float v[6];
   unsigned i;

   /* ---- DualSense, USB -------------------------------------------- */
   memset(r, 0, sizeof(r));
   r[0] = 0x01;
   put16(r + 16,  100);  put16(r + 18, -200);  put16(r + 20,  300); /* gyro  */
   put16(r + 22, -400);  put16(r + 24, 8192);  put16(r + 26, -600); /* accel */
   CHECK(sony_pad_motion_parse(&m, SONY_PAD_DUALSENSE, r, 64),
         "a DualSense's USB report was refused");
   CHECK(m.gyro[0] == 100 && m.gyro[1] == -200 && m.gyro[2] == 300
         && m.accel[0] == -400 && m.accel[1] == 8192 && m.accel[2] == -600,
         "DualSense: gyro %d %d %d, accel %d %d %d", m.gyro[0], m.gyro[1], m.gyro[2],
         m.accel[0], m.accel[1], m.accel[2]);
   /* ---- DualShock 4, USB ------------------------------------------ */
   memset(r, 0, sizeof(r));
   r[0] = 0x01;
   put16(r + 13,  -1);  put16(r + 15, 2);     put16(r + 17, -3);
   put16(r + 19,   4);  put16(r + 21, -8192); put16(r + 23, 6);
   CHECK(sony_pad_motion_parse(&m, SONY_PAD_DS4, r, 64),
         "a DualShock 4's USB report was refused");
   CHECK(m.gyro[0] == -1 && m.gyro[1] == 2 && m.gyro[2] == -3
         && m.accel[0] == 4 && m.accel[1] == -8192 && m.accel[2] == 6,
         "DualShock 4: gyro %d %d %d, accel %d %d %d", m.gyro[0], m.gyro[1], m.gyro[2],
         m.accel[0], m.accel[1], m.accel[2]);
   printf("   ok   the six values are read from where each pad's report has them, low byte first and signed\n");

   /* ---- reports without them -------------------------------------- */
   r[0] = 0x01;
   CHECK(!sony_pad_motion_parse(&m, SONY_PAD_DUALSENSE, r, 10),
         "the short report a DualSense sends over Bluetooth was read for sensors");
   CHECK(!sony_pad_motion_parse(&m, SONY_PAD_DS4, r, 10),
         "the short report a DualShock 4 sends over Bluetooth was read for sensors");
   r[0] = 0x31;
   CHECK(!sony_pad_motion_parse(&m, SONY_PAD_DUALSENSE, r, 64), "another report id was read for sensors");
   r[0] = 0x01;
   CHECK(!sony_pad_motion_parse(&m, SONY_PAD_NONE, r, 64), "a report of no known pad was read for sensors");
   CHECK(!sony_pad_motion_parse(&m, SONY_PAD_DUALSENSE, NULL, 64), "no report at all was read for sensors");
   printf("   ok   a short Bluetooth report, another report id and an unknown pad are refused\n");

   /* ---- what a core is given -------------------------------------- */
   /* flat on a table, still: gravity up out of the pad's face, which
    * is the pad's Y and libretro's Z */
   memset(&m, 0, sizeof(m));
   m.accel[1] = 8192;
   for (i = 0; i < 6; i++)
      CHECK(sony_pad_motion_value(&m, RETRO_SENSOR_ACCELEROMETER_X + i, &v[i]), "sensor value %u was refused", i);
   CHECK(near(v[0], 0) && near(v[1], 0) && near(v[2], 1.0f),
         "flat and still: the accelerometer reads %f %f %f, want 0 0 1", v[0], v[1], v[2]);
   CHECK(near(v[3], 0) && near(v[4], 0) && near(v[5], 0), "flat and still: the gyroscope reads %f %f %f", v[3], v[4], v[5]);
   /* a turn of 90 degrees a second about each of the pad's axes */
   memset(&m, 0, sizeof(m));
   m.gyro[0] = (int16_t)(90 * 16);
   sony_pad_motion_value(&m, RETRO_SENSOR_GYROSCOPE_X, &v[0]);
   CHECK(near(v[0], 1.5707963f), "90 degrees a second about X reads %f, want pi/2", v[0]);
   memset(&m, 0, sizeof(m));
   m.gyro[1] = (int16_t)(90 * 16);
   sony_pad_motion_value(&m, RETRO_SENSOR_GYROSCOPE_Z, &v[0]);
   sony_pad_motion_value(&m, RETRO_SENSOR_GYROSCOPE_Y, &v[1]);
   CHECK(near(v[0], 1.5707963f) && near(v[1], 0), "a turn about the pad's Y reads %f on libretro's Z and %f on its Y", v[0], v[1]);
   memset(&m, 0, sizeof(m));
   m.gyro[2] = (int16_t)(90 * 16);
   sony_pad_motion_value(&m, RETRO_SENSOR_GYROSCOPE_Y, &v[0]);
   CHECK(near(v[0], -1.5707963f), "a turn about the pad's Z reads %f on libretro's Y, want -pi/2", v[0]);
   CHECK(!sony_pad_motion_value(&m, RETRO_SENSOR_ILLUMINANCE, &v[0]), "the light sensor was answered for");
   printf("   ok   flat and still is one g on libretro's Z; a turn is radians a second, on the axes and with the signs the SDL driver gives\n");

   if (failures)
   {
      printf("FAIL sony_pad_motion_test: %u\n", failures);
      return 1;
   }
   printf("PASS sony_pad_motion_test\n");
   return 0;
}
