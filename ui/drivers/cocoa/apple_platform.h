#ifndef COCOA_APPLE_PLATFORM_H
#define COCOA_APPLE_PLATFORM_H

#if defined(HAVE_COCOA_METAL)
id<ApplePlatform> apple_platform;
@interface RetroArch_OSX : NSObject <ApplePlatform, NSApplicationDelegate>
{
    NSWindow* _window;
    apple_view_type_t _vt;
    NSView* _renderView;
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
    NSWindow* _window;
}
#endif

@property (nonatomic, retain) NSWindow IBOutlet* window;

@end

#endif
