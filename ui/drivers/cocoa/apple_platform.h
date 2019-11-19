#ifndef COCOA_APPLE_PLATFORM_H
#define COCOA_APPLE_PLATFORM_H

#if defined(HAVE_COCOA_METAL)
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

@interface WindowListener : NSResponder <NSWindowDelegate>
@end

@implementation WindowListener

/* Similarly to SDL, we'll respond to key events by doing nothing so we don't beep.
 */
- (void)flagsChanged:(NSEvent *)event
{
}

- (void)keyDown:(NSEvent *)event
{
}

- (void)keyUp:(NSEvent *)event
{
}

@end
#endif

#if defined(HAVE_COCOA_METAL)
id<ApplePlatform> apple_platform;
@interface RetroArch_OSX : NSObject <ApplePlatform, NSApplicationDelegate>
{
   NSWindow *_window;
   apple_view_type_t _vt;
   NSView *_renderView;
   id _sleepActivity;
   WindowListener *_listener;
}
#elif defined(HAVE_COCOA)
id apple_platform;
#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
@interface RetroArch_OSX : NSObject
#else
@interface RetroArch_OSX : NSObject <NSApplicationDelegate>
#endif
{
   NSWindow *_window;
}
#endif

@property(nonatomic, retain) NSWindow IBOutlet *window;

@end

#endif
