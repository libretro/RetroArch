/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Chris Moeller
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

/* One implementation for every Apple toolchain back to Xcode 3.1 on
 * Tiger and Leopard, PowerPC included, with nothing gated on the SDK.
 * The ring between the writer and the render callback is lock-free
 * on retro_atomic, which has a backend for each of those (C11,
 * clang/GCC builtins, or OSAtomic on the oldest); the resampler is an
 * AudioConverter, in AudioToolbox, which every one of those SDKs ships
 * and the Makefile links. The writer waits
 * between callbacks on a Mach semaphore, in the kernel since 10.0:
 * signal is lock-free and safe from the real-time render thread, and
 * the wait is timed. dispatch_semaphore, which needed a 10.7 SDK and
 * a second copy of the driver for the toolchains without it, is a
 * userspace counter over exactly this primitive; at one signal per
 * callback the counter saves nothing worth a gate. */
#include <AvailabilityMacros.h>
/* TargetConditionals defines every TARGET_OS_* macro to 0 or 1, so the
 * platform split is always #if TARGET_OS_IPHONE / #if !TARGET_OS_IPHONE
 * and never #ifdef, which on a macOS SDK sees the macro defined as 0
 * and takes the iOS branch. */
#include <TargetConditionals.h>
#include <lists/string_list.h>

#include <stdlib.h>
#include <math.h>

#include <boolean.h>
#include <retro_atomic.h>

#if TARGET_OS_IPHONE
#include <AudioToolbox/AudioToolbox.h>
#else
#include <CoreAudio/CoreAudio.h>
#endif
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AUComponent.h>

#include <retro_endianness.h>
#include <string/stdstring.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <dlfcn.h>

/* --- Runtime resolution of the component API ------------------------
 *
 * The output unit is opened through AudioComponentFindNext /
 * AudioComponentInstanceNew / AudioComponentInstanceDispose on 10.6
 * and later, and through the Component Manager's FindNextComponent /
 * OpenAComponent / CloseComponent before that. Which one a binary
 * uses was a compile-time choice on the build SDK, which baked the
 * build machine into what the binary could do. Both triples are
 * resolved once at runtime instead: the modern one if the process
 * has it, the old one otherwise, and no header from either side is
 * needed since the description structs share a layout of five 32-bit
 * words and every handle is a pointer. dlsym is in 10.4. */
typedef struct
{
   UInt32 type, subtype, manufacturer, flags, mask;
} ca_component_desc_t;

typedef void *(*ca_find_next_t)(void *after, const ca_component_desc_t *desc);
typedef OSStatus (*ca_open_t)(void *component, AudioUnit *unit);
typedef OSStatus (*ca_close_t)(AudioUnit unit);

static struct
{
   ca_find_next_t find_next;
   ca_open_t      open;
   ca_close_t     close;
   bool           resolved;
   bool           modern;
} ca_cm;

static bool ca_cm_resolve(void)
{
   if (ca_cm.resolved)
      return ca_cm.find_next != NULL;
   ca_cm.resolved  = true;
   ca_cm.find_next = (ca_find_next_t)dlsym(RTLD_DEFAULT, "AudioComponentFindNext");
   ca_cm.open      = (ca_open_t)dlsym(RTLD_DEFAULT, "AudioComponentInstanceNew");
   ca_cm.close     = (ca_close_t)dlsym(RTLD_DEFAULT, "AudioComponentInstanceDispose");
   ca_cm.modern    = ca_cm.find_next && ca_cm.open && ca_cm.close;
   if (!ca_cm.modern)
   {
      ca_cm.find_next = (ca_find_next_t)dlsym(RTLD_DEFAULT, "FindNextComponent");
      ca_cm.open      = (ca_open_t)dlsym(RTLD_DEFAULT, "OpenAComponent");
      ca_cm.close     = (ca_close_t)dlsym(RTLD_DEFAULT, "CloseComponent");
   }
   if (!(ca_cm.find_next && ca_cm.open && ca_cm.close))
   {
      ca_cm.find_next = NULL;
      return false;
   }
   return true;
}

#if !TARGET_OS_IPHONE
/* macOS only: the HAL's device and stream objects, the property
 * scopes and the hardware error code belong to the desktop CoreAudio
 * the phone platforms do not carry. iOS and tvOS drive the unit
 * directly and never ask an AudioObject for anything. */
/* Property reads, resolved the same way and for the same reason as
 * the Component Manager calls above.
 *
 * AudioObjectGetPropertyData() and its neighbours arrived in 10.5.
 * Before that the HAL had a call per kind of object -
 * AudioHardwareGetProperty() for the system, AudioDeviceGetProperty()
 * for a device, AudioStreamGetProperty() for a stream - which are
 * still there, deprecated, in every SDK since. So both sets exist in
 * the one binary and which is used is decided when the process
 * starts, not when it is built: a build on a current SDK still runs
 * on 10.4, and a build on a 10.4 SDK still uses the newer calls when
 * it finds them.
 *
 * The old calls take an element and an is-input flag where the new
 * ones take an address, so the scope becomes that flag; every read
 * here is on an output or a global scope but the microphone half
 * asks for input, and that is the only thing the mapping has to get
 * right.
 *
 * The names are looked up as strings, so neither set needs to be
 * declared by the SDK in hand - which is what lets a 10.4 SDK, where
 * AudioObjectGetPropertyData is not declared at all, still resolve it
 * at runtime on a newer system. */
typedef UInt32 ca_obj_id_t;

typedef struct
{
   UInt32 mSelector;
   UInt32 mScope;
   UInt32 mElement;
} ca_addr_t;

typedef OSStatus (*ca_obj_get_t)(ca_obj_id_t, const ca_addr_t*,
      UInt32, const void*, UInt32*, void*);
typedef OSStatus (*ca_obj_size_t)(ca_obj_id_t, const ca_addr_t*,
      UInt32, const void*, UInt32*);
typedef Boolean  (*ca_obj_has_t)(ca_obj_id_t, const ca_addr_t*);
/* The pre-10.5 calls, by object kind. */
typedef OSStatus (*ca_hw_get_t)(UInt32, UInt32*, void*);
typedef OSStatus (*ca_hw_info_t)(UInt32, UInt32*, Boolean*);
typedef OSStatus (*ca_dev_get_t)(ca_obj_id_t, UInt32, Boolean, UInt32, UInt32*, void*);
typedef OSStatus (*ca_dev_info_t)(ca_obj_id_t, UInt32, Boolean, UInt32, UInt32*, Boolean*);
typedef OSStatus (*ca_str_get_t)(ca_obj_id_t, UInt32, UInt32, UInt32*, void*);
typedef OSStatus (*ca_str_info_t)(ca_obj_id_t, UInt32, UInt32, UInt32*, Boolean*);
/* Property listeners, which also come in the two shapes: the newer
 * one is handed the object and the addresses that changed, the older
 * one only the selector. */
typedef OSStatus (*ca_obj_listener_t)(ca_obj_id_t, UInt32, const ca_addr_t*, void*);
typedef OSStatus (*ca_hw_listener_t)(UInt32, void*);
typedef OSStatus (*ca_obj_addlis_t)(ca_obj_id_t, const ca_addr_t*, ca_obj_listener_t, void*);
typedef OSStatus (*ca_obj_remlis_t)(ca_obj_id_t, const ca_addr_t*, ca_obj_listener_t, void*);
typedef OSStatus (*ca_hw_addlis_t)(UInt32, ca_hw_listener_t, void*);
typedef OSStatus (*ca_hw_remlis_t)(UInt32, ca_hw_listener_t, void*);

static struct
{
   bool          resolved;
   ca_obj_get_t  obj_get;
   ca_obj_size_t obj_size;
   ca_obj_has_t  obj_has;
   ca_hw_get_t   hw_get;
   ca_hw_info_t  hw_info;
   ca_dev_get_t  dev_get;
   ca_dev_info_t dev_info;
   ca_str_get_t  str_get;
   ca_str_info_t str_info;
   ca_obj_addlis_t obj_addlis;
   ca_obj_remlis_t obj_remlis;
   ca_hw_addlis_t  hw_addlis;
   ca_hw_remlis_t  hw_remlis;
} ca_prop;

#define CA_SYSTEM_OBJECT 1u   /* kAudioObjectSystemObject */

static void ca_prop_resolve(void)
{
   if (ca_prop.resolved)
      return;
   ca_prop.resolved = true;
   ca_prop.obj_get  = (ca_obj_get_t) dlsym(RTLD_DEFAULT, "AudioObjectGetPropertyData");
   ca_prop.obj_size = (ca_obj_size_t)dlsym(RTLD_DEFAULT, "AudioObjectGetPropertyDataSize");
   ca_prop.obj_has  = (ca_obj_has_t) dlsym(RTLD_DEFAULT, "AudioObjectHasProperty");
   ca_prop.hw_get   = (ca_hw_get_t)  dlsym(RTLD_DEFAULT, "AudioHardwareGetProperty");
   ca_prop.hw_info  = (ca_hw_info_t) dlsym(RTLD_DEFAULT, "AudioHardwareGetPropertyInfo");
   ca_prop.dev_get  = (ca_dev_get_t) dlsym(RTLD_DEFAULT, "AudioDeviceGetProperty");
   ca_prop.dev_info = (ca_dev_info_t)dlsym(RTLD_DEFAULT, "AudioDeviceGetPropertyInfo");
   ca_prop.str_get  = (ca_str_get_t) dlsym(RTLD_DEFAULT, "AudioStreamGetProperty");
   ca_prop.str_info = (ca_str_info_t)dlsym(RTLD_DEFAULT, "AudioStreamGetPropertyInfo");
   ca_prop.obj_addlis = (ca_obj_addlis_t)dlsym(RTLD_DEFAULT, "AudioObjectAddPropertyListener");
   ca_prop.obj_remlis = (ca_obj_remlis_t)dlsym(RTLD_DEFAULT, "AudioObjectRemovePropertyListener");
   ca_prop.hw_addlis  = (ca_hw_addlis_t) dlsym(RTLD_DEFAULT, "AudioHardwareAddPropertyListener");
   ca_prop.hw_remlis  = (ca_hw_remlis_t) dlsym(RTLD_DEFAULT, "AudioHardwareRemovePropertyListener");
}

/* An input scope means the is-input flag the old calls take. */
static Boolean ca_scope_is_input(UInt32 scope)
{
   return scope == kAudioDevicePropertyScopeInput;
}

static OSStatus ca_prop_get(ca_obj_id_t id, const ca_addr_t *addr,
      UInt32 *size, void *data, bool is_stream)
{
   ca_prop_resolve();
   if (ca_prop.obj_get)
      return ca_prop.obj_get(id, addr, 0, NULL, size, data);
   if (id == CA_SYSTEM_OBJECT)
      return ca_prop.hw_get ? ca_prop.hw_get(addr->mSelector, size, data)
            : kAudioHardwareUnspecifiedError;
   if (is_stream)
      return ca_prop.str_get ? ca_prop.str_get(id, addr->mElement,
            addr->mSelector, size, data) : kAudioHardwareUnspecifiedError;
   return ca_prop.dev_get ? ca_prop.dev_get(id, addr->mElement,
         ca_scope_is_input(addr->mScope), addr->mSelector, size, data)
         : kAudioHardwareUnspecifiedError;
}

static OSStatus ca_prop_size(ca_obj_id_t id, const ca_addr_t *addr,
      UInt32 *size, bool is_stream)
{
   ca_prop_resolve();
   if (ca_prop.obj_size)
      return ca_prop.obj_size(id, addr, 0, NULL, size);
   if (id == CA_SYSTEM_OBJECT)
      return ca_prop.hw_info ? ca_prop.hw_info(addr->mSelector, size, NULL)
            : kAudioHardwareUnspecifiedError;
   if (is_stream)
      return ca_prop.str_info ? ca_prop.str_info(id, addr->mElement,
            addr->mSelector, size, NULL) : kAudioHardwareUnspecifiedError;
   return ca_prop.dev_info ? ca_prop.dev_info(id, addr->mElement,
         ca_scope_is_input(addr->mScope), addr->mSelector, size, NULL)
         : kAudioHardwareUnspecifiedError;
}

/* Whether the object carries the property at all. The old calls have
 * no such question, so asking for its size is the question. */
static bool ca_prop_has(ca_obj_id_t id, const ca_addr_t *addr, bool is_stream)
{
   UInt32 size = 0;
   ca_prop_resolve();
   if (ca_prop.obj_has)
      return ca_prop.obj_has(id, addr) != 0;
   return ca_prop_size(id, addr, &size, is_stream) == noErr && size > 0;
}

#endif /* !TARGET_OS_IPHONE */

/* The constants the AudioObject era brought with it, spelled here
 * rather than taken from the SDK. A 10.4 SDK declares none of them,
 * and they are enumerators rather than macros, so there is nothing to
 * test with #ifndef - the only way to name them on every SDK is to
 * name them ourselves. Their values are fixed by the ABI the HAL
 * speaks, which is what both sets of calls see. */
#define CA_OBJECT_UNKNOWN 0u                  /* kAudioObjectUnknown */
#define CA_SCOPE_GLOBAL   0x676C6F62u         /* 'glob', kAudioObjectPropertyScopeGlobal */
/* kAudioDevicePropertyUsesVariableBufferFrameSizes. Where a device
 * carries it, its value is the largest buffer the IOProc may be passed
 * and kAudioDevicePropertyBufferFrameSize is only the smallest. */
#define CA_PROP_VARIABLE_BUFFER_FRAMES 0x76626673u /* 'vbfs' */

/* kAudioObjectPropertyElementMaster was renamed ElementMain in 12.0;
 * both are 0, and the number is what the HAL sees. */
#define CA_ELEMENT_MAIN 0

/* AudioConverter lives in AudioToolbox, which the common includes above
 * pull in only on iOS; on macOS they pull in CoreAudio, which does not
 * declare it. */
#include <AudioToolbox/AudioToolbox.h>


typedef struct coreaudio
{
   /* What the writer waits on between render callbacks; see
    * coreaudio_signal() and coreaudio_wait(). */
   semaphore_t sema;
   bool        sema_alive;
   /* Writers currently inside coreaudio_wait(); the callback signals
    * only while this is non-zero. */
   retro_atomic_int_t waiters;

   /* Lock-free ring buffer */
   float *buffer;
   size_t capacity;           /* Power of 2 for fast masking */
   /* Samples the ring is allowed to hold: the setting, which the
    * power-of-two capacity is only the container for. Free space,
    * buffer_size() and the wait are counted against this. */
   size_t usable;
   size_t write_ptr;          /* Only touched by main thread */
   size_t read_ptr;           /* Only touched by audio callback */
   retro_atomic_size_t filled; /* Samples currently in buffer */
   /* Samples the device has asked the render callback for, since the
    * unit started. Written only by the callback, read by the frontend
    * through coreaudio_frames_consumed(). */
   retro_atomic_size_t consumed;
   /* Pulls the callback could not fill from the ring: one atomic add
    * on that path, read by the frontend's overlay, and by it once at
    * teardown for the log. Never logged from here. */
   retro_atomic_size_t underruns;

   /* The largest number_frames the callback may be handed, as the
    * device and the unit describe themselves, and the largest it was
    * actually handed. The two are separate on purpose: the first is
    * what the ring is sized against before a note has played, the
    * second is what says at teardown whether that was right. A pull
    * larger than the ring can hold is a callback that plays what is
    * there and pads the rest with silence, every time - which is a
    * buzz at the pull rate over a signal that is mostly gaps. */
   size_t              max_pull_frames;
   /* The pull the unit is expected to make, on every platform: what the
    * ring needs before starting is worth. Zero until known, which makes
    * coreaudio_run() start on the first frame written. */
   size_t              period_pull;
   retro_atomic_size_t max_pull_observed;
   retro_atomic_size_t oversized_pulls;
   /* Render callbacks handed a buffer list that is not the single
    * interleaved buffer the stream format asked for. */
   retro_atomic_size_t format_errors;

   /* The worst a callback came up short by, in samples, and how full
    * the ring was that time. Written by the render thread with two
    * atomic stores and read once at teardown - never logged from
    * there. A buzz is a callback finding less than a period and
    * padding the rest with silence, so these two numbers say whether
    * that is what is happening and by how much, which "it buzzes"
    * cannot. Out here rather than beside period_frames, which is the
    * HAL's and macOS-only: the callback and the teardown log read
    * these on every Apple platform, so declaring them under
    * !TARGET_OS_IPHONE left the iOS and tvOS builds referring to
    * members that were not there. */
   retro_atomic_size_t worst_short;
   retro_atomic_size_t worst_short_avail;

   /* The output unit: ComponentInstance or AudioComponentInstance,
    * both of which are this type on every SDK. */
   AudioUnit dev;

   /* AudioConverter for system sample-rate conversion */
#if !TARGET_OS_IPHONE
   /* Frames the HAL asks the render callback for at a time, as the
    * device actually settled it - or zero, where neither the unit nor
    * the device object would say. Never the value that was asked for:
    * that is the question, not the answer. */
   size_t            period_frames;
   /* Whether this output follows the system's default, which is only
    * so when the user named no device: a named one is not disturbed
    * by the default moving. */
   bool              follows_default;
   bool              listening_default;
   /* The device the unit is bound to, and where its clock stood when
    * the stream started. The HAL will give the device's own sample
    * position on request, which is what frames_consumed() is
    * specified to return - the callback counter beside it is a good
    * approximation, since a render callback is device-paced, but it
    * is still callbacks counted rather than the device asked. */
   AudioDeviceID     device_id;
   double            clock_base;      /* mSampleTime at the first read */
   bool              clock_based;
#endif
   unsigned output_rate;  /* Hardware output rate */
   /* The layout the output unit's input bus was set to, as the
    * frontend's mask, and its channel count: the unit routes each
    * position to the hardware's speaker of that position, or mixes
    * where it lacks one. The ring holds frames of this many floats. */
   uint32_t layout;
   unsigned channels;

   bool dev_alive;
   bool is_paused;
   bool nonblock;
   /* The output unit is not started when it is asked for, it is started
    * when there is something for it to play; see coreaudio_run(). want
    * is what start() and stop() set, running is what the unit is
    * actually doing. */
   bool want_running;
   bool unit_running;
} coreaudio_t;

static bool coreaudio_wait_init(coreaudio_t *dev)
{
   if (semaphore_create(mach_task_self(), &dev->sema,
            SYNC_POLICY_FIFO, 0) != KERN_SUCCESS)
      return false;
   dev->sema_alive = true;
   retro_atomic_int_init(&dev->waiters, 0);
   return true;
}

static void coreaudio_wait_free(coreaudio_t *dev)
{
   if (dev->sema_alive)
      semaphore_destroy(mach_task_self(), dev->sema);
}

/* Lock-free ring buffer operations */

static inline size_t rb_write_avail(coreaudio_t *dev)
{
   size_t filled = retro_atomic_load_acquire_size(&dev->filled);
   return (filled < dev->usable) ? dev->usable - filled : 0;
}

static void rb_write(coreaudio_t *dev, const float *data, size_t count)
{
   size_t first = dev->capacity - dev->write_ptr;
   if (first > count)
      first = count;

   memcpy(dev->buffer + dev->write_ptr, data, first * sizeof(float));
   memcpy(dev->buffer, data + first, (count - first) * sizeof(float));

   dev->write_ptr = (dev->write_ptr + count) & (dev->capacity - 1);
   retro_atomic_fetch_add_size(&dev->filled, count);
}

static void rb_read(coreaudio_t *dev, float *data, size_t count)
{
   size_t first = dev->capacity - dev->read_ptr;
   if (first > count)
      first = count;

   memcpy(data, dev->buffer + dev->read_ptr, first * sizeof(float));
   memcpy(data + first, dev->buffer, (count - first) * sizeof(float));

   dev->read_ptr = (dev->read_ptr + count) & (dev->capacity - 1);
   retro_atomic_fetch_sub_size(&dev->filled, count);
}

/* The wait between callbacks, on a Mach semaphore: semaphore_signal
 * takes no lock and is safe from the real-time render thread, and
 * semaphore_timedwait is the wait. The timeout is a ceiling for a
 * unit that has stopped rendering and will never signal; in play the
 * writer wakes when the callback frees space, after the kernel's wake
 * latency, which is the floor on Darwin - pthread_cond, dispatch and
 * os_unfair_lock's waiters all bottom out on this same wait.
 *
 * The waiter count is what dispatch_semaphore keeps in userspace and
 * what a bare Mach semaphore lacks: without it every callback signals
 * whether anyone waits or not, and the signals accumulate while the
 * writer is non-blocking - by one per callback, without bound - so
 * that the next real wait returns at once, again and again, until the
 * lap cap trips and the frontend drops audio it could have delivered.
 * The writer raises the count, then rechecks the ring: a callback that
 * ran before the raise saw no waiter and did not signal, and the
 * recheck sees the space it freed instead. A callback that ran after
 * the raise signals, and if the writer had already left, at most one
 * stale count remains, which the loop around this absorbs. Both sides
 * pair an acq_rel read-modify-write with an acquire load, which no
 * backend reorders. */
static void coreaudio_signal(coreaudio_t *dev)
{
   if (retro_atomic_load_acquire_int(&dev->waiters))
      semaphore_signal(dev->sema);
}

static void coreaudio_wait(coreaudio_t *dev, size_t want_samples, unsigned ms)
{
   retro_atomic_fetch_add_int(&dev->waiters, 1);
   if (rb_write_avail(dev) < want_samples)
   {
      mach_timespec_t ts;
      ts.tv_sec  = ms / 1000;
      ts.tv_nsec = (ms % 1000) * 1000000;
      semaphore_timedwait(dev->sema, ts);
   }
   retro_atomic_fetch_sub_int(&dev->waiters, 1);
}

/* The int16 fast path is gone. What it did - hand the core's int16
 * straight to an AudioConverter and let the system resample, saving
 * the frontend's resampler every frame - is still worth having, so
 * what was wrong with it is recorded here rather than lost with the
 * code.
 *
 * The driver is handed a rate adjustment on every write and has to
 * apply it. That path rebuilt the converter only when the adjustment
 * moved by more than half a percent, and the frontend's rate control
 * range is half a percent, so in ordinary use every correction fell
 * inside the dead zone and none was applied; the one that eventually
 * did not arrived as a step, converter destroyed and remade, filter
 * history lost. Lowering the threshold trades the dead zone for a
 * rebuild whenever the buffer moves, which is worse.
 *
 * What a second attempt needs first: a converter whose ratio can be
 * changed in place and keep its state, or a way for a driver to say it
 * cannot follow a rate adjustment so the frontend keeps its own
 * resampler - and then output compared against the sinc resampler's
 * sample for sample rather than assumed equivalent.
 *
 * One thing it should not do, since it reads plausibly and has been
 * suggested: set kAudioConverterPrimeMethod to
 * kConverterPrimeMethod_None to save latency. It does the opposite.
 * Normal, the default, primes with trailing input only and generates
 * no latency at the output; None assumes silence at both ends and puts
 * trailingFrames of through latency at the start of the output. None
 * exists for a source that cannot be read ahead of. A queue of the
 * core's audio is exactly a source that can be, so Normal is right
 * here for the reason it is the default. What is worth doing instead
 * is asking: kAudioConverterPrimeInfo reports leadingFrames and
 * trailingFrames for the converter as configured, so the read-ahead
 * can be measured rather than guessed at in either direction. */

#if !TARGET_OS_IPHONE
/* Defined with the other listeners, below the microphone half. */
static void coreaudio_listen_default_output(coreaudio_t *dev, bool on);
#endif

static void coreaudio_free(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   size_t       n;

   if (!dev)
      return;

   /* How the callback fared, said once, here. The frontend already
    * reports the underrun count; what this adds is the size of the
    * shortfall against the ring and the period, which is what says
    * whether a buzz is the ring being too small for the device's
    * period rather than the source being late. */
   if ((n = retro_atomic_load_acquire_size(&dev->underruns)))
      RARCH_LOG("[CoreAudio] The callback came up short %u time%s; at worst it wanted %u samples more than the %u it found, against a %u-sample ring and a %u-frame device period.\n",
            (unsigned)n, n == 1 ? "" : "s",
            (unsigned)retro_atomic_load_acquire_size(&dev->worst_short),
            (unsigned)retro_atomic_load_acquire_size(&dev->worst_short_avail),
            (unsigned)dev->usable,
#if !TARGET_OS_IPHONE
            (unsigned)dev->period_frames
#else
            0u
#endif
            );

   /* What the device was said to be able to ask for, and what it did
    * ask for. Said whether or not anything went wrong, because the
    * case worth catching is the one where they disagree and the ring
    * happened to be large enough anyway. */
   RARCH_LOG("[CoreAudio] Pulls: %u frames the largest the device declared, %u the largest it made, %u over the declared maximum.\n",
         (unsigned)dev->max_pull_frames,
         (unsigned)retro_atomic_load_acquire_size(&dev->max_pull_observed),
         (unsigned)retro_atomic_load_acquire_size(&dev->oversized_pulls));

   if ((n = retro_atomic_load_acquire_size(&dev->format_errors)))
      RARCH_WARN("[CoreAudio] %u render callback%s arrived with a buffer list this driver does not handle, and were silenced.\n",
            (unsigned)n, n == 1 ? "" : "s");

#if !TARGET_OS_IPHONE
   /* Before anything else: the HAL calls the listener on a thread of
    * its own, and it is handed this pointer. */
   coreaudio_listen_default_output(dev, false);
#endif

   if (dev->dev_alive)
   {
      AudioOutputUnitStop(dev->dev);
      ca_cm.close(dev->dev);
   }


   if (dev->buffer)
      free(dev->buffer);

   coreaudio_wait_free(dev);

   free(dev);
}

static OSStatus coreaudio_audio_write_cb(void *userdata,
      AudioUnitRenderActionFlags *action_flags,
      const AudioTimeStamp *time_stamp, UInt32 bus_number,
      UInt32 number_frames, AudioBufferList *io_data)
{
   size_t avail;
   float *outbuf;
   size_t frames_needed;
   size_t have_bytes;
   coreaudio_t *dev = (coreaudio_t*)userdata;

   (void)time_stamp;
   (void)bus_number;

   if (!io_data || io_data->mNumberBuffers < 1)
      return noErr;

   /* What the device actually asks for, recorded whatever happens
    * below: two atomic operations on the render thread, and the only
    * measurement that can contradict what init() worked out the
    * largest pull would be. Nothing is logged from here. */
   if ((size_t)number_frames
         > retro_atomic_load_acquire_size(&dev->max_pull_observed))
      retro_atomic_store_release_size(&dev->max_pull_observed,
            (size_t)number_frames);
   if (     dev->max_pull_frames
         && (size_t)number_frames > dev->max_pull_frames)
      retro_atomic_fetch_add_size(&dev->oversized_pulls, 1);

   /* Not the one interleaved buffer the stream format asked for.
    * Returning noErr with the buffers untouched hands the device
    * whatever happened to be in them; silence all of them and count
    * it, so a topology this driver does not handle is audibly and
    * countably nothing rather than undefined. */
   if (io_data->mNumberBuffers != 1)
   {
      UInt32 b;
      for (b = 0; b < io_data->mNumberBuffers; b++)
         if (io_data->mBuffers[b].mData)
            memset(io_data->mBuffers[b].mData, 0,
                  io_data->mBuffers[b].mDataByteSize);
      if (action_flags)
         *action_flags |= kAudioUnitRenderAction_OutputIsSilence;
      retro_atomic_fetch_add_size(&dev->format_errors, 1);
      return noErr;
   }

   outbuf        = (float *)io_data->mBuffers[0].mData;
   /* What the unit asked for, which is what number_frames is: frames
    * of every channel. The byte capacity of the buffer was being used
    * to infer it, which happens to agree for the interleaved format
    * this driver sets up and is a different question - a buffer is
    * allowed to be larger than the request. */
   frames_needed = (size_t)number_frames * dev->channels;
   have_bytes    = io_data->mBuffers[0].mDataByteSize;
   if (frames_needed * sizeof(float) > have_bytes)
      frames_needed = have_bytes / sizeof(float);
   avail         = retro_atomic_load_acquire_size(&dev->filled);

   if (avail < frames_needed)
   {
      /* Underrun: what there is, and silence after it.
       *
       * The silence flag says the buffer holds nothing but silence,
       * and a unit is entitled to skip a buffer that says so. Setting
       * it for a partial underrun therefore threw away the samples
       * just read into the buffer: a period that should have been
       * mostly audio became one of silence entirely, every time the
       * ring ran a little short. It is set only when there was
       * nothing at all.
       *
       * And ored in, not assigned: the flags are the unit's, and it
       * may have set some of its own on the way in. */
      /* And whole frames out, for the same reason: a short read that
       * ends mid-frame leaves the ring's read cursor offset from its
       * write cursor by a sample, which never comes back. */
      avail -= avail % dev->channels;
      if (avail > 0)
         rb_read(dev, outbuf, avail);
      memset(outbuf + avail, 0, (frames_needed - avail) * sizeof(float));
      if (!avail && action_flags)
         *action_flags |= kAudioUnitRenderAction_OutputIsSilence;
      retro_atomic_fetch_add_size(&dev->underruns, 1);
      {
         size_t shortfall = frames_needed - avail;
         if (shortfall > retro_atomic_load_acquire_size(&dev->worst_short))
         {
            retro_atomic_store_release_size(&dev->worst_short, shortfall);
            retro_atomic_store_release_size(&dev->worst_short_avail, avail);
         }
      }
   }
   else
      rb_read(dev, outbuf, frames_needed);

   /* What the device took, silence included: an underrun still consumes
    * a period of device time, and it is device time this measures. */
   retro_atomic_fetch_add_size(&dev->consumed, frames_needed);

   /* Wake writer if it might be waiting */
   coreaudio_signal(dev);

   return noErr;
}

#if !TARGET_OS_IPHONE
/* The HAL's output devices. kAudioHardwarePropertyDevices answers on
 * the output scope from 10.6 and only on the global scope before;
 * asked at runtime rather than decided by the build SDK. Returns a
 * malloc'd array the caller frees, or NULL. */
static AudioDeviceID *coreaudio_hal_devices(UInt32 *count)
{
   ca_addr_t propaddr;
   AudioDeviceID *devices = NULL;
   UInt32 size            = 0;

   propaddr.mSelector = kAudioHardwarePropertyDevices;
   propaddr.mScope    = kAudioDevicePropertyScopeOutput;
   propaddr.mElement  = CA_ELEMENT_MAIN;
   if (!ca_prop_has(CA_SYSTEM_OBJECT, &propaddr, false))
      propaddr.mScope = CA_SCOPE_GLOBAL;

   if (ca_prop_size(CA_SYSTEM_OBJECT, &propaddr, &size, false) != noErr || !size)
      return NULL;
   if (!(devices = (AudioDeviceID*)malloc(size)))
      return NULL;
   if (ca_prop_get(CA_SYSTEM_OBJECT, &propaddr, &size, devices, false) != noErr)
   {
      free(devices);
      return NULL;
   }
   *count = size / sizeof(AudioDeviceID);
   return devices;
}

static bool coreaudio_hal_device_name(AudioDeviceID id, char *s, size_t len)
{
   ca_addr_t propaddr;
   UInt32 size        = (UInt32)len;
   propaddr.mSelector = kAudioDevicePropertyDeviceName;
   propaddr.mScope    = kAudioDevicePropertyScopeOutput;
   propaddr.mElement  = CA_ELEMENT_MAIN;
   s[0]               = 0;
   return ca_prop_get(id, &propaddr, &size, s, false) == noErr
         && s[0];
}

/* A CFString property of a device, as UTF-8. The microphone half has
 * the same reader, behind HAVE_MICROPHONE, so this one stands on its
 * own rather than reaching across that guard. */
static bool coreaudio_hal_device_string(AudioDeviceID id,
      UInt32 selector, char *s, size_t len)
{
   ca_addr_t prop;
   CFStringRef cf = NULL;
   UInt32 size    = sizeof(cf);
   bool ok;
   prop.mSelector = selector;
   prop.mScope    = CA_SCOPE_GLOBAL;
   prop.mElement  = CA_ELEMENT_MAIN;
   s[0]           = 0;
   if (ca_prop_get(id, &prop, &size, &cf, false) != noErr || !cf)
      return false;
   ok = CFStringGetCString(cf, s, (CFIndex)len, kCFStringEncodingUTF8) && s[0];
   CFRelease(cf);
   return ok;
}

/* The saved setting is matched against the device's UID first and its
 * name second, which is what the microphone half already does.
 *
 * A name is what a user reads and what two devices can share - plug
 * in a second "USB Audio Device" and which one a saved setting means
 * is whichever the HAL happens to enumerate first - and it changes
 * when the device is renamed. The UID does neither. A setting saved
 * before this carries a name, which is why the name is still tried;
 * one saved after carries the UID. */
static void coreaudio_choose_output_device(coreaudio_t *dev, const char* device)
{
   UInt32 i, device_count = 0, pass;
   AudioDeviceID *devices;

   if (string_is_empty(device))
      return;
   if (!(devices = coreaudio_hal_devices(&device_count)))
      return;

   for (pass = 0; pass < 3; pass++)
   {
      for (i = 0; i < device_count; i++)
      {
         char s[1024];
         bool got;

         if (pass == 0)
            got = coreaudio_hal_device_string(devices[i],
                  kAudioDevicePropertyDeviceUID, s, sizeof(s));
         else if (pass == 1)
            got = coreaudio_hal_device_string(devices[i],
                  kAudioDevicePropertyDeviceNameCFString, s, sizeof(s));
         else
            got = coreaudio_hal_device_name(devices[i], s, sizeof(s));

         if (got && string_is_equal(s, device))
         {
            AudioUnitSetProperty(dev->dev, kAudioOutputUnitProperty_CurrentDevice,
                  kAudioUnitScope_Global, 0, &devices[i], sizeof(AudioDeviceID));
            RARCH_LOG("[CoreAudio] Output device \"%s\" matched by %s.\n",
                  device, pass == 0 ? "UID" : "name");
            free(devices);
            return;
         }
      }
   }

   RARCH_WARN("[CoreAudio] No output device matches \"%s\"; using the default.\n",
         device);
   free(devices);
}
#endif

/* Query the actual hardware sample rate */
static unsigned coreaudio_get_hardware_sample_rate(AudioUnit dev)
{
   AudioStreamBasicDescription hw_desc;
   UInt32 size = sizeof(hw_desc);

#if TARGET_OS_IPHONE
   /* On iOS, query the output scope of RemoteIO to get hardware rate */
   if (AudioUnitGetProperty(dev, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output, 0, &hw_desc, &size) == noErr)
   {
      if (hw_desc.mSampleRate > 0)
         return (unsigned)hw_desc.mSampleRate;
   }
#else
   /* On macOS, query the current output device's nominal sample rate */
   {
      AudioDeviceID device_id = 0;
      UInt32 device_size = sizeof(device_id);
      ca_addr_t prop;
      Float64 nominal_rate = 0;

      /* Get the current device from the AudioUnit */
      if (AudioUnitGetProperty(dev, kAudioOutputUnitProperty_CurrentDevice,
               kAudioUnitScope_Global, 0, &device_id, &device_size) == noErr
            && device_id != 0)
      {
         prop.mSelector = kAudioDevicePropertyNominalSampleRate;
         prop.mScope    = CA_SCOPE_GLOBAL;
         prop.mElement  = CA_ELEMENT_MAIN;
         size = sizeof(nominal_rate);

         if (ca_prop_get(device_id, &prop, &size, &nominal_rate, false) == noErr && nominal_rate > 0)
            return (unsigned)nominal_rate;
      }
   }
#endif

   return 0; /* Failed to determine, caller should use fallback */
}

static void *coreaudio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   size_t buffer_samples;
   UInt32 i_size;
   AudioStreamBasicDescription real_desc;
#if !TARGET_OS_IPHONE
   AudioChannelLayout layout               = {0};
#endif
   AURenderCallbackStruct cb               = {0};
   AudioStreamBasicDescription stream_desc = {0};
   ca_component_desc_t desc                = {0};
   void *comp;
   coreaudio_t *dev;

   if (!ca_cm_resolve())
   {
      RARCH_ERR("[CoreAudio] Neither the AudioComponent API nor the Component Manager is available.\n");
      return NULL;
   }
   RARCH_DBG("[CoreAudio] Output unit through %s.\n",
         ca_cm.modern ? "AudioComponent" : "the Component Manager");

   if (!(dev = (coreaudio_t*)calloc(1, sizeof(*dev))))
      return NULL;

   /* The layout the frontend asked for; the unit routes its positions
    * to the hardware's speakers, mixing where it lacks one, so what
    * is asked is carried - unless the unit will not take the stream
    * format at that width, in which case stereo. */
   dev->layout   = audio_driver_requested_layout();
   dev->channels = audio_layout_channels(dev->layout);

   if (!coreaudio_wait_init(dev))
      goto error;

   /* Open the output unit */
   desc.type         = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
   desc.subtype      = kAudioUnitSubType_RemoteIO;
#else
   desc.subtype      = kAudioUnitSubType_HALOutput;
#endif
   desc.manufacturer = kAudioUnitManufacturer_Apple;

   if (!(comp = ca_cm.find_next(NULL, &desc)))
      goto error;
   if (ca_cm.open(comp, &dev->dev) != noErr)
      goto error;

#if !TARGET_OS_IPHONE
   if (device)
      coreaudio_choose_output_device(dev, device);
   /* No device named means this follows the system's default, and a
    * default that moves while content runs leaves the frontend on
    * hardware the user has just stopped using. */
   dev->follows_default = string_is_empty(device);
   coreaudio_listen_default_output(dev, dev->follows_default);
#endif

   dev->dev_alive                = true;

   /* Query actual hardware sample rate to avoid double resampling */
   {
      unsigned hw_rate = coreaudio_get_hardware_sample_rate(dev->dev);
      if (hw_rate > 0 && hw_rate != rate)
      {
         RARCH_LOG("[CoreAudio] Hardware sample rate is %u Hz (requested %u Hz), using hardware rate.\n",
               hw_rate, rate);
         rate = hw_rate;
      }
   }

   /* Set audio format */
   stream_desc.mSampleRate       = rate;
   stream_desc.mBitsPerChannel   = sizeof(float) * CHAR_BIT;
   stream_desc.mChannelsPerFrame = dev->channels;
   stream_desc.mBytesPerPacket   = dev->channels * sizeof(float);
   stream_desc.mBytesPerFrame    = dev->channels * sizeof(float);
   stream_desc.mFramesPerPacket  = 1;
   stream_desc.mFormatID         = kAudioFormatLinearPCM;
   stream_desc.mFormatFlags      = kAudioFormatFlagIsFloat
                                 | kAudioFormatFlagIsPacked;

   if (!is_little_endian())
      stream_desc.mFormatFlags  |= kAudioFormatFlagIsBigEndian;

   /* Interleaved float stereo on the input bus; the unit mixes or
    * downmixes to whatever the hardware has. RemoteIO has been seen to
    * refuse the first set and take the second, so one retry. */
   if (     AudioUnitSetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input, 0, &stream_desc, sizeof(stream_desc)) != noErr
         && AudioUnitSetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input, 0, &stream_desc, sizeof(stream_desc)) != noErr)
      goto error;

   /* Check returned audio format. */
   i_size = sizeof(real_desc);
   if (AudioUnitGetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input, 0, &real_desc, &i_size) != noErr)
      goto error;

   if (real_desc.mChannelsPerFrame != stream_desc.mChannelsPerFrame)
   {
      if (dev->channels > 2)
      {
         RARCH_WARN("[CoreAudio] The output unit would not take a %u-channel stream (layout 0x%03x); opening stereo.\n",
               dev->channels, dev->layout);
         dev->layout   = AUDIO_LAYOUT_STEREO;
         dev->channels = 2;
         stream_desc.mChannelsPerFrame = 2;
         stream_desc.mBytesPerPacket   = 2 * sizeof(float);
         stream_desc.mBytesPerFrame    = 2 * sizeof(float);
         if (AudioUnitSetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
                  kAudioUnitScope_Input, 0, &stream_desc, sizeof(stream_desc)) != noErr)
            goto error;
         i_size = sizeof(real_desc);
         if (AudioUnitGetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
                  kAudioUnitScope_Input, 0, &real_desc, &i_size) != noErr)
            goto error;
      }
      if (real_desc.mChannelsPerFrame != stream_desc.mChannelsPerFrame)
         goto error;
   }
   if (real_desc.mBitsPerChannel != stream_desc.mBitsPerChannel)
      goto error;
   if (real_desc.mFormatFlags != stream_desc.mFormatFlags)
      goto error;
   if (real_desc.mFormatID != stream_desc.mFormatID)
      goto error;

   RARCH_LOG("[CoreAudio] Using output sample rate of %.1f Hz.\n",
         (float)real_desc.mSampleRate);
   *new_rate = real_desc.mSampleRate;
   dev->output_rate = *new_rate;


   /* Tell the HAL unit the two channels are a stereo pair. RemoteIO
    * refuses the property, hence macOS only; and it is advisory - the
    * stream format above already fixed two channels - so a HAL that
    * refuses it too is logged and not treated as a failed open. This
    * had been under #ifndef, which a macOS SDK defining the macro as
    * 0 turned into "never", so the layout was not being set at all. */
#if !TARGET_OS_IPHONE
   /* The bus's speaker positions. Core Audio's channel bitmap uses the
    * same bits as the frontend's mask - Left 1, Right 2, Center 4, LFE
    * 8, LeftSurround 0x10 (the back pair), ..., LeftSurroundDirect
    * 0x200 (the side pair) - in the same ascending order, so a wider
    * layout goes across as itself and the two 5.1s stay apart. Stereo
    * keeps its named tag. The field is a UInt32 in every SDK; the
    * AudioChannelBitmap name for it came later than the 10.4 SDK the
    * PPC build uses. */
   if (dev->channels > 2)
   {
      layout.mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap;
      layout.mChannelBitmap    = (UInt32)dev->layout;
   }
   else
      layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
   if (AudioUnitSetProperty(dev->dev, kAudioUnitProperty_AudioChannelLayout,
         kAudioUnitScope_Input, 0, &layout, sizeof(layout)) != noErr)
      RARCH_WARN("[CoreAudio] The output unit declined the channel layout (0x%03x); continuing with the %u-channel stream format.\n",
            dev->layout, dev->channels);
#endif

   /* Set callbacks and finish up. */
   cb.inputProc       = coreaudio_audio_write_cb;
   cb.inputProcRefCon = dev;

   if (AudioUnitSetProperty(dev->dev, kAudioUnitProperty_SetRenderCallback,
         kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr)
      goto error;

   /* Enforce minimum latency to prevent buffer issues */
   if (latency < 8)
      latency = 8;

#if !TARGET_OS_IPHONE
   /* The HAL's IO buffer: how much the render callback is asked for at
    * a time, and a stage of the device latency in its own right - the
    * HAL holds a whole pull before it feeds any of it on. A quarter of
    * the ring, so the stage stays small against the setting and room
    * comes back to the writer in small pieces.
    *
    * What that is clamped to is the device's own range, asked for
    * rather than guessed. It used to be held between 64 and 512,
    * which is wrong in both directions: a device that will do 32
    * frames was kept at 64 and lost half the latency it was offering,
    * and a device whose minimum is 128 was asked for 64 and simply
    * refused, leaving whatever it had. The property is advisory
    * either way, so what was actually taken is read back and logged
    * rather than assumed. */
   {
      UInt32 period    = (UInt32)((latency * (*new_rate)) / 1000 / 4);
      UInt32 wanted    = period;
      UInt32 got       = 0;
      UInt32 size      = sizeof(got);
      AudioDeviceID id = 0;
      UInt32 id_size   = sizeof(id);
      AudioValueRange range;

      range.mMinimum = 0.0;
      range.mMaximum = 0.0;
      if (AudioUnitGetProperty(dev->dev, kAudioOutputUnitProperty_CurrentDevice,
               kAudioUnitScope_Global, 0, &id, &id_size) == noErr && id != 0)
      {
         ca_addr_t prop;
         UInt32 rsize   = sizeof(range);
         prop.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
         prop.mScope    = kAudioDevicePropertyScopeOutput;
         prop.mElement  = CA_ELEMENT_MAIN;
         if (ca_prop_get(id, &prop, &rsize, &range, false) != noErr)
            range.mMinimum = range.mMaximum = 0.0;
      }

      if (range.mMinimum > 0.0 && range.mMaximum >= range.mMinimum)
      {
         if ((double)period < range.mMinimum)
            period = (UInt32)range.mMinimum;
         else if ((double)period > range.mMaximum)
            period = (UInt32)range.mMaximum;
      }
      else if (period < 64)   /* no range to go on: the old bounds */
         period = 64;
      else if (period > 512)
         period = 512;

      AudioUnitSetProperty(dev->dev, kAudioDevicePropertyBufferFrameSize,
            kAudioUnitScope_Global, 0, &period, sizeof(period));
      if (AudioUnitGetProperty(dev->dev, kAudioDevicePropertyBufferFrameSize,
               kAudioUnitScope_Global, 0, &got, &size) != noErr)
         got = 0;
      /* What the device actually took, for the ring below to be sized
       * against. It is not necessarily what was asked for: a device
       * whose minimum is above the quarter-ring target is clamped up
       * to that minimum, and then the ring has to hold enough periods
       * of it or every callback is a partial fill.
       *
       * The unit's readback is the AUHAL's view of the device; the
       * device object is the HAL's own, and answers when the unit will
       * not. If neither does, this stays zero - unknown. It used to
       * take the requested value in that case, which is the one number
       * that cannot be the answer, since the whole point of reading
       * back is that the request is advisory. */
      if (!got && id != 0)
      {
         ca_addr_t prop;
         UInt32 fsize   = sizeof(got);
         prop.mSelector = kAudioDevicePropertyBufferFrameSize;
         prop.mScope    = kAudioDevicePropertyScopeOutput;
         prop.mElement  = CA_ELEMENT_MAIN;
         if (ca_prop_get(id, &prop, &fsize, &got, false) != noErr)
            got = 0;
      }
      dev->period_frames   = (size_t)got;

      /* And the largest pull, which is a different question. The
       * period is what the device asks for normally; a device that
       * carries kAudioDevicePropertyUsesVariableBufferFrameSizes says
       * with it the largest it may ask for, and the period is then only
       * the smallest. Where the device says nothing either way, the
       * unit's MaximumFramesPerSlice is the ceiling it has been
       * prepared for - taken only as the fallback, since it is a
       * capability and not a pull, and sizing the ring against it
       * unasked would throw away the latency setting on every machine
       * that has one. */
      dev->max_pull_frames = dev->period_frames;
      if (id != 0)
      {
         ca_addr_t prop;
         UInt32 variable = 0;
         UInt32 vsize    = sizeof(variable);
         prop.mSelector  = CA_PROP_VARIABLE_BUFFER_FRAMES;
         prop.mScope     = kAudioDevicePropertyScopeOutput;
         prop.mElement   = CA_ELEMENT_MAIN;
         if (     ca_prop_has(id, &prop, false)
               && ca_prop_get(id, &prop, &vsize, &variable, false) == noErr
               && (size_t)variable > dev->max_pull_frames)
         {
            dev->max_pull_frames = (size_t)variable;
            RARCH_LOG("[CoreAudio] The device varies its buffer: up to %u frames a pull, against a %u-frame nominal.\n",
                  (unsigned)variable, (unsigned)dev->period_frames);
         }
      }
      if (!dev->max_pull_frames)
      {
         UInt32 slice = 0;
         UInt32 ssize = sizeof(slice);
         if (     AudioUnitGetProperty(dev->dev,
                  kAudioUnitProperty_MaximumFramesPerSlice,
                  kAudioUnitScope_Global, 0, &slice, &ssize) == noErr
               && slice)
            dev->max_pull_frames = (size_t)slice;
         RARCH_WARN("[CoreAudio] Neither the unit nor the device would say what buffer size it settled on; sizing the ring against the unit's %u-frame slice ceiling.\n",
               (unsigned)dev->max_pull_frames);
      }

      if (range.mMinimum > 0.0)
         RARCH_LOG("[CoreAudio] IO buffer: asked %u, device takes %u to %u, got %u frames (%.2f ms).\n",
               (unsigned)wanted, (unsigned)range.mMinimum,
               (unsigned)range.mMaximum, (unsigned)got,
               got ? (double)got * 1000.0 / (double)(*new_rate) : 0.0);
      else
         RARCH_LOG("[CoreAudio] IO buffer: asked %u, got %u frames (%.2f ms); the device names no range.\n",
               (unsigned)wanted, (unsigned)got,
               got ? (double)got * 1000.0 / (double)(*new_rate) : 0.0);
   }
#endif

   if (AudioUnitInitialize(dev->dev) != noErr)
      goto error;

   /* Calculate buffer size in samples (stereo) */
   buffer_samples   = (latency * (*new_rate)) / 1000;

#if !TARGET_OS_IPHONE
   /* Never fewer than four of the device's periods. The latency
    * setting alone was enough while the period was held at 512 or
    * below, because the setting is always several times that - but
    * the period is now whatever the device says it takes, and a
    * device whose minimum is large (some aggregate and HDMI devices
    * ask for thousands of frames) gets clamped up to it. A ring
    * holding one or two such periods cannot fill a callback: rate
    * control keeps it about half full, so every callback takes what
    * is there and pads the rest with silence, which is heard as a
    * buzz at the period rate and a signal that is mostly gaps. */
   /* Two floors, and the ring takes the higher. Four of the nominal
    * period, so the writer has somewhere to be between callbacks - and
    * twice the largest pull, which is the harder requirement: rate
    * control holds the ring around half full, so a ring under twice the
    * largest pull hands the callback less than it asked for at the
    * setpoint, every time, and the callback pads the difference with
    * silence. Four periods was the only floor here, and on a device
    * whose largest pull is bigger than its period it is the wrong
    * multiple of the wrong number. */
   {
      size_t floor_frames = dev->period_frames * 4;
      if (dev->max_pull_frames * 2 > floor_frames)
         floor_frames = dev->max_pull_frames * 2;
      if (floor_frames && buffer_samples < floor_frames)
      {
         RARCH_LOG("[CoreAudio] A %u-frame period and pulls of up to %u need a larger buffer than the %u ms setting gives; using %u frames.\n",
               (unsigned)dev->period_frames, (unsigned)dev->max_pull_frames,
               latency, (unsigned)floor_frames);
         buffer_samples = floor_frames;
      }
   }
#endif

   buffer_samples  *= dev->channels;

   /* Round up to next power of 2 for fast modulo via masking; the ring
    * holds the setting, not the container. */
   dev->capacity = 1;
   while (dev->capacity < buffer_samples)
      dev->capacity <<= 1;
   dev->usable = buffer_samples;

   dev->buffer = (float *)calloc(dev->capacity, sizeof(float));
   if (!dev->buffer)
      goto error;

   retro_atomic_size_init(&dev->filled, 0);
   retro_atomic_size_init(&dev->consumed, 0);
   retro_atomic_size_init(&dev->underruns, 0);
   /* What coreaudio_run() waits for before starting; the HAL's pull on
    * macOS, and on iOS the ring's own quarter, which is what the unit
    * was asked to use. */
#if !TARGET_OS_IPHONE
   dev->period_pull = dev->period_frames;
#endif
   if (!dev->period_pull)
      dev->period_pull = (dev->usable / dev->channels) / 4;
   retro_atomic_size_init(&dev->max_pull_observed, 0);
   retro_atomic_size_init(&dev->oversized_pulls, 0);
   retro_atomic_size_init(&dev->format_errors, 0);
   retro_atomic_size_init(&dev->worst_short, 0);
   retro_atomic_size_init(&dev->worst_short_avail, 0);
   dev->write_ptr = 0;
   dev->read_ptr  = 0;

   RARCH_LOG("[CoreAudio] Buffer: %u samples (%u bytes, %.1f ms).\n",
         (unsigned)dev->usable,
         (unsigned)(dev->usable * sizeof(float)),
         (float)dev->usable * 1000.0f / (*new_rate) / (float)dev->channels);

#if !TARGET_OS_IPHONE
   /* The device's own stage behind the ring, for the statistics
    * overlay: the HAL's IO buffer, which the render callback fills a
    * whole one of at a time, the device's latency and safety offset,
    * and the output stream's latency - the sum a HAL client is told to
    * expect between a render and the jack. Each is a property the
    * device or stream may lack; whichever it has are summed. */
   {
      AudioDeviceID device_id = 0;
      UInt32 device_size      = sizeof(device_id);
      if (AudioUnitGetProperty(dev->dev, kAudioOutputUnitProperty_CurrentDevice,
               kAudioUnitScope_Global, 0, &device_id, &device_size) == noErr
            && device_id != 0)
      {
         /* Kept for the clock, which is asked of the device rather
          * than of this driver's own counting. */
         dev->device_id = device_id;
         static const AudioObjectPropertySelector dev_sel[3] = {
            kAudioDevicePropertyBufferFrameSize,
            kAudioDevicePropertyLatency,
            kAudioDevicePropertySafetyOffset
         };
         ca_addr_t prop;
         AudioStreamID stream_id = 0;
         UInt32 total = 0, size, i;
         prop.mScope   = kAudioDevicePropertyScopeOutput;
         prop.mElement = CA_ELEMENT_MAIN;
         for (i = 0; i < 3; i++)
         {
            UInt32 value   = 0;
            size           = sizeof(value);
            prop.mSelector = dev_sel[i];
            if (     ca_prop_has(device_id, &prop, false)
                  && ca_prop_get(device_id, &prop, &size, &value, false) == noErr)
               total += value;
         }
         /* The first output stream's latency; the property lives on
          * the stream object, not the device. */
         prop.mSelector = kAudioDevicePropertyStreams;
         size           = sizeof(stream_id);
         if (     ca_prop_has(device_id, &prop, false)
               && ca_prop_get(device_id, &prop, &size, &stream_id, false) == noErr
               && stream_id != 0)
         {
            UInt32 value   = 0;
            size           = sizeof(value);
            /* The stream's latency selector is the same four
             * characters as the device's - 'ltnc' - and the HAL reads
             * the number, not the spelling. kAudioDevicePropertyLatency
             * is the spelling every SDK back to 10.0 declares, where
             * kAudioStreamPropertyLatency is not; asking through the
             * one that is always there costs nothing and removes a
             * name a 10.4 SDK may not have. */
            prop.mSelector = kAudioDevicePropertyLatency;
            prop.mScope    = CA_SCOPE_GLOBAL;
            if (     ca_prop_has(stream_id, &prop, true)
                  && ca_prop_get(stream_id, &prop, &size, &value, true) == noErr)
               total += value;
         }
         audio_driver_set_device_latency((size_t)total);
      }
   }
#endif

   /* Not started here. The unit is started by coreaudio_run() once the
    * ring has a period in it; starting it against an empty ring is a
    * burst of underruns for the whole of the frontend's priming. */
   dev->want_running = true;

   return dev;

error:
   RARCH_ERR("[CoreAudio] Failed to initialize driver.\n");
   coreaudio_free(dev);
   return NULL;
}

/* Start the unit, once there is a period in the ring for it to take.
 *
 * It used to start at the end of init() and in start(), with the ring
 * empty either time. The device begins pulling immediately, the
 * frontend has not written yet - and on the threaded pipeline it will
 * not for a while, since the consumer's first pass deliberately waits
 * for the pipe's target and the device's buffer before it runs - so
 * every pull in that window is a full underrun. Measured against a
 * clocked device (samples/audio/pipeline_clocked) it is 32 to 80 ms of
 * unbroken silence on every init, whatever the latency setting, and an
 * init happens on every audio latency change, every vsync toggle and
 * every device change. That is the burst of noise heard on each of
 * them.
 *
 * Waiting for a whole period rather than a single frame so the first
 * pull is a full one; the ring being full counts too, for a period
 * larger than the ring can be asked to hold. A caller that is waiting
 * for room has to start it regardless - room only comes from the
 * device - which is why wait_writable() calls this before it sleeps
 * rather than after. */
static void coreaudio_run(coreaudio_t *dev, bool force)
{
   if (!dev->want_running || dev->unit_running || dev->is_paused)
      return;
   if (     !force
         && retro_atomic_load_acquire_size(&dev->filled)
               < dev->period_pull * dev->channels
         && rb_write_avail(dev))
      return;
   if (AudioOutputUnitStart(dev->dev) == noErr)
      dev->unit_running = true;
}

/* Whether the unit is rendering, for the writer's bail-out. A unit that
 * has not been started yet is not a stopped unit: the caller must not
 * treat it as one and give up, it is about to be started. */
static bool coreaudio_unit_stalled(coreaudio_t *dev)
{
   UInt32 running = 0;
   UInt32 size    = sizeof(running);
   if (!dev->unit_running)
      return false;
   return AudioUnitGetProperty(dev->dev,
         kAudioOutputUnitProperty_IsRunning,
         kAudioUnitScope_Global, 0, &running, &size) == noErr && !running;
}

static ssize_t coreaudio_write(void *data, const void *buf_, size_t len)
{
   coreaudio_t *dev   = (coreaudio_t*)data;
   const float *buf   = (const float *)buf_;
   size_t samples     = len / sizeof(float);
   size_t written     = 0;
   /* Each wait below is bounded; this bounds the loop, for a unit that
    * reports running but never renders. */
   int laps           = 8;

   while (!dev->is_paused && samples > 0)
   {
      size_t avail    = rb_write_avail(dev);
      size_t to_write = (avail < samples) ? avail : samples;

      /* Whole frames only. The ring is counted in samples and the
       * free space in it is whatever the callback happened to leave,
       * so without this a write can end half way through a frame -
       * and from then on every frame in the ring straddles two of the
       * source's, with left in right's place for the rest of the
       * session. On correlated stereo that also cancels, which is
       * heard as a buzz over something much quieter than it should
       * be.
       *
       * This was here to be found for as long as the write path
       * existed; what uncovered it was the int16 fast path being
       * switched off, which moved every core onto this function
       * instead of only the float ones. */
      to_write -= to_write % dev->channels;

      if (to_write > 0)
      {
         rb_write(dev, buf, to_write);
         buf     += to_write;
         written += to_write;
         samples -= to_write;
      }

      /* Whatever went in may be enough to start on. */
      coreaudio_run(dev, false);

      if (dev->nonblock)
         break;

      if (samples > 0)
      {
         /* If the audio unit has stopped (e.g. audio session interrupted
          * by a phone call), bail out - the callback will never drain.
          * There is more to write than fits, so start now whether a
          * period has accumulated or not: nothing else will drain it. */
         coreaudio_run(dev, true);
         if (coreaudio_unit_stalled(dev))
            break;
         if (--laps < 0)
            break;
         /* Brief timeout as safety net for the race where the unit
          * stops during the wait; we'll re-check on the next iteration. */
         coreaudio_wait(dev, 1, 100);
      }
   }

   return written * sizeof(float);
}


static void coreaudio_set_nonblock_state(void *data, bool state)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (dev)
      dev->nonblock = state;
}

static bool coreaudio_alive(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (!dev)
      return false;
   return !dev->is_paused;
}

static bool coreaudio_stop(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (dev)
   {
      dev->want_running = false;
      if (!dev->unit_running)
      {
         dev->is_paused = true;
         return true;
      }
      dev->is_paused = (AudioOutputUnitStop(dev->dev) == noErr) ? true : false;
      if (dev->is_paused)
      {
         dev->unit_running = false;
         return true;
      }
   }
   return false;
}

/* Also the far end of an audio session interruption on iOS and tvOS:
 * the Cocoa side observes AVAudioSessionInterruptionNotification and
 * calls audio_driver_stop() at Began and audio_driver_start() at Ended,
 * which arrive here. The converter reset that used to happen here -
 * for a converter left stuck at end-of-stream when the system stopped
 * the unit, the tvOS 13/14 symptom - went with the fast path it
 * belonged to, and belongs with any second attempt at it. */
static bool coreaudio_start(void *data, bool is_shutdown)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (dev)
   {
      /* Asked for, not done: coreaudio_run() starts the unit when the
       * ring has something in it. A start that put the unit straight
       * back to pulling an empty ring is the burst of noise on every
       * unpause and every device change. */
      dev->want_running = true;
      dev->is_paused    = false;
      return true;
   }
   return false;
}

static bool coreaudio_use_float(void *data) { return true; }

static uint32_t coreaudio_layout(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   return dev ? dev->layout : AUDIO_LAYOUT_STEREO;
}

static size_t coreaudio_underruns(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   return dev ? retro_atomic_load_acquire_size(&dev->underruns) : 0;
}

static size_t coreaudio_write_avail(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   return rb_write_avail(dev) * sizeof(float);
}

static size_t coreaudio_buffer_size(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   return dev->usable * sizeof(float);
}

/* Wait on what the render callback signals after every pull until at
 * least len bytes fit in the ring. Returns the free space then, or 0
 * once the unit has stopped (paused, or interrupted, which the running
 * check catches as coreaudio_write() does).
 *
 * It waited for half the ring instead, whenever len was more than that.
 * The caller's question is whether len can be accepted, and it acts on
 * the answer by taking that much out of its own queue and writing it -
 * so a yes that meant half of it left the write to block inside itself,
 * or, non-blocking, to drop the tail. len was over half routinely, not
 * rarely: the frontend capped its pass at half the buffer and then
 * asked for the resampler's bound on it, which is that plus a margin.
 * The cap is the inverse of the bound now, so the two agree; the clamp
 * that was papering over the gap is gone with it. */
static size_t coreaudio_wait_writable(void *data, size_t len)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   size_t want      = len / sizeof(float);
   int    laps      = 8;

   if (!dev || !dev->channels)
      return 0;
   /* Whole frames: the writer only ever moves whole ones into the ring,
    * so room for part of one is not room the write can use. */
   if (want % dev->channels)
      want += dev->channels - want % dev->channels;
   /* The ring empty is the most that can ever be accepted at once.
    * Above that there is no wait that could end in a yes, so the
    * ceiling is the ring rather than half of it, and the free space
    * returned says what was really there. */
   if (want > dev->usable)
      want = dev->usable;

   for (;;)
   {
      size_t avail;

      if (dev->is_paused)
         break;
      avail = rb_write_avail(dev);
      if (avail >= want)
         return avail * sizeof(float);
      /* About to sleep on room that only the device can make, so it has
       * to be running by now whatever the ring holds. */
      coreaudio_run(dev, true);
      if (coreaudio_unit_stalled(dev))
         break;
      /* Each wait is bounded; this bounds the loop, for a unit that
       * reports running but never renders. */
      if (--laps < 0)
         break;
      coreaudio_wait(dev, want, 100);
   }
   return 0;
}

/* Enumerates output devices from the HAL. Needs no driver instance:
 * kAudioObjectSystemObject is always there. Same query as
 * coreaudio_choose_output_device() above, collected instead of
 * matched. iOS has no HAL device enumeration for output; NULL there. */
static void *coreaudio_device_list_new(void *data)
{
#if TARGET_OS_IPHONE
   (void)data;
   return NULL;
#else
   UInt32 i, device_count = 0;
   AudioDeviceID *devices;
   union string_list_elem_attr attr;
   struct string_list *sl = string_list_new();

   (void)data;
   attr.i = 0;
   if (!sl)
      return NULL;

   if (!(devices = coreaudio_hal_devices(&device_count)))
   {
      string_list_free(sl);
      return NULL;
   }

   for (i = 0; i < device_count; i++)
   {
      char device_name[1024];
      if (coreaudio_hal_device_name(devices[i], device_name, sizeof(device_name)))
         string_list_append(sl, device_name, attr);
   }

   free(devices);
   return sl;
#endif
}

static void coreaudio_device_list_free(void *data, void *array_list_data)
{
   struct string_list *sl = (struct string_list*)array_list_data;
   (void)data;
   if (sl)
      string_list_free(sl);
}

/* Frames the device has taken since the unit started.
 *
 * The render callback is the device asking for exactly one period, so
 * counting what it asks for counts device time - the same way the
 * WASAPI pump and the ASIO callback do it, and unlike ALSA, which has a
 * queue to subtract. Silence during an underrun counts: the period
 * elapsed whether or not there was audio for it, and this measures
 * elapsed device time rather than delivered audio.
 *
 * The ring is counted in samples, so the interleaved stereo the output
 * bus is fixed to makes a frame two of them. */
static size_t coreaudio_frames_consumed(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (!dev)
      return 0;

#if !TARGET_OS_IPHONE
   /* AudioDeviceGetCurrentTime() is one of the original HAL calls -
    * available since 10.0 and not among the ones deprecated in 10.5
    * or 10.6, which are the property accessors and the IOProc
    * registration. So this adds nothing to what the file already
    * needs; see the note on the macOS floor at the top.
    *
    * The device's own sample position, where the HAL will give it.
    * That is what this call is specified to return, and it is the
    * device that is asked rather than this driver's count of the
    * callbacks it was handed - which is close, a render callback
    * being device-paced, but is still an inference.
    *
    * It fails while the device is not running, which is every call
    * before the first render, so the counter answers until it
    * succeeds and the position is taken relative to where the clock
    * stood at that first success. */
   if (dev->device_id)
   {
      AudioTimeStamp ts;
      memset(&ts, 0, sizeof(ts));
      if (AudioDeviceGetCurrentTime(dev->device_id, &ts) == noErr
            && (ts.mFlags & kAudioTimeStampSampleTimeValid))
      {
         if (!dev->clock_based)
         {
            dev->clock_base  = ts.mSampleTime;
            dev->clock_based = true;
         }
         if (ts.mSampleTime >= dev->clock_base)
            return (size_t)(ts.mSampleTime - dev->clock_base);
      }
   }
#endif

   return retro_atomic_load_acquire_size(&dev->consumed) / dev->channels;
}

/* What this driver counted for itself: the frames each render
 * callback asked for, summed. Reported so the two can be compared -
 * a render callback is device-paced and this should track the
 * device's own clock closely, and where it does not the difference
 * is worth seeing. Never acted on. */
static size_t coreaudio_frames_consumed_fallback(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (!dev)
      return 0;
   return retro_atomic_load_acquire_size(&dev->consumed) / dev->channels;
}

audio_driver_t audio_coreaudio = {
   coreaudio_init,
   coreaudio_write,
   coreaudio_stop,
   coreaudio_start,
   coreaudio_alive,
   coreaudio_set_nonblock_state,
   coreaudio_free,
   coreaudio_use_float,
   "coreaudio",
   coreaudio_device_list_new,
   coreaudio_device_list_free,
   coreaudio_write_avail,
   coreaudio_buffer_size,
   NULL, /* write_raw: see the note on the int16 fast path above */
   coreaudio_wait_writable,
   coreaudio_frames_consumed,
   coreaudio_underruns,
   coreaudio_layout,
   coreaudio_frames_consumed_fallback
};


#if !TARGET_OS_IPHONE
/* The default output moving, in the two listener shapes. The unit
 * cannot be rebound from a HAL thread with the render callback live,
 * and rebinding at all means renegotiating the rate, the period, the
 * layout and the latency - so what happens here is what the WASAPI
 * device-change path does: the frontend is asked to reinitialise
 * audio, on its own thread, in its own time, and this driver is
 * built again against whatever the default now is. */
static void coreaudio_output_default_moved(coreaudio_t *dev)
{
   if (dev && dev->follows_default)
      retro_atomic_store_release_int(
            &audio_state_get_ptr()->reinit_request, 1);
}

static OSStatus coreaudio_output_default_listener(ca_obj_id_t obj,
      UInt32 n, const ca_addr_t *addrs, void *data)
{
   (void)obj;
   (void)n;
   (void)addrs;
   coreaudio_output_default_moved((coreaudio_t*)data);
   return noErr;
}

static OSStatus coreaudio_output_default_listener_old(UInt32 selector, void *data)
{
   (void)selector;
   coreaudio_output_default_moved((coreaudio_t*)data);
   return noErr;
}

static void coreaudio_listen_default_output(coreaudio_t *dev, bool on)
{
   ca_addr_t prop;
   if (!dev || on == dev->listening_default)
      return;
   ca_prop_resolve();
   prop.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
   prop.mScope    = CA_SCOPE_GLOBAL;
   prop.mElement  = CA_ELEMENT_MAIN;

   if (ca_prop.obj_addlis && ca_prop.obj_remlis)
   {
      if (on)
         ca_prop.obj_addlis(CA_SYSTEM_OBJECT, &prop,
               coreaudio_output_default_listener, dev);
      else
         ca_prop.obj_remlis(CA_SYSTEM_OBJECT, &prop,
               coreaudio_output_default_listener, dev);
   }
   else if (on)
   {
      if (!ca_prop.hw_addlis)
         return;
      ca_prop.hw_addlis(prop.mSelector,
            coreaudio_output_default_listener_old, dev);
   }
   else
   {
      if (!ca_prop.hw_remlis)
         return;
      ca_prop.hw_remlis(prop.mSelector,
            coreaudio_output_default_listener_old, dev);
   }
   dev->listening_default = on;
}
#endif /* !TARGET_OS_IPHONE */

#ifdef HAVE_MICROPHONE
/* =====================================================================
 * Microphone. One driver for macOS, iOS and tvOS, C like the rest of
 * the file: an input-enabled output unit - HALOutput or RemoteIO - whose
 * input callback pulls each slice with AudioUnitRender into a fifo the
 * frontend reads. macOS adds device selection by UID or name, an input
 * device list, and following the default input device when none was
 * named; iOS and tvOS add nothing but the session category, which is
 * Objective-C and lives in the Cocoa layer behind
 * cocoa_audio_session_begin_record().
 *
 * The capture queue is a lock-free SPSC ring (retro_spsc): the
 * real-time thread never waits and never drops a slice for contention.
 * The reader blocks on the condition the callback signals, timed, in
 * blocking mode.
 * ===================================================================== */
#include <retro_spsc.h>
#include <rthreads/rthreads.h>
#include <string.h>
#include "../microphone_driver.h"
#if TARGET_OS_IPHONE
#include "../../ui/drivers/cocoa/cocoa_audio_session.h"
#endif

typedef struct coreaudio_mic
{
   AudioUnit unit;
   /* Captured samples.  Single producer (the audio unit's IO thread in
    * coreaudio_mic_input_cb), single consumer (the core thread in
    * coreaudio_mic_read): a lock-free retro_spsc ring.  The callback
    * used to slock_try_lock() a mutex around a fifo and drop the whole
    * slice whenever the reader held it; with the ring it never waits
    * and never drops for contention.  lock/cond remain only for the
    * reader's timed wait.  retro_spsc rounds capacity up to a power of
    * two; ring_size is the latency-derived size asked for and the
    * producer never fills past it. */
   retro_spsc_t ring;
   size_t ring_size;
   bool ring_init;
   slock_t *lock;
   scond_t *cond;
   AudioStreamBasicDescription format;
   retro_atomic_int_t running;
   retro_atomic_int_t initialized;
   void *cb_buf;
   size_t cb_buf_size;
   unsigned sample_rate;
   bool nonblock;
   bool use_float;
#if !TARGET_OS_IPHONE
   AudioDeviceID device;
   bool follow_default;
   retro_atomic_int_t device_changed;
#endif
} coreaudio_mic_t;

/* Driver-wide context: created by init(), destroyed by free(). The
 * frontend holds it for as long as the driver is selected, longer than
 * any microphone opened through it, so it is never the same allocation
 * as one. It latches the non-blocking state so a mic opened later
 * inherits it. */
typedef struct coreaudio_mic_driver
{
   coreaudio_mic_t *mic;
   bool nonblock;
} coreaudio_mic_driver_t;

static void coreaudio_mic_close(void *driver_context, void *mic_context);
static bool coreaudio_mic_stop(void *driver_context, void *mic_context);

static void coreaudio_mic_set_format(coreaudio_mic_t *mic, bool use_float)
{
   AudioStreamBasicDescription *f = &mic->format;
   mic->use_float      = use_float;
   memset(f, 0, sizeof(*f));
   f->mSampleRate      = mic->sample_rate;
   f->mFormatID        = kAudioFormatLinearPCM;
   f->mFormatFlags     = use_float
      ? (kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked)
      : (kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked);
   f->mFramesPerPacket = 1;
   f->mChannelsPerFrame = 1; /* mono */
   f->mBitsPerChannel  = use_float ? 32 : 16;
   f->mBytesPerFrame   = f->mChannelsPerFrame * f->mBitsPerChannel / 8;
   f->mBytesPerPacket  = f->mBytesPerFrame * f->mFramesPerPacket;
}

/* The input callback, on the real-time thread. */
/* Discard whatever is buffered, from the consumer side: skipping
 * read_avail() bytes is safe against a live producer, unlike
 * retro_spsc_clear(), which requires both sides quiesced.  Every
 * caller has stopped the unit first anyway; this just does not
 * depend on AudioOutputUnitStop() having joined the IO thread. */
static void coreaudio_mic_drain(coreaudio_mic_t *mic)
{
   if (mic && mic->ring_init)
      retro_spsc_skip(&mic->ring, retro_spsc_read_avail(&mic->ring));
}

static OSStatus coreaudio_mic_input_cb(void *ref,
      AudioUnitRenderActionFlags *flags, const AudioTimeStamp *ts,
      UInt32 bus, UInt32 frames, AudioBufferList *io_data)
{
   coreaudio_mic_t *mic = (coreaudio_mic_t*)ref;
   AudioBufferList list;
   size_t bytes;

   (void)bus;
   (void)io_data;

   if (!mic || !retro_atomic_load_acquire_int(&mic->running))
      return noErr;

   bytes = (size_t)frames * mic->format.mBytesPerFrame;
   /* A slice larger than the buffer sized from MaximumFramesPerSlice
    * is dropped rather than allocated for on this thread. */
   if (!bytes || bytes > mic->cb_buf_size)
      return noErr;

   list.mNumberBuffers              = 1;
   list.mBuffers[0].mNumberChannels = mic->format.mChannelsPerFrame;
   list.mBuffers[0].mDataByteSize   = (UInt32)bytes;
   list.mBuffers[0].mData           = mic->cb_buf;

   if (AudioUnitRender(mic->unit, flags, ts, 1, frames, &list) == noErr
         && list.mBuffers[0].mData && list.mBuffers[0].mDataByteSize)
   {
      size_t got = list.mBuffers[0].mDataByteSize;
      if (got > bytes)
         got = bytes;
      /* Lock-free: the ring is SPSC and this thread is its only
       * producer.  Room is measured against ring_size, not the
       * rounded-up physical capacity.  Signalled without the lock,
       * as before; the reader's waits are timed so a signal raised
       * between its check and its wait costs at most one timeout. */
      size_t room   = retro_spsc_write_avail(&mic->ring);
      size_t excess = mic->ring.capacity - mic->ring_size;
      room          = room > excess ? room - excess : 0;
      if (room >= got)
         retro_spsc_write(&mic->ring, list.mBuffers[0].mData, got);
      scond_signal(mic->cond);
   }
   /* Always noErr: an error return can stop the callbacks for good. */
   return noErr;
}

#if !TARGET_OS_IPHONE
/* --- macOS: HAL input devices ------------------------------------- */

static bool coreaudio_mic_device_has_input(AudioDeviceID id)
{
   ca_addr_t prop;
   UInt32 size        = 0;
   prop.mSelector     = kAudioDevicePropertyStreams;
   prop.mScope        = kAudioDevicePropertyScopeInput;
   prop.mElement      = CA_ELEMENT_MAIN;
   return ca_prop_size(id, &prop, &size, false) == noErr
         && size > 0;
}

/* A CFString property of a device as UTF-8; false if absent. */
static bool coreaudio_mic_device_string(AudioDeviceID id,
      UInt32 selector, char *s, size_t len)
{
   ca_addr_t prop;
   CFStringRef cf = NULL;
   UInt32 size    = sizeof(cf);
   bool ok;
   prop.mSelector = selector;
   prop.mScope    = CA_SCOPE_GLOBAL;
   prop.mElement  = CA_ELEMENT_MAIN;
   s[0]           = 0;
   if (ca_prop_get(id, &prop, &size, &cf, false) != noErr || !cf)
      return false;
   ok = CFStringGetCString(cf, s, (CFIndex)len, kCFStringEncodingUTF8) && s[0];
   CFRelease(cf);
   return ok;
}

static AudioDeviceID coreaudio_mic_default_device(void)
{
   ca_addr_t prop;
   AudioDeviceID id = CA_OBJECT_UNKNOWN;
   UInt32 size      = sizeof(id);
   prop.mSelector   = kAudioHardwarePropertyDefaultInputDevice;
   prop.mScope      = CA_SCOPE_GLOBAL;
   prop.mElement    = CA_ELEMENT_MAIN;
   if (ca_prop_get(CA_SYSTEM_OBJECT, &prop, &size, &id, false) != noErr)
      return CA_OBJECT_UNKNOWN;
   return id;
}

/* The device list stores the UID beside each name so a chosen entry
 * survives renaming; a saved setting may carry either, so the lookup
 * tries the UID first and the name second. */
static AudioDeviceID coreaudio_mic_find_device(const char *uid_or_name)
{
   UInt32 i, count = 0, pass;
   AudioDeviceID found = CA_OBJECT_UNKNOWN;
   AudioDeviceID *devices;

   if (string_is_empty(uid_or_name) || string_is_equal(uid_or_name, "default"))
      return CA_OBJECT_UNKNOWN;
   if (!(devices = coreaudio_hal_devices(&count)))
      return CA_OBJECT_UNKNOWN;

   for (pass = 0; pass < 2 && found == CA_OBJECT_UNKNOWN; pass++)
   {
      for (i = 0; i < count; i++)
      {
         char s[256];
         if (!coreaudio_mic_device_has_input(devices[i]))
            continue;
         if (     coreaudio_mic_device_string(devices[i],
                  pass == 0 ? kAudioDevicePropertyDeviceUID
                            : kAudioDevicePropertyDeviceNameCFString,
                  s, sizeof(s))
               && string_is_equal(s, uid_or_name))
         {
            found = devices[i];
            break;
         }
      }
   }

   free(devices);
   if (found == CA_OBJECT_UNKNOWN)
      RARCH_WARN("[CoreAudio] Input device \"%s\" not found; using the default.\n",
            uid_or_name);
   return found;
}


/* Called by the HAL on a thread of its own when the default input
 * device changes; the reconnect happens on the reader's thread. One
 * for each shape of listener the HAL has had - the newer is handed
 * the object and what changed on it, the older only the selector -
 * and both do the same thing with it. */
static OSStatus coreaudio_mic_default_listener(ca_obj_id_t obj,
      UInt32 n, const ca_addr_t *addrs, void *data)
{
   coreaudio_mic_t *mic = (coreaudio_mic_t*)data;
   (void)obj;
   (void)n;
   (void)addrs;
   if (mic)
      retro_atomic_store_release_int(&mic->device_changed, 1);
   return noErr;
}

static OSStatus coreaudio_mic_default_listener_old(UInt32 selector, void *data)
{
   coreaudio_mic_t *mic = (coreaudio_mic_t*)data;
   (void)selector;
   if (mic)
      retro_atomic_store_release_int(&mic->device_changed, 1);
   return noErr;
}

static void coreaudio_mic_listen_default(coreaudio_mic_t *mic, bool on)
{
   ca_addr_t prop;
   ca_prop_resolve();
   prop.mSelector = kAudioHardwarePropertyDefaultInputDevice;
   prop.mScope    = CA_SCOPE_GLOBAL;
   prop.mElement  = CA_ELEMENT_MAIN;

   if (ca_prop.obj_addlis && ca_prop.obj_remlis)
   {
      if (on)
         ca_prop.obj_addlis(CA_SYSTEM_OBJECT, &prop,
               coreaudio_mic_default_listener, mic);
      else
         ca_prop.obj_remlis(CA_SYSTEM_OBJECT, &prop,
               coreaudio_mic_default_listener, mic);
      return;
   }

   if (on)
   {
      if (ca_prop.hw_addlis)
         ca_prop.hw_addlis(prop.mSelector,
               coreaudio_mic_default_listener_old, mic);
   }
   else if (ca_prop.hw_remlis)
      ca_prop.hw_remlis(prop.mSelector,
            coreaudio_mic_default_listener_old, mic);
}

/* Move a mic that follows the default onto the new default, keeping
 * the format - and so the rate reported to the core - it opened with;
 * the unit's own converter covers any difference to the new hardware. */
static void coreaudio_mic_reconnect(coreaudio_mic_t *mic)
{
   AudioDeviceID dev = coreaudio_mic_default_device();
   bool was_running;

   if (dev == CA_OBJECT_UNKNOWN || dev == mic->device)
      return;
   RARCH_LOG("[CoreAudio] Default input device changed; reconnecting.\n");

   was_running = retro_atomic_load_acquire_int(&mic->running) != 0;
   if (was_running)
      AudioOutputUnitStop(mic->unit);
   retro_atomic_store_release_int(&mic->running, 0);
   if (retro_atomic_load_acquire_int(&mic->initialized))
   {
      AudioUnitUninitialize(mic->unit);
      retro_atomic_store_release_int(&mic->initialized, 0);
   }

   mic->device = dev;
   if (AudioUnitSetProperty(mic->unit, kAudioOutputUnitProperty_CurrentDevice,
            kAudioUnitScope_Global, 0, &dev, sizeof(dev)) != noErr)
      return;
   AudioUnitSetProperty(mic->unit, kAudioUnitProperty_StreamFormat,
         kAudioUnitScope_Output, 1, &mic->format, sizeof(mic->format));
   if (AudioUnitInitialize(mic->unit) != noErr)
      return;
   retro_atomic_store_release_int(&mic->initialized, 1);

   coreaudio_mic_drain(mic);

   if (was_running && AudioOutputUnitStart(mic->unit) == noErr)
      retro_atomic_store_release_int(&mic->running, 1);
}
#endif /* !TARGET_OS_IPHONE */

/* --- The driver ------------------------------------------------------ */

static void *coreaudio_mic_init(void)
{
   return calloc(1, sizeof(coreaudio_mic_driver_t));
}

static void coreaudio_mic_free(void *driver_context)
{
   coreaudio_mic_driver_t *drv = (coreaudio_mic_driver_t*)driver_context;
   if (!drv)
      return;
   /* The frontend closes every microphone first; do not leak one
    * should that ever stop being true. */
   if (drv->mic)
      coreaudio_mic_close(drv, drv->mic);
   free(drv);
}

/* Sleeps until the capture fifo holds len bytes, then says how many it
 * holds. The same bounded wait coreaudio_mic_read() does - one slice of
 * the render callback's period - without the copy, so a unit that has
 * stopped delivering returns what it has and the caller comes back
 * later rather than parking here. */
static size_t coreaudio_mic_wait_readable(void *driver_context,
      void *mic_context, size_t len)
{
   coreaudio_mic_t *mic = (coreaudio_mic_t*)mic_context;
   size_t avail;

   if (!mic || !mic->ring_init)
      return 0;

   avail = retro_spsc_read_avail(&mic->ring);
   if (avail < len)
   {
      slock_lock(mic->lock);
      scond_wait_timeout(mic->cond, mic->lock, 10000);
      slock_unlock(mic->lock);
      avail = retro_spsc_read_avail(&mic->ring);
   }
   return avail;
}

static int coreaudio_mic_read(void *driver_context, void *mic_context,
      void *buf, size_t len)
{
   coreaudio_mic_t *mic = (coreaudio_mic_t*)mic_context;
   size_t avail, n;

   (void)driver_context;
   if (!mic || !mic->ring_init || !buf)
      return -1;

#if !TARGET_OS_IPHONE
   /* A pending default-device change, before the lock is taken. */
   if (retro_atomic_load_acquire_int(&mic->device_changed))
   {
      retro_atomic_store_release_int(&mic->device_changed, 0);
      coreaudio_mic_reconnect(mic);
   }
#endif

   avail = retro_spsc_read_avail(&mic->ring);
   n     = avail < len ? avail : len;
   if (!n && !mic->nonblock)
   {
      /* Blocking: one slice's worth of wait for the callback, timed. */
      slock_lock(mic->lock);
      scond_wait_timeout(mic->cond, mic->lock, 10000);
      slock_unlock(mic->lock);
      avail = retro_spsc_read_avail(&mic->ring);
      n     = avail < len ? avail : len;
   }
   if (n)
      retro_spsc_read(&mic->ring, buf, n);
   return (int)n;
}

static void coreaudio_mic_set_nonblock_state(void *driver_context, bool state)
{
   coreaudio_mic_driver_t *drv = (coreaudio_mic_driver_t*)driver_context;
   if (!drv)
      return;
   drv->nonblock = state;
   if (drv->mic)
      drv->mic->nonblock = state;
}

static struct string_list *coreaudio_mic_device_list_new(const void *driver_context)
{
#if TARGET_OS_IPHONE
   (void)driver_context;
   return NULL;
#else
   UInt32 i, count = 0;
   AudioDeviceID *devices;
   struct string_list *sl;
   union string_list_elem_attr attr;

   (void)driver_context;
   if (!(sl = string_list_new()))
      return NULL;
   if (!(devices = coreaudio_hal_devices(&count)))
   {
      string_list_free(sl);
      return NULL;
   }

   for (i = 0; i < count; i++)
   {
      char name[256], uid[256];
      if (!coreaudio_mic_device_has_input(devices[i]))
         continue;
      if (     !coreaudio_mic_device_string(devices[i],
                  kAudioDevicePropertyDeviceNameCFString, name, sizeof(name))
            || !coreaudio_mic_device_string(devices[i],
                  kAudioDevicePropertyDeviceUID, uid, sizeof(uid)))
         continue;
      /* The UID rides along in attr.p, owned by the list; see the
       * free below. */
      if (!(attr.p = strdup(uid)))
         continue;
      if (!string_list_append(sl, name, attr))
         free(attr.p);
   }

   free(devices);
   if (!sl->size)
   {
      string_list_free(sl);
      return NULL;
   }
   return sl;
#endif
}

static void coreaudio_mic_device_list_free(const void *driver_context,
      struct string_list *sl)
{
   size_t i;
   (void)driver_context;
   if (!sl)
      return;
   for (i = 0; i < sl->size; i++)
   {
      if (sl->elems[i].attr.p)
         free(sl->elems[i].attr.p);
      sl->elems[i].attr.p = NULL;
   }
   string_list_free(sl);
}

static void *coreaudio_mic_open(void *driver_context, const char *device,
      unsigned rate, unsigned latency, unsigned *new_rate)
{
   coreaudio_mic_driver_t *drv = (coreaudio_mic_driver_t*)driver_context;
   coreaudio_mic_t *mic;
   ca_component_desc_t desc = {0};
   AURenderCallbackStruct cb;
   void *comp;
   UInt32 one = 1, zero = 0, max_frames = 4096, max_frames_size = sizeof(max_frames);
   size_t fifo_size;

   if (!drv)
      return NULL;
   if (drv->mic)
   {
      RARCH_WARN("[CoreAudio] A microphone is already open; closing it first.\n");
      coreaudio_mic_close(drv, drv->mic);
   }
   if (!ca_cm_resolve())
      return NULL;
   if (!(mic = (coreaudio_mic_t*)calloc(1, sizeof(*mic))))
      return NULL;

   retro_atomic_int_init(&mic->running, 0);
   retro_atomic_int_init(&mic->initialized, 0);
   mic->lock        = slock_new();
   mic->cond        = scond_new();
   mic->sample_rate = rate;
   mic->nonblock    = drv->nonblock;
   if (!mic->lock || !mic->cond)
      goto error;

#if TARGET_OS_IPHONE
   /* The session must be in a record category before the unit will
    * capture, and its rate is the unit's rate. Objective-C, in Cocoa. */
   {
      unsigned actual = 0;
      if (!cocoa_audio_session_begin_record(rate, &actual))
         goto error;
      if (actual)
         mic->sample_rate = actual;
   }
#else
   retro_atomic_int_init(&mic->device_changed, 0);
   mic->device = coreaudio_mic_find_device(device);
   if (mic->device == CA_OBJECT_UNKNOWN)
   {
      mic->device         = coreaudio_mic_default_device();
      mic->follow_default = true;
   }
#endif

   /* The unit: input enabled on bus 1, output disabled on bus 0. */
   desc.type         = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
   desc.subtype      = kAudioUnitSubType_RemoteIO;
#else
   desc.subtype      = kAudioUnitSubType_HALOutput;
#endif
   desc.manufacturer = kAudioUnitManufacturer_Apple;
   if (!(comp = ca_cm.find_next(NULL, &desc)))
      goto error;
   if (ca_cm.open(comp, &mic->unit) != noErr || !mic->unit)
      goto error;
   if (AudioUnitSetProperty(mic->unit, kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input, 1, &one, sizeof(one)) != noErr)
      goto error;
   AudioUnitSetProperty(mic->unit, kAudioOutputUnitProperty_EnableIO,
         kAudioUnitScope_Output, 0, &zero, sizeof(zero));

#if !TARGET_OS_IPHONE
   if (mic->device != CA_OBJECT_UNKNOWN
         && AudioUnitSetProperty(mic->unit, kAudioOutputUnitProperty_CurrentDevice,
            kAudioUnitScope_Global, 0, &mic->device, sizeof(mic->device)) != noErr)
   {
      RARCH_WARN("[CoreAudio] Input device %u refused; using the default.\n",
            (unsigned)mic->device);
      mic->device = CA_OBJECT_UNKNOWN;
   }
   {
      AudioDeviceID actual = CA_OBJECT_UNKNOWN;
      UInt32 size          = sizeof(actual);
      if (AudioUnitGetProperty(mic->unit, kAudioOutputUnitProperty_CurrentDevice,
               kAudioUnitScope_Global, 0, &actual, &size) == noErr)
         mic->device = actual;
   }
#endif

   /* The client format on the output side of the input bus: mono int16
    * at the device's own rate, so the unit converts nothing it need not. */
   coreaudio_mic_set_format(mic, false);
#if !TARGET_OS_IPHONE
   {
      AudioStreamBasicDescription hw = {0};
      UInt32 size = sizeof(hw);
      if (AudioUnitGetProperty(mic->unit, kAudioUnitProperty_StreamFormat,
               kAudioUnitScope_Input, 1, &hw, &size) == noErr && hw.mSampleRate > 0)
      {
         mic->sample_rate         = (unsigned)hw.mSampleRate;
         mic->format.mSampleRate  = hw.mSampleRate;
      }
   }
#endif
   if (AudioUnitSetProperty(mic->unit, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output, 1, &mic->format, sizeof(mic->format)) != noErr)
      goto error;

   cb.inputProc       = coreaudio_mic_input_cb;
   cb.inputProcRefCon = mic;
   if (AudioUnitSetProperty(mic->unit, kAudioOutputUnitProperty_SetInputCallback,
            kAudioUnitScope_Global, 0, &cb, sizeof(cb)) != noErr)
      goto error;

#if !TARGET_OS_IPHONE
   /* A short device period for the capture side. Advisory. */
   {
      UInt32 period = 256;
      AudioUnitSetProperty(mic->unit, kAudioDevicePropertyBufferFrameSize,
            kAudioUnitScope_Global, 0, &period, sizeof(period));
   }
#endif

   if (AudioUnitInitialize(mic->unit) != noErr)
      goto error;
   retro_atomic_store_release_int(&mic->initialized, 1);

   /* The render buffer, sized from what the unit may hand over per
    * slice, so the callback allocates nothing. */
   AudioUnitGetProperty(mic->unit, kAudioUnitProperty_MaximumFramesPerSlice,
         kAudioUnitScope_Global, 0, &max_frames, &max_frames_size);
   mic->cb_buf_size = (size_t)max_frames * mic->format.mBytesPerFrame;
   if (!(mic->cb_buf = calloc(1, mic->cb_buf_size)))
      goto error;

   fifo_size = (size_t)latency * mic->sample_rate * mic->format.mBytesPerFrame / 1000;
   if (!fifo_size)
      fifo_size = (size_t)mic->sample_rate * mic->format.mBytesPerFrame / 10;
   mic->ring_size = fifo_size;
   if (!(mic->ring_init = retro_spsc_init(&mic->ring, fifo_size)))
      goto error;

#if !TARGET_OS_IPHONE
   if (mic->follow_default)
      coreaudio_mic_listen_default(mic, true);
#endif

   RARCH_LOG("[CoreAudio] Microphone open: %u Hz mono %s, %u ms fifo.\n",
         mic->sample_rate, mic->use_float ? "float" : "int16", latency);
   if (new_rate)
      *new_rate = mic->sample_rate;
   drv->mic = mic;
   return mic;

error:
   RARCH_ERR("[CoreAudio] Failed to open the microphone.\n");
   /* close() takes a handle that was only partly built. */
   coreaudio_mic_close(drv, mic);
   return NULL;
}

static void coreaudio_mic_close(void *driver_context, void *mic_context)
{
   coreaudio_mic_driver_t *drv = (coreaudio_mic_driver_t*)driver_context;
   coreaudio_mic_t *mic        = (coreaudio_mic_t*)mic_context;
   if (!mic)
      return;

#if !TARGET_OS_IPHONE
   if (mic->follow_default)
      coreaudio_mic_listen_default(mic, false);
#endif
   coreaudio_mic_stop(drv, mic);

   /* Stopping does not wait for a callback in flight; disposing does,
    * so everything the callback touches outlives the dispose. */
   if (mic->unit)
   {
      if (retro_atomic_load_acquire_int(&mic->initialized))
         AudioUnitUninitialize(mic->unit);
      ca_cm.close(mic->unit);
      mic->unit = NULL;
   }
   if (mic->cb_buf)
      free(mic->cb_buf);
   if (mic->ring_init)
      retro_spsc_free(&mic->ring);
   if (mic->lock)
      slock_free(mic->lock);
   if (mic->cond)
      scond_free(mic->cond);
   if (drv && drv->mic == mic)
      drv->mic = NULL;
   free(mic);
}

static bool coreaudio_mic_alive(const void *driver_context, const void *mic_context)
{
   const coreaudio_mic_t *mic = (const coreaudio_mic_t*)mic_context;
   (void)driver_context;
   return mic && retro_atomic_load_acquire_int(&mic->running);
}

static bool coreaudio_mic_start(void *driver_context, void *mic_context)
{
   coreaudio_mic_t *mic = (coreaudio_mic_t*)mic_context;
   (void)driver_context;
   if (!mic || !mic->unit || !retro_atomic_load_acquire_int(&mic->initialized))
      return false;
   if (retro_atomic_load_acquire_int(&mic->running))
      return true;
   coreaudio_mic_drain(mic);
   if (AudioOutputUnitStart(mic->unit) != noErr)
   {
      RARCH_ERR("[CoreAudio] Failed to start the microphone.\n");
      return false;
   }
   retro_atomic_store_release_int(&mic->running, 1);
   return true;
}

static bool coreaudio_mic_stop(void *driver_context, void *mic_context)
{
   coreaudio_mic_t *mic = (coreaudio_mic_t*)mic_context;
   OSStatus status;
   (void)driver_context;
   if (!mic || !mic->unit || !retro_atomic_load_acquire_int(&mic->running))
      return true;
   status = AudioOutputUnitStop(mic->unit);
   /* Not running from here on either way. */
   retro_atomic_store_release_int(&mic->running, 0);
   coreaudio_mic_drain(mic);
   return status == noErr;
}

static bool coreaudio_mic_use_float(const void *driver_context, const void *mic_context)
{
   const coreaudio_mic_t *mic = (const coreaudio_mic_t*)mic_context;
   (void)driver_context;
   return mic && mic->use_float;
}

microphone_driver_t microphone_coreaudio = {
   coreaudio_mic_init,
   coreaudio_mic_free,
   coreaudio_mic_read,
   coreaudio_mic_set_nonblock_state,
   "coreaudio",
   coreaudio_mic_device_list_new,
   coreaudio_mic_device_list_free,
   coreaudio_mic_open,
   coreaudio_mic_close,
   coreaudio_mic_alive,
   coreaudio_mic_start,
   coreaudio_mic_stop,
   coreaudio_mic_use_float,
   coreaudio_mic_wait_readable
};
#endif /* HAVE_MICROPHONE */
