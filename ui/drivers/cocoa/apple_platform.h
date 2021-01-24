#ifndef COCOA_APPLE_PLATFORM_H
#define COCOA_APPLE_PLATFORM_H

#ifdef HAVE_METAL
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#endif

#if defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH)
@interface WindowListener : NSResponder<NSWindowDelegate>
@end
#endif

#if defined(HAVE_COCOA_METAL) || defined(HAVE_COCOATOUCH)
@protocol ApplePlatform

/*! @brief renderView returns the current render view based on the viewType */
@property(readonly) id renderView;
/*! @brief isActive returns true if the application has focus */
@property(readonly) bool hasFocus;
@property(readwrite) apple_view_type_t viewType;

/*! @brief setVideoMode adjusts the video display to the specified mode */
- (void)setVideoMode:(gfx_ctx_mode_t)mode;
/*! @brief setCursorVisible specifies whether the cursor is visible */
- (void)setCursorVisible:(bool)v;
/*! @brief controls whether the screen saver should be disabled and
 * the displays should not sleep.
 */
- (bool)setDisableDisplaySleep:(bool)disable;
@end

#endif

#if defined(HAVE_COCOA_METAL) || defined(HAVE_COCOATOUCH)
extern id<ApplePlatform> apple_platform;

id<ApplePlatform> apple_platform;
#else
id apple_platform;
#endif

#if defined(HAVE_COCOATOUCH)
@interface RetroArch_iOS : UINavigationController<ApplePlatform, UIApplicationDelegate,
UINavigationControllerDelegate> {
    UIView *_renderView;
    apple_view_type_t _vt;
}

@property (nonatomic) UIWindow* window;
@property (nonatomic) NSString* documentsDirectory;
@property (nonatomic) int menu_count;

+ (RetroArch_iOS*)get;

- (void)showGameView;
- (void)supportOtherAudioSessions;

- (void)refreshSystemConfig;
@end
#else
#if defined(HAVE_COCOA_METAL)
@interface RetroArch_OSX : NSObject<ApplePlatform, NSApplicationDelegate> {
#elif (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
@interface RetroArch_OSX : NSObject {
#else
@interface RetroArch_OSX : NSObject<NSApplicationDelegate> {
#endif
	NSWindow *_window;
	apple_view_type_t _vt;
	NSView *_renderView;
	id _sleepActivity;
#if defined(HAVE_COCOA_METAL)
	WindowListener *_listener;
#endif
}

@property(nonatomic, retain) NSWindow IBOutlet *window;

@end
#endif

#endif
