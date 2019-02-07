/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#import <AvailabilityMacros.h>
#include <sys/stat.h>
#include "cocoa_common_metal.h"
#ifdef HAVE_COCOA_METAL
#include "../ui_cocoa_metal.h"
#endif

#include <retro_assert.h>

#include "../../../verbosity.h"

#include "../../../location/location_driver.h"
#include "../../../camera/camera_driver.h"

@implementation MetalView

- (void)keyDown:(NSEvent*)theEvent
{
}

/* Stop the annoying sound when pressing a key. */
- (BOOL)acceptsFirstResponder
{
   return YES;
}

- (BOOL)isFlipped
{
   return YES;
}
@end

static CocoaView* g_instance;

#if defined(HAVE_COCOA_METAL)
void *nsview_get_ptr(void)
{
    return (BRIDGE void *)g_instance;
}
#endif

/* forward declarations */
void cocoagl_gfx_ctx_update(void);
void *glkitview_init(void);

@implementation CocoaView

#if defined(HAVE_COCOA_METAL)
#include "../../../input/drivers/cocoa_input.h"

- (void)scrollWheel:(NSEvent *)theEvent {
    cocoa_input_data_t *apple = (cocoa_input_data_t*)input_driver_get_data();
    (void)apple;
}

#endif

+ (CocoaView*)get
{
   if (!g_instance)
      g_instance = [CocoaView new];

   return g_instance;
}

- (id)init
{
   self = [super init];

#if defined(HAVE_COCOA_METAL)
   [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
   [self registerForDraggedTypes:@[NSColorPboardType, NSFilenamesPboardType]];
#elif defined(HAVE_COCOATOUCH)
   self.view = (__bridge GLKView*)glkitview_init();

   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showPauseIndicator) name:UIApplicationWillEnterForegroundNotification object:nil];
#endif

   return self;
}

#if defined(HAVE_COCOA_METAL)
- (BOOL)layer:(CALayer *)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow *)window {
   return YES;
}

- (void)setFrame:(NSRect)frameRect
{
   [super setFrame:frameRect];
   cocoagl_gfx_ctx_update();
}

/* Stop the annoying sound when pressing a key. */
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

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    NSDragOperation sourceDragMask = [sender draggingSourceOperationMask];
    NSPasteboard *pboard = [sender draggingPasteboard];

    if ( [[pboard types] containsObject:NSFilenamesPboardType] )
    {
        if (sourceDragMask & NSDragOperationCopy)
            return NSDragOperationCopy;
    }

    return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];

    if ( [[pboard types] containsObject:NSURLPboardType])
    {
        NSURL *fileURL = [NSURL URLFromPasteboard:pboard];
        NSString *s = [fileURL path];
        if (s != nil)
        {
           RARCH_LOG("Drop name is: %s\n", [s UTF8String]);
        }
    }
    return YES;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay: YES];
}

#elif defined(HAVE_COCOATOUCH)
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
    return UIRectEdgeBottom;
}

-(BOOL)prefersHomeIndicatorAutoHidden
{
    return NO;
}

- (void)viewDidAppear:(BOOL)animated
{
   /* Pause Menus. */
   [self showPauseIndicator];
   if (@available(iOS 11.0, *)) {
        [self setNeedsUpdateOfHomeIndicatorAutoHidden];
   }
}

- (void)showPauseIndicator
{
   g_pause_indicator_view.alpha = 1.0f;
   [NSObject cancelPreviousPerformRequestsWithTarget:g_instance];
   [g_instance performSelector:@selector(hidePauseButton) withObject:g_instance afterDelay:3.0f];
}

- (void)viewWillLayoutSubviews
{
   float width = 0.0f, height = 0.0f, tenpctw, tenpcth;
   RAScreen *screen  = (__bridge RAScreen*)get_chosen_screen();
   UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];
   CGRect screenSize = [screen bounds];
   SEL selector = NSSelectorFromString(BOXSTRING("coordinateSpace"));

    if ([screen respondsToSelector:selector])
    {
        screenSize  = [[screen coordinateSpace] bounds];
        width       = CGRectGetWidth(screenSize);
        height      = CGRectGetHeight(screenSize);
    }
    else
    {
        width       = ((int)orientation < 3) ? CGRectGetWidth(screenSize) : CGRectGetHeight(screenSize);
        height      = ((int)orientation < 3) ? CGRectGetHeight(screenSize) : CGRectGetWidth(screenSize);
    }

   tenpctw          = width  / 10.0f;
   tenpcth          = height / 10.0f;

   g_pause_indicator_view.frame = CGRectMake(tenpctw * 4.0f, 0.0f, tenpctw * 2.0f, tenpcth);
   [g_pause_indicator_view viewWithTag:1].frame = CGRectMake(0, 0, tenpctw * 2.0f, tenpcth);

    [self adjustViewFrameForSafeArea];
}

-(void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    if (@available(iOS 11, *)) {
        [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
            [self adjustViewFrameForSafeArea];
        } completion:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        }];
    }
}

-(void)adjustViewFrameForSafeArea {
    // This is for adjusting the view frame to account for the notch in iPhone X phones
    if (@available(iOS 11, *)) {
        RAScreen *screen  = (__bridge RAScreen*)get_chosen_screen();
        CGRect screenSize = [screen bounds];
        UIEdgeInsets inset = [[UIApplication sharedApplication] delegate].window.safeAreaInsets;
        UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];
        CGRect newFrame = screenSize;
        if ( orientation == UIInterfaceOrientationPortrait ) {
            newFrame = CGRectMake(screenSize.origin.x, screenSize.origin.y + inset.top, screenSize.size.width, screenSize.size.height - inset.top);
        } else if ( orientation == UIInterfaceOrientationLandscapeLeft ) {
            newFrame = CGRectMake(screenSize.origin.x, screenSize.origin.y, screenSize.size.width - inset.right, screenSize.size.height);
        } else if ( orientation == UIInterfaceOrientationLandscapeRight ) {
            newFrame = CGRectMake(screenSize.origin.x + inset.left, screenSize.origin.y, screenSize.size.width - inset.left, screenSize.size.height);
        }
        self.view.frame = newFrame;
    }
}

#define ALMOST_INVISIBLE (.021f)

- (void)hidePauseButton
{
   [UIView animateWithDuration:0.2
      animations:^{ g_pause_indicator_view.alpha = ALMOST_INVISIBLE; }
      completion:^(BOOL finished) { }
   ];
}

/* NOTE: This version runs on iOS6+. */
- (NSUInteger)supportedInterfaceOrientations
{
   return (NSUInteger)apple_frontend_settings.orientation_flags;
}

/* NOTE: This version runs on iOS2-iOS5, but not iOS6+. */
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

      default:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskAll);
   }

   return YES;
}
#endif

@end
