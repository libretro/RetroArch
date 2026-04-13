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
#include <queues/fifo_queue.h>

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

typedef long ASIOSampleType;
#define ASIOSTInt16MSB       0L
#define ASIOSTInt24MSB       1L
#define ASIOSTInt32MSB       2L
#define ASIOSTFloat32MSB     3L
#define ASIOSTFloat64MSB     4L
#define ASIOSTInt32MSB16     8L
#define ASIOSTInt32MSB18     9L
#define ASIOSTInt32MSB20    10L
#define ASIOSTInt32MSB24    11L
#define ASIOSTInt16LSB      16L
#define ASIOSTInt24LSB      17L
#define ASIOSTInt32LSB      18L
#define ASIOSTFloat32LSB    19L
#define ASIOSTFloat64LSB    20L
#define ASIOSTInt32LSB16    24L
#define ASIOSTInt32LSB18    25L
#define ASIOSTInt32LSB20    26L
#define ASIOSTInt32LSB24    27L

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
#define ASIO_RING_MULT       4

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
   HRESULT hr  = CoCreateInstance(clsid, NULL,
         CLSCTX_INPROC_SERVER, clsid, &iface);
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
   fifo_buffer_t     *ring;         /* Ring buffer between write() and callback */
#ifdef HAVE_THREADS
   scond_t           *cond;
   slock_t           *cond_lock;
#endif
   ASIOBufferInfo     buf_info[2];  /* L and R output channels */
   ASIOSampleType     sample_type;
   long               buffer_frames;
   size_t             ring_size;
   unsigned           sample_rate;
   volatile bool      running;
   volatile bool      shutdown;
   bool               nonblock;
   bool               is_paused;
   bool               com_initialized;
   bool               buffers_created;
} ra_asio_t;

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

static size_t asio_bytes_per_sample(ASIOSampleType type)
{
   switch (type)
   {
      case ASIOSTInt16LSB:
      case ASIOSTInt16MSB:
         return 2;
      case ASIOSTInt24LSB:
      case ASIOSTInt24MSB:
         return 3;
      case ASIOSTInt32LSB:
      case ASIOSTInt32MSB:
      case ASIOSTInt32LSB16:
      case ASIOSTInt32LSB18:
      case ASIOSTInt32LSB20:
      case ASIOSTInt32LSB24:
      case ASIOSTInt32MSB16:
      case ASIOSTInt32MSB18:
      case ASIOSTInt32MSB20:
      case ASIOSTInt32MSB24:
      case ASIOSTFloat32LSB:
      case ASIOSTFloat32MSB:
         return 4;
      case ASIOSTFloat64LSB:
      case ASIOSTFloat64MSB:
         return 8;
      default:
         return 4;
   }
}

static void asio_deinterleave_to_buffers(ra_asio_t *ad,
      long index, long frames)
{
   long i;
   void *buf_l  = ad->buf_info[0].buffers[index];
   void *buf_r  = ad->buf_info[1].buffers[index];
   size_t avail = FIFO_READ_AVAIL(ad->ring);
   long have    = (long)(avail / (2 * sizeof(float)));

   if (have > frames)
      have = frames;

   switch (ad->sample_type)
   {
      case ASIOSTFloat32LSB:
      {
         float *dl = (float *)buf_l;
         float *dr = (float *)buf_r;
         float tmp[2];
         for (i = 0; i < have; i++)
         {
            fifo_read(ad->ring, tmp, sizeof(tmp));
            dl[i] = tmp[0];
            dr[i] = tmp[1];
         }
         for (; i < frames; i++) { dl[i] = 0.0f; dr[i] = 0.0f; }
         break;
      }

      case ASIOSTFloat64LSB:
      {
         double *dl = (double *)buf_l;
         double *dr = (double *)buf_r;
         float tmp[2];
         for (i = 0; i < have; i++)
         {
            fifo_read(ad->ring, tmp, sizeof(tmp));
            dl[i] = (double)tmp[0];
            dr[i] = (double)tmp[1];
         }
         for (; i < frames; i++) { dl[i] = 0.0; dr[i] = 0.0; }
         break;
      }

      case ASIOSTInt32LSB:
      {
         int32_t *dl = (int32_t *)buf_l;
         int32_t *dr = (int32_t *)buf_r;
         float tmp[2];
         for (i = 0; i < have; i++)
         {
            fifo_read(ad->ring, tmp, sizeof(tmp));
            dl[i] = (int32_t)((double)tmp[0] * 2147483647.0);
            dr[i] = (int32_t)((double)tmp[1] * 2147483647.0);
         }
         for (; i < frames; i++) { dl[i] = 0; dr[i] = 0; }
         break;
      }

      case ASIOSTInt24LSB:
      {
         char *dl = (char *)buf_l;
         char *dr = (char *)buf_r;
         float tmp[2];
         for (i = 0; i < have; i++)
         {
            fifo_read(ad->ring, tmp, sizeof(tmp));
            int32_t l = (int32_t)(tmp[0] * 8388607.0f);
            int32_t r = (int32_t)(tmp[1] * 8388607.0f);
            l = l >  8388607 ?  8388607 : (l < -8388608 ? -8388608 : l);
            r = r >  8388607 ?  8388607 : (r < -8388608 ? -8388608 : r);
            dl[i*3+0]=(char)(l&0xFF); dl[i*3+1]=(char)((l>>8)&0xFF); dl[i*3+2]=(char)((l>>16)&0xFF);
            dr[i*3+0]=(char)(r&0xFF); dr[i*3+1]=(char)((r>>8)&0xFF); dr[i*3+2]=(char)((r>>16)&0xFF);
         }
         for (; i < frames; i++)
         {
            dl[i*3+0]=dl[i*3+1]=dl[i*3+2]=0;
            dr[i*3+0]=dr[i*3+1]=dr[i*3+2]=0;
         }
         break;
      }

      case ASIOSTInt16LSB:
      {
         int16_t *dl = (int16_t *)buf_l;
         int16_t *dr = (int16_t *)buf_r;
         float tmp[2];
         for (i = 0; i < have; i++)
         {
            fifo_read(ad->ring, tmp, sizeof(tmp));
            int32_t l = (int32_t)(tmp[0] * 32767.0f);
            int32_t r = (int32_t)(tmp[1] * 32767.0f);
            dl[i] = (int16_t)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
            dr[i] = (int16_t)(r > 32767 ? 32767 : (r < -32768 ? -32768 : r));
         }
         for (; i < frames; i++) { dl[i] = 0; dr[i] = 0; }
         break;
      }

      default:
      {
         size_t sz = frames * asio_bytes_per_sample(ad->sample_type);
         memset(buf_l, 0, sz);
         memset(buf_r, 0, sz);
         break;
      }
   }
}

/* ═══════════════════════════════════════════════════════════════════
 *  ASIO callbacks
 * ═══════════════════════════════════════════════════════════════════ */

static void asio_cb_buffer_switch(long index,
      ASIOBool direct_process)
{
   ra_asio_t *ad = g_asio;

   if (!ad || !ad->ring || ad->is_paused || ad->shutdown)
   {
      if (ad && ad->buf_info[0].buffers[index])
      {
         size_t bsz = ad->buffer_frames
               * asio_bytes_per_sample(ad->sample_type);
         memset(ad->buf_info[0].buffers[index], 0, bsz);
         memset(ad->buf_info[1].buffers[index], 0, bsz);
      }
      return;
   }

   asio_deinterleave_to_buffers(ad, index, ad->buffer_frames);

#ifdef HAVE_THREADS
   if (ad->cond)
      scond_signal(ad->cond);
#endif

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
               return 1L;
         }
         return 0L;
      case kAsioEngineVersion:
         return 2L;
      case kAsioResetRequest:
         RARCH_WARN("[ASIO] Driver requests reset.\n");
         if (g_asio)
            g_asio->shutdown = true;
         return 1L;
      case kAsioBufferSizeChange:
         RARCH_WARN("[ASIO] Buffer size change requested.\n");
         if (g_asio)
            g_asio->shutdown = true;
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
static void asio_atexit_cleanup(void)
{
   ra_asio_t *ad = g_asio_persistent;
   if (!ad)
      ad = g_asio;
   if (!ad)
      return;

   g_asio            = NULL;
   g_asio_persistent = NULL;

   if (ad->iasio)
   {
      ASIO_CALL_STOP(ad->iasio);
      if (ad->buffers_created)
         ASIO_CALL_DISPOSE_BUFFERS(ad->iasio);
      ASIO_CALL_RELEASE(ad->iasio);
   }

   if (ad->ring)
      fifo_free(ad->ring);

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

/* ═══════════════════════════════════════════════════════════════════
 *  RetroArch audio_driver_t implementation
 * ═══════════════════════════════════════════════════════════════════ */

static void *ra_asio_init(const char *device, unsigned rate,
      unsigned latency, unsigned block_frames, unsigned *new_rate)
{
   int i, num_drivers;
   asio_driver_entry_t drivers[ASIO_MAX_DRIVERS];
   const CLSID *use_clsid = NULL;
   char drv_name[64];
   char err_msg[128];
   long in_ch, out_ch;
   long min_sz, max_sz, pref_sz, gran;
   long in_lat, out_lat;
   ASIOChannelInfo ch_info;
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

      ad->shutdown  = false;
      ad->is_paused = false;
      ad->nonblock  = false;

      /* Update sample rate if the new core wants something different */
      if (rate != ad->sample_rate
            && ASIO_CALL_CAN_SAMPLE_RATE(ad->iasio, (ASIOSampleRate)rate) == ASE_OK)
      {
         ASIO_CALL_SET_SAMPLE_RATE(ad->iasio, (ASIOSampleRate)rate);
         ad->sample_rate = rate;
      }

      if (new_rate)
         *new_rate = ad->sample_rate;

      fifo_clear(ad->ring);

      g_asio = ad;
      ad->running = true;

      if (ASIO_CALL_START(ad->iasio) != ASE_OK)
      {
         RARCH_ERR("[ASIO] Failed to restart.\n");
         ad->running = false;
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
   if (ASIO_CALL_GET_BUFFER_SIZE(ad->iasio,
            &min_sz, &max_sz, &pref_sz, &gran) != ASE_OK)
   {
      RARCH_ERR("[ASIO] Failed to query buffer size.\n");
      goto error;
   }

   RARCH_LOG("[ASIO] Buffer sizes: min=%ld, max=%ld, preferred=%ld, granularity=%ld\n",
         min_sz, max_sz, pref_sz, gran);

   ad->buffer_frames = pref_sz;
   RARCH_LOG("[ASIO] Using buffer size: %ld frames (%.1f ms).\n",
         ad->buffer_frames,
         (float)ad->buffer_frames * 1000.0f / ad->sample_rate);

   /* Query output channel sample type */
   memset(&ch_info, 0, sizeof(ch_info));
   ch_info.channel  = 0;
   ch_info.isInput  = ASIOFalse;
   if (ASIO_CALL_GET_CHANNEL_INFO(ad->iasio, &ch_info) != ASE_OK)
   {
      RARCH_ERR("[ASIO] Failed to query channel info.\n");
      goto error;
   }
   ad->sample_type = ch_info.type;
   RARCH_LOG("[ASIO] Output sample type: %ld (%s)\n",
         (long)ad->sample_type, ch_info.name);

   /* Prepare buffer descriptors — stereo output only */
   memset(ad->buf_info, 0, sizeof(ad->buf_info));
   ad->buf_info[0].isInput    = ASIOFalse;
   ad->buf_info[0].channelNum = 0;
   ad->buf_info[1].isInput    = ASIOFalse;
   ad->buf_info[1].channelNum = 1;

   /* Create ring buffer BEFORE ASIO buffers — the driver may issue
    * a bufferSwitch callback during ASIOCreateBuffers, and the
    * callback needs the ring buffer to exist (even if empty). */
   ad->ring_size = pref_sz * 2 * sizeof(float) * ASIO_RING_MULT;
   ad->ring      = fifo_new(ad->ring_size);
   if (!ad->ring)
   {
      RARCH_ERR("[ASIO] Failed to create ring buffer.\n");
      goto error;
   }

#ifdef HAVE_THREADS
   ad->cond      = scond_new();
   ad->cond_lock = slock_new();
   if (!ad->cond || !ad->cond_lock)
   {
      RARCH_ERR("[ASIO] Failed to create sync primitives.\n");
      goto error;
   }
#endif

   /* Set global pointer BEFORE creating ASIO buffers — the driver
    * may call bufferSwitch during ASIOCreateBuffers or ASIOStart,
    * and the callback needs g_asio to be valid. */
   g_asio = ad;

   /* Create ASIO buffers */
   if (ASIO_CALL_CREATE_BUFFERS(ad->iasio,
            ad->buf_info, 2, ad->buffer_frames,
            &g_asio_callbacks) != ASE_OK)
   {
      RARCH_ERR("[ASIO] Failed to create buffers.\n");
      g_asio = NULL;
      goto error;
   }
   ad->buffers_created = true;

   /* Query latencies */
   if (ASIO_CALL_GET_LATENCIES(ad->iasio, &in_lat, &out_lat) == ASE_OK)
      RARCH_LOG("[ASIO] Latencies: input=%ld, output=%ld frames (%.1f ms).\n",
            in_lat, out_lat,
            (float)out_lat * 1000.0f / ad->sample_rate);

   /* Start streaming — the driver will issue bufferSwitch callbacks
    * to prefill its output buffers.  The callback will output silence
    * from the empty ring buffer, which is correct. */
   ad->running = true;

   if (ASIO_CALL_START(ad->iasio) != ASE_OK)
   {
      RARCH_ERR("[ASIO] Failed to start.\n");
      ad->running = false;
      g_asio = NULL;
      goto error;
   }

   /* Check if driver supports ASIOOutputReady optimization */
   {
      ASIOError or_err = ASIO_CALL_OUTPUT_READY(ad->iasio);
      RARCH_LOG("[ASIO] ASIOOutputReady: %s.\n",
            or_err == ASE_OK ? "supported" : "not supported");
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
   if (ad->ring)
      fifo_free(ad->ring);
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

static ssize_t ra_asio_write(void *data, const void *buf, size_t len)
{
   ra_asio_t *ad      = (ra_asio_t *)data;
   const char *src    = (const char *)buf;
   size_t written     = 0;

   if (!ad || ad->shutdown)
      return -1;

   while (len > 0)
   {
      size_t avail, to_write;

      if (ad->shutdown)
         return -1;

      avail    = FIFO_WRITE_AVAIL(ad->ring);
      to_write = (len < avail) ? len : avail;
      /* Align to frame boundary (stereo float = 8 bytes) */
      to_write = (to_write / 8) * 8;

      if (to_write > 0)
      {
         fifo_write(ad->ring, src, to_write);
         src     += to_write;
         len     -= to_write;
         written += to_write;
      }
      else if (!ad->nonblock)
      {
#ifdef HAVE_THREADS
         slock_lock(ad->cond_lock);
         scond_wait(ad->cond, ad->cond_lock);
         slock_unlock(ad->cond_lock);
#else
         Sleep(1);
#endif
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
      ad->is_paused = true;
   return true;
}

static bool ra_asio_start(void *data, bool u)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (ad)
      ad->is_paused = false;
   return true;
}

static bool ra_asio_alive(void *data)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (!ad)
      return false;
   return !ad->is_paused && !ad->shutdown;
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

   ad->shutdown  = true;
   ad->running   = false;
   ad->is_paused = false;

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
   if (ad->iasio)
      ASIO_CALL_STOP(ad->iasio);

   /* Detach from the callback — silence output while parked */
   g_asio = NULL;

   /* Flush stale audio */
   if (ad->ring)
      fifo_clear(ad->ring);

   /* Store for reuse */
   g_asio_persistent = ad;

   RARCH_LOG("[ASIO] Driver parked for reuse.\n");
}

static bool ra_asio_use_float(void *data) { return true; }

static size_t ra_asio_write_avail(void *data)
{
   ra_asio_t *ad = (ra_asio_t *)data;
   if (!ad || !ad->ring)
      return 0;
   return FIFO_WRITE_AVAIL(ad->ring);
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
   NULL /* write_raw — ASIO cannot dynamically adjust sample rate
         * for A/V sync rate control.  Software resampler handles it. */
};

/* Called from the menu to open the ASIO driver's control panel.
 * This allows the user to select the output device, configure
 * buffer sizes, and adjust driver-specific settings.  Essential
 * for drivers like ASIO4ALL that require the user to enable
 * specific audio endpoints before streaming can work. */
void audio_asio_open_control_panel(void)
{
   ra_asio_t *ad = g_asio ? g_asio : g_asio_persistent;
   if (ad && ad->iasio)
   {
      RARCH_LOG("[ASIO] Opening driver control panel...\n");
      ASIO_CALL_CONTROL_PANEL(ad->iasio);
      RARCH_LOG("[ASIO] Control panel closed.\n");
   }
   else
      RARCH_WARN("[ASIO] Cannot open control panel — driver not initialized.\n");
}

#endif /* HAVE_ASIO */
