/*  RetroArch - A frontend for libretro.
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

#import <TargetConditionals.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif
#include <stddef.h>
#include <string.h>
#include "../verbosity.h"
#include "../video_display_server.h"
#include "../modeline/modeline_edid.h"
#include "../video_driver.h"
#include "../../ui/drivers/cocoa/apple_platform.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include "../../configuration.h"
/* RARCH_RELEASE, and the NSWindowStyleMask* polyfills the
 * fullscreen check below still uses. */
#include <defines/cocoa_defines.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
/* ApplicationServices on every SDK rather than <CoreGraphics/CoreGraphics.h>
 * on some. The CoreGraphics umbrella is 10.8 and later; the
 * ApplicationServices one re-exports the same types and has since
 * 10.0, so one include reaches them everywhere and the SDK the build
 * happens to use stops deciding. */
#include <AvailabilityMacros.h>
#import <ApplicationServices/ApplicationServices.h>
/* IOKit's display registry, for the EDID: IODisplayCreateInfoDictionary
 * has published the block under kIODisplayEDIDKey since 10.0 and IOKit
 * is already linked for the power-source query. On Apple Silicon the
 * IODisplayConnect services this walks do not exist (the display stack
 * moved to DCP) and Apple published no replacement, so the walk finds
 * nothing and the op reports -1 there. */
#include <dlfcn.h>
#include <math.h>
#include <mach/mach_time.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
/* RARCH_HAS_CGDISPLAYMODE_API is defined in cocoa_common.h.  The
 * CGDisplayModeRef family (CGDisplayCopyAllDisplayModes,
 * CGDisplayModeGetWidth, CGDisplaySetDisplayMode, ...) arrived in
 * 10.6 Snow Leopard.  The 10.5 SDK only offers the older
 * CGDisplayAvailableModes / CFDictionaryRef path, which is a
 * different enough API that we just stub the resolution list on
 * pre-10.6 targets rather than port to both. */
#endif

#if TARGET_OS_OSX
static bool apple_display_server_set_window_opacity(void *data, unsigned opacity)
{
   settings_t *settings      = config_get_ptr();
   bool windowed_full        = settings->bools.video_windowed_fullscreen;
   NSWindow *window          = [((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]) window];
   if (windowed_full || ![window isKeyWindow])
      return false;
   [window setAlphaValue:(CGFloat)opacity / (CGFloat)100.0f];
   return true;
}

/* The dock tile's progress bar.
 *
 * Built once, on first use. That used to be dispatch_once() with an
 * Objective-C block, which put the function behind a 10.6 gate - and
 * behind RARCH_HAS_CGDISPLAYMODE_API, a macro about the display-mode
 * API that happened to also mean "10.6 SDK", so the feature was
 * absent from builds for reasons unrelated to anything it does.
 * Neither the once nor the block earned that. dispatch_once's fast
 * path is an acquire load and a compare; a plain flag is a load and a
 * compare, and the acquire is for publishing to another thread, which
 * does not arise here: this runs from a task callback on the main
 * thread, and every call below it is AppKit, which must be on the
 * main thread anyway. The block is worse than the once, being a
 * compiler feature rather than a runtime one - GCC 4.0, which is what
 * Xcode 3.1 has, cannot parse it at all.
 *
 * NSDockTile is 10.5, so -dockTile is asked for rather than assumed,
 * and the tile is held as id so no SDK needs to declare the class.
 * On 10.4 the selector is absent, the function reports failure once
 * and the frontend carries on without a dock progress bar, which is
 * what it did on those systems when this was compiled out. */
static bool apple_display_server_set_window_progress(void *data, int progress, bool finished)
{
   static NSProgressIndicator *indicator;
   static bool tried;

   if (!tried)
   {
      tried = true;

      if ([NSApp respondsToSelector:@selector(dockTile)])
      {
         id dock_tile    = [NSApp performSelector:@selector(dockTile)];
         NSImageView *iv = [[NSImageView alloc] init];
         /* -[NSDockTile size] returns a struct, which performSelector:
          * cannot carry, and the class is not declared on a 10.4 SDK so
          * it cannot be sent directly either. KVC boxes NSSize, which
          * gets it across without either. The tile has been 128x128
          * since dock tiles existed, so a nil there is not worth
          * failing over. */
         NSValue *size_v = [dock_tile valueForKey:@"size"];
         NSSize size     = size_v ? [size_v sizeValue] : NSMakeSize(128, 128);

         [iv setImage:[[NSApplication sharedApplication] applicationIconImage]];
         [dock_tile performSelector:@selector(setContentView:) withObject:iv];

         indicator = [[NSProgressIndicator alloc]
               initWithFrame:NSMakeRect(0, 0, size.width, 20)];
         [indicator setIndeterminate:NO];
         [indicator setMinValue:0];
         [indicator setMaxValue:100];
         [indicator setDoubleValue:0];

         [iv addSubview:indicator];

         /* This file is compiled under BOTH build systems:
          *   - qb/top-level Makefile:  MRC (not in the per-file ARC
          *     override list at Makefile:275 alongside metal.o /
          *     mfi_joypad.o).
          *   - pkg/apple/<anyfile>.xcodeproj:  ARC, via griffin_objc.m which
          *     #include's this file into a TU compiled with
          *     CLANG_ENABLE_OBJC_ARC=YES (pkg/apple/BaseConfig.xcconfig:182).
          *
          * Under MRC, 'iv' is a local +1 from alloc+init.  setContentView:
          * and addSubview: each retain their own reference, so those are
          * legitimately owned; the +1 from alloc+init is ours to release
          * before the local goes out of scope, or it leaks for the app's
          * lifetime.  Under ARC, the compiler inserts the matching release
          * at scope exit automatically.
          *
          * RARCH_RELEASE handles both: MRC -> [(x) release], ARC -> ((void)0).
          * Raw '[iv release]' is a compile error under ARC
          * ('ARC forbids explicit message send of release') which the
          * Xcode build would have flagged immediately.
          *
          * 'indicator' is intentionally kept +1 - it is a file-scope
          * static holding that +1 across the app lifetime, which is how
          * it stays referenced for the setDoubleValue / setHidden calls
          * below. */
         RARCH_RELEASE(iv);
      }
   }

   if (!indicator)
      return false;

   if (finished)
      [indicator setDoubleValue:(double)-1];
   else
      [indicator setDoubleValue:(double)progress];
   [indicator setHidden:finished];
   [[NSApp performSelector:@selector(dockTile)] performSelector:@selector(display)];
   return true;
}

static bool apple_display_server_set_window_decorations(void *data, bool on)
{
   settings_t *settings      = config_get_ptr();
   bool windowed_full        = settings->bools.video_windowed_fullscreen;
   NSWindow *window          = [((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]) window];
   if (windowed_full)
      return false;
   /* -setStyleMask: is 10.6; before that a window's style is fixed at
    * creation and there is nothing to toggle, so it is asked for rather
    * than sent. The constant needs no such care: cocoa_defines.h maps
    * NSWindowStyleMaskTitled to NSTitledWindowMask, its name before
    * 10.12, and that one is 10.0. */
   if (![window respondsToSelector:@selector(setStyleMask:)])
      return false;
   if (on)
      [window setStyleMask:([window styleMask] | NSWindowStyleMaskTitled)];
   else
      [window setStyleMask:([window styleMask] & ~NSWindowStyleMaskTitled)];
   return true;
}
#endif

#if TARGET_OS_OSX && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
static bool apple_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz,
      int center, int monitor_index, int xoffset, int padjust)
{
   CocoaView *view = [CocoaView get];
   if (@available(macOS 14.0, *))
   {
      if (!view || !view.displayLink)
      {
         RARCH_WARN("[Video] CocoaView not ready, skipping refresh rate change to %.3f Hz\n", hz);
         return false;
      }
   }
   else
   {
      RARCH_WARN("[Video] displayLink not supported on this macOS version, skipping refresh rate change to %.3f Hz\n", hz);
      return false;
   }

   /* macOS: Support resolution changes in addition to refresh rate */
   if (width > 0 && height > 0)
   {
      CGDirectDisplayID mainDisplayID = CGMainDisplayID();
      CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(mainDisplayID, NULL);
      CGDisplayModeRef bestMode = NULL;

      /* Returns NULL for an invalid display, which CGMainDisplayID()
       * can hand back on a headless Mac or across a hotplug between
       * the two calls.  CFArrayGetCount(NULL) is not tolerant. */
      if (!displayModes)
      {
         RARCH_ERR("[Video] Could not enumerate display modes\n");
         return false;
      }

      RARCH_LOG("[Video] Looking for display mode: %ux%u @ %.3f Hz\n", width, height, hz);

      /* Find the best matching display mode */
      for (CFIndex i = 0; i < CFArrayGetCount(displayModes); i++)
      {
         CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
         size_t modeWidth = CGDisplayModeGetWidth(mode);
         size_t modeHeight = CGDisplayModeGetHeight(mode);
         double refreshRate = CGDisplayModeGetRefreshRate(mode);

         /* Exact match preferred */
         if (modeWidth == width && modeHeight == height && fabs(refreshRate - hz) < 0.1)
         {
            bestMode = mode;
            break;
         }
         /* Fallback: match resolution, any refresh rate */
         else if (modeWidth == width && modeHeight == height && !bestMode)
            bestMode = mode;
      }

      if (bestMode)
      {
         CGError result = CGDisplaySetDisplayMode(mainDisplayID, bestMode, NULL);
         if (result == kCGErrorSuccess)
         {
            RARCH_LOG("[Video] Successfully changed display mode to %ux%u @ %.3f Hz\n",
                     width, height, hz);

            /* Notify the window and video context about the resolution change */
            NSWindow *window = ((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]).window;
            if (window)
            {
               /* Force the window to update its backing store */
               [[window contentView] setNeedsDisplay:YES];

               /* If fullscreen, update the window frame to match new resolution */
               if ((window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen)
               {
                  NSScreen *screen = [NSScreen mainScreen];
                  [window setFrame:screen.frame display:YES];
               }

               /* Notify the view about the change */
               CocoaView *cView = [CocoaView get];
               if (cView)
               {
                  [cView setNeedsDisplay:YES];
                  [cView setFrame:[[window contentView] bounds]];
               }
            }
         }
         else
         {
            RARCH_ERR("[Video] Failed to change display mode: CGError %d\n", result);
            CFRelease(displayModes);
            return false;
         }
      }
      else
      {
         RARCH_WARN("[Video] No matching display mode found for %ux%u @ %.3f Hz\n",
                    width, height, hz);
         CFRelease(displayModes);
         return false;
      }

      CFRelease(displayModes);
   }
   else
      RARCH_DBG("[Video] Setting refresh rate to %.3f Hz (no resolution change)\n", hz);

   /* Set refresh rate for display link */
   if (@available(macOS 14, *))
      view.displayLink.preferredFrameRateRange = CAFrameRateRangeMake(hz * 0.9, hz * 1.2, hz);
   return true;
}
#elif TARGET_OS_IPHONE
static bool apple_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz,
      int center, int monitor_index, int xoffset, int padjust)
{
   CocoaView *view = [CocoaView get];
   if (!view || !view.displayLink)
   {
      RARCH_WARN("[Video] CocoaView not ready, skipping refresh rate change to %.3f Hz\n", hz);
      return false;
   }

   /* iOS: Only refresh rate changes */
   RARCH_DBG("[Video] Setting refresh rate to %.3f Hz\n", hz);
#if (TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 150000) || (TARGET_OS_TV && __TV_OS_VERSION_MAX_ALLOWED >= 150000)
    if (@available(iOS 15, tvOS 15, *))
       view.displayLink.preferredFrameRateRange = CAFrameRateRangeMake(hz * 0.9, hz * 1.2, hz);
   else
#endif
      view.displayLink.preferredFramesPerSecond = hz;
    return true;
}
#endif

static void *apple_display_server_get_resolution_list(
      void *data, unsigned *len)
{
   unsigned j                        = 0;
   struct video_display_config *conf = NULL;
   double currentRate;

#if TARGET_OS_OSX
#ifdef RARCH_HAS_CGDISPLAYMODE_API
   CGDirectDisplayID mainDisplayID = CGMainDisplayID();
   CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(mainDisplayID);

   /* NULL for an invalid display; every accessor below dereferences
    * it. */
   if (!currentMode)
   {
      *len = 0;
      return NULL;
   }

   currentRate = CGDisplayModeGetRefreshRate(currentMode);

   /* Use pixel dimensions when available (macOS 10.8+), otherwise fall back to logical dimensions */
   size_t currentWidth, currentHeight;
   if (@available(macOS 10.8, *))
   {
      currentWidth = CGDisplayModeGetPixelWidth(currentMode);
      currentHeight = CGDisplayModeGetPixelHeight(currentMode);
   }
   else
   {
      currentWidth = CGDisplayModeGetWidth(currentMode);
      currentHeight = CGDisplayModeGetHeight(currentMode);
   }

   CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(mainDisplayID, NULL);
   NSMutableSet *resolutions = [NSMutableSet set];

   if (!displayModes)
   {
      CFRelease(currentMode);
      *len = 0;
      return NULL;
   }

   for (CFIndex i = 0; i < CFArrayGetCount(displayModes); i++)
   {
      CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
      size_t modeWidth, modeHeight;
      if (@available(macOS 10.8, *))
      {
         modeWidth = CGDisplayModeGetPixelWidth(mode);
         modeHeight = CGDisplayModeGetPixelHeight(mode);
      }
      else
      {
         modeWidth = CGDisplayModeGetWidth(mode);
         modeHeight = CGDisplayModeGetHeight(mode);
      }
      double refreshRate = CGDisplayModeGetRefreshRate(mode);

      if (refreshRate > 0)
      {
         NSString *resolution = [NSString stringWithFormat:@"%zux%zu", modeWidth, modeHeight];
         [resolutions addObject:resolution];
      }
   }

   /* Build config array with all available resolution/refresh rate combinations */
   NSMutableArray *configArray = [NSMutableArray array];

   for (CFIndex i = 0; i < CFArrayGetCount(displayModes); i++)
   {
      CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
      size_t modeWidth, modeHeight;
      if (@available(macOS 10.8, *))
      {
         modeWidth = CGDisplayModeGetPixelWidth(mode);
         modeHeight = CGDisplayModeGetPixelHeight(mode);
      }
      else
      {
         modeWidth = CGDisplayModeGetWidth(mode);
         modeHeight = CGDisplayModeGetHeight(mode);
      }
      double refreshRate = CGDisplayModeGetRefreshRate(mode);

      if (refreshRate > 0)
      {
         struct video_display_config config;
         config.width = (unsigned)modeWidth;
         config.height = (unsigned)modeHeight;
         config.bpp = 32;
         config.refreshrate = (unsigned)refreshRate;
         config.refreshrate_float = (float)refreshRate;
         config.interlaced = false;
         config.dblscan = false;
         config.idx = (unsigned)[configArray count];
         config.current = (modeWidth == currentWidth && modeHeight == currentHeight && fabs(refreshRate - currentRate) < 0.1);

         [configArray addObject:[NSValue valueWithBytes:&config objCType:@encode(struct video_display_config)]];
      }
   }

   /* Set length and allocate config array for macOS */
   *len = (unsigned)[configArray count];
   if (!(conf = (struct video_display_config*)calloc(*len, sizeof(struct video_display_config))))
   {
      /* displayModes and currentMode are CFRetain'd +1 by their
       * respective CGDisplayCopy* calls above (~line 250 / 266)
       * and must be CFRelease'd on every exit path.  The success
       * return below does this at lines 339-340; the pre-patch
       * OOM path bypassed both and leaked them until the process
       * died.  Each is a small CF object (handful of bytes) but
       * a leak of system-owned Core Graphics state is still
       * worth plugging. */
      CFRelease(displayModes);
      CFRelease(currentMode);
      return NULL;
   }

   for (j = 0; j < *len; j++)
   {
      NSValue *configValue = configArray[j];
      [configValue getValue:&conf[j]];
   }

   CFRelease(displayModes);
   CFRelease(currentMode);
   RARCH_LOG("Found %u display modes on macOS\n", *len);
   return conf;
#else
   /* pre-10.6 Leopard/Tiger fallback: CGDisplayModeRef doesn't exist
    * here and the older CGDisplayAvailableModes API is a different
    * shape.  Just report the current resolution as a single entry
    * and skip mode enumeration; resolution-switching isn't supported
    * on these targets anyway. */
   CGDirectDisplayID mainDisplayID = CGMainDisplayID();
   size_t currentWidth             = CGDisplayPixelsWide(mainDisplayID);
   size_t currentHeight            = CGDisplayPixelsHigh(mainDisplayID);

   *len = 1;
   if (!(conf = (struct video_display_config*)calloc(1, sizeof(*conf))))
      return NULL;
   conf[0].width            = (unsigned)currentWidth;
   conf[0].height           = (unsigned)currentHeight;
   conf[0].bpp              = 32;
   conf[0].refreshrate      = 60;
   conf[0].refreshrate_float = 60.0f;
   conf[0].interlaced       = false;
   conf[0].dblscan          = false;
   conf[0].idx              = 0;
   conf[0].current          = true;
   (void)currentRate;
   RARCH_LOG("[Video] Legacy macOS: reporting current mode %ux%u only\n",
         conf[0].width, conf[0].height);
   return conf;
#endif /* RARCH_HAS_CGDISPLAYMODE_API */
#else
   /* iOS/tvOS: Only enumerate refresh rates for current resolution */
   unsigned width, height;
   NSMutableSet *rates = [NSMutableSet set];

   /* Use nativeBounds to get physical screen resolution
    * (works correctly in multitasking/Split View modes) */
   UIScreen *mainScreen = [UIScreen mainScreen];
   CGRect nativeBounds = mainScreen.nativeBounds;
   width = (unsigned)nativeBounds.size.width;
   height = (unsigned)nativeBounds.size.height;
#if (TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 150000) || (TARGET_OS_TV && __TV_OS_VERSION_MAX_ALLOWED >= 150000)
   if (@available(iOS 15, tvOS 15, *))
      currentRate = [CocoaView get].displayLink.preferredFrameRateRange.preferred;
   else
#endif
      currentRate = [CocoaView get].displayLink.preferredFramesPerSecond;

   /* Detect ProMotion displays and available refresh rates */
#if !TARGET_OS_TV
   if (@available(iOS 10.3, *))
   {
      NSInteger maxFPS = mainScreen.maximumFramesPerSecond;

      /* ProMotion displays (120Hz) */
      if (maxFPS >= 120)
      {
         [rates addObjectsFromArray:@[@(24), @(30), @(40), @(48), @(60), @(80), @(120)]];
      }
      /* iPad Pro 10.5" and 11" 2nd gen (120Hz) */
      else if (maxFPS > 60)
      {
         [rates addObjectsFromArray:@[@(24), @(30), @(48), @(60), @(maxFPS)]];
      }
      /* Standard 60Hz displays */
      else
      {
         [rates addObjectsFromArray:@[@(30), @(60)]];
      }
   }
   else
#endif
   {
      /* Fallback for older iOS versions */
      [rates addObject:@(60)];
   }

   NSArray *sorted = [[rates allObjects] sortedArrayUsingSelector:@selector(compare:)];
   *len = (unsigned)[sorted count];
   RARCH_LOG("Available screen refresh rates: %s\n", [[NSString stringWithFormat:@"%@", sorted] UTF8String]);

   if (!(conf = (struct video_display_config*)calloc(*len, sizeof(struct video_display_config))))
      return NULL;

   for (j = 0; j < *len; j++)
   {
      NSNumber *rate = sorted[j];
      conf[j].width       = width;
      conf[j].height      = height;
      conf[j].bpp         = 32;
      conf[j].refreshrate = [rate unsignedIntValue];
      conf[j].refreshrate_float = [rate floatValue];
      conf[j].interlaced  = false;
      conf[j].dblscan     = false;
      conf[j].idx         = j;
      conf[j].current     = ([rate doubleValue] == currentRate);
   }
   return conf;
#endif
}

#if TARGET_OS_IOS
static void apple_display_server_set_screen_orientation(void *data, enum rotation rotation)
{
    switch (rotation)
    {
        case ORIENTATION_VERTICAL:
            [[CocoaView get] setShouldLockCurrentInterfaceOrientation:YES];
            [[CocoaView get] setLockInterfaceOrientation:UIInterfaceOrientationLandscapeRight];
            break;
        case ORIENTATION_FLIPPED:
            [[CocoaView get] setShouldLockCurrentInterfaceOrientation:YES];
            [[CocoaView get] setLockInterfaceOrientation:UIInterfaceOrientationPortraitUpsideDown];
            break;
        case ORIENTATION_FLIPPED_ROTATED:
            [[CocoaView get] setShouldLockCurrentInterfaceOrientation:YES];
            [[CocoaView get] setLockInterfaceOrientation:UIInterfaceOrientationLandscapeLeft];
            break;
        case ORIENTATION_NORMAL:
        default:
            [[CocoaView get] setShouldLockCurrentInterfaceOrientation:NO];
            break;
    }
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 160000
    if (@available(iOS 16.0, *))
    {
        [[CocoaView get] setNeedsUpdateOfSupportedInterfaceOrientations];
    }
#endif
}

static enum rotation apple_display_server_get_screen_orientation(void *data)
{
    if (![[CocoaView get] shouldLockCurrentInterfaceOrientation])
        return ORIENTATION_NORMAL;
    UIInterfaceOrientation orientation = [[CocoaView get] lockInterfaceOrientation];
    switch (orientation)
    {
        case UIInterfaceOrientationLandscapeRight:
            return ORIENTATION_VERTICAL;
        case UIInterfaceOrientationPortraitUpsideDown:
            return ORIENTATION_FLIPPED;
        case UIInterfaceOrientationLandscapeLeft:
            return ORIENTATION_FLIPPED_ROTATED;
        default:
            return ORIENTATION_NORMAL;
    }
}
#endif

typedef struct
{
#if TARGET_OS_OSX && defined(RARCH_HAS_CGDISPLAYMODE_API)
   CGDisplayModeRef original_mode;
   CGDirectDisplayID display_id;
#endif
} apple_display_server_t;

static void *apple_display_server_init(void)
{
   apple_display_server_t *apple = (apple_display_server_t*)calloc(1, sizeof(*apple));
   if (!apple)
      return NULL;

#if TARGET_OS_OSX && defined(RARCH_HAS_CGDISPLAYMODE_API)
   /* Store original display mode for restoration */
   apple->display_id = CGMainDisplayID();
   if ((apple->original_mode = CGDisplayCopyDisplayMode(apple->display_id)))
      RARCH_LOG("[Video] Stored original display mode for restoration\n");
   else
      RARCH_WARN("[Video] Could not read the current display mode;"
            " it will not be restored on exit\n");
#endif

   /* Sync the display link to the configured refresh rate.
    * The display link starts at the display's native rate before
    * config is parsed, so apply the user's setting now. */
   {
      settings_t *settings = config_get_ptr();
      if (  settings
         && settings->floats.video_refresh_rate >= 10.0f
         && settings->floats.video_refresh_rate <= 250.0f)
      {
#if TARGET_OS_IPHONE
         float hz        = settings->floats.video_refresh_rate;
         CocoaView *view = [CocoaView get];
         if (view && view.displayLink)
         {
            RARCH_DBG("[Video] Setting initial refresh rate to %.3f Hz\n", hz);
#if (TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 150000) || (TARGET_OS_TV && __TV_OS_VERSION_MAX_ALLOWED >= 150000)
            if (@available(iOS 15, tvOS 15, *))
               view.displayLink.preferredFrameRateRange =
                  CAFrameRateRangeMake(hz * 0.9, hz * 1.2, hz);
            else
#endif
               view.displayLink.preferredFramesPerSecond = hz;
         }
#elif TARGET_OS_OSX && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
         float hz        = settings->floats.video_refresh_rate;
         CocoaView *view = [CocoaView get];
         if (view)
         {
            if (@available(macOS 14, *))
            {
               RARCH_DBG("[Video] Setting initial refresh rate to %.3f Hz\n", hz);
               view.displayLink.preferredFrameRateRange =
                  CAFrameRateRangeMake(hz * 0.9, hz * 1.2, hz);
            }
         }
#endif
      }
   }

   return apple;
}

static void apple_display_server_destroy(void *data)
{
   apple_display_server_t *apple = (apple_display_server_t*)data;
   if (!apple)
      return;

#if TARGET_OS_OSX && defined(RARCH_HAS_CGDISPLAYMODE_API)
   /* Restore original display mode */
   if (apple->original_mode)
   {
      CGError result = CGDisplaySetDisplayMode(apple->display_id, apple->original_mode, NULL);
      if (result == kCGErrorSuccess)
      {
         RARCH_LOG("[Video] Restored original display mode\n");
      }
      else
      {
         RARCH_ERR("[Video] Failed to restore original display mode: CGError %d\n", result);
      }
      CFRelease(apple->original_mode);
   }
#endif

   free(apple);
}

/* Thin wrappers around cocoa_common.m helpers to match the
 * video_display_server_t signatures (which take a leading void*).
 * Shared implementation lives in cocoa_common.m so the poke /
 * gfx_ctx_driver_t vtables can reach it too - see cocoa_common.h. */
static float apple_display_server_get_refresh_rate(void *data)
{
   return cocoa_get_refresh_rate();
}

static void apple_display_server_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   cocoa_get_video_output_size(width, height, desc, desc_len);
}

#if TARGET_OS_OSX
/* ---- Apple Silicon: the display coprocessor's copy ----
 *
 * On Apple Silicon the IODisplayConnect services the IOKit path below
 * walks do not exist: the display stack moved to the DCP, a separate
 * coprocessor with its own firmware that owns the link. What it does
 * expose is one DCPAVServiceProxy per connected display, and IOKit
 * carries two unpublished functions to get at it -
 * IOAVServiceCreateWithService() to bind a proxy and
 * IOAVServiceCopyEDID() to ask the DCP for the block it read over
 * DDC. This is the route BetterDisplay and Lunar take; there is no
 * public equivalent.
 *
 * Unpublished means the symbols can go away, so they are resolved at
 * runtime rather than linked: a macOS that drops them leaves the
 * pointers NULL and the menu says "not available" instead of the
 * binary refusing to launch. Plain C throughout - no Objective-C is
 * needed for any of it, only CoreFoundation and IOKit.
 *
 * The proxies are not labelled with a CoreGraphics display ID, so the
 * one belonging to the display in use is found by reading each
 * candidate's EDID and matching the identity bytes (manufacturer at
 * 8-9, product at 10-11, serial at 12-15) against what CoreGraphics
 * reports for that display. That is the same three-way match the
 * IOKit path makes, done on the block itself. */
typedef CFTypeRef IOAVServiceRef;
typedef IOAVServiceRef (*apple_avservice_create_t)(CFAllocatorRef, io_service_t);
typedef IOReturn (*apple_avservice_copy_edid_t)(IOAVServiceRef, CFDataRef*);

/* The block a proxy reports, if it is a whole number of 128-byte
 * blocks and at least one. Returns the length, or 0. */
static size_t apple_dcp_service_edid(apple_avservice_copy_edid_t copy_edid,
      IOAVServiceRef av, uint8_t *out, size_t max, IOReturn *rc)
{
   CFDataRef edid = NULL;
   size_t len     = 0;
   if ((*rc = copy_edid(av, &edid)) != kIOReturnSuccess || !edid)
      return 0;
   len = (size_t)CFDataGetLength(edid);
   if (len > max)
      len = max;
   len -= len % 128;
   if (len >= 128)
      memcpy(out, CFDataGetBytePtr(edid), len);
   else
      len = 0;
   CFRelease(edid);
   return len;
}

static int apple_dcp_get_edid(uint32_t vendor, uint32_t product,
      uint32_t serial, uint8_t *out, size_t max)
{
   static apple_avservice_create_t    create_service;
   static apple_avservice_copy_edid_t copy_edid;
   static bool resolved;
   io_iterator_t it = 0;
   io_service_t svc;
   int n            = -1;
   int candidates   = 0;
   int only_len     = 0;
   int proxies      = 0;
   int externals    = 0;
   IOReturn last_rc = kIOReturnSuccess;

   if (!resolved)
   {
      resolved       = true;
      create_service = (apple_avservice_create_t)
         dlsym(RTLD_DEFAULT, "IOAVServiceCreateWithService");
      copy_edid      = (apple_avservice_copy_edid_t)
         dlsym(RTLD_DEFAULT, "IOAVServiceCopyEDID");
      /* RTLD_DEFAULT only searches what is already loaded with global
       * scope; open the framework by name when it comes up empty */
      if (!create_service || !copy_edid)
      {
         void *iokit = dlopen(
               "/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_LAZY);
         if (iokit)
         {
            if (!create_service)
               create_service = (apple_avservice_create_t)
                  dlsym(iokit, "IOAVServiceCreateWithService");
            if (!copy_edid)
               copy_edid = (apple_avservice_copy_edid_t)
                  dlsym(iokit, "IOAVServiceCopyEDID");
         }
      }
      if (!create_service || !copy_edid)
         RARCH_LOG("[Video] IOAVService is not available on this macOS;"
               " the display's EDID cannot be read through the DCP.\n");
   }
   if (!create_service || !copy_edid)
      return -1;

   if (IOServiceGetMatchingServices(0,
            IOServiceMatching("DCPAVServiceProxy"), &it) != KERN_SUCCESS)
      return -1;

   while ((svc = IOIteratorNext(it)))
   {
      proxies++;
      /* Location is External on a connected display and Embedded on
       * the built-in panel, which has no EDID to read */
      CFStringRef location = (CFStringRef)IORegistryEntrySearchCFProperty(
            svc, kIOServicePlane, CFSTR("Location"), kCFAllocatorDefault,
            kIORegistryIterateRecursively);
      bool external = location
         && CFGetTypeID(location) == CFStringGetTypeID()
         && CFStringCompare(location, CFSTR("External"), 0) == kCFCompareEqualTo;
      if (location)
         CFRelease(location);
      if (external)
      {
         IOAVServiceRef av = create_service(kCFAllocatorDefault, svc);
         externals++;
         if (av)
         {
            size_t len = apple_dcp_service_edid(copy_edid, av, out, max,
                  &last_rc);
            if (len)
            {
               uint32_t v  = ((uint32_t)out[8] << 8) | out[9];
               uint32_t p  = out[10] | ((uint32_t)out[11] << 8);
               uint32_t sn = out[12] | ((uint32_t)out[13] << 8)
                           | ((uint32_t)out[14] << 16)
                           | ((uint32_t)out[15] << 24);
               candidates++;
               only_len = (int)len;
               /* the serial only separates two identical monitors,
                * and only when both sides report one */
               if (v == vendor && p == product
                     && (!serial || !sn || sn == serial))
                  n = (int)len;
            }
            CFRelease(av);
         }
      }
      IOObjectRelease(svc);
      if (n > 0)
         break;
   }
   IOObjectRelease(it);

   /* No identity match, but exactly one external display answered and
    * out still holds its block: that is the display in use. A second
    * one makes the guess unsafe, so it is not made. */
   if (n < 0 && candidates == 1)
      n = only_len;
   if (n < 0)
      RARCH_LOG("[Video] DCP: %d service(s), %d external, %d with an EDID"
            " (last IOAVServiceCopyEDID 0x%x). A built-in panel reports"
            " none.\n", proxies, externals, candidates, (unsigned)last_rc);
   return n;
}

/* ---- Apple Silicon: a block built from what the DCP publishes ----
 *
 * The internal panel has no EDID: it is driven over an internal bus
 * with no DDC behind it, so IOAVServiceCopyEDID fails and nothing
 * ever negotiated a block. The DCP still knows the facts a block
 * would carry, and publishes them on its AppleCLCD2 node as ordinary
 * registry properties - no private API:
 *
 *   TimingElements     an array, one entry per mode, each with
 *                      Horizontal/VerticalAttributes giving Total,
 *                      Active, FrontPorch, SyncWidth, SyncPolarity
 *                      and PreciseSyncRate (16.16 fixed point, kHz
 *                      horizontally and Hz vertically), plus
 *                      IsInterlaced and IsPreferred
 *   IOMFBDisplayRefresh  the refresh interval bounds in mach ticks
 *   IOMFBMaxSrcPixels    the pipe's maximum pixel clock
 *
 * That is a detailed timing and a range-limits descriptor. With the
 * identity and physical size from CoreGraphics it makes a base block,
 * which modeline_edid_synthesize() assembles so the menu decodes it
 * with the same parser as a real one. Nothing is written anywhere.
 *
 * Only for Apple Silicon: Intel and PowerPC Macs have a real EDID
 * through IODisplayConnect and must keep showing that. */
#if defined(__aarch64__) || defined(__arm64__)
static bool apple_dcp_num(CFDictionaryRef d, const char *key, long *out)
{
   CFNumberRef n;
   CFStringRef k;
   bool ok = false;
   if (!d)
      return false;
   if (!(k = CFStringCreateWithCString(kCFAllocatorDefault, key,
               kCFStringEncodingUTF8)))
      return false;
   if ((n = (CFNumberRef)CFDictionaryGetValue(d, k))
         && CFGetTypeID(n) == CFNumberGetTypeID())
      ok = CFNumberGetValue(n, kCFNumberLongType, out) ? true : false;
   CFRelease(k);
   return ok;
}

static bool apple_dcp_bool(CFDictionaryRef d, const char *key)
{
   CFBooleanRef b;
   CFStringRef k;
   bool v = false;
   if (!d)
      return false;
   if (!(k = CFStringCreateWithCString(kCFAllocatorDefault, key,
               kCFStringEncodingUTF8)))
      return false;
   if ((b = (CFBooleanRef)CFDictionaryGetValue(d, k))
         && CFGetTypeID(b) == CFBooleanGetTypeID())
      v = CFBooleanGetValue(b) ? true : false;
   CFRelease(k);
   return v;
}

/* One TimingElements entry. The porches and the sync width are given;
 * the pixel clock is the horizontal rate times the line length. */
static bool apple_dcp_timing(CFDictionaryRef elem, video_edid_timing_t *t)
{
   CFDictionaryRef h = (CFDictionaryRef)CFDictionaryGetValue(elem,
         CFSTR("HorizontalAttributes"));
   CFDictionaryRef v = (CFDictionaryRef)CFDictionaryGetValue(elem,
         CFSTR("VerticalAttributes"));
   long htotal = 0, hactive = 0, hfront = 0, hsync = 0, hpol = 0, hrate = 0;
   long vtotal = 0, vactive = 0, vfront = 0, vsync = 0, vpol = 0;

   if (!h || !v || CFGetTypeID(h) != CFDictionaryGetTypeID()
              || CFGetTypeID(v) != CFDictionaryGetTypeID())
      return false;
   if (   !apple_dcp_num(h, "Total", &htotal)
       || !apple_dcp_num(h, "Active", &hactive)
       || !apple_dcp_num(v, "Total", &vtotal)
       || !apple_dcp_num(v, "Active", &vactive))
      return false;
   if (htotal <= hactive || vtotal <= vactive || hactive <= 0 || vactive <= 0)
      return false;
   apple_dcp_num(h, "FrontPorch", &hfront);
   apple_dcp_num(h, "SyncWidth", &hsync);
   apple_dcp_num(h, "SyncPolarity", &hpol);
   apple_dcp_num(v, "FrontPorch", &vfront);
   apple_dcp_num(v, "SyncWidth", &vsync);
   apple_dcp_num(v, "SyncPolarity", &vpol);
   if (!apple_dcp_num(h, "PreciseSyncRate", &hrate) || hrate <= 0)
      apple_dcp_num(h, "SyncRate", &hrate);
   if (hrate <= 0)
      return false;

   memset(t, 0, sizeof(*t));
   t->src       = MODELINE_EDID_SRC_BASE;
   t->hactive   = (unsigned)hactive;
   t->hblank    = (unsigned)(htotal - hactive);
   t->hfront    = (unsigned)hfront;
   t->hsync     = (unsigned)hsync;
   t->vactive   = (unsigned)vactive;
   t->vblank    = (unsigned)(vtotal - vactive);
   t->vfront    = (unsigned)vfront;
   t->vsync     = (unsigned)vsync;
   t->hsync_pos = hpol != 0;
   t->vsync_pos = vpol != 0;
   t->sync_type = 3;
   t->interlace = apple_dcp_bool(elem, "IsInterlaced");
   /* 16.16 fixed point in kHz, so the clock is rate * 1000 * htotal */
   t->pclock    = (unsigned)((((uint64_t)hrate * 1000) * (uint64_t)htotal) >> 16);
   return t->pclock != 0;
}

/* s15Fixed16 as a double */
static double apple_icc_fixed(const uint8_t *p)
{
   int32_t v = (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
                       | ((uint32_t)p[2] << 8) | p[3]);
   return (double)v / 65536.0;
}

/* One XYZType tag: signature, four reserved bytes, three s15Fixed16 */
static bool apple_icc_xyz(const uint8_t *icc, size_t len, uint32_t off,
      uint32_t size, double xyz[3])
{
   if (off + 20 > len || size < 20)
      return false;
   if (memcmp(icc + off, "XYZ ", 4))
      return false;
   xyz[0] = apple_icc_fixed(icc + off + 8);
   xyz[1] = apple_icc_fixed(icc + off + 12);
   xyz[2] = apple_icc_fixed(icc + off + 16);
   return true;
}

static bool apple_icc_tag(const uint8_t *icc, size_t len, const char *sig,
      uint32_t *off, uint32_t *size)
{
   uint32_t i, count;
   if (len < 132)
      return false;
   count = ((uint32_t)icc[128] << 24) | ((uint32_t)icc[129] << 16)
         | ((uint32_t)icc[130] << 8) | icc[131];
   if (count > 256 || 132 + count * 12 > len)
      return false;
   for (i = 0; i < count; i++)
   {
      const uint8_t *e = icc + 132 + i * 12;
      if (memcmp(e, sig, 4))
         continue;
      *off  = ((uint32_t)e[4] << 24) | ((uint32_t)e[5] << 16)
            | ((uint32_t)e[6] << 8) | e[7];
      *size = ((uint32_t)e[8] << 24) | ((uint32_t)e[9] << 16)
            | ((uint32_t)e[10] << 8) | e[11];
      return true;
   }
   return false;
}

/* The primaries and white point of the display's colour profile, in
 * thousandths, as the EDID chromaticity bytes hold them.
 *
 * An ICC profile stores its colorants chromatically adapted to D50,
 * with the adaptation it used in the 'chad' tag; reporting the D50
 * numbers as the display's primaries would be wrong, so the
 * adaptation is undone first. Without a 'chad' tag there is nothing
 * to undo it with and nothing is reported. */
static bool apple_display_chromaticity(CGDirectDisplayID display,
      unsigned *out)
{
   CGColorSpaceRef cs = CGDisplayCopyColorSpace(display);
   CFDataRef icc      = NULL;
   const uint8_t *b;
   size_t len;
   uint32_t off, size;
   double chad[9], inv[9], det;
   double xyz[4][3];
   bool ok = false;
   int i;
   static const char *tags[4] = { "rXYZ", "gXYZ", "bXYZ", "wtpt" };

   if (!cs)
      return false;
   icc = CGColorSpaceCopyICCData(cs);
   CGColorSpaceRelease(cs);
   if (!icc)
      return false;
   b   = CFDataGetBytePtr(icc);
   len = (size_t)CFDataGetLength(icc);

   if (!apple_icc_tag(b, len, "chad", &off, &size) || size < 8 + 36
         || off + 8 + 36 > len || memcmp(b + off, "sf32", 4))
      goto done;
   for (i = 0; i < 9; i++)
      chad[i] = apple_icc_fixed(b + off + 8 + i * 4);
   det = chad[0] * (chad[4] * chad[8] - chad[5] * chad[7])
       - chad[1] * (chad[3] * chad[8] - chad[5] * chad[6])
       + chad[2] * (chad[3] * chad[7] - chad[4] * chad[6]);
   if (det > -1e-9 && det < 1e-9)
      goto done;
   inv[0] = (chad[4] * chad[8] - chad[5] * chad[7]) / det;
   inv[1] = (chad[2] * chad[7] - chad[1] * chad[8]) / det;
   inv[2] = (chad[1] * chad[5] - chad[2] * chad[4]) / det;
   inv[3] = (chad[5] * chad[6] - chad[3] * chad[8]) / det;
   inv[4] = (chad[0] * chad[8] - chad[2] * chad[6]) / det;
   inv[5] = (chad[2] * chad[3] - chad[0] * chad[5]) / det;
   inv[6] = (chad[3] * chad[7] - chad[4] * chad[6]) / det;
   inv[7] = (chad[1] * chad[6] - chad[0] * chad[7]) / det;
   inv[8] = (chad[0] * chad[4] - chad[1] * chad[3]) / det;

   for (i = 0; i < 4; i++)
   {
      double v[3], n[3], sum;
      if (!apple_icc_tag(b, len, tags[i], &off, &size)
            || !apple_icc_xyz(b, len, off, size, v))
         goto done;
      n[0] = inv[0] * v[0] + inv[1] * v[1] + inv[2] * v[2];
      n[1] = inv[3] * v[0] + inv[4] * v[1] + inv[5] * v[2];
      n[2] = inv[6] * v[0] + inv[7] * v[1] + inv[8] * v[2];
      sum  = n[0] + n[1] + n[2];
      if (sum < 1e-9)
         goto done;
      xyz[i][0] = n[0] / sum;
      xyz[i][1] = n[1] / sum;
      ok = true;
   }
   if (ok)
      for (i = 0; i < 4; i++)
      {
         out[i * 2]     = (unsigned)(xyz[i][0] * 1000.0 + 0.5);
         out[i * 2 + 1] = (unsigned)(xyz[i][1] * 1000.0 + 0.5);
      }
done:
   CFRelease(icc);
   return ok;
}

static int apple_dcp_synthesize_edid(CGDirectDisplayID display,
      uint8_t *out, size_t max)
{
   video_edid_synth_t in;
   io_iterator_t it = 0;
   io_service_t svc;
   CGSize size_mm;
   bool built_in    = CGDisplayIsBuiltin(display) ? true : false;
   bool got         = false;

   memset(&in, 0, sizeof(in));
   if (IOServiceGetMatchingServices(0,
            IOServiceMatching("AppleCLCD2"), &it) != KERN_SUCCESS)
      return -1;

   while (!got && (svc = IOIteratorNext(it)))
   {
      CFMutableDictionaryRef props = NULL;
      if (IORegistryEntryCreateCFProperties(svc, &props,
               kCFAllocatorDefault, kNilOptions) == KERN_SUCCESS && props)
      {
         /* One node per pipe; "external" marks the ones that are not
          * the built-in panel, and only the active pipe describes a
          * display that is actually lit */
         bool external = apple_dcp_bool(props, "external");
         if (external != built_in && apple_dcp_bool(props, "NormalModeActive"))
         {
            CFArrayRef timings = (CFArrayRef)CFDictionaryGetValue(props,
                  CFSTR("TimingElements"));
            CFDictionaryRef refresh = (CFDictionaryRef)CFDictionaryGetValue(
                  props, CFSTR("IOMFBDisplayRefresh"));
            CFDictionaryRef maxpix  = (CFDictionaryRef)CFDictionaryGetValue(
                  props, CFSTR("IOMFBMaxSrcPixels"));
            long nits = 0;

            /* The peak luminance the backlight is capped at, 16.16
             * fixed point, which is what HDR static metadata codes as
             * 50 * 2^(v/32) cd/m2 */
            if (apple_dcp_num(props, "BLNitsCap", &nits) && nits > 0)
            {
               double cd = (double)nits / 65536.0;
               if (cd >= 50.0 && cd <= 10000.0)
                  in.cta_hdr_max_lum =
                     (uint8_t)(32.0 * (log(cd / 50.0) / log(2.0)) + 0.5);
            }
            CFIndex i, n = (timings && CFGetTypeID(timings) == CFArrayGetTypeID())
               ? CFArrayGetCount(timings) : 0;

            /* The preferred timing first, then one more if the block
             * has a descriptor slot left for it */
            for (i = 0; i < n && in.n_timings < 2; i++)
            {
               CFDictionaryRef elem = (CFDictionaryRef)
                  CFArrayGetValueAtIndex(timings, i);
               video_edid_timing_t t;
               if (!elem || CFGetTypeID(elem) != CFDictionaryGetTypeID())
                  continue;
               if (!apple_dcp_timing(elem, &t))
                  continue;
               /* The colour modes the DCP lists for this timing: bit
                * depth, whether anything but RGB 4:4:4 is offered, the
                * colorimetry bits and the EOTFs. A panel with one SDR
                * RGB mode reports exactly that. */
               {
                  CFArrayRef modes = (CFArrayRef)CFDictionaryGetValue(elem,
                        CFSTR("ColorModes"));
                  CFIndex m, mn = (modes
                        && CFGetTypeID(modes) == CFArrayGetTypeID())
                     ? CFArrayGetCount(modes) : 0;
                  for (m = 0; m < mn; m++)
                  {
                     CFDictionaryRef cm = (CFDictionaryRef)
                        CFArrayGetValueAtIndex(modes, m);
                     long v = 0;
                     if (!cm || CFGetTypeID(cm) != CFDictionaryGetTypeID())
                        continue;
                     if (apple_dcp_num(cm, "Depth", &v) && v >= 6 && v <= 16)
                        in.bit_depth = (uint8_t)v;
                     if (apple_dcp_num(cm, "PixelEncoding", &v))
                     {
                        if (v == 1)
                           in.ycbcr444 = true;
                        else if (v == 2)
                           in.ycbcr422 = true;
                     }
                     if (apple_dcp_num(cm, "Colorimetry", &v) && v > 0)
                     {
                        in.cta_colorimetry |= (uint8_t)(v & 0xff);
                        in.cta = true;
                     }
                     /* EOTF 0 is SDR, which is bit 0 of the HDR block */
                     if (apple_dcp_num(cm, "EOTF", &v))
                     {
                        in.cta_hdr_eotf |= (uint8_t)(1u << (v & 3));
                        in.cta = true;
                     }
                  }
               }
               if (apple_dcp_bool(elem, "IsPreferred") && in.n_timings)
               {
                  in.timing[1] = in.timing[0];
                  in.timing[0] = t;
               }
               else
                  in.timing[in.n_timings] = t;
               in.n_timings++;
            }

            /* Refresh bounds: mach ticks per frame, so the shorter
             * interval is the higher rate */
            if (refresh && CFGetTypeID(refresh) == CFDictionaryGetTypeID())
            {
               long lo = 0, hi = 0;
               mach_timebase_info_data_t tb;
               if (mach_timebase_info(&tb) == KERN_SUCCESS && tb.denom
                     && apple_dcp_num(refresh,
                        "displayMinRefreshIntervalMachTime", &lo)
                     && apple_dcp_num(refresh,
                        "displayMaxRefreshIntervalMachTime", &hi)
                     && lo > 0 && hi > 0)
               {
                  double lo_s = (double)lo * tb.numer / tb.denom / 1000000000.0;
                  double hi_s = (double)hi * tb.numer / tb.denom / 1000000000.0;
                  in.vfreq_max = (unsigned)(1.0 / lo_s + 0.5);
                  in.vfreq_min = (unsigned)(1.0 / hi_s + 0.5);
               }
            }
            if (maxpix && CFGetTypeID(maxpix) == CFDictionaryGetTypeID())
            {
               long clk = 0;
               if (apple_dcp_num(maxpix, "PixelClock", &clk) && clk > 0)
                  in.pclock_max = (unsigned)clk;
            }
            got = in.n_timings > 0;
         }
         CFRelease(props);
      }
      IOObjectRelease(svc);
   }
   IOObjectRelease(it);
   if (!got)
      return -1;

   /* The horizontal band the timings themselves span, when the
    * refresh bounds did not give one */
   {
      unsigned i;
      for (i = 0; i < in.n_timings; i++)
      {
         const video_edid_timing_t *t = &in.timing[i];
         unsigned htotal = t->hactive + t->hblank;
         unsigned hfreq  = htotal ? t->pclock / htotal : 0;
         if (!hfreq)
            continue;
         if (!in.hfreq_min || hfreq < in.hfreq_min)
            in.hfreq_min = hfreq;
         if (hfreq > in.hfreq_max)
            in.hfreq_max = hfreq;
      }
      /* A variable-refresh panel reaches the same lines at its lowest
       * rate, so the band runs down with the refresh range */
      if (in.vfreq_min && in.vfreq_max && in.hfreq_min)
         in.hfreq_min = (unsigned)((double)in.hfreq_min
               * in.vfreq_min / in.vfreq_max);
   }

   in.vendor    = CGDisplayVendorNumber(display);
   in.product   = CGDisplayModelNumber(display);
   in.serial    = CGDisplaySerialNumber(display);
   size_mm      = CGDisplayScreenSize(display);
   in.width_mm  = (unsigned)(size_mm.width  + 0.5);
   in.height_mm = (unsigned)(size_mm.height + 0.5);
   if (!in.bit_depth)
      in.bit_depth = 8;   /* the DCP's ColorModes usually say; 8 if not */
   in.interface = 5;   /* DisplayPort, which is what the DCP drives */
   {
      unsigned c[8];
      if (apple_display_chromaticity(display, c))
      {
         in.red_x   = c[0]; in.red_y   = c[1];
         in.green_x = c[2]; in.green_y = c[3];
         in.blue_x  = c[4]; in.blue_y  = c[5];
         in.white_x = c[6]; in.white_y = c[7];
      }
   }
   strlcpy(in.name, built_in ? "Built-in" : "DCP display", sizeof(in.name));
   /* The unspecified-text descriptor says where the block came from,
    * so the menu shows it and nobody mistakes it for a read block */
   strlcpy(in.text, "DCP timings", sizeof(in.text));

   return (int)modeline_edid_synthesize(&in, out, max);
}
#endif

/* The EDID of the display the RetroArch window is on. CoreGraphics
 * hands out the display's vendor, model and serial; the IODisplayConnect
 * service carrying the same three under kDisplayVendorID /
 * kDisplayProductID / kDisplaySerialNumber is that display, and its
 * info dictionary holds the raw block. Matching on the identifiers
 * rather than CGDisplayIOServicePort keeps this off an API that was
 * deprecated in 10.9. */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
static int apple_display_server_get_edid(void *data, uint8_t *out, size_t max)
{
   CGDirectDisplayID display = CGMainDisplayID();
   uint32_t vendor, product, serial;
   io_iterator_t it          = 0;
   io_service_t svc;
   int n                     = -1;
   NSWindow *window          = [((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]) window];

   if (!out || max < 128)
      return -1;

   if (window && [window screen])
   {
      NSNumber *num = [[[window screen] deviceDescription] objectForKey:@"NSScreenNumber"];
      if (num)
         display = (CGDirectDisplayID)[num unsignedIntValue];
   }
   vendor  = CGDisplayVendorNumber(display);
   product = CGDisplayModelNumber(display);
   serial  = CGDisplaySerialNumber(display);

   /* port 0 is the default master port on every release, without
    * naming kIOMasterPortDefault (renamed in 12). The walk failing is
    * not the end of it: Apple Silicon has no IODisplayConnect at all,
    * and the DCP below is the whole answer there. */
   if (IOServiceGetMatchingServices(0, IOServiceMatching("IODisplayConnect"), &it)
         != KERN_SUCCESS)
      it = 0;

   while (it && n < 0 && (svc = IOIteratorNext(it)))
   {
      CFDictionaryRef info = IODisplayCreateInfoDictionary(svc,
            kIODisplayOnlyPreferredName);
      if (info)
      {
         CFNumberRef cf_vendor  = (CFNumberRef)CFDictionaryGetValue(info, CFSTR(kDisplayVendorID));
         CFNumberRef cf_product = (CFNumberRef)CFDictionaryGetValue(info, CFSTR(kDisplayProductID));
         CFNumberRef cf_serial  = (CFNumberRef)CFDictionaryGetValue(info, CFSTR(kDisplaySerialNumber));
         uint32_t v = 0, p = 0, sn = 0;
         if (cf_vendor)
            CFNumberGetValue(cf_vendor, kCFNumberSInt32Type, &v);
         if (cf_product)
            CFNumberGetValue(cf_product, kCFNumberSInt32Type, &p);
         if (cf_serial)
            CFNumberGetValue(cf_serial, kCFNumberSInt32Type, &sn);
         /* the serial only separates two identical monitors, and only
          * when both sides report one */
         if (cf_vendor && cf_product && v == vendor && p == product
               && (!serial || !sn || sn == serial))
         {
            CFDataRef edid = (CFDataRef)CFDictionaryGetValue(info, CFSTR(kIODisplayEDIDKey));
            if (edid)
            {
               size_t len = (size_t)CFDataGetLength(edid);
               if (len > max)
                  len = max;
               len -= len % 128;
               if (len >= 128)
               {
                  memcpy(out, CFDataGetBytePtr(edid), len);
                  n = (int)len;
               }
            }
         }
         CFRelease(info);
      }
      IOObjectRelease(svc);
   }
   if (it)
      IOObjectRelease(it);

   /* Apple Silicon has no IODisplayConnect; ask the DCP instead */
   if (n < 0)
      n = apple_dcp_get_edid(vendor, product, serial, out, max);
#if defined(__aarch64__) || defined(__arm64__)
   /* Still nothing: the panel has no EDID to read, but the DCP knows
    * the timings a block would carry. Build one for display. */
   if (n < 0)
      n = apple_dcp_synthesize_edid(display, out, max);
#endif
   if (n < 0)
      RARCH_LOG("[Video] No EDID for display 0x%x (vendor 0x%04x product 0x%04x).\n",
            (unsigned)display, vendor, product);
   return n;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

const video_display_server_t dispserv_apple = {
   apple_display_server_init,
   apple_display_server_destroy,
#if TARGET_OS_OSX
   apple_display_server_set_window_opacity,
   apple_display_server_set_window_progress,
   apple_display_server_set_window_decorations,
#else
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
#endif
#if !TARGET_OS_OSX || __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
   apple_display_server_set_resolution,
#else
   NULL,
#endif
   apple_display_server_get_resolution_list,
   NULL, /* get_output_options */
#if TARGET_OS_IOS
    apple_display_server_set_screen_orientation,
    apple_display_server_get_screen_orientation,
#else
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
#endif
   apple_display_server_get_refresh_rate,
   apple_display_server_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   cocoa_get_metrics,
   NULL, /* get_flags */
   NULL, /* get_scanline */
   NULL, /* wait_vblank */
   NULL, /* modeline_list_outputs */
   NULL, /* modeline_open */
   NULL, /* modeline_close */
   NULL, /* modeline_caps */
   NULL, /* modeline_enum */
   NULL, /* modeline_add */
   NULL, /* modeline_update */
   NULL, /* modeline_delete */
   NULL, /* modeline_set */
   NULL, /* modeline_flush */
#if TARGET_OS_OSX
   apple_display_server_get_edid,
#else
   NULL, /* get_edid */
#endif
   "apple"
};
