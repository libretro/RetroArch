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
#ifndef RETRO_DISABLE_HOST_CLOCK
retro_perf_tick_t cpu_features_get_perf_counter(void);
#endif

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
#ifndef RETRO_DISABLE_HOST_CLOCK
retro_time_t cpu_features_get_time_usec(void);
#endif

#ifndef RETRO_DISABLE_HOST_CLOCK
/**
 * cpu_features_perf_calibrate:
 * @now : a cpu_features_get_time_usec() reading the caller already
 *        took, as close as it can to this call.
 *
 * Measures cpu_features_get_perf_counter()'s rate against @now where
 * the platform does not state it, so that durations taken on the
 * counter can be told in microseconds without reading the clock for
 * them. One caller, on one thread, at a steady cadence (once a frame);
 * where the rate is stated it is taken once and nothing is read, and
 * where it is measured the counter is read only once a conversion has
 * been asked for. ARM64 Android verifies a stated rate against these
 * samples once before taking that fixed-rate fast path.
 */
void cpu_features_perf_calibrate(retro_time_t now);

/**
 * cpu_features_perf_to_usec:
 * @ticks : a difference of two cpu_features_get_perf_counter() readings.
 *
 * Any thread.
 *
 * @return @ticks in microseconds; 0 for a span that runs backwards
 *         (a counter not kept in step across cores), and 0 while the
 *         rate is not yet known: where it is measured, from the first
 *         conversion asked for until two cpu_features_perf_calibrate()
 *         calls a millisecond apart after it.
 */
retro_time_t cpu_features_perf_to_usec(retro_perf_tick_t ticks);

/**
 * cpu_features_perf_from_usec:
 * @usec : a span in microseconds.
 *
 * Any thread.
 *
 * @return @usec in cpu_features_get_perf_counter() ticks, to add to a
 *         reading for the reading a wait ends at; 0 for a span not
 *         above 0 and while the rate is not yet known, as
 *         cpu_features_perf_to_usec() has it.
 */
retro_perf_tick_t cpu_features_perf_from_usec(retro_time_t usec);

/* Where cpu_features_get_perf_counter() is a register read, cheaper
 * than cpu_features_get_time_usec(): the x86 time-stamp counter, the
 * ARMv8 virtual counter, the PowerPC timebase. Elsewhere the counter is
 * the clock itself, a system call, or absent. Follows the counter's own
 * branches in features_cpu.c. */
#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) \
   || (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__) \
         || defined(__aarch64__))) \
   || defined(GEKKO_NATIVE) || defined(GEKKO) \
   || (!defined(__MACH__) && !defined(__FreeBSD__) && (defined(_XBOX360) \
         || defined(__powerpc__) || defined(__ppc__) \
         || defined(__POWERPC__) || defined(__PSL1GHT__) \
         || defined(__PPC64__) || defined(__powerpc64__)))
#define CPU_FEATURES_PERF_COUNTER_CHEAP 1
#endif

/* A mark to time a span by, and the span between two marks in
 * microseconds: the perf counter where it is cheap, the clock where
 * it is not, so a span costs the cheaper of the two reads */
/* CPU_FEATURES_MARK_WITH(now): the mark for the instant a
 * cpu_features_get_time_usec() reading @now was taken, read beside it */
/* CPU_FEATURES_USEC_TO_MARK(usec): a span above 0 as marks, to hold
 * the difference of two marks against; 0 while that is not known, and
 * the clock has to bound the wait */
#ifdef CPU_FEATURES_PERF_COUNTER_CHEAP
#define CPU_FEATURES_MARK()             cpu_features_get_perf_counter()
#define CPU_FEATURES_MARK_WITH(now)     cpu_features_get_perf_counter()
#define CPU_FEATURES_MARK_TO_USEC(span) cpu_features_perf_to_usec(span)
#define CPU_FEATURES_USEC_TO_MARK(usec) cpu_features_perf_from_usec(usec)
#else
#define CPU_FEATURES_MARK()             ((retro_perf_tick_t)cpu_features_get_time_usec())
#define CPU_FEATURES_MARK_WITH(now)     ((retro_perf_tick_t)(now))
#define CPU_FEATURES_MARK_TO_USEC(span) ((retro_time_t)(span))
#define CPU_FEATURES_USEC_TO_MARK(usec) ((retro_perf_tick_t)(usec))
#endif
#endif

/**
 * retro_sleep_until_us:
 * @deadline : an instant on cpu_features_get_time_usec()'s clock.
 *
 * Sleeps until that clock reads at least @deadline. Absolute where the
 * platform offers it (clock_nanosleep TIMER_ABSTIME on Linux and
 * Android, mach_wait_until on Darwin), a re-armed high-resolution
 * relative wait elsewhere. Never early against that clock; may be
 * late, as every sleep may. Not available to the salamander launchers,
 * which do not link features_cpu.c.
 **/
#ifndef RETRO_DISABLE_HOST_CLOCK
void retro_sleep_until_us(retro_time_t deadline);
#endif

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

#if (defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i686__) || (defined(_M_X64) && _MSC_VER > 1310) || (defined(_M_IX86) && _MSC_VER > 1310)) && !defined(__MACH__)
/**
 * Executes the x86 CPUID instruction for the requested leaf, writing the
 * EAX, EBX, ECX and EDX result registers into flags[0], flags[1], flags[2]
 * and flags[3] respectively. Available on x86/x86-64 targets only.
 *
 * @param func     The CPUID leaf (function number) to query.
 * @param[out] flags Array of four 32-bit words receiving EAX/EBX/ECX/EDX.
 */
void x86_cpuid(uint32_t func, int32_t flags[4]);
#endif

/**
 * @return The number of logical processors available -- hardware
 * threads rather than physical cores, so an SMT part reports both of
 * each core -- or 1 if it could not be determined.
 *
 * @see cpu_features_get_core_amount_physical
 */
unsigned cpu_features_get_core_amount(void);

/**
 * The count to size work by where a thread wants a core to itself,
 * rather than the thread count cpu_features_get_core_amount() returns.
 *
 * @return The number of physical cores available, or the value
 * cpu_features_get_core_amount() gives where the distinction cannot be
 * drawn, which is never larger and so stays a safe answer.
 */
unsigned cpu_features_get_core_amount_physical(void);

/**
 * Order the processors best-first, for a caller pinning a thread that
 * wants the strongest processor still free.
 *
 * Entries are operating system processor identifiers, the numbering an
 * affinity mask is built from, ordered by descending core performance
 * (the fast class first, then the larger last-level cache, then the
 * higher clock) and with an SMT sibling placed after the processor it
 * shares a core with. Processors outside the calling thread's affinity
 * mask are left out. Where the platform publishes no topology the order is simply
 * ascending, which names every processor exactly once and so remains
 * usable, just unranked.
 *
 * @param s   Receives the identifiers.
 * @param len Number of entries @s has room for.
 * @return The number of entries written, which is 0 where no
 * identifiers could be established and the caller should leave
 * affinity alone.
 */
size_t cpu_features_get_processor_order(unsigned *s, size_t len);

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
