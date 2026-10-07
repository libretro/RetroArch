/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
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

#ifndef __APPLE_RUNTIME_H
#define __APPLE_RUNTIME_H

/* Runtime replacement for @available().
 *
 * @available needs a clang new enough to know the syntax (Xcode 9+)
 * and calls __isPlatformVersionAtLeast() every time it is evaluated.
 * The helpers here compile on any toolchain down to the ancient ones
 * building the legacy iOS/macOS targets, work on any OS version at
 * runtime, and resolve the OS version exactly once per process -
 * afterwards every check is a cached integer compare.
 */

#include <TargetConditionals.h>
#import <Foundation/Foundation.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

/* Encodes a version as major * 10000 + minor * 100 + patch */
#define APPLE_RUNTIME_VER(maj, min, pat) ((maj) * 10000 + (min) * 100 + (pat))

/* Returns the running OS version, APPLE_RUNTIME_VER-encoded.
 * Resolved once; the benign race on the static is idempotent. */
/* unused: a unit may include this header for the macros alone. */
static inline __attribute__((unused)) int apple_runtime_os_version(void)
{
   static int ver = -1;
   if (ver == -1)
   {
      int part[3]   = {0, 0, 0};
      int i         = 0;
      const char *s = NULL;
#if TARGET_OS_IPHONE
      /* Never shimmed, works on any iOS/tvOS version */
      s = [[[UIDevice currentDevice] systemVersion] UTF8String];
#else
      /* kern.osproductversion (10.13.4+) reports the real OS version;
       * the NSProcessInfo APIs are subject to the 10.16 compatibility
       * shim for binaries linked against pre-11.0 SDKs. */
      char buf[64];
      size_t len = sizeof(buf);
      if (sysctlbyname("kern.osproductversion", buf, &len, NULL, 0) == 0)
         s = buf;
      else
      {
         /* Pre-10.13.4 fallback: "Version 10.9.5 (Build 13F34)" -
          * skip ahead to the first digit */
         s = [[[NSProcessInfo processInfo] operatingSystemVersionString]
               UTF8String];
         while (s && *s && (*s < '0' || *s > '9'))
            s++;
      }
#endif
      if (s)
      {
         for (; *s && i < 3; s++)
         {
            if (*s >= '0' && *s <= '9')
               part[i] = part[i] * 10 + (*s - '0');
            else if (*s == '.')
               i++;
            else
               break;
         }
      }
      ver = APPLE_RUNTIME_VER(part[0], part[1], part[2]);
#if !TARGET_OS_IPHONE
      /* Belt and braces: if the shim still got to us, 10.16 == 11.0 */
      if (ver == APPLE_RUNTIME_VER(10, 16, 0))
         ver = APPLE_RUNTIME_VER(11, 0, 0);
#endif
   }
   return ver;
}

/* Drop-in replacement for if (@available(macOS a, iOS b, tvOS c, *)).
 * Pass 0 for platforms the original check did not list: @available
 * returns true on platforms not named (that is the '*'), and any
 * real OS version is >= 0. */
#if defined(TARGET_OS_TV) && TARGET_OS_TV
#define apple_runtime_available(macos_v, ios_v, tvos_v) \
   (apple_runtime_os_version() >= (tvos_v))
#elif TARGET_OS_IPHONE
#define apple_runtime_available(macos_v, ios_v, tvos_v) \
   (apple_runtime_os_version() >= (ios_v))
#else
#define apple_runtime_available(macos_v, ios_v, tvos_v) \
   (apple_runtime_os_version() >= (macos_v))
#endif

/* Message sends the compiler does not see.
 *
 * One binary runs from Mac OS X 10.4 / iOS 4.0 up to the current
 * release, so an API newer than the deployment floor is called only
 * behind an apple_runtime_available() check.  Written as a plain
 * message expression that call still trips -Wunguarded-availability,
 * and on an SDK too old to declare the method it does not compile at
 * all.  These macros send the message through objc_msgSend with a
 * selector looked up by name, which is the very call a message
 * expression compiles to: nothing is checked, nothing is slower, and
 * being macros there is no inlining for a compiler to decline.
 *
 * Cache the SEL in a static where a send sits on a hot path;
 * sel_registerName is a table lookup, @selector() is free but needs
 * the SDK to declare the method. */
#include <dlfcn.h>
#include <objc/runtime.h>
#include <objc/message.h>

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#define apple_rt_send_void(obj, sel) \
   ((void (*)(id, SEL))objc_msgSend)((id)(obj), (sel))
#define apple_rt_send_bool(obj, sel, v) \
   ((void (*)(id, SEL, BOOL))objc_msgSend)((id)(obj), (sel), (BOOL)(v))
#define apple_rt_send_id(obj, sel, v) \
   ((void (*)(id, SEL, id))objc_msgSend)((id)(obj), (sel), (id)(v))
#define apple_rt_get_id(obj, sel) \
   ((id (*)(id, SEL))objc_msgSend)((id)(obj), (sel))
#define apple_rt_get_long(obj, sel) \
   ((long (*)(id, SEL))objc_msgSend)((id)(obj), (sel))

/* i386 returns floating point on the x87 stack and needs the _fpret
 * entry; every other ABI returns it in a register through objc_msgSend. */
#if defined(__i386__)
#define apple_rt_get_double(obj, sel) \
   ((double (*)(id, SEL))objc_msgSend_fpret)((id)(obj), (sel))
#else
#define apple_rt_get_double(obj, sel) \
   ((double (*)(id, SEL))objc_msgSend)((id)(obj), (sel))
#endif

/* A class by name, nil when this OS (or this binary) does not have it. */
#define apple_rt_class(name) ((id)objc_getClass(name))

/* The address of an exported object constant (an NSString * such as
 * AVCaptureDeviceTypeExternal) by name, NULL when this OS does not
 * export it; apple_rt_obj_at reads the object, which stays the
 * framework's own. */
#define apple_rt_constant_addr(name) ((void **)dlsym(RTLD_DEFAULT, (name)))
#if __has_feature(objc_arc)
#define apple_rt_obj_at(p) ((__bridge id)*(p))
#else
#define apple_rt_obj_at(p) ((id)*(p))
#endif

#endif /* __APPLE_RUNTIME_H */
