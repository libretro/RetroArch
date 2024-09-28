#ifndef COCOA_APPLE_PLATFORM_H
#define COCOA_APPLE_PLATFORM_H

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

#ifdef __OBJC__

#import <Foundation/Foundation.h>

#ifdef HAVE_METAL
#import <Metal/Metal.h>
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

#if defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH)
@interface WindowListener : NSResponder<NSWindowDelegate>
@property (nonatomic) NSWindow* window;
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
#else
extern id apple_platform;
#endif

#if defined(HAVE_COCOATOUCH)
void rarch_start_draw_observer(void);
void rarch_stop_draw_observer(void);

#if defined(HAVE_COCOA_METAL)
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

@property (nonatomic) UIWindow* window;
@property (nonatomic) NSString* documentsDirectory;
@property (nonatomic) int menu_count;
@property (nonatomic) NSDate *bgDate;

+ (RetroArch_iOS*)get;

- (void)showGameView;
- (void)supportOtherAudioSessions;

- (void)refreshSystemConfig;
@end

#else

#import <AppKit/AppKit.h>

#if defined(HAVE_COCOA_METAL)
@interface RetroArch_OSX : NSObject<ApplePlatform, NSApplicationDelegate> {
#elif (defined(__MACH__)  && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200))
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

#endif
