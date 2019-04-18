/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (features_cpu.h).
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

#ifndef _LIBRETRO_SDK_CPU_INFO_H
#define _LIBRETRO_SDK_CPU_INFO_H

#include <retro_common_api.h>

#include <stdint.h>

#include <libretro.h>

RETRO_BEGIN_DECLS

/**
 * cpu_features_get_perf_counter:
 *
 * Gets performance counter.
 *
 * Returns: performance counter.
 **/
retro_perf_tick_t cpu_features_get_perf_counter(void);

/**
 * cpu_features_get_time_usec:
 *
 * Gets time in microseconds, from an undefined epoch.
 * The epoch may change between computers or across reboots.
 *
 * Returns: time in microseconds
 **/
retro_time_t cpu_features_get_time_usec(void);

/**
 * cpu_features_get:
 *
 * Gets CPU features.
 *
 * Returns: bitmask of all CPU features available.
 **/
uint64_t cpu_features_get(void);

/**
 * cpu_features_get_core_amount:
 *
 * Gets the amount of available CPU cores.
 *
 * Returns: amount of CPU cores available.
 **/
unsigned cpu_features_get_core_amount(void);

void cpu_features_get_model_name(char *name, int len);

RETRO_END_DECLS

#endif
