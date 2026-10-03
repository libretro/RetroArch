/* Stand-in for the AppKit declarations the Metal driver's headers use */
#ifndef STUB_APPKIT_H
#define STUB_APPKIT_H
#include <Foundation/Foundation.h>
@interface NSResponder : NSObject
@end
@protocol NSWindowDelegate <NSObject>
@end
@protocol NSApplicationDelegate <NSObject>
@end
@interface NSView : NSResponder
- (NSRect)frame;
- (NSRect)bounds;
- (void)setFrame:(NSRect)r;
- (id)window;
- (id)layer;
- (void)setLayer:(id)l;
- (void)setWantsLayer:(BOOL)b;
- (NSSize)convertSizeToBacking:(NSSize)s;
- (NSRect)convertRectToBacking:(NSRect)r;
- (void)addSubview:(NSView *)v;
- (void)removeFromSuperview;
- (void)setNeedsDisplay:(BOOL)b;
@end
@interface NSWindow : NSResponder
- (CGFloat)backingScaleFactor;
- (id)contentView;
- (id)screen;
@end
@interface NSScreen : NSObject
@property (readonly) CGFloat maximumPotentialExtendedDynamicRangeColorComponentValue;
+ (NSScreen *)mainScreen;
- (CGFloat)backingScaleFactor;
- (NSRect)frame;
@end
@interface NSApplication : NSResponder
+ (NSApplication *)sharedApplication;
@end
@interface NSColor : NSObject
@end
@interface NSImage : NSObject
@end
@interface NSEvent : NSObject
@end
#endif
