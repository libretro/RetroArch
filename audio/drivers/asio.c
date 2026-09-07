/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024-2026 - RetroArch contributors
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  ASIO audio driver — SDK-free COM-based implementation.
 *
 *  This driver accesses ASIO hardware through the COM vtable interface
 *  directly, without requiring the proprietary Steinberg ASIO SDK.
 *  All necessary types, constants, and structures are defined here
 *  from the publicly documented ASIO specification.
 *
 *  The thiscall calling convention problem (MSVC passes 'this' in ECX,
 *  GCC/MinGW passes it on the stack) is solved with inline assembly
 *  wrappers for each vtable call.
 *
 *  NOTE ON write_raw: This driver does NOT implement write_raw.
 *  RetroArch's audio rate control system works by dynamically adjusting
 *  the sinc resampler ratio each frame to keep the driver's audio buffer
 *  at ~50% saturation.  The write_raw fast path bypasses the software
 *  resampler and passes the rate_adjust parameter to the driver, which
 *  must apply it internally.  ASIO locks its sample rate at init time
 *  via ASIOSetSampleRate() and provides no mechanism to adjust it
 *  dynamically during streaming — ASIOSetSampleRate() during playback
 *  triggers a full driver reset (kAsioResetRequest).  Without dynamic
 *  rate adjustment, the audio buffer would slowly drift until it
 *  underruns or overruns, breaking A/V sync within minutes.  The
 *  software sinc resampler remains the only viable path.
 */

#ifdef HAVE_ASIO

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>

#include <boolean.h>
#include <retro_inline.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <retro_atomic.h>

#include "asio_convert.h"
#include "asio_ring.h"
#include <retro_spsc.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "../audio_driver.h"
#include "../../verbosity.h"
#include "../../configuration.h"

/* ═══════════════════════════════════════════════════════════════════
 *  ASIO type definitions (from the public ASIO specification)
 * ═══════════════════════════════════════════════════════════════════ */

typedef long ASIOBool;
#define ASIOFalse 0L
#define ASIOTrue  1L

typedef long ASIOError;
#define ASE_OK               0L
#define ASE_SUCCESS          0x3f4847a0L
#define ASE_NotPresent      (-1000L)
#define ASE_HWMalfunction   (-999L)
#define ASE_InvalidParameter (-998L)
#define ASE_InvalidMode     (-997L)
#define ASE_SPNotAdvancing  (-996L)
#define ASE_NoClock         (-995L)
#define ASE_NoMemory        (-994L)

/* ASIOSampleType and its values, with the conversion that writes them,
 * live in asio_convert.h: pure C, tested on any host. */

typedef double ASIOSampleRate;

typedef struct ASIODriverInfo
{
   long asioVersion;
   long driverVersion;
   char name[32];
   char errorMessage[124];
   void *sysRef;
} ASIODriverInfo;

typedef struct ASIOClockSource
{
   long index;
   long associatedChannel;
   long associatedGroup;
   ASIOBool isCurrentSource;
   char name[32];
} ASIOClockSource;

typedef struct ASIOChannelInfo
{
   long channel;
   ASIOBool isInput;
   ASIOBool isActive;
   long channelGroup;
   ASIOSampleType type;
   char name[32];
} ASIOChannelInfo;

typedef struct ASIOBufferInfo
{
   ASIOBool isInput;
   long channelNum;
   void *buffers[2];
} ASIOBufferInfo;

typedef struct ASIOTime ASIOTime;

/* Callback function signatures */
typedef void (*asio_buffer_switch_fn)(long index, ASIOBool directProcess);
typedef void (*asio_sample_rate_changed_fn)(ASIOSampleRate sRate);
typedef long (*asio_message_fn)(long selector, long value,
      void *message, double *opt);
typedef ASIOTime *(*asio_buffer_switch_time_info_fn)(
      ASIOTime *params, long index, ASIOBool directProcess);

typedef struct ASIOCallbacks
{
   asio_buffer_switch_fn           bufferSwitch;
   asio_sample_rate_changed_fn     sampleRateDidChange;
   asio_message_fn                 asioMessage;
   asio_buffer_switch_time_info_fn bufferSwitchTimeInfo;
} ASIOCallbacks;

/* ASIOMessage selectors */
#define kAsioSelectorSupported  1L
#define kAsioEngineVersion      2L
#define kAsioResetRequest       3L
#define kAsioBufferSizeChange   4L
#define kAsioResyncRequest      5L
#define kAsioLatenciesChanged   6L
#define kAsioSupportsTimeInfo   7L

/* ═══════════════════════════════════════════════════════════════════
 *  IASIO COM vtable layout
 *
 *  ASIO drivers are COM in-process servers.  The IASIO interface
 *  inherits from IUnknown (QueryInterface, AddRef, Release) and
 *  adds the ASIO methods in a fixed vtable order.
 *
 *  On MSVC, these use __thiscall (this in ECX).
 *  On GCC/MinGW, we must call through inline asm wrappers.
 * ═══════════════════════════════════════════════════════════════════ */

/* Vtable slot indices (after IUnknown's 3 slots) */
#define CYCLED_VTABLE_OFFSET 3
enum iasio_vtable_index
{
   CYCLED_IASIO_INIT = 0,           /* ASIOBool init(void *sysHandle) */
   CYCLED_IASIO_GET_DRIVER_NAME,     /* void getDriverName(char *name) */
   CYCLED_IASIO_GET_DRIVER_VERSION,  /* long getDriverVersion() */
   CYCLED_IASIO_GET_ERROR_MESSAGE,   /* void getErrorMessage(char *str) */
   CYCLED_IASIO_START,               /* ASIOError start() */
   CYCLED_IASIO_STOP,                /* ASIOError stop() */
   CYCLED_IASIO_GET_CHANNELS,        /* ASIOError getChannels(long*, long*) */
   CYCLED_IASIO_GET_LATENCIES,       /* ASIOError getLatencies(long*, long*) */
   CYCLED_IASIO_GET_BUFFER_SIZE,     /* ASIOError getBufferSize(long*, long*, long*, long*) */
   CYCLED_IASIO_CAN_SAMPLE_RATE,     /* ASIOError canSampleRate(ASIOSampleRate) */
   CYCLED_IASIO_GET_SAMPLE_RATE,     /* ASIOError getSampleRate(ASIOSampleRate*) */
   CYCLED_IASIO_SET_SAMPLE_RATE,     /* ASIOError setSampleRate(ASIOSampleRate) */
   CYCLED_IASIO_GET_CLOCK_SOURCES,   /* ASIOError getClockSources(ASIOClockSource*, long*) */
   CYCLED_IASIO_SET_CLOCK_SOURCE,    /* ASIOError setClockSource(long) */
   CYCLED_IASIO_GET_SAMPLE_POSITION, /* ASIOError getSamplePosition(int64*, int64*) */
   CYCLED_IASIO_GET_CHANNEL_INFO,    /* ASIOError getChannelInfo(ASIOChannelInfo*) */
   CYCLED_IASIO_CREATE_BUFFERS,      /* ASIOError createBuffers(ASIOBufferInfo*, long, long, ASIOCallbacks*) */
   CYCLED_IASIO_DISPOSE_BUFFERS,     /* ASIOError disposeBuffers() */
   CYCLED_IASIO_CONTROL_PANEL,       /* ASIOError controlPanel() */
   CYCLED_IASIO_FUTURE,              /* ASIOError future(long, void*) */
   CYCLED_IASIO_OUTPUT_READY         /* ASIOError outputReady() */
};

/* ═══════════════════════════════════════════════════════════════════
 *  Thiscall wrappers for GCC/MinGW
 *
 *  ASIO COM objects use MSVC's __thiscall convention: 'this' is
 *  passed in ECX, all other args on the stack right-to-left.
 *  GCC doesn't support __thiscall natively, so we use inline asm
 *  to load ECX before calling through the vtable.
 *
 *  For x86_64 (64-bit), Windows uses a uniform calling convention
 *  where 'this' is the first arg in RCX — no special handling needed.
 * ═══════════════════════════════════════════════════════════════════ */

/* Get a vtable function pointer from a COM interface */
#define IASIO_VTBL(iface, idx) \
   (((void **)(*(void **)(iface)))[CYCLED_VTABLE_OFFSET + (idx)])

/* Cast a void* vtable slot to a typed function pointer.
 * Direct (fntype)void_ptr is forbidden by ISO C (-Wpedantic).
 * Going through memcpy is the standards-blessed workaround
 * that all compilers optimize to a no-op. */
#define IASIO_CALL(fntype, iface, idx) \
   (*(fntype *)&(((void **)(*(void **)(iface)))[CYCLED_VTABLE_OFFSET + (idx)]))

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64)
/* ── 64-bit (x86_64 and ARM64, both MSVC and GCC/MinGW/Clang) ──
 * Windows x64 and ARM64 both use a single unified calling convention
 * — 'this' goes as the first argument (RCX on x64, X0 on ARM64).
 * No special handling needed. */

typedef ASIOBool  (__cdecl *iasio_init_fn)(void *this_, void *sysHandle);
typedef void      (__cdecl *iasio_get_str_fn)(void *this_, char *str);
typedef long      (__cdecl *iasio_get_long_fn)(void *this_);
typedef ASIOError (__cdecl *iasio_no_arg_fn)(void *this_);
typedef ASIOError (__cdecl *iasio_get_channels_fn)(void *this_, long *, long *);
typedef ASIOError (__cdecl *iasio_get_buffer_size_fn)(void *this_, long *, long *, long *, long *);
typedef ASIOError (__cdecl *iasio_sample_rate_fn)(void *this_, ASIOSampleRate);
typedef ASIOError (__cdecl *iasio_get_sample_rate_fn)(void *this_, ASIOSampleRate *);
typedef ASIOError (__cdecl *iasio_get_channel_info_fn)(void *this_, ASIOChannelInfo *);
typedef ASIOError (__cdecl *iasio_create_buffers_fn)(void *this_, ASIOBufferInfo *, long, long, ASIOCallbacks *);
typedef ASIOError (__cdecl *iasio_set_long_fn)(void *this_, long);
typedef ASIOError (__cdecl *iasio_future_fn)(void *this_, long, void *);

#define ASIO_CALL_INIT(iface, sysHandle) \
   IASIO_CALL(iasio_init_fn, iface, CYCLED_IASIO_INIT)(iface, sysHandle)
#define ASIO_CALL_GET_DRIVER_NAME(iface, name) \
   IASIO_CALL(iasio_get_str_fn, iface, CYCLED_IASIO_GET_DRIVER_NAME)(iface, name)
#define ASIO_CALL_GET_DRIVER_VERSION(iface) \
   IASIO_CALL(iasio_get_long_fn, iface, CYCLED_IASIO_GET_DRIVER_VERSION)(iface)
#define ASIO_CALL_GET_ERROR_MESSAGE(iface, msg) \
   IASIO_CALL(iasio_get_str_fn, iface, CYCLED_IASIO_GET_ERROR_MESSAGE)(iface, msg)
#define ASIO_CALL_START(iface) \
   IASIO_CALL(iasio_no_arg_fn, iface, CYCLED_IASIO_START)(iface)
#define ASIO_CALL_STOP(iface) \
   IASIO_CALL(iasio_no_arg_fn, iface, CYCLED_IASIO_STOP)(iface)
#define ASIO_CALL_GET_CHANNELS(iface, inp, outp) \
   IASIO_CALL(iasio_get_channels_fn, iface, CYCLED_IASIO_GET_CHANNELS)(iface, inp, outp)
#define ASIO_CALL_GET_LATENCIES(iface, inp, outp) \
   IASIO_CALL(iasio_get_channels_fn, iface, CYCLED_IASIO_GET_LATENCIES)(iface, inp, outp)
#define ASIO_CALL_GET_BUFFER_SIZE(iface, a, b, c, d) \
   IASIO_CALL(iasio_get_buffer_size_fn, iface, CYCLED_IASIO_GET_BUFFER_SIZE)(iface, a, b, c, d)
#define ASIO_CALL_CAN_SAMPLE_RATE(iface, r) \
   IASIO_CALL(iasio_sample_rate_fn, iface, CYCLED_IASIO_CAN_SAMPLE_RATE)(iface, r)
#define ASIO_CALL_GET_SAMPLE_RATE(iface, r) \
   IASIO_CALL(iasio_get_sample_rate_fn, iface, CYCLED_IASIO_GET_SAMPLE_RATE)(iface, r)
#define ASIO_CALL_SET_SAMPLE_RATE(iface, r) \
   IASIO_CALL(iasio_sample_rate_fn, iface, CYCLED_IASIO_SET_SAMPLE_RATE)(iface, r)
#define ASIO_CALL_GET_CHANNEL_INFO(iface, ci) \
   IASIO_CALL(iasio_get_channel_info_fn, iface, CYCLED_IASIO_GET_CHANNEL_INFO)(iface, ci)
#define ASIO_CALL_CREATE_BUFFERS(iface, bi, nc, bs, cb) \
   IASIO_CALL(iasio_create_buffers_fn, iface, CYCLED_IASIO_CREATE_BUFFERS)(iface, bi, nc, bs, cb)
#define ASIO_CALL_DISPOSE_BUFFERS(iface) \
   IASIO_CALL(iasio_no_arg_fn, iface, CYCLED_IASIO_DISPOSE_BUFFERS)(iface)
#define ASIO_CALL_CONTROL_PANEL(iface) \
   IASIO_CALL(iasio_no_arg_fn, iface, CYCLED_IASIO_CONTROL_PANEL)(iface)
#define ASIO_CALL_OUTPUT_READY(iface) \
   IASIO_CALL(iasio_no_arg_fn, iface, CYCLED_IASIO_OUTPUT_READY)(iface)
#define ASIO_CALL_RELEASE(iface) \
   (*(iasio_no_arg_fn *)&(((void **)(*(void **)(iface)))[2]))(iface)

#elif defined(_MSC_VER) && defined(_M_IX86)
/* ── 32-bit MSVC ──
 * MSVC natively supports __thiscall, so we can use it directly
 * in the function pointer typedefs. */

typedef ASIOBool  (__thiscall *iasio_init_fn)(void *this_, void *sysHandle);
typedef void      (__thiscall *iasio_get_str_fn)(void *this_, char *str);
typedef long      (__thiscall *iasio_get_long_fn)(void *this_);
typedef ASIOError (__thiscall *iasio_no_arg_fn)(void *this_);
typedef ASIOError (__thiscall *iasio_get_channels_fn)(void *this_, long *, long *);
typedef ASIOError (__thiscall *iasio_get_buffer_size_fn)(void *this_, long *, long *, long *, long *);
typedef ASIOError (__thiscall *iasio_sample_rate_fn)(void *this_, ASIOSampleRate);
typedef ASIOError (__thiscall *iasio_get_sample_rate_fn)(void *this_, ASIOSampleRate *);
typedef ASIOError (__thiscall *iasio_get_channel_info_fn)(void *this_, ASIOChannelInfo *);
typedef ASIOError (__thiscall *iasio_create_buffers_fn)(void *this_, ASIOBufferInfo *, long, long, ASIOCallbacks *);
typedef ASIOError (__thiscall *iasio_set_long_fn)(void *this_, long);
typedef ASIOError (__thiscall *iasio_future_fn)(void *this_, long, void *);

#define ASIO_CALL_INIT(iface, sysHandle) \
   ((iasio_init_fn)IASIO_VTBL(iface, CYCLED_IASIO_INIT))(iface, sysHandle)
#define ASIO_CALL_GET_DRIVER_NAME(iface, name) \
   ((iasio_get_str_fn)IASIO_VTBL(iface, CYCLED_IASIO_GET_DRIVER_NAME))(iface, name)
#define ASIO_CALL_GET_DRIVER_VERSION(iface) \
   ((iasio_get_long_fn)IASIO_VTBL(iface, CYCLED_IASIO_GET_DRIVER_VERSION))(iface)
#define ASIO_CALL_GET_ERROR_MESSAGE(iface, msg) \
   ((iasio_get_str_fn)IASIO_VTBL(iface, CYCLED_IASIO_GET_ERROR_MESSAGE))(iface, msg)
#define ASIO_CALL_START(iface) \
   ((iasio_no_arg_fn)IASIO_VTBL(iface, CYCLED_IASIO_START))(iface)
#define ASIO_CALL_STOP(iface) \
   ((iasio_no_arg_fn)IASIO_VTBL(iface, CYCLED_IASIO_STOP))(iface)
#define ASIO_CALL_GET_CHANNELS(iface, inp, outp) \
   ((iasio_get_channels_fn)IASIO_VTBL(iface, CYCLED_IASIO_GET_CHANNELS))(iface, inp, outp)
#define ASIO_CALL_GET_LATENCIES(iface, inp, outp) \
   ((iasio_get_channels_fn)IASIO_VTBL(iface, CYCLED_IASIO_GET_LATENCIES))(iface, inp, outp)
#define ASIO_CALL_GET_BUFFER_SIZE(iface, a, b, c, d) \
   ((iasio_get_buffer_size_fn)IASIO_VTBL(iface, CYCLED_IASIO_GET_BUFFER_SIZE))(iface, a, b, c, d)
#define ASIO_CALL_CAN_SAMPLE_RATE(iface, r) \
   ((iasio_sample_rate_fn)IASIO_VTBL(iface, CYCLED_IASIO_CAN_SAMPLE_RATE))(iface, r)
#define ASIO_CALL_GET_SAMPLE_RATE(iface, r) \
   ((iasio_get_sample_rate_fn)IASIO_VTBL(iface, CYCLED_IASIO_GET_SAMPLE_RATE))(iface, r)
#define ASIO_CALL_SET_SAMPLE_RATE(iface, r) \
   ((iasio_sample_rate_fn)IASIO_VTBL(iface, CYCLED_IASIO_SET_SAMPLE_RATE))(iface, r)
#define ASIO_CALL_GET_CHANNEL_INFO(iface, ci) \
   ((iasio_get_channel_info_fn)IASIO_VTBL(iface, CYCLED_IASIO_GET_CHANNEL_INFO))(iface, ci)
#define ASIO_CALL_CREATE_BUFFERS(iface, bi, nc, bs, cb) \
   ((iasio_create_buffers_fn)IASIO_VTBL(iface, CYCLED_IASIO_CREATE_BUFFERS))(iface, bi, nc, bs, cb)
#define ASIO_CALL_DISPOSE_BUFFERS(iface) \
   ((iasio_no_arg_fn)IASIO_VTBL(iface, CYCLED_IASIO_DISPOSE_BUFFERS))(iface)
#define ASIO_CALL_CONTROL_PANEL(iface) \
   ((iasio_no_arg_fn)IASIO_VTBL(iface, CYCLED_IASIO_CONTROL_PANEL))(iface)
#define ASIO_CALL_OUTPUT_READY(iface) \
   ((iasio_no_arg_fn)IASIO_VTBL(iface, CYCLED_IASIO_OUTPUT_READY))(iface)
#define ASIO_CALL_RELEASE(iface) \
   ((iasio_no_arg_fn)(((void **)(*(void **)(iface)))[2]))(iface)

#elif defined(__i386__) && !defined(_MSC_VER)
/* ── 32-bit GCC/MinGW ──
 * GCC doesn't support __thiscall.  Must use inline asm wrappers
 * to load 'this' into ECX before each vtable call. */

static INLINE ASIOBool asio_thiscall_init(void *iface, void *sysHandle)
{
   ASIOBool ret;
   void *fn = IASIO_VTBL(iface, CYCLED_IASIO_INIT);
   __asm__ __volatile__ (
      "pushl %2\n\t"
      "movl  %1, %%ecx\n\t"
      "call  *%3\n\t"
      : "=a"(ret)
      : "r"(iface), "r"(sysHandle), "r"(fn)
      : "ecx", "edx", "memory"
   );
   return ret;
}

static INLINE void asio_thiscall_get_str(void *iface, int idx, char *str)
{
   void *fn = IASIO_VTBL(iface, idx);
   __asm__ __volatile__ (
      "pushl %1\n\t"
      "movl  %0, %%ecx\n\t"
      "call  *%2\n\t"
      :
      : "r"(iface), "r"(str), "r"(fn)
      : "eax", "ecx", "edx", "memory"
   );
}

static INLINE long asio_thiscall_get_long(void *iface, int idx)
{
   long ret;
   void *fn = IASIO_VTBL(iface, idx);
   __asm__ __volatile__ (
      "movl %1, %%ecx\n\t"
      "call *%2\n\t"
      : "=a"(ret)
      : "r"(iface), "r"(fn)
      : "ecx", "edx", "memory"
   );
   return ret;
}

static INLINE ASIOError asio_thiscall_no_arg(void *iface, int idx)
{
   ASIOError ret;
   void *fn = IASIO_VTBL(iface, idx);
   __asm__ __volatile__ (
      "movl %1, %%ecx\n\t"
      "call *%2\n\t"
      : "=a"(ret)
      : "r"(iface), "r"(fn)
      : "ecx", "edx", "memory"
   );
   return ret;
}

static INLINE ASIOError asio_thiscall_two_longs(void *iface, int idx,
      long *a, long *b)
{
   ASIOError ret;
   void *fn = IASIO_VTBL(iface, idx);
   __asm__ __volatile__ (
      "pushl %3\n\t"
      "pushl %2\n\t"
      "movl  %1, %%ecx\n\t"
      "call  *%4\n\t"
      : "=a"(ret)
      : "r"(iface), "r"(a), "r"(b), "r"(fn)
      : "ecx", "edx", "memory"
   );
   return ret;
}

static INLINE ASIOError asio_thiscall_four_longs(void *iface, int idx,
      long *a, long *b, long *c, long *d)
{
   ASIOError ret;
   void *fn = IASIO_VTBL(iface, idx);
   /* Store args in a local array so asm only needs 2 register inputs
    * (this + fn).  Avoids "impossible constraints" on i686 where GCC
    * cannot allocate 6 simultaneous register operands. */
   void *args[4];
   args[0] = (void *)a;
   args[1] = (void *)b;
   args[2] = (void *)c;
   args[3] = (void *)d;
   __asm__ __volatile__ (
      "pushl 12(%2)\n\t"    /* args[3] = d */
      "pushl  8(%2)\n\t"    /* args[2] = c */
      "pushl  4(%2)\n\t"    /* args[1] = b */
      "pushl   (%2)\n\t"    /* args[0] = a */
      "movl  %1, %%ecx\n\t"
      "call  *%3\n\t"
      : "=a"(ret)
      : "r"(iface), "r"(args), "r"(fn)
      : "ecx", "edx", "memory"
   );
   return ret;
}

/* ASIOSampleRate is a double — passed on the FPU stack or as two
 * 32-bit words on the regular stack depending on the driver.
 * Most drivers expect it as a 64-bit value pushed onto the stack. */
static INLINE ASIOError asio_thiscall_set_sample_rate(void *iface,
      int idx, ASIOSampleRate rate)
{
   ASIOError ret;
   void *fn = IASIO_VTBL(iface, idx);
   __asm__ __volatile__ (
      "subl  $8, %%esp\n\t"
      "fldl  %2\n\t"
      "fstpl (%%esp)\n\t"
      "movl  %1, %%ecx\n\t"
      "call  *%3\n\t"
      : "=a"(ret)
      : "r"(iface), "m"(rate), "r"(fn)
      : "ecx", "edx", "memory", "st"
   );
   return ret;
}

static INLINE ASIOError asio_thiscall_get_sample_rate(void *iface,
      int idx, ASIOSampleRate *rate)
{
   ASIOError ret;
   void *fn = IASIO_VTBL(iface, idx);
   __asm__ __volatile__ (
      "pushl %2\n\t"
      "movl  %1, %%ecx\n\t"
      "call  *%3\n\t"
      : "=a"(ret)
      : "r"(iface), "r"(rate), "r"(fn)
      : "ecx", "edx", "memory"
   );
   return ret;
}

static INLINE ASIOError asio_thiscall_get_channel_info(void *iface,
      ASIOChannelInfo *ci)
{
   ASIOError ret;
   void *fn = IASIO_VTBL(iface, CYCLED_IASIO_GET_CHANNEL_INFO);
   __asm__ __volatile__ (
      "pushl %2\n\t"
      "movl  %1, %%ecx\n\t"
      "call  *%3\n\t"
      : "=a"(ret)
      : "r"(iface), "r"(ci), "r"(fn)
      : "ecx", "edx", "memory"
   );
   return ret;
}

static INLINE ASIOError asio_thiscall_create_buffers(void *iface,
      ASIOBufferInfo *bi, long nc, long bs, ASIOCallbacks *cb)
{
   ASIOError ret;
   void *fn = IASIO_VTBL(iface, CYCLED_IASIO_CREATE_BUFFERS);
   /* Store args in a local array — same register-pressure fix as
    * asio_thiscall_four_longs above. */
   void *args[4];
   args[0] = (void *)bi;
   args[1] = (void *)(intptr_t)nc;
   args[2] = (void *)(intptr_t)bs;
   args[3] = (void *)cb;
   __asm__ __volatile__ (
      "pushl 12(%2)\n\t"    /* args[3] = cb */
      "pushl  8(%2)\n\t"    /* args[2] = bs */
      "pushl  4(%2)\n\t"    /* args[1] = nc */
      "pushl   (%2)\n\t"    /* args[0] = bi */
      "movl  %1, %%ecx\n\t"
      "call  *%3\n\t"
      : "=a"(ret)
      : "r"(iface), "r"(args), "r"(fn)
      : "ecx", "edx", "memory"
   );
   return ret;
}

static INLINE void asio_thiscall_release(void *iface)
{
   void *fn = ((void **)(*(void **)(iface)))[2]; /* IUnknown::Release */
   __asm__ __volatile__ (
      "movl %0, %%ecx\n\t"
      "call *%1\n\t"
      :
      : "r"(iface), "r"(fn)
      : "eax", "ecx", "edx", "memory"
   );
}

#define ASIO_CALL_INIT(iface, sh)           asio_thiscall_init(iface, sh)
#define ASIO_CALL_GET_DRIVER_NAME(iface, n) asio_thiscall_get_str(iface, CYCLED_IASIO_GET_DRIVER_NAME, n)
#define ASIO_CALL_GET_DRIVER_VERSION(iface) asio_thiscall_get_long(iface, CYCLED_IASIO_GET_DRIVER_VERSION)
#define ASIO_CALL_GET_ERROR_MESSAGE(iface, m) asio_thiscall_get_str(iface, CYCLED_IASIO_GET_ERROR_MESSAGE, m)
#define ASIO_CALL_START(iface)              asio_thiscall_no_arg(iface, CYCLED_IASIO_START)
#define ASIO_CALL_STOP(iface)               asio_thiscall_no_arg(iface, CYCLED_IASIO_STOP)
#define ASIO_CALL_GET_CHANNELS(iface, i, o) asio_thiscall_two_longs(iface, CYCLED_IASIO_GET_CHANNELS, i, o)
#define ASIO_CALL_GET_LATENCIES(iface, i, o) asio_thiscall_two_longs(iface, CYCLED_IASIO_GET_LATENCIES, i, o)
#define ASIO_CALL_GET_BUFFER_SIZE(iface, a, b, c, d) asio_thiscall_four_longs(iface, CYCLED_IASIO_GET_BUFFER_SIZE, a, b, c, d)
#define ASIO_CALL_CAN_SAMPLE_RATE(iface, r) asio_thiscall_set_sample_rate(iface, CYCLED_IASIO_CAN_SAMPLE_RATE, r)
#define ASIO_CALL_GET_SAMPLE_RATE(iface, r) asio_thiscall_get_sample_rate(iface, CYCLED_IASIO_GET_SAMPLE_RATE, r)
#define ASIO_CALL_SET_SAMPLE_RATE(iface, r) asio_thiscall_set_sample_rate(iface, CYCLED_IASIO_SET_SAMPLE_RATE, r)
#define ASIO_CALL_GET_CHANNEL_INFO(iface, ci) asio_thiscall_get_channel_info(iface, ci)
#define ASIO_CALL_CREATE_BUFFERS(iface, bi, nc, bs, cb) asio_thiscall_create_buffers(iface, bi, nc, bs, cb)
#define ASIO_CALL_DISPOSE_BUFFERS(iface)    asio_thiscall_no_arg(iface, CYCLED_IASIO_DISPOSE_BUFFERS)
#define ASIO_CALL_CONTROL_PANEL(iface)     asio_thiscall_no_arg(iface, CYCLED_IASIO_CONTROL_PANEL)
#define ASIO_CALL_OUTPUT_READY(iface)       asio_thiscall_no_arg(iface, CYCLED_IASIO_OUTPUT_READY)
#define ASIO_CALL_RELEASE(iface)            asio_thiscall_release(iface)

#else
#error "Unsupported architecture for ASIO driver"
#endif

/* ═══════════════════════════════════════════════════════════════════
 *  Registry-based driver enumeration
 * ═══════════════════════════════════════════════════════════════════ */

#define ASIO_MAX_DRIVERS     32
#define ASIO_REG_PATH        "SOFTWARE\\ASIO"

typedef struct asio_driver_entry
{
   char name[64];
   CLSID clsid;
} asio_driver_entry_t;

/* Enumerate installed ASIO drivers from the registry.
 * Returns number of drivers found, up to max_entries. */
static int asio_enumerate_drivers(asio_driver_entry_t *entries,
      int max_entries)
{
   HKEY asio_key;
   int count = 0;
   LONG rc;

   rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, ASIO_REG_PATH,
         0, KEY_READ, &asio_key);
   if (rc != ERROR_SUCCESS)
      return 0;

   for (count = 0; count < max_entries; count++)
   {
      char sub_name[128];
      DWORD sub_name_len = sizeof(sub_name);
      HKEY sub_key;
      char clsid_str[64];
      DWORD clsid_len = sizeof(clsid_str);
      wchar_t clsid_w[64];

      rc = RegEnumKeyExA(asio_key, count, sub_name, &sub_name_len,
            NULL, NULL, NULL, NULL);
      if (rc != ERROR_SUCCESS)
         break;

      rc = RegOpenKeyExA(asio_key, sub_name, 0, KEY_READ, &sub_key);
      if (rc != ERROR_SUCCESS)
         continue;

      rc = RegQueryValueExA(sub_key, "CLSID", NULL, NULL,
            (LPBYTE)clsid_str, &clsid_len);
      RegCloseKey(sub_key);

      if (rc != ERROR_SUCCESS)
         continue;

      /* Convert CLSID string to GUID */
      MultiByteToWideChar(CP_ACP, 0, clsid_str, -1, clsid_w, 64);
      if (FAILED(CLSIDFromString(clsid_w, &entries[count].clsid)))
         continue;

      strlcpy(entries[count].name, sub_name,
            sizeof(entries[count].name));
   }

   RegCloseKey(asio_key);
   return count;
}

/* Load an ASIO driver COM object by CLSID.
 * Note: ASIO uses the CLSID as both the class ID and the interface ID. */
static void *asio_load_driver(const CLSID *clsid)
{
   void *iface = NULL;
   /* In C, REFCLSID/REFIID (CoCreateInstance's 1st and 4th args) are
    * 'const IID *'; in C++ they are 'const IID &'. ASIO uses the CLSID
    * as both the class ID and the interface ID, so pass the same GUID
    * for both, dereferencing under CXX_BUILD. */
#ifdef __cplusplus
   HRESULT hr  = CoCreateInstance(*clsid, NULL,
         CLSCTX_INPROC_SERVER, *clsid, &iface);
#else
   HRESULT hr  = CoCreateInstance(clsid, NULL,
         CLSCTX_INPROC_SERVER, clsid, &iface);
#endif
   if (FAILED(hr))
      return NULL;
   return iface;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Driver state and globals
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct ra_asio
{
   void              *iasio;        /* COM interface pointer */
   /* Lock-free SPSC ring buffer between asio_write (producer, main
    * thread) and asio_cb_buffer_switch (consumer, ASIO callback
    * thread).  Pre-port this used fifo_buffer_t with no surrounding
    * lock, which was a real cross-thread race on first/end -- see
    * commit message.  retro_spsc_t is the SPSC primitive designed
    * for this exact pattern: lock-free (avoiding the priority-
    * inversion concern of locking from a real-time audio callback),
    * acquire/release ordering on the cursors, single-producer /
    * single-consumer enforced by the type contract.
    *
    * Embedded by value (not via pointer) so the lifetime exactly
    * tracks ra_asio_t.  Initialised with retro_spsc_init in
    * ra_asio_init / ra_asio_init_via_persistent and freed with
    * retro_spsc_free in the corresponding teardown paths.  The
    * `ring_initialized` flag below distinguishes "never-initialised"
    * from "init succeeded" so cleanup paths know whether to call
    * retro_spsc_free.  retro_spsc_t doesn't carry that bit
    * internally because most callers know their own lifecycle. */
   retro_spsc_t       ring;
   bool               ring_initialized;
#ifdef HAVE_THREADS
   scond_t           *cond;
   slock_t           *cond_lock;
#endif
   ASIOBufferInfo     buf_info[2];  /* L and R output channels */
   /* Deinterleave scratch, owned by the consumer (ASIO callback).
    * One bulk retro_spsc_read per callback lands here and the format
    * conversion below reads out of it, instead of taking the ring's
    * cursors once per frame.  Sized for buffer_frames stereo float
    * frames; allocated once buffer_frames is known in ra_asio_init and
    * kept across park/reclaim, since the ASIO buffer size is fixed by
    * the driver for the lifetime of the COM object. */
   float             *scratch;
   ASIOSampleType     sample_type;
   long               buffer_frames;
   /* ASIOGetLatencies' output figure, in frames, once buffers exist:
    * the device stage behind the ring. Zero until then, and the ring
    * is sized from two periods instead. */
   long               output_latency;
   /* Whether ASIOOutputReady() returned ASE_OK when probed after the
    * buffers were created; the callback calls it only then. */
   bool               output_ready_supported;
   /* Whether the last period ended in silence for want of audio, so
    * the next audio is faded in. Callback thread only. */
   bool               last_underran;
   /* Periods that ended in silence for want of audio: one atomic add
    * on that path, read by the frontend's overlay. */
   retro_atomic_size_t underruns;
   /* Frames the device has taken: a period per callback, silence
    * included. Written by the callback thread, read by the writer; a
    * single aligned word, and a count only ever compared with itself. */
   size_t             consumed;
   /* Set by the message callback on kAsioResetRequest or
    * kAsioBufferSizeChange: the buffers are to be disposed and created
    * anew - the driver's preferred size may have changed - when the
    * frontend reinitialises audio, which the same callback requests. */
   retro_atomic_int_t rebuild_pending;
   /* Set by kAsioLatenciesChanged: the writer re-reads the latencies on
    * its own thread, which owns the COM apartment, and logs them. The
    * ring keeps its size until the next reinit; rate control absorbs
    * the change meanwhile. */
   retro_atomic_int_t latencies_changed;
   /* The device's output count, and the first of the two outputs the
    * buffers were created on: the audio_asio_output_channel setting,
    * clamped to the device. */
   long               out_channels;
   long               out_left;
   size_t             ring_size;
   unsigned           sample_rate;
   /* Read by asio_cb_buffer_switch() on the driver's realtime thread
    * and written from the main thread; shutdown is also written from
    * the driver thread by asio_cb_message(). volatile carries no
    * ordering under MSVC /volatile:iso, which is what ARM64 builds
    * get, so state it explicitly like everything else this callback
    * touches. */
   retro_atomic_int_t shutdown;
   retro_atomic_int_t is_paused;
   bool               nonblock;
   bool               com_initialized;
   bool               buffers_created;
} ra_asio_t;

/* The device stage behind the ring, in frames: what ASIOGetLatencies
 * reports for output once buffers exist - a native driver says about
 * two periods, ASIO4ALL the WDM-KS pipeline it wraps, several times
 * that - or two periods until it has been asked. */
static size_t asio_device_frames(const ra_asio_t *ad)
{
   if (ad->output_latency > 0)
      return (size_t)ad->output_latency;
   return (size_t)ad->buffer_frames * 2;
}

/* Frames for the ring: the setting less the device stage, floored as
 * asio_ring.h says. retro_spsc rounds the result up to a power of two;
 * asio_size_ring() below keeps the size asked for. */
static size_t asio_ring_frames(const ra_asio_t *ad, unsigned latency)
{
   return asio_ring_frames_for(ad->sample_rate, latency,
         asio_device_frames(ad), (size_t)ad->buffer_frames);
}

/* Recreates the ring at the size the setting and the device stage call
 * for, when that differs enough from what it is; otherwise clears it.
 * Only while no callback can run: before ASIOStart, or between a stop
 * and a restart. Returns false when the new ring could not be made,
 * with the old one gone. */
static bool asio_size_ring(ra_asio_t *ad, unsigned latency)
{
   size_t want = asio_ring_frames(ad, latency) * 2 * sizeof(float);

   /* retro_spsc rounds its capacity up to a power of two. The ring's
    * size for every purpose here - what is reported, what is written
    * into, what the room is measured against - is the size asked for,
    * and the capacity beyond it goes unused: asio_ring_room() below
    * subtracts the excess. A 648-frame request came out as a 1024-frame
    * ring otherwise, 21 ms reported against a 16 ms setting. The
    * physical ring is remade only when the request no longer fits it
    * or would leave more than half of it idle. */
   if (ad->ring_initialized
         && want <= ad->ring.capacity
         && want * 2 > ad->ring.capacity)
   {
      retro_spsc_clear(&ad->ring);
      ad->ring_size = want;
      return true;
   }
   if (ad->ring_initialized)
      retro_spsc_free(&ad->ring);
   ad->ring_initialized = false;
   if (!retro_spsc_init(&ad->ring, want))
      return false;
   ad->ring_initialized = true;
   ad->ring_size        = want;
   return true;
}

/* Room in the ring against its logical size: the physical room less
 * the capacity that lies beyond the size asked for. */
static size_t asio_ring_room(const ra_asio_t *ad)
{
   size_t room   = retro_spsc_write_avail(&ad->ring);
   size_t excess = ad->ring.capacity - ad->ring_size;
   return room > excess ? room - excess : 0;
}

static void asio_log_stages(const ra_asio_t *ad, unsigned latency)
{
   size_t ring_frames   = ad->ring_size / (2 * sizeof(float));
   size_t device_frames = asio_device_frames(ad);
   double ring_ms       = (double)ring_frames * 1000.0 / ad->sample_rate;
   double device_ms     = (double)device_frames * 1000.0 / ad->sample_rate;
   /* For the statistics overlay, next to the ring. */
   audio_driver_set_device_latency(device_frames);
   /* Rate control holds the ring about half full, so half the ring plus
    * the device stage is what leaves RetroArch on this path. */
   RARCH_LOG("[ASIO] %u ms setting: a %u-frame ring (%.1f ms, rate control holds it about half full) in front of the device's %s of %u frames (%.1f ms); about %.1f ms from write to the device.\n",
         latency, (unsigned)ring_frames, ring_ms,
         ad->output_latency > 0 ? "reported output latency" : "double buffer",
         (unsigned)device_frames, device_ms,
         ring_ms / 2.0 + device_ms);
}

/* Singleton — ASIO callbacks have no user-data parameter */
static ra_asio_t *g_asio = NULL;

/* Persistent instance that survives free/init cycles (core switches).
 * ASIO4ALL crashes if we destroy and recreate its COM object, so we
 * keep the driver alive and reuse it on the next init.  free() parks
 * the instance here; init() reclaims it. */
static ra_asio_t *g_asio_persistent = NULL;

/* ═══════════════════════════════════════════════════════════════════
 *  Sample conversion: ring buffer → ASIO deinterleaved output
 * ═══════════════════════════════════════════════════════════════════ */

static void asio_deinterleave_to_buffers(ra_asio_t *ad,
      long index, long frames)
{
   void *buf_l  = ad->buf_info[0].buffers[index];
   void *buf_r  = ad->buf_info[1].buffers[index];
   /* Acquire-load on the producer's head cursor.  Pairs with the
    * release-store inside retro_spsc_write that asio_write
    * issues on the main thread, so the bytes we're about to read
    * out via retro_spsc_read are guaranteed visible. */
   size_t avail = retro_spsc_read_avail(&ad->ring);
   long have    = (long)(avail / (2 * sizeof(float)));

   if (have > frames)
      have = frames;

   /* Drain what we're going to use in one go.  The cursors are touched
    * exactly once per callback rather than once per frame; everything
    * below reads out of thread-local scratch.  retro_spsc_read is
    * capped by the read_avail above so it returns the full request,
    * but use its return value in case that contract changes. */
   if (have > 0)
      have = (long)(retro_spsc_read(&ad->ring, ad->scratch,
            (size_t)have * 2 * sizeof(float)) / (2 * sizeof(float)));

   /* Every type the specification defines, in asio_convert.h; the
    * five this used to handle left every other one - the Int32LSB24
    * family that pro interfaces report among them - to a silent
    * memset with nothing in the log. Audio that starts after a period
    * that ended in silence is faded in, and audio that runs out is
    * faded out, so an underrun's edges do not click. */
   asio_convert_frames(ad->sample_type, ad->scratch, have, frames,
         buf_l, buf_r, ad->last_underran);
   ad->last_underran = (have < frames);
   if (have < frames)
      retro_atomic_fetch_add_size(&ad->underruns, 1);
   ad->consumed     += (size_t)frames;
}

/* ═══════════════════════════════════════════════════════════════════
 *  ASIO callbacks
 * ═══════════════════════════════════════════════════════════════════ */

static void asio_cb_buffer_switch(long index,
      ASIOBool direct_process)
{
   ra_asio_t *ad = g_asio;

   if (     !ad || !ad->ring_initialized || !ad->scratch
         || retro_atomic_load_acquire_int(&ad->is_paused)
         || retro_atomic_load_acquire_int(&ad->shutdown))
   {
      if (ad && ad->buf_info[0].buffers[index])
      {
         size_t bsz = ad->buffer_frames
               * asio_bytes_per_sample(ad->sample_type);
         memset(ad->buf_info[0].buffers[index], 0, bsz);
         memset(ad->buf_info[1].buffers[index], 0, bsz);
         /* The zeros are output too; a driver that waits for the
          * notification would otherwise hold the previous half. */
         if (ad->output_ready_supported)
            ASIO_CALL_OUTPUT_READY(ad->iasio);
      }
      return;
   }

   asio_deinterleave_to_buffers(ad, index, ad->buffer_frames);

#ifdef HAVE_THREADS
   if (ad->cond)
      scond_signal(ad->cond);
#endif

   /* After the last store to this half, and only on a driver that
    * said it wants it: a driver that copies its buffers to the device
    * can ship this half now rather than at the next switch, one period
    * sooner. A driver that does not support it returns an error to
    * every call, and was being called every period regardless. */
   if (ad->output_ready_supported)
      ASIO_CALL_OUTPUT_READY(ad->iasio);
}

static void asio_cb_sample_rate_changed(ASIOSampleRate rate)
{
   RARCH_LOG("[ASIO] Sample rate changed to %.0f Hz.\n", rate);
}

static long asio_cb_message(long selector, long value,
      void *message, double *opt)
{
   switch (selector)
   {
      case kAsioSelectorSupported:
         switch (value)
         {
            case kAsioEngineVersion:
            case kAsioSupportsTimeInfo:
            case kAsioResetRequest:
            case kAsioBufferSizeChange:
            case kAsioResyncRequest:
            case kAsioLatenciesChanged:
               return 1L;
         }
         return 0L;
      case kAsioEngineVersion:
         return 2L;
      case kAsioResetRequest:
      case kAsioBufferSizeChange:
         /* The driver's buffer size or sample rate has changed and it
          * wants the host to tear its buffers down and make them again.
          * As the WASAPI device-change path does, ask the frontend to
          * reinitialise audio - on the main thread, in its own time -
          * and have the reclaim path rebuild the buffers when it does.
          * This used to raise shutdown, which ended audio for the
          * session on any change from the driver's control panel. */
         RARCH_WARN("[ASIO] Driver requests %s; audio will reinitialise.\n",
               selector == kAsioResetRequest ? "a reset" : "a buffer size change");
         if (g_asio)
            retro_atomic_store_release_int(&g_asio->rebuild_pending, 1);
         retro_atomic_store_release_int(&audio_state_get_ptr()->reinit_request, 1);
         return 1L;
      case kAsioResyncRequest:
         /* The driver lost its place - a system pause, a clock that
          * stalled - and asks the host to resync. Nothing here keeps
          * time of its own: the ring's fill is what rate control
          * steers, and it re-converges. Acknowledged, and said. */
         RARCH_LOG("[ASIO] Driver requests a resync; rate control will re-converge.\n");
         return 1L;
      case kAsioLatenciesChanged:
         /* Re-read on the writer's thread, not this one. */
         RARCH_LOG("[ASIO] Driver reports its latencies changed.\n");
         if (g_asio)
            retro_atomic_store_release_int(&g_asio->latencies_changed, 1);
         return 1L;
      case kAsioSupportsTimeInfo:
         return 1L; /* We implement bufferSwitchTimeInfo */
      default:
         return 0L;
   }
}

static ASIOTime * asio_cb_buffer_switch_time_info(
      ASIOTime *params, long index, ASIOBool direct_process)
{
   asio_cb_buffer_switch(index, direct_process);
   return params;
}

static ASIOCallbacks g_asio_callbacks = {
   asio_cb_buffer_switch,
   asio_cb_sample_rate_changed,
   asio_cb_message,
   asio_cb_buffer_switch_time_info
};

/* One thing worth noting: if RetroArch switches away from the ASIO driver to a different audio driver (e.g. user changes from "asio" to "wasapi" in settings), free() parks the instance but init() is never called again for ASIO — so g_asio_persistent holds the parked instance until exit. That's not a growing leak (it's a fixed ~100 bytes plus the ring buffer), but it does hold the ASIO COM object and device open. If you wanted to handle that edge case, you'd need a destructor that runs on actual driver unload, but RetroArch doesn't have that mechanism */

/* Called at process exit to clean up a parked ASIO instance.
 * This prevents COM object leaks and satisfies leak checkers. */
/* The whole teardown: stop, dispose, release the COM object, free what
 * the instance owns. The callback is gated on g_asio, which the caller
 * has cleared; the pause after the stop lets a callback that some
 * drivers - ASIO4ALL among them - still have in flight when ASIOStop
 * returns run out before the buffers under it go. */
static void asio_destroy(ra_asio_t *ad)
{
   if (ad->iasio)
   {
      ASIO_CALL_STOP(ad->iasio);
      Sleep(20);
      if (ad->buffers_created)
         ASIO_CALL_DISPOSE_BUFFERS(ad->iasio);
      ASIO_CALL_RELEASE(ad->iasio);
   }

   if (ad->ring_initialized)
   {
      retro_spsc_free(&ad->ring);
      ad->ring_initialized = false;
   }

   if (ad->scratch)
   {
      free(ad->scratch);
      ad->scratch = NULL;
   }

#ifdef HAVE_THREADS
   if (ad->cond_lock)
      slock_free(ad->cond_lock);
   if (ad->cond)
      scond_free(ad->cond);
#endif

   if (ad->com_initialized)
      CoUninitialize();

   free(ad);
}

static void asio_atexit_cleanup(void)
{
   ra_asio_t *ad = g_asio_persistent;
   if (!ad)
      ad = g_asio;
   if (!ad)
      return;

   g_asio            = NULL;
   g_asio_persistent = NULL;
   asio_destroy(ad);
}

/* ═══════════════════════════════════════════════════════════════════
 *  RetroArch audio_driver_t implementation
 * ═══════════════════════════════════════════════════════════════════ */

/* Prime the ring to the rate-control setpoint (half capacity) with
 * silence.  Init/reclaim-time only - not on any streaming path.
 *
 * Streaming starts the moment ASIOStart is called, but the writer has
 * produced nothing yet: an empty ring means a deterministic burst of
 * underruns (a pop) on every fresh init and on every park/reclaim -
 * i.e. every content load, fullscreen toggle and settings change.
 * Half capacity is exactly where rate control holds the ring in
 * steady state, so priming there adds no latency beyond the setpoint
 * and no convergence transient: the stream begins already balanced,
 * with silence draining ahead of the first real audio. */
static void asio_prime_ring(ra_asio_t *ad)
{
   static const char zeros[512]; /* zero-initialised */
   size_t left = asio_ring_room(ad) / 2;
   while (left > 0)
   {
      size_t n = (left < sizeof(zeros)) ? left : sizeof(zeros);
      retro_spsc_write(&ad->ring, zeros, n);
      left -= n;
   }
}

static void asio_dispose_buffers(ra_asio_t *ad)
{
   if (ad->buffers_created)
      ASIO_CALL_DISPOSE_BUFFERS(ad->iasio);
   ad->buffers_created = false;
}

/* Everything that depends on the driver's buffer size, from asking for
 * it to a primed ring: the period, the scratch it needs, the output
 * channel's sample type, the buffers themselves, the OutputReady probe,
 * the latencies, and the ring sized from them. Run at first init, and
 * again on reclaim after the driver asked for a reset, when its
 * preferred size may have changed. g_asio is set by the caller before
 * this, as the driver may call back during ASIOCreateBuffers. */
/* The device period to ask for. The driver's preferred size is the
 * default and the ceiling: it is what the driver was tuned around and
 * what its control panel shows. Below it only when the preferred size
 * is more than a quarter of the latency setting - the device stage is
 * about two periods, and two periods of more than a quarter would be
 * more than half the setting in the device alone - and then the
 * smallest legal size at or above that quarter. Legal sizes follow
 * the specification's granularity: a positive step from the minimum,
 * only the three advertised when zero, powers of two when negative.
 * A device whose sizes are locked gets its preferred size. */
static long asio_choose_period(unsigned sample_rate, unsigned latency,
      long min_sz, long max_sz, long pref_sz, long gran)
{
   long target = (long)((uint64_t)sample_rate * latency / 4000);
   long chosen;

   if (target < min_sz)
      target = min_sz;
   if (pref_sz <= target || min_sz >= pref_sz)
      return pref_sz;

   if (gran > 0)
   {
      chosen = min_sz + ((target - min_sz + gran - 1) / gran) * gran;
   }
   else if (gran < 0)
   {
      chosen = min_sz;
      while (chosen < target && chosen < max_sz)
         chosen <<= 1;
   }
   else
      return pref_sz;

   if (chosen > pref_sz)
      chosen = pref_sz;
   if (chosen > max_sz)
      chosen = max_sz;
   if (chosen < min_sz)
      chosen = min_sz;
   return chosen;
}

static bool asio_create_buffers(ra_asio_t *ad, unsigned latency)
{
   long min_sz, max_sz, pref_sz, gran;
   long in_lat, out_lat;
   ASIOChannelInfo ch_info;

   if (ASIO_CALL_GET_BUFFER_SIZE(ad->iasio,
            &min_sz, &max_sz, &pref_sz, &gran) != ASE_OK)
   {
      RARCH_ERR("[ASIO] Failed to query buffer size.\n");
      return false;
   }
   RARCH_LOG("[ASIO] Buffer sizes: min=%ld, max=%ld, preferred=%ld, granularity=%ld\n",
         min_sz, max_sz, pref_sz, gran);
   ad->buffer_frames = asio_choose_period(ad->sample_rate, latency,
         min_sz, max_sz, pref_sz, gran);
   RARCH_LOG("[ASIO] Using buffer size: %ld frames (%.1f ms)%s.\n",
         ad->buffer_frames,
         (float)ad->buffer_frames * 1000.0f / ad->sample_rate,
         ad->buffer_frames == pref_sz ? ", the driver's preferred size"
               : ", below the driver's preferred size for the latency setting");

   /* Scratch for one period of interleaved float; sized to the period,
    * so made again when the period is. */
   free(ad->scratch);
   ad->scratch = (float *)malloc((size_t)ad->buffer_frames
         * 2 * sizeof(float));
   if (!ad->scratch)
   {
      RARCH_ERR("[ASIO] Failed to allocate deinterleave scratch.\n");
      return false;
   }

   /* The pair of outputs to play through: the setting names the first,
    * the second follows it. A device lists its outputs as numbered
    * mono channels, and on a multi-output interface the first two are
    * not always the ones the user is listening to - a digital pair
    * before the analog ones is common. A setting past the device's
    * outputs falls back to the first pair and says so. */
   {
      long left = (long)config_get_ptr()->uints.audio_asio_output_channel;
      if (left < 0 || left + 1 >= ad->out_channels)
      {
         if (left != 0)
            RARCH_WARN("[ASIO] Output channel setting %ld is past this device's %ld outputs; using the first pair.\n",
                  left, ad->out_channels);
         left = 0;
      }
      ad->out_left = left;
   }

   memset(&ch_info, 0, sizeof(ch_info));
   ch_info.channel  = ad->out_left;
   ch_info.isInput  = ASIOFalse;
   if (ASIO_CALL_GET_CHANNEL_INFO(ad->iasio, &ch_info) != ASE_OK)
   {
      RARCH_ERR("[ASIO] Failed to query channel info.\n");
      return false;
   }
   ad->sample_type = ch_info.type;
   if (!asio_convert_known(ad->sample_type))
      RARCH_ERR("[ASIO] Output sample type %ld is not one this driver converts; the device will be silent.\n",
            (long)ad->sample_type);
   RARCH_LOG("[ASIO] Output sample type: %ld (%s)\n",
         (long)ad->sample_type, ch_info.name);
   {
      ASIOChannelInfo right;
      memset(&right, 0, sizeof(right));
      right.channel = ad->out_left + 1;
      right.isInput = ASIOFalse;
      if (ASIO_CALL_GET_CHANNEL_INFO(ad->iasio, &right) != ASE_OK)
         strlcpy(right.name, "?", sizeof(right.name));
      RARCH_LOG("[ASIO] Playing through outputs %ld and %ld: \"%s\" and \"%s\".\n",
            ad->out_left + 1, ad->out_left + 2, ch_info.name, right.name);
   }

   memset(ad->buf_info, 0, sizeof(ad->buf_info));
   ad->buf_info[0].isInput    = ASIOFalse;
   ad->buf_info[0].channelNum = ad->out_left;
   ad->buf_info[1].isInput    = ASIOFalse;
   ad->buf_info[1].channelNum = ad->out_left + 1;

   /* Sized from two periods for now - the device's reported latency is
    * only known once buffers exist - and resized to it below, before
    * the stream starts. */
   ad->output_latency = 0;
   if (!asio_size_ring(ad, latency))
   {
      RARCH_ERR("[ASIO] Failed to create ring buffer.\n");
      return false;
   }

   if (ASIO_CALL_CREATE_BUFFERS(ad->iasio,
            ad->buf_info, 2, ad->buffer_frames,
            &g_asio_callbacks) != ASE_OK)
   {
      RARCH_ERR("[ASIO] Failed to create buffers.\n");
      return false;
   }
   ad->buffers_created = true;

   /* Probe ASIOOutputReady once, now that buffers exist and before the
    * latencies are read: a driver that honours it knows from this call
    * that the host will notify it, and reports the shorter output
    * latency that follows. */
   ad->output_ready_supported = (ASIO_CALL_OUTPUT_READY(ad->iasio) == ASE_OK);
   RARCH_LOG("[ASIO] ASIOOutputReady: %s.\n",
         ad->output_ready_supported ? "supported; the callback will notify each half" : "not supported");

   if (ASIO_CALL_GET_LATENCIES(ad->iasio, &in_lat, &out_lat) == ASE_OK)
   {
      RARCH_LOG("[ASIO] Latencies: input=%ld, output=%ld frames (%.1f ms).\n",
            in_lat, out_lat,
            (float)out_lat * 1000.0f / ad->sample_rate);
      ad->output_latency = out_lat;
   }

   /* The device stage is known now; size the ring to the setting less
    * it. No callback runs between ASIOCreateBuffers returning and
    * ASIOStart, so the ring is single-threaded here. */
   if (!asio_size_ring(ad, latency))
   {
      RARCH_ERR("[ASIO] Failed to size ring buffer.\n");
      return false;
   }
   asio_prime_ring(ad);
   asio_log_stages(ad, latency);
   return true;
}

static void *ra_asio_init(const char *device, unsigned rate,
      unsigned latency, unsigned block_frames, unsigned *new_rate)
{
   int i, num_drivers;
   asio_driver_entry_t drivers[ASIO_MAX_DRIVERS];
   const CLSID *use_clsid = NULL;
   char drv_name[64];
   char err_msg[128];
   long in_ch, out_ch;
   ASIOSampleRate current_rate;
   ra_asio_t *ad;

   if (g_asio)
   {
      RARCH_ERR("[ASIO] Already initialized (singleton).\n");
      return NULL;
   }

   /* Reclaim a parked instance from a previous free() call.
    * This avoids destroying and recreating the ASIO COM object,
    * which crashes ASIO4ALL whose audio thread doesn't terminate
    * synchronously with ASIOStop. */
   if (g_asio_persistent)
   {
      ra_asio_t *ad = g_asio_persistent;
      g_asio_persistent = NULL;

      RARCH_LOG("[ASIO] Reclaiming parked driver instance.\n");

      retro_atomic_int_init(&ad->shutdown, 0);
      retro_atomic_int_init(&ad->is_paused, 0);
      ad->nonblock  = false;

      /* The driver asked for a reset while it ran - its buffer size or
       * sample rate changed, from its control panel or otherwise - and
       * the frontend reinitialised audio for it. The buffers are
       * disposed and made again at whatever size it prefers now, in
       * place of ending audio for the session. */
      if (     retro_atomic_load_acquire_int(&ad->rebuild_pending)
            || (long)config_get_ptr()->uints.audio_asio_output_channel != ad->out_left)
      {
         RARCH_LOG("[ASIO] Rebuilding buffers: %s.\n",
               retro_atomic_load_acquire_int(&ad->rebuild_pending)
               ? "the driver asked for a reset" : "the output channels changed");
         retro_atomic_store_release_int(&ad->rebuild_pending, 0);
         asio_dispose_buffers(ad);
         g_asio = ad;
         if (!asio_create_buffers(ad, latency))
         {
            g_asio = NULL;
            g_asio_persistent = ad; /* Park it again */
            return NULL;
         }
         g_asio = NULL;
      }

      /* Update sample rate if the new core wants something different */
      if (rate != ad->sample_rate
            && ASIO_CALL_CAN_SAMPLE_RATE(ad->iasio, (ASIOSampleRate)rate) == ASE_OK)
      {
         ASIO_CALL_SET_SAMPLE_RATE(ad->iasio, (ASIOSampleRate)rate);
         ad->sample_rate = rate;
      }

      if (new_rate)
         *new_rate = ad->sample_rate;

      /* Discard any stale audio left over from the previous
       * session.  Safe here because the ASIO callback isn't
       * running yet (g_asio is still NULL until the next line),
       * so the SPSC is single-threaded at this point.  For the
       * same reason it is safe to recreate the ring outright when
       * the latency-derived size changed (audio settings changes
       * reinit the driver through free()/init(), which lands here
       * on the reuse path - without this, a latency change would
       * silently keep the old ring size). */
      if (!asio_size_ring(ad, latency))
      {
         RARCH_ERR("[ASIO] Failed to resize ring buffer.\n");
         g_asio_persistent = ad; /* Park it again */
         return NULL;
      }
      asio_log_stages(ad, latency);
      asio_prime_ring(ad);

      g_asio = ad;

      if (ASIO_CALL_START(ad->iasio) != ASE_OK)
      {
         RARCH_ERR("[ASIO] Failed to restart.\n");
         g_asio = NULL;
         g_asio_persistent = ad; /* Park it again */
         return NULL;
      }

      RARCH_LOG("[ASIO] Restarted successfully.\n");
      return ad;
   }

   ad = (ra_asio_t *)calloc(1, sizeof(ra_asio_t));
   if (!ad)
      return NULL;
   retro_atomic_size_init(&ad->underruns, 0);

   /* Register cleanup for process exit — ensures the parked
    * instance is properly torn down even if free() only parks it. */
   {
      static bool atexit_registered = false;
      if (!atexit_registered)
      {
         atexit(asio_atexit_cleanup);
         atexit_registered = true;
      }
   }

   /* Initialize COM — ASIO drivers are in-process COM servers.
    * Must use single-threaded apartment (STA) because most ASIO
    * drivers use COM marshaling or window messages to dispatch
    * bufferSwitch callbacks, which requires an STA message pump
    * on the calling thread. */
   if (SUCCEEDED(CoInitialize(NULL)))
      ad->com_initialized = true;

   /* Enumerate available ASIO drivers */
   num_drivers = asio_enumerate_drivers(drivers, ASIO_MAX_DRIVERS);
   if (num_drivers <= 0)
   {
      RARCH_ERR("[ASIO] No ASIO drivers found in registry.\n");
      goto error;
   }

   RARCH_LOG("[ASIO] Found %d driver(s):\n", num_drivers);
   for (i = 0; i < num_drivers; i++)
      RARCH_LOG("[ASIO]   %d: %s\n", i, drivers[i].name);

   /* Select driver — match by name or use first available */
   use_clsid = &drivers[0].clsid;
   if (device && *device)
   {
      for (i = 0; i < num_drivers; i++)
      {
         if (string_is_equal_noncase(drivers[i].name, device))
         {
            use_clsid = &drivers[i].clsid;
            RARCH_LOG("[ASIO] Selected driver: %s\n", drivers[i].name);
            break;
         }
      }
      if (i == num_drivers)
         RARCH_WARN("[ASIO] Driver '%s' not found, using '%s'.\n",
               device, drivers[0].name);
   }

   /* Load the COM object */
   ad->iasio = asio_load_driver(use_clsid);
   if (!ad->iasio)
   {
      RARCH_ERR("[ASIO] Failed to load ASIO driver COM object.\n");
      goto error;
   }

   /* Initialize the driver.
    * sysHandle must be the application's main window handle (HWND)
    * on Windows.  Many ASIO drivers (especially Realtek) use this
    * to create internal message-only windows for dispatching
    * bufferSwitch callbacks via the message pump.  Without a valid
    * HWND, the driver may initialize successfully but never issue
    * any callbacks. */
   {
      HWND hwnd = GetForegroundWindow();
      if (!hwnd)
         hwnd = GetDesktopWindow();
      if (!ASIO_CALL_INIT(ad->iasio, hwnd))
      {
         ASIO_CALL_GET_ERROR_MESSAGE(ad->iasio, err_msg);
         RARCH_ERR("[ASIO] Init failed: %s\n", err_msg);
         goto error;
      }
   }

   ASIO_CALL_GET_DRIVER_NAME(ad->iasio, drv_name);
   RARCH_LOG("[ASIO] Driver: %s (v%ld)\n",
         drv_name, ASIO_CALL_GET_DRIVER_VERSION(ad->iasio));

   /* Query channels */
   if (ASIO_CALL_GET_CHANNELS(ad->iasio, &in_ch, &out_ch) != ASE_OK
         || out_ch < 2)
   {
      RARCH_ERR("[ASIO] Need at least 2 output channels (have %ld).\n", out_ch);
      goto error;
   }
   RARCH_LOG("[ASIO] Channels: %ld in, %ld out.\n", in_ch, out_ch);
   ad->out_channels = out_ch;

   /* Set sample rate */
   if (ASIO_CALL_CAN_SAMPLE_RATE(ad->iasio, (ASIOSampleRate)rate) == ASE_OK)
   {
      ASIO_CALL_SET_SAMPLE_RATE(ad->iasio, (ASIOSampleRate)rate);
      ad->sample_rate = rate;
   }
   else
   {
      /* Use whatever the driver is currently set to */
      ASIO_CALL_GET_SAMPLE_RATE(ad->iasio, &current_rate);
      ad->sample_rate = (unsigned)current_rate;
      RARCH_WARN("[ASIO] Requested %u Hz not supported, using %.0f Hz.\n",
            rate, current_rate);
   }

   if (new_rate)
      *new_rate = ad->sample_rate;

   /* Query buffer size */
#ifdef HAVE_THREADS
   ad->cond      = scond_new();
   ad->cond_lock = slock_new();
   if (!ad->cond || !ad->cond_lock)
   {
      RARCH_ERR("[ASIO] Failed to create sync primitives.\n");
      goto error;
   }
#endif

   /* g_asio before the buffers: the driver may call back during
    * ASIOCreateBuffers. */
   g_asio = ad;
   if (!asio_create_buffers(ad, latency))
   {
      g_asio = NULL;
      goto error;
   }

   /* Start streaming — the driver will issue bufferSwitch callbacks
    * to prefill its output buffers.  The callback will output silence
    * from the empty ring buffer, which is correct. */
   if (ASIO_CALL_START(ad->iasio) != ASE_OK)
   {
      RARCH_ERR("[ASIO] Failed to start.\n");
      g_asio = NULL;
      goto error;
   }

   RARCH_LOG("[ASIO] Started successfully.\n");
   return ad;

error:
   if (ad->iasio)
   {
      if (ad->buffers_created)
         ASIO_CALL_DISPOSE_BUFFERS(ad->iasio);
      ASIO_CALL_RELEASE(ad->iasio);
   }
   if (ad->ring_initialized)
   {
      retro_spsc_free(&ad->ring);
      ad->ring_initialized = false;
   }
   if (ad->scratch)
   {
      free(ad->scratch);
      ad->scratch = NULL;
   }
#ifdef HAVE_THREADS
   if (ad->cond_lock)
      slock_free(ad->cond_lock);
   if (ad->cond)
      scond_free(ad->cond);
#endif
   if (ad->com_initialized)
      CoUninitialize();
   free(ad);
   return NULL;
}

/* How many period-long waits a blocked write or wait_writable() may
 * take before giving up on the callback making room. The callback
 * drains and signals every period while streaming; a driver that has
 * stopped calling it back - reset, device lost, stalled - never does,
 * and shutdown is not raised for that. */
#define ASIO_WAIT_LAPS 8

static ssize_t ra_asio_write(void *data, const void *buf, size_t len)
{
   ra_asio_t *ad      = (ra_asio_t *)data;
   const char *src    = (const char *)buf;
   size_t written     = 0;
   int64_t wait_us    = 1000;
   int laps           = ASIO_WAIT_LAPS;

   if (!ad || retro_atomic_load_acquire_int(&ad->shutdown))
      return -1;

   if (retro_atomic_load_acquire_int(&ad->latencies_changed))
   {
      long in_lat = 0, out_lat = 0;
      retro_atomic_store_release_int(&ad->latencies_changed, 0);
      if (ASIO_CALL_GET_LATENCIES(ad->iasio, &in_lat, &out_lat) == ASE_OK)
      {
         RARCH_LOG("[ASIO] Latencies now: input=%ld, output=%ld frames (%.1f ms); the ring is resized at the next reinit.\n",
               in_lat, out_lat, (float)out_lat * 1000.0f / ad->sample_rate);
         ad->output_latency = out_lat;
         audio_driver_set_device_latency(asio_device_frames(ad));
      }
   }

   /* Bound for the blocking wait below, one device period.  Both
    * operands are fixed for the lifetime of the COM object, so this is
    * loop-invariant. */
   if (ad->sample_rate)
   {
      wait_us = (int64_t)ad->buffer_frames * 1000000 / ad->sample_rate;
      if (wait_us < 1000)
         wait_us = 1000;
   }

   while (len > 0)
   {
      size_t avail, to_write;

      if (retro_atomic_load_acquire_int(&ad->shutdown))
         return -1;

      avail    = asio_ring_room(ad);
      to_write = (len < avail) ? len : avail;
      /* Align to frame boundary (stereo float = 8 bytes) */
      to_write = (to_write / 8) * 8;

      if (to_write > 0)
      {
         /* retro_spsc_write returns bytes actually written.  We've
          * already capped to_write by retro_spsc_write_avail above,
          * so the return value will equal to_write -- but use it
          * defensively in case the contract ever changes. */
         size_t actually_written =
            retro_spsc_write(&ad->ring, src, to_write);
         src     += actually_written;
         len     -= actually_written;
         written += actually_written;
      }
      else if (!ad->nonblock)
      {
         /* Paused, the callback zero-fills and returns before it drains
          * or signals: nothing here would ever be woken. The write
          * returns what went; the rest is the caller's to retry once
          * the stream is started. */
         if (retro_atomic_load_acquire_int(&ad->is_paused))
            break;
#ifdef HAVE_THREADS
         /* Timed, not indefinite.  The predicate here is the ring's
          * write_avail, which is lock-free by design - the consumer is
          * a real-time ASIO callback and must not take a lock - so
          * cond_lock cannot also guard the predicate and a signal
          * raised between the write_avail test above and this wait has
          * no waiter to reach.  Where a condition variable can lose a
          * wakeup by construction, the correct shape is a timed wait
          * inside a loop that rechecks, which is what the enclosing
          * while does: it retests ad->shutdown and write_avail on
          * every pass.
          *
          * This is also the only thing standing between a driver reset
          * and a hung emulator thread.  asio_cb_buffer_switch is the
          * sole routine that signals during streaming, and it returns
          * early - before signalling - once shutdown or is_paused is
          * set.  An untimed wait entered before that point was never
          * woken again. */
         slock_lock(ad->cond_lock);
         scond_wait_timeout(ad->cond, ad->cond_lock, wait_us);
         slock_unlock(ad->cond_lock);
#else
         Sleep(1);
#endif
         /* And bounded overall: a driver that has stopped calling back
          * without shutting down ends the write with what went. */
         if (--laps < 0)
            break;
      }
      else
         break;
   }

   return (ssize_t)written;
}

static bool ra_asio_stop(void *data)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (ad)
      retro_atomic_store_release_int(&ad->is_paused, 1);
   return true;
}

static bool ra_asio_start(void *data, bool u)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (ad)
      retro_atomic_store_release_int(&ad->is_paused, 0);
   return true;
}

static bool ra_asio_alive(void *data)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (!ad)
      return false;
   return    !retro_atomic_load_acquire_int(&ad->is_paused)
          && !retro_atomic_load_acquire_int(&ad->shutdown);
}

static void ra_asio_set_nonblock_state(void *data, bool state)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (ad)
      ad->nonblock = state;
}

static void ra_asio_free(void *data)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (!ad)
      return;

   retro_atomic_store_release_int(&ad->shutdown, 1);
   retro_atomic_store_release_int(&ad->is_paused, 0);

#ifdef HAVE_THREADS
   if (ad->cond)
      scond_signal(ad->cond);
#endif

   /* Park the instance for reuse on the next init() call.
    * ASIO4ALL's audio thread does not terminate synchronously
    * with ASIOStop, so destroying the COM object here causes
    * a crash when init() calls CoCreateInstance again.
    * Instead we keep the driver alive — ASIOStop halts
    * streaming but the COM object and buffers remain valid. */
   /* Detach from the callback - silence output while parked or gone. */
   g_asio = NULL;

   /* Leaving the driver, not restarting it: the audio driver setting
    * is written before the reinit that follows a change of driver, so
    * a setting that no longer says asio here means the next init will
    * be another driver's. Parking would hold the device - exclusive,
    * under ASIO - for the rest of the session while another driver
    * tries to use it; the instance is released instead. A core swap or
    * an audio setting change leaves the setting at asio, and parks. */
   if (!string_is_equal(config_get_ptr()->arrays.audio_driver, "asio"))
   {
      RARCH_LOG("[ASIO] Driver released: the audio driver setting is no longer asio.\n");
      asio_destroy(ad);
      return;
   }

   if (ad->iasio)
      ASIO_CALL_STOP(ad->iasio);

   /* No retro_spsc_clear here.  The pre-port fifo_clear at this
    * site was racy with stray ASIO callbacks that may still be
    * running after ASIO_CALL_STOP returns (some drivers, notably
    * ASIO4ALL, don't synchronously join their audio thread).
    * The restart path in ra_asio_init_via_persistent calls
    * retro_spsc_clear anyway, so any stale data will be flushed
    * before the next run -- after the callback is provably
    * stopped (g_asio == NULL gates it). */

   /* Store for reuse */
   g_asio_persistent = ad;

   RARCH_LOG("[ASIO] Driver parked for reuse.\n");
}

static bool ra_asio_use_float(void *data) { return true; }

/* Sleep on the condition the ASIO buffer-switch callback signals until
 * at least len bytes fit in the ring, capped at half of it so the wait
 * always ends; timed at one hardware buffer, as ra_asio_write() waits.
 * Returns the free space then, or 0 once the driver has shut down. */
static size_t ra_asio_wait_writable(void *data, size_t len)
{
   ra_asio_t *ad   = (ra_asio_t *)data;
   int64_t wait_us = 1000;
   size_t avail;
   int laps        = ASIO_WAIT_LAPS;

   if (!ad || !ad->ring_initialized)
      return 0;
   if (ad->sample_rate)
   {
      wait_us = (int64_t)ad->buffer_frames * 1000000 / ad->sample_rate;
      if (wait_us < 1000)
         wait_us = 1000;
   }
   if (len > ad->ring_size / 2)
      len = ad->ring_size / 2;

   for (;;)
   {
      /* Shut down or paused, the callback drains nothing and signals
       * nothing: no space is coming from this call. */
      if (     retro_atomic_load_acquire_int(&ad->shutdown)
            || retro_atomic_load_acquire_int(&ad->is_paused))
         return 0;
      avail = asio_ring_room(ad);
      if (avail >= len)
         return avail;
      /* No room after this many periods: the driver is not calling
       * back, and the pass is handed back rather than waited on. */
      if (--laps < 0)
         return 0;
#ifdef HAVE_THREADS
      slock_lock(ad->cond_lock);
      scond_wait_timeout(ad->cond, ad->cond_lock, wait_us);
      slock_unlock(ad->cond_lock);
#else
      /* Nothing to wait on without threads, and nothing calls this
       * without them either: the threaded pipeline is the only caller.
       * Report that no wait is possible rather than spin. */
      return 0;
#endif
   }
}

static size_t ra_asio_underruns(void *data)
{
   ra_asio_t *ad = (ra_asio_t*)data;
   return ad ? retro_atomic_load_acquire_size(&ad->underruns) : 0;
}

static size_t ra_asio_frames_consumed(void *data)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   return ad ? ad->consumed : 0;
}

static size_t ra_asio_write_avail(void *data)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (!ad || !ad->ring_initialized)
      return 0;
   return asio_ring_room(ad);
}

static size_t ra_asio_buffer_size(void *data)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (!ad)
      return 0;
   return ad->ring_size;
}

static void *ra_asio_device_list_new(void *data)
{
   int i, num;
   asio_driver_entry_t drivers[ASIO_MAX_DRIVERS];
   union string_list_elem_attr attr;
   bool com_init = false;
   struct string_list *sl = string_list_new();

   if (!sl)
      return NULL;

   if (SUCCEEDED(CoInitialize(NULL)))
      com_init = true;

   num = asio_enumerate_drivers(drivers, ASIO_MAX_DRIVERS);

   attr.i = 0;
   for (i = 0; i < num; i++)
      string_list_append(sl, drivers[i].name, attr);

   if (com_init)
      CoUninitialize();

   return sl;
}

static void ra_asio_device_list_free(void *data, void *slp)
{
   struct string_list *sl = (struct string_list *)slp;
   if (sl)
      string_list_free(sl);
}

audio_driver_t audio_asio = {
   ra_asio_init,
   ra_asio_write,
   ra_asio_stop,
   ra_asio_start,
   ra_asio_alive,
   ra_asio_set_nonblock_state,
   ra_asio_free,
   ra_asio_use_float,
   "asio",
   ra_asio_device_list_new,
   ra_asio_device_list_free,
   ra_asio_write_avail,
   ra_asio_buffer_size,
   NULL, /* write_raw — ASIO cannot dynamically adjust sample rate
         * for A/V sync rate control.  Software resampler handles it. */
   ra_asio_wait_writable,
   ra_asio_frames_consumed,
   ra_asio_underruns
};

/* Called from the menu to open the ASIO driver's control panel.
 * This allows the user to select the output device, configure
 * buffer sizes, and adjust driver-specific settings.  Essential
 * for drivers like ASIO4ALL that require the user to enable
 * specific audio endpoints before streaming can work. */
bool audio_asio_open_control_panel(void)
{
   ra_asio_t *ad = g_asio ? g_asio : g_asio_persistent;
   if (ad && ad->iasio)
   {
      RARCH_LOG("[ASIO] Opening driver control panel...\n");
      ASIO_CALL_CONTROL_PANEL(ad->iasio);
      RARCH_LOG("[ASIO] Control panel closed.\n");
      return true;
   }
   RARCH_WARN("[ASIO] Cannot open control panel: driver not initialized.\n");
   return false;
}

/* The device's name for output channel ch - "Analog 1", "SPDIF L" -
 * from the running or parked instance, for the menu to show beside the
 * output channel setting. False, and buf untouched, when no instance
 * exists or the channel does not. */
bool audio_asio_output_channel_name(unsigned ch, char *buf, size_t len)
{
   ra_asio_t *ad = g_asio ? g_asio : g_asio_persistent;
   ASIOChannelInfo info;
   if (!ad || !ad->iasio || (long)ch >= ad->out_channels)
      return false;
   memset(&info, 0, sizeof(info));
   info.channel = (long)ch;
   info.isInput = ASIOFalse;
   if (ASIO_CALL_GET_CHANNEL_INFO(ad->iasio, &info) != ASE_OK)
      return false;
   strlcpy(buf, info.name, len);
   return true;
}

/* The device's output count, or 0 with no instance. */
unsigned audio_asio_output_channel_count(void)
{
   ra_asio_t *ad = g_asio ? g_asio : g_asio_persistent;
   return (ad && ad->iasio && ad->out_channels > 0) ? (unsigned)ad->out_channels : 0;
}

#endif /* HAVE_ASIO */
