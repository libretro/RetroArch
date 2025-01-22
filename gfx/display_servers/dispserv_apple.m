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

static bool apple_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz,
      int center, int monitor_index, int xoffset, int padjust)
{
#if (defined(OSX) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000)
   if (@available(macOS 14, *))
      [CocoaView get].displayLink.preferredFrameRateRange = CAFrameRateRangeMake(hz * 0.9, hz * 1.2, hz);
#elif defined(IOS)
#if (TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 150000) || (TARGET_OS_TV && __TV_OS_VERSION_MAX_ALLOWED >= 150000)
    if (@available(iOS 15, tvOS 15, *))
       [CocoaView get].displayLink.preferredFrameRateRange = CAFrameRateRangeMake(hz * 0.9, hz * 1.2, hz);
   else
#endif
      [CocoaView get].displayLink.preferredFramesPerSecond = hz;
#endif
   return true;
}

static void *apple_display_server_get_resolution_list(
      void *data, unsigned *len)
{
   unsigned j                        = 0;
   struct video_display_config *conf = NULL;

   unsigned width, height;
   NSMutableSet *rates = [NSMutableSet set];
   double currentRate;

#ifdef OSX
   NSRect bounds = [CocoaView get].bounds;
   float scale = cocoa_screen_get_backing_scale_factor();
   width = bounds.size.width * scale;
   height = bounds.size.height * scale;

   CGDirectDisplayID mainDisplayID = CGMainDisplayID();
   CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(mainDisplayID);
   currentRate = CGDisplayModeGetRefreshRate(currentMode);
   CFRelease(currentMode);
   CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(mainDisplayID, NULL);
   for (CFIndex i = 0; i < CFArrayGetCount(displayModes); i++)
   {
      CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
      double refreshRate = CGDisplayModeGetRefreshRate(mode);
      if (refreshRate > 0)
         [rates addObject:@(refreshRate)];
   }
   CFRelease(displayModes);
#else
   CGRect bounds = [CocoaView get].view.bounds;
   float scale = cocoa_screen_get_native_scale();
   width = bounds.size.width * scale;
   height = bounds.size.height * scale;

   UIScreen *mainScreen = [UIScreen mainScreen];
#if (TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 150000) || (TARGET_OS_TV && __TV_OS_VERSION_MAX_ALLOWED >= 150000)
   if (@available(iOS 15, tvOS 15, *))
      currentRate = [CocoaView get].displayLink.preferredFrameRateRange.preferred;
   else
#endif
      currentRate = [CocoaView get].displayLink.preferredFramesPerSecond;
#if !TARGET_OS_TV
   if (@available(iOS 15, *))
      [rates addObjectsFromArray:@[@(24), @(30), @(40), @(48), @(60), @(120)]];
   else
#endif
      [rates addObject:@(mainScreen.maximumFramesPerSecond)];
#endif

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
}

const video_display_server_t dispserv_apple = {
   NULL, /* init */
   NULL, /* destroy */
#ifdef OSX
   apple_display_server_set_window_opacity,
   apple_display_server_set_window_progress,
   apple_display_server_set_window_decorations,
#else
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
#endif
   apple_display_server_set_resolution,
   apple_display_server_get_resolution_list,
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   NULL, /* get_flags */
   "apple"
};
