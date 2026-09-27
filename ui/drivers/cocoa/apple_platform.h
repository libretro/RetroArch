#ifndef COCOA_APPLE_PLATFORM_H
#define COCOA_APPLE_PLATFORM_H
#ifdef __MACH__
#include <TargetConditionals.h>
#endif

extern bool RAIsVoiceOverRunning(void);

#if TARGET_OS_TV
#include "config_file.h"
extern config_file_t *open_userdefaults_config_file(void);
extern void write_userdefaults_config_file(void);
extern void update_topshelf(void);
#endif

#if TARGET_OS_IOS
extern void ios_show_file_sheet(void);
extern bool ios_running_on_ipad(void);
#endif

#if TARGET_OS_IPHONE
/* iOS native keyboard support */
typedef void (*input_keyboard_line_complete_t)(void *userdata, const char *line);
extern bool ios_keyboard_start(char **buffer_ptr, size_t *size_ptr, size_t *ptr_ptr,
                                const char *label,
                                input_keyboard_line_complete_t callback, void *userdata);
extern bool ios_keyboard_active(void);
extern void ios_keyboard_end(void);
#endif

#if TARGET_OS_OSX
extern void osx_show_file_sheet(void);
#endif

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#include <defines/cocoa_defines.h>

/* The Vulkan render view is a CAMetalLayer-backed view and needs the
 * Metal device API even in a build without the Metal video driver. */
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
#import <Metal/Metal.h>
#endif
#ifdef HAVE_METAL
#import <MetalKit/MetalKit.h>
#endif

typedef enum apple_view_type
{
   APPLE_VIEW_TYPE_NONE = 0,
   APPLE_VIEW_TYPE_OPENGL_ES,
   APPLE_VIEW_TYPE_OPENGL,
   APPLE_VIEW_TYPE_VULKAN,
   APPLE_VIEW_TYPE_METAL
} apple_view_type_t;

/* ApplePlatform is the one interface the video context and video
 * drivers use to reach the host application on every Apple target:
 * it owns the render view, swaps it for the kind the current video
 * driver needs, and performs the window and full-screen surgery, all
 * of it on the main thread. */
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
#if !defined(HAVE_COCOATOUCH)
- (void)openDocument:(id)sender;
#endif
@end

extern id<ApplePlatform> apple_platform;

void rarch_start_draw_observer(void);
void rarch_stop_draw_observer(void);

#if TARGET_OS_IPHONE && defined(HAVE_COCOATOUCH)
#ifdef HAVE_VULKAN
@interface MetalLayerView : UIView
@property (nonatomic, readonly) CAMetalLayer *metalLayer;
@end
#endif

#import <UIKit/UIKit.h>

@interface RetroArch_iOS : UINavigationController<ApplePlatform, UIApplicationDelegate,
UINavigationControllerDelegate> {
    UIView *_renderView;
    apple_view_type_t _vt;
}

/* Explicit retain / copy qualifiers so these properties are correct
 * under MRR as well as ARC.  Under ARC the object-typed property
 * default is 'strong', which behaves identically to 'retain' here;
 * under MRR the default is 'assign', which would silently drop the
 * retain and release the referent at the next autorelease pool
 * drain.  Spelling the ownership out keeps ui_cocoatouch.m
 * MRR-buildable in the same spirit as ui_cocoa.m. */
@property (nonatomic, retain) UIWindow *window;
@property (nonatomic, copy)   NSString *documentsDirectory;
@property (nonatomic)         int menu_count;
@property (nonatomic, retain) NSDate *bgDate;

+ (RetroArch_iOS*)get;

- (void)showGameView;
- (void)supportOtherAudioSessions;
- (BOOL)openRetroArchURL:(NSURL *)url;

@end

#else

#import <AppKit/AppKit.h>

/* The main window's delegate and the responder behind the render view:
 * swallows key events so AppKit does not beep, and records the window
 * geometry for the remember-position setting. */
@interface WindowListener : NSResponder RARCH_PROTO_NSWINDOWDELEGATE
{
	/* Declared, not synthesized: the fragile 32-bit runtime GCC 4.0
	 * targets cannot add ivars for a property on its own.  Unretained,
	 * as the assign property it backs requires under ARC. */
	RARCH_UNSAFE_UNRETAINED NSWindow *_window;
}
/* assign (not retain) - WindowListener is a delegate; the window owns it, not vice versa */
@property (nonatomic, assign) NSWindow *window;
@end

@interface RetroArch_OSX : NSObject<ApplePlatform RARCH_PROTO_NSAPPLICATIONDELEGATE> {
	NSWindow *_window;
	/* Host of the render view while in the pre-10.7 borderless
	 * full-screen mode; nil otherwise.  See -setVideoMode:. */
	NSWindow *_fullscreenWindow;
	WindowListener *_listener;
	NSView *_renderView;
	id _sleepActivity;
	apple_view_type_t _vt;
}

@property(nonatomic, retain) NSWindow IBOutlet *window;

/* The window currently hosting the render view: the borderless
 * full-screen window while that mode is active, the main window
 * otherwise. */
- (NSWindow *)hostWindow;
- (void)setupMainWindow;
- (void)updateWindowedSize:(gfx_ctx_mode_t)mode;

@end
#endif

#endif

#endif
