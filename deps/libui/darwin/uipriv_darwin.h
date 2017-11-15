// 6 january 2015
#define MAC_OS_X_VERSION_MIN_REQUIRED MAC_OS_X_VERSION_10_8
#define MAC_OS_X_VERSION_MAX_ALLOWED MAC_OS_X_VERSION_10_8
#import <Cocoa/Cocoa.h>
#import "../ui.h"
#import "../ui_darwin.h"
#import "../common/uipriv.h"

#if __has_feature(objc_arc)
#error Sorry, libui cannot be compiled with ARC.
#endif

#define toNSString(str) [NSString stringWithUTF8String:(str)]
#define fromNSString(str) [(str) UTF8String]

#ifndef NSAppKitVersionNumber10_9
#define NSAppKitVersionNumber10_9 1265
#endif

/*TODO remove this*/typedef struct uiImage uiImage;

// menu.m
@interface menuManager : NSObject {
	struct mapTable *items;
	BOOL hasQuit;
	BOOL hasPreferences;
	BOOL hasAbout;
}
@property (strong) NSMenuItem *quitItem;
@property (strong) NSMenuItem *preferencesItem;
@property (strong) NSMenuItem *aboutItem;
// NSMenuValidation is only informal
- (BOOL)validateMenuItem:(NSMenuItem *)item;
- (NSMenu *)makeMenubar;
@end
extern void finalizeMenus(void);
extern void uninitMenus(void);

// main.m
@interface applicationClass : NSApplication
@end
// this is needed because NSApp is of type id, confusing clang
#define realNSApp() ((applicationClass *) NSApp)
@interface appDelegate : NSObject <NSApplicationDelegate>
@property (strong) menuManager *menuManager;
@end
#define appDelegate() ((appDelegate *) [realNSApp() delegate])
struct nextEventArgs {
	NSEventMask mask;
	NSDate *duration;
	// LONGTERM no NSRunLoopMode?
	NSString *mode;
	BOOL dequeue;
};
extern int mainStep(struct nextEventArgs *nea, BOOL (^interceptEvent)(NSEvent *));

// util.m
extern void disableAutocorrect(NSTextView *);

// entry.m
extern void finishNewTextField(NSTextField *, BOOL);
extern NSTextField *newEditableTextField(void);

// window.m
@interface libuiNSWindow : NSWindow
- (void)libui_doMove:(NSEvent *)initialEvent;
- (void)libui_doResize:(NSEvent *)initialEvent on:(uiWindowResizeEdge)edge;
@end
extern uiWindow *windowFromNSWindow(NSWindow *);

// alloc.m
extern NSMutableArray *delegates;
extern void initAlloc(void);
extern void uninitAlloc(void);

// autolayout.m
extern NSLayoutConstraint *mkConstraint(id view1, NSLayoutAttribute attr1, NSLayoutRelation relation, id view2, NSLayoutAttribute attr2, CGFloat multiplier, CGFloat c, NSString *desc);
extern void jiggleViewLayout(NSView *view);
struct singleChildConstraints {
	NSLayoutConstraint *leadingConstraint;
	NSLayoutConstraint *topConstraint;
	NSLayoutConstraint *trailingConstraintGreater;
	NSLayoutConstraint *trailingConstraintEqual;
	NSLayoutConstraint *bottomConstraintGreater;
	NSLayoutConstraint *bottomConstraintEqual;
};
extern void singleChildConstraintsEstablish(struct singleChildConstraints *c, NSView *contentView, NSView *childView, BOOL hugsTrailing, BOOL hugsBottom, int margined, NSString *desc);
extern void singleChildConstraintsRemove(struct singleChildConstraints *c, NSView *cv);
extern void singleChildConstraintsSetMargined(struct singleChildConstraints *c, int margined);

// map.m
extern struct mapTable *newMap(void);
extern void mapDestroy(struct mapTable *m);
extern void *mapGet(struct mapTable *m, void *key);
extern void mapSet(struct mapTable *m, void *key, void *value);
extern void mapDelete(struct mapTable *m, void *key);
extern void mapWalk(struct mapTable *m, void (*f)(void *key, void *value));
extern void mapReset(struct mapTable *m);

// area.m
extern int sendAreaEvents(NSEvent *);

// areaevents.m
extern BOOL fromKeycode(unsigned short keycode, uiAreaKeyEvent *ke);
extern BOOL keycodeModifier(unsigned short keycode, uiModifiers *mod);

// draw.m
extern uiDrawContext *newContext(CGContextRef, CGFloat);
extern void freeContext(uiDrawContext *);

// drawtext.m
extern uiDrawTextFont *mkTextFont(CTFontRef f, BOOL retain);
extern uiDrawTextFont *mkTextFontFromNSFont(NSFont *f);
extern void doDrawText(CGContextRef c, CGFloat cheight, double x, double y, uiDrawTextLayout *layout);

// fontbutton.m
extern BOOL fontButtonInhibitSendAction(SEL sel, id from, id to);
extern BOOL fontButtonOverrideTargetForAction(SEL sel, id from, id to, id *override);
extern void setupFontPanel(void);

// colorbutton.m
extern BOOL colorButtonInhibitSendAction(SEL sel, id from, id to);

// scrollview.m
struct scrollViewCreateParams {
	NSView *DocumentView;
	NSColor *BackgroundColor;
	BOOL DrawsBackground;
	BOOL Bordered;
	BOOL HScroll;
	BOOL VScroll;
};
struct scrollViewData;
extern NSScrollView *mkScrollView(struct scrollViewCreateParams *p, struct scrollViewData **dout);
extern void scrollViewSetScrolling(NSScrollView *sv, struct scrollViewData *d, BOOL hscroll, BOOL vscroll);
extern void scrollViewFreeData(NSScrollView *sv, struct scrollViewData *d);

// label.m
extern NSTextField *newLabel(NSString *str);

// image.m
extern NSImage *imageImage(uiImage *);

// winmoveresize.m
extern void doManualMove(NSWindow *w, NSEvent *initialEvent);
extern void doManualResize(NSWindow *w, NSEvent *initialEvent, uiWindowResizeEdge edge);
