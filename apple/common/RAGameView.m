/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#import <Availability.h>
#import "RetroArch_Apple.h"
#include "../../general.h"

// Define compatibility symbols and categories
#ifdef IOS

#ifdef HAVE_CAMERA
#include <AVFoundation/AVCaptureSession.h>
#include <AVFoundation/AVCaptureDevice.h>
#include <AVFoundation/AVCaptureOutput.h>
#include <AVFoundation/AVCaptureInput.h>
#include <AVFoundation/AVMediaFormat.h>
#include <CoreVideo/CVOpenGLESTextureCache.h>
#endif

#define GLContextClass EAGLContext
#elif defined(OSX)
#define GLContextClass NSOpenGLContext
#define g_view g_instance // < RAGameView is a container on iOS; on OSX these are both the same object
#endif

#ifdef IOS

#include <GLKit/GLKit.h>
#include "../iOS/views.h"
#define ALMOST_INVISIBLE (.021f)
static GLKView *g_view;
static UIView *g_pause_indicator_view;

#endif

static RAGameView* g_instance;
static GLContextClass* g_context;

@implementation RAGameView
+ (RAGameView*)get
{
   if (!g_instance)
      g_instance = [RAGameView new];
   
   return g_instance;
}

- (id)init
{
   self = [super init];
   
#if defined(OSX)
   [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
#elif defined(IOS)
   /* iOS Pause menu and lifecycle. */
   UINib *xib = (UINib*)[UINib nibWithNibName:BOXSTRING("PauseIndicatorView") bundle:nil];
   g_pause_indicator_view = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];
   
   g_view = [GLKView new];
   g_view.multipleTouchEnabled = YES;
   g_view.enableSetNeedsDisplay = NO;
   [g_view addSubview:g_pause_indicator_view];
   
   self.view = g_view;
   
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showPauseIndicator) name:UIApplicationWillEnterForegroundNotification object:nil];
#endif
   
   return self;
}

#ifdef OSX

- (void)setFrame:(NSRect)frameRect
{
   [super setFrame:frameRect];

   if (g_view && g_context)
      [g_context update];
}

- (void)display
{
   [g_context flushBuffer];
}

// Stop the annoying sound when pressing a key
- (BOOL)acceptsFirstResponder
{
   return YES;
}

- (BOOL)isFlipped
{
   return YES;
}

- (void)keyDown:(NSEvent*)theEvent
{
}

#elif defined(IOS)
// Pause Menus
- (void)viewDidAppear:(BOOL)animated
{
   [self showPauseIndicator];
}

- (void)showPauseIndicator
{
   g_pause_indicator_view.alpha = 1.0f;
   [NSObject cancelPreviousPerformRequestsWithTarget:g_instance];
   [g_instance performSelector:@selector(hidePauseButton) withObject:g_instance afterDelay:3.0f];
}

- (void)viewWillLayoutSubviews
{
   UIInterfaceOrientation orientation = self.interfaceOrientation;
   CGRect screenSize = [[UIScreen mainScreen] bounds];
   float width = ((int)orientation < 3) ? CGRectGetWidth(screenSize) : CGRectGetHeight(screenSize);
   float height = ((int)orientation < 3) ? CGRectGetHeight(screenSize) : CGRectGetWidth(screenSize);
   float tenpctw = width / 10.0f;
   float tenpcth = height / 10.0f;
   
   g_pause_indicator_view.frame = CGRectMake(tenpctw * 4.0f, 0.0f, tenpctw * 2.0f, tenpcth);
   [g_pause_indicator_view viewWithTag:1].frame = CGRectMake(0, 0, tenpctw * 2.0f, tenpcth);
}

- (void)hidePauseButton
{
   [UIView animateWithDuration:0.2
      animations:^{ g_pause_indicator_view.alpha = ALMOST_INVISIBLE; }
      completion:^(BOOL finished) { }
   ];
}

// NOTE: This version runs on iOS6+
- (NSUInteger)supportedInterfaceOrientations
{
   return apple_frontend_settings.orientation_flags;
}

// NOTE: This version runs on iOS2-iOS5, but not iOS6+
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
   switch (interfaceOrientation)
   {
      case UIInterfaceOrientationPortrait:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskPortrait);
      case UIInterfaceOrientationPortraitUpsideDown:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskPortraitUpsideDown);
      case UIInterfaceOrientationLandscapeLeft:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskLandscapeLeft);
      case UIInterfaceOrientationLandscapeRight:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskLandscapeRight);

#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_8_0
      case UIInterfaceOrientationUnknown:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskAll);
#endif
   }
   
   return YES;
}

#ifdef HAVE_CAMERA
#include "contentview_camera_ios.m.inl"
#endif

#endif

#ifdef HAVE_LOCATION
#include "contentview_location.m.inl"
#endif

@end

#if defined(HAVE_CAMERA) && defined(IOS)
#include "apple_camera_ios.c.inl"
#endif

#ifdef HAVE_LOCATION
#include "apple_location.c.inl"
#endif

#include "apple_gfx_context.c.inl"
