/* Copyright  (C) 2010-2020 The RetroArch team
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
 * Gets the time in ticks since some unspecified epoch.
 * The notion of a "tick" varies per platform.
 *
 * The epoch may change between devices or across reboots.
 *
 * Suitable for use as a default implementation of \c retro_perf_callback::get_perf_counter,
 * (or as a fallback by the core),
 * although a frontend may provide its own implementation.
 *
 * @return The current time, in ticks.
 * @see retro_perf_callback::get_perf_counter
 */
retro_perf_tick_t cpu_features_get_perf_counter(void);

/**
 * Gets the time in microseconds since some unspecified epoch.
 *
 * The epoch may change between devices or across reboots.
 *
 * Suitable for use as a default implementation of \c retro_perf_callback::get_time_usec,
 * (or as a fallback by the core),
 * although a frontend may provide its own implementation.
 *
 * @return The current time, in microseconds.
 * @see retro_perf_callback::get_time_usec
 */
retro_time_t cpu_features_get_time_usec(void);

/**
 * Returns the available features (mostly SIMD extensions)
 * supported by this CPU.
 *
 * Suitable for use as a default implementation of \c retro_perf_callback::get_time_usec,
 * (or as a fallback by the core),
 * although a frontend may provide its own implementation.
 *
 * @return Bitmask of all CPU features available.
 * @see RETRO_SIMD
 * @see retro_perf_callback::get_cpu_features
 */
uint64_t cpu_features_get(void);

/**
 * @return The number of CPU cores available,
 * or 1 if the number of cores could not be determined.
 */
unsigned cpu_features_get_core_amount(void);

/**
 * Returns the name of the CPU model.
 *
 * @param[out] name Pointer to a buffer to store the name.
 * Will be \c NULL-terminated.
 * If \c NULL, this value will not be modified.
 * @param len The amount of space available in \c name.
 */
void cpu_features_get_model_name(char *name, int len);

RETRO_END_DECLS

#endif
