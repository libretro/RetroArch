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
#ifdef IOS
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif
#include <stddef.h>
#include "../verbosity.h"
#include "../video_display_server.h"
#include "../video_driver.h"
#include "../../ui/drivers/cocoa/apple_platform.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include "../../configuration.h"

#ifdef OSX
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#endif

#ifdef OSX
static bool apple_display_server_set_window_opacity(void *data, unsigned opacity)
{
   settings_t *settings      = config_get_ptr();
   bool windowed_full        = settings->bools.video_windowed_fullscreen;
   NSWindow *window          = ((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]).window;
   if (windowed_full || !window.keyWindow)
      return false;
   window.alphaValue = (CGFloat)opacity / (CGFloat)100.0f;
   return true;
}

static bool apple_display_server_set_window_progress(void *data, int progress, bool finished)
{
   static NSProgressIndicator *indicator;
   static dispatch_once_t once;
   dispatch_once(&once, ^{
      NSDockTile *dockTile = [NSApp dockTile];
      NSImageView *iv = [[NSImageView alloc] init];
      [iv setImage:[[NSApplication sharedApplication] applicationIconImage]];
      [dockTile setContentView:iv];

      indicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, dockTile.size.width, 20)];
      indicator.indeterminate = NO;
      indicator.minValue = 0;
      indicator.maxValue = 100;
      indicator.doubleValue = 0;

      // Create a custom view for the dock tile
      [iv addSubview:indicator];
   });
   if (finished)
      indicator.doubleValue = (double)-1;
   else
      indicator.doubleValue = (double)progress;
   indicator.hidden = finished;
   [[NSApp dockTile] display];
   return true;
}

static bool apple_display_server_set_window_decorations(void *data, bool on)
{
   settings_t *settings      = config_get_ptr();
   bool windowed_full        = settings->bools.video_windowed_fullscreen;
   NSWindow *window          = ((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]).window;
   if (windowed_full)
      return false;
   if (on)
      window.styleMask |= NSWindowStyleMaskTitled;
   else
      window.styleMask &= ~NSWindowStyleMaskTitled;
   return true;
}
#endif

#if defined(OSX) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
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
#elif defined(IOS)
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

#ifdef OSX
   CGDirectDisplayID mainDisplayID = CGMainDisplayID();
   CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(mainDisplayID);
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
      return NULL;

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
#ifdef OSX
   CGDisplayModeRef original_mode;
   CGDirectDisplayID display_id;
#endif
} apple_display_server_t;

static void *apple_display_server_init(void)
{
   apple_display_server_t *apple = (apple_display_server_t*)calloc(1, sizeof(*apple));
   if (!apple)
      return NULL;

#ifdef OSX
   /* Store original display mode for restoration */
   apple->display_id = CGMainDisplayID();
   apple->original_mode = CGDisplayCopyDisplayMode(apple->display_id);
   RARCH_LOG("[Video] Stored original display mode for restoration\n");
#endif

   return apple;
}

static void apple_display_server_destroy(void *data)
{
   apple_display_server_t *apple = (apple_display_server_t*)data;
   if (!apple)
      return;

#ifdef OSX
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

const video_display_server_t dispserv_apple = {
   apple_display_server_init,
   apple_display_server_destroy,
#ifdef OSX
   apple_display_server_set_window_opacity,
   apple_display_server_set_window_progress,
   apple_display_server_set_window_decorations,
#else
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
#endif
#if !defined(OSX) || __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
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
   NULL, /* get_flags */
   "apple"
};
