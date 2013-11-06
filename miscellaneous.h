/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RARCH_MISCELLANEOUS_H
#define __RARCH_MISCELLANEOUS_H

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <sys/timer.h>
#elif defined(XENON)
#include <time/time.h>
#else
#include <time.h>
#endif

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "retroarch_logger.h"

// Some platforms do not set this value.
// Just assume a value. It's usually 4KiB.
// Platforms with a known value (like Win32)
// set this value explicitly in platform specific headers.
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define rarch_assert(cond) do { \
   if (!(cond)) { RARCH_ERR("Assertion failed at %s:%d.\n", __FILE__, __LINE__); exit(2); } \
} while(0)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define RARCH_SCALE_BASE 256

static inline void rarch_sleep(unsigned msec)
{
#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   sys_timer_usleep(1000 * msec);
#elif defined(PSP)
   sceKernelDelayThread(1000 * msec);
#elif defined(_WIN32)
   Sleep(msec);
#elif defined(XENON)
   udelay(1000 * msec);
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
   usleep(1000 * msec);
#else
   struct timespec tv = {0};
   tv.tv_sec = msec / 1000;
   tv.tv_nsec = (msec % 1000) * 1000000;
   nanosleep(&tv, NULL);
#endif
}

static inline uint32_t next_pow2(uint32_t v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;
   return v;
}

static inline uint32_t prev_pow2(uint32_t v)
{
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   return v - (v >> 1);
}

static inline uint8_t is_little_endian(void)
{
   union
   {
      uint16_t x;
      uint8_t y[2];
   } u;

   u.x = 1;
   return u.y[0];
}

static inline uint32_t swap_if_big32(uint32_t val)
{
   if (is_little_endian()) // Little-endian
      return val;
   else
      return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
}

static inline uint32_t swap_if_little32(uint32_t val)
{
   if (is_little_endian())
      return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
   else
      return val;
}

static inline uint16_t swap_if_big16(uint16_t val)
{
   if (is_little_endian())
      return val;
   else
      return (val >> 8) | (val << 8);
}

static inline uint16_t swap_if_little16(uint16_t val)
{
   if (is_little_endian())
      return (val >> 8) | (val << 8);
   else
      return val;
}

// Helper macros and struct to keep track of many booleans.
// To check for multiple bits, use &&, not &.
// For OR, | can be used.
typedef struct
{
   uint32_t data[8];
} rarch_bits_t;

#define BIT_SET(a, bit)   ((a).data[(bit) >> 5] |= 1 << ((bit) & 31))
#define BIT_CLEAR(a, bit) ((a).data[(bit) >> 5] &= ~(1 << ((bit) & 31)))
#define BIT_GET(a, bit)   ((a).data[(bit) >> 5] & (1 << ((bit) & 31)))
#define BIT_CLEAR_ALL(a)  memset(&(a), 0, sizeof(a));

#endif
