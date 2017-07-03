// 9 september 2015
#import "uipriv_darwin.h"

// 10.8 fixups
#define NSEventModifierFlags NSUInteger

@interface areaView : NSView {
	uiArea *libui_a;
	NSTrackingArea *libui_ta;
	NSSize libui_ss;
	BOOL libui_enabled;
}
- (id)initWithFrame:(NSRect)r area:(uiArea *)a;
- (uiModifiers)parseModifiers:(NSEvent *)e;
- (void)doMouseEvent:(NSEvent *)e;
- (int)sendKeyEvent:(uiAreaKeyEvent *)ke;
- (int)doKeyDownUp:(NSEvent *)e up:(int)up;
- (int)doKeyDown:(NSEvent *)e;
- (int)doKeyUp:(NSEvent *)e;
- (int)doFlagsChanged:(NSEvent *)e;
- (void)setupNewTrackingArea;
- (void)setScrollingSize:(NSSize)s;
- (BOOL)isEnabled;
- (void)setEnabled:(BOOL)e;
@end

struct uiArea {
	uiDarwinControl c;
	NSView *view;			// either sv or area depending on whether it is scrolling
	NSScrollView *sv;
	areaView *area;
	struct scrollViewData *d;
	uiAreaHandler *ah;
	BOOL scrolling;
	NSEvent *dragevent;
};

@implementation areaView

- (id)initWithFrame:(NSRect)r area:(uiArea *)a
{
	self = [super initWithFrame:r];
	if (self) {
		self->libui_a = a;
		[self setupNewTrackingArea];
		self->libui_ss = r.size;
		self->libui_enabled = YES;
	}
	return self;
}

- (void)drawRect:(NSRect)r
{
	uiArea *a = self->libui_a;
	CGContextRef c;
	uiAreaDrawParams dp;

	c = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
	// see draw.m under text for why we need the height
	dp.Context = newContext(c, [self bounds].size.height);

	dp.AreaWidth = 0;
	dp.AreaHeight = 0;
	if (!a->scrolling) {
		dp.AreaWidth = [self frame].size.width;
		dp.AreaHeight = [self frame].size.height;
	}

	dp.ClipX = r.origin.x;
	dp.ClipY = r.origin.y;
	dp.ClipWidth = r.size.width;
	dp.ClipHeight = r.size.height;

	// no need to save or restore the graphics state to reset transformations; Cocoa creates a brand-new context each time
	(*(a->ah->Draw))(a->ah, a, &dp);

	freeContext(dp.Context);
}

- (BOOL)isFlipped
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (uiModifiers)parseModifiers:(NSEvent *)e
{
	NSEventModifierFlags mods;
	uiModifiers m;

	m = 0;
	mods = [e modifierFlags];
	if ((mods & NSControlKeyMask) != 0)
		m |= uiModifierCtrl;
	if ((mods & NSAlternateKeyMask) != 0)
		m |= uiModifierAlt;
	if ((mods & NSShiftKeyMask) != 0)
		m |= uiModifierShift;
	if ((mods & NSCommandKeyMask) != 0)
		m |= uiModifierSuper;
	return m;
}

- (void)setupNewTrackingArea
{
	self->libui_ta = [[NSTrackingArea alloc] initWithRect:[self bounds]
		options:(NSTrackingMouseEnteredAndExited |
			NSTrackingMouseMoved |
			NSTrackingActiveAlways |
			NSTrackingInVisibleRect |
			NSTrackingEnabledDuringMouseDrag)
		owner:self
		userInfo:nil];
	[self addTrackingArea:self->libui_ta];
}

- (void)updateTrackingAreas
{
	[self removeTrackingArea:self->libui_ta];
	[self->libui_ta release];
	[self setupNewTrackingArea];
}

// capture on drag is done automatically on OS X
- (void)doMouseEvent:(NSEvent *)e
{
	uiArea *a = self->libui_a;
	uiAreaMouseEvent me;
	NSPoint point;
	int buttonNumber;
	NSUInteger pmb;
	unsigned int i, max;

	// this will convert point to drawing space
	// thanks swillits in irc.freenode.net/#macdev
	point = [self convertPoint:[e locationInWindow] fromView:nil];
	me.X = point.x;
	me.Y = point.y;

	me.AreaWidth = 0;
	me.AreaHeight = 0;
	if (!a->scrolling) {
		me.AreaWidth = [self frame].size.width;
		me.AreaHeight = [self frame].size.height;
	}

	buttonNumber = [e buttonNumber] + 1;
	// swap button numbers 2 and 3 (right and middle)
	if (buttonNumber == 2)
		buttonNumber = 3;
	else if (buttonNumber == 3)
		buttonNumber = 2;

	me.Down = 0;
	me.Up = 0;
	me.Count = 0;
	switch ([e type]) {
	case NSLeftMouseDown:
	case NSRightMouseDown:
	case NSOtherMouseDown:
		me.Down = buttonNumber;
		me.Count = [e clickCount];
		break;
	case NSLeftMouseUp:
	case NSRightMouseUp:
	case NSOtherMouseUp:
		me.Up = buttonNumber;
		break;
	case NSLeftMouseDragged:
	case NSRightMouseDragged:
	case NSOtherMouseDragged:
		// we include the button that triggered the dragged event in the Held fields
		buttonNumber = 0;
		break;
	}

	me.Modifiers = [self parseModifiers:e];

	pmb = [NSEvent pressedMouseButtons];
	me.Held1To64 = 0;
	if (buttonNumber != 1 && (pmb & 1) != 0)
		me.Held1To64 |= 1;
	if (buttonNumber != 2 && (pmb & 4) != 0)
		me.Held1To64 |= 2;
	if (buttonNumber != 3 && (pmb & 2) != 0)
		me.Held1To64 |= 4;
	// buttons 4..32
	// https://developer.apple.com/library/mac/documentation/Carbon/Reference/QuartzEventServicesRef/index.html#//apple_ref/c/tdef/CGMouseButton says Quartz only supports up to 32 buttons
	max = 32;
	for (i = 4; i <= max; i++) {
		uint64_t j;

		if (buttonNumber == i)
			continue;
		j = 1 << (i - 1);
		if ((pmb & j) != 0)
			me.Held1To64 |= j;
	}

	if (self->libui_enabled) {
		// and allow dragging here
		a->dragevent = e;
		(*(a->ah->MouseEvent))(a->ah, a, &me);
		a->dragevent = nil;
	}
}

#define mouseEvent(name) \
	- (void)name:(NSEvent *)e \
	{ \
		[self doMouseEvent:e]; \
	}
mouseEvent(mouseMoved)
mouseEvent(mouseDragged)
mouseEvent(rightMouseDragged)
mouseEvent(otherMouseDragged)
mouseEvent(mouseDown)
mouseEvent(rightMouseDown)
mouseEvent(otherMouseDown)
mouseEvent(mouseUp)
mouseEvent(rightMouseUp)
mouseEvent(otherMouseUp)

- (void)mouseEntered:(NSEvent *)e
{
	uiArea *a = self->libui_a;

	if (self->libui_enabled)
		(*(a->ah->MouseCrossed))(a->ah, a, 0);
}

- (void)mouseExited:(NSEvent *)e
{
	uiArea *a = self->libui_a;

	if (self->libui_enabled)
		(*(a->ah->MouseCrossed))(a->ah, a, 1);
}

// note: there is no equivalent to WM_CAPTURECHANGED on Mac OS X; there literally is no way to break a grab like that
// even if I invoke the task switcher and switch processes, the mouse grab will still be held until I let go of all buttons
// therefore, no DragBroken()

- (int)sendKeyEvent:(uiAreaKeyEvent *)ke
{
	uiArea *a = self->libui_a;

	return (*(a->ah->KeyEvent))(a->ah, a, ke);
}

- (int)doKeyDownUp:(NSEvent *)e up:(int)up
{
	uiAreaKeyEvent ke;

	ke.Key = 0;
	ke.ExtKey = 0;
	ke.Modifier = 0;

	ke.Modifiers = [self parseModifiers:e];

	ke.Up = up;

	if (!fromKeycode([e keyCode], &ke))
		return 0;
	return [self sendKeyEvent:&ke];
}

- (int)doKeyDown:(NSEvent *)e
{
	return [self doKeyDownUp:e up:0];
}

- (int)doKeyUp:(NSEvent *)e
{
	return [self doKeyDownUp:e up:1];
}

- (int)doFlagsChanged:(NSEvent *)e
{
	uiAreaKeyEvent ke;
	uiModifiers whichmod;

	ke.Key = 0;
	ke.ExtKey = 0;

	// Mac OS X sends this event on both key up and key down.
	// Fortunately -[e keyCode] IS valid here, so we can simply map from key code to Modifiers, get the value of [e modifierFlags], and check if the respective bit is set or not — that will give us the up/down state
	if (!keycodeModifier([e keyCode], &whichmod))
		return 0;
	ke.Modifier = whichmod;
	ke.Modifiers = [self parseModifiers:e];
	ke.Up = (ke.Modifiers & ke.Modifier) == 0;
	// and then drop the current modifier from Modifiers
	ke.Modifiers &= ~ke.Modifier;
	return [self sendKeyEvent:&ke];
}

- (void)setFrameSize:(NSSize)size
{
	uiArea *a = self->libui_a;

	[super setFrameSize:size];
	if (!a->scrolling)
		// we must redraw everything on resize because Windows requires it
		[self setNeedsDisplay:YES];
}

// TODO does this update the frame?
- (void)setScrollingSize:(NSSize)s
{
	self->libui_ss = s;
	[self invalidateIntrinsicContentSize];
}

- (NSSize)intrinsicContentSize
{
	if (!self->libui_a->scrolling)
		return [super intrinsicContentSize];
	return self->libui_ss;
}

- (BOOL)becomeFirstResponder
{
	return [self isEnabled];
}

- (BOOL)isEnabled
{
	return self->libui_enabled;
}

- (void)setEnabled:(BOOL)e
{
	self->libui_enabled = e;
	if (!self->libui_enabled && [self window] != nil)
		if ([[self window] firstResponder] == self)
			[[self window] makeFirstResponder:nil];
}

@end

uiDarwinControlAllDefaultsExceptDestroy(uiArea, view)

static void uiAreaDestroy(uiControl *c)
{
	uiArea *a = uiArea(c);

	if (a->scrolling)
		scrollViewFreeData(a->sv, a->d);
	[a->area release];
	if (a->scrolling)
		[a->sv release];
	uiFreeControl(uiControl(a));
}

// called by subclasses of -[NSApplication sendEvent:]
// by default, NSApplication eats some key events
// this prevents that from happening with uiArea
// see http://stackoverflow.com/questions/24099063/how-do-i-detect-keyup-in-my-nsview-with-the-command-key-held and http://lists.apple.com/archives/cocoa-dev/2003/Oct/msg00442.html
int sendAreaEvents(NSEvent *e)
{
	NSEventType type;
	id focused;
	areaView *view;

	type = [e type];
	if (type != NSKeyDown && type != NSKeyUp && type != NSFlagsChanged)
		return 0;
	focused = [[e window] firstResponder];
	if (focused == nil)
		return 0;
	if (![focused isKindOfClass:[areaView class]])
		return 0;
	view = (areaView *) focused;
	switch (type) {
	case NSKeyDown:
		return [view doKeyDown:e];
	case NSKeyUp:
		return [view doKeyUp:e];
	case NSFlagsChanged:
		return [view doFlagsChanged:e];
	}
	return 0;
}

void uiAreaSetSize(uiArea *a, int width, int height)
{
	if (!a->scrolling)
		userbug("You cannot call uiAreaSetSize() on a non-scrolling uiArea. (area: %p)", a);
	[a->area setScrollingSize:NSMakeSize(width, height)];
}

void uiAreaQueueRedrawAll(uiArea *a)
{
	[a->area setNeedsDisplay:YES];
}

void uiAreaScrollTo(uiArea *a, double x, double y, double width, double height)
{
	if (!a->scrolling)
		userbug("You cannot call uiAreaScrollTo() on a non-scrolling uiArea. (area: %p)", a);
	[a->area scrollRectToVisible:NSMakeRect(x, y, width, height)];
	// don't worry about the return value; it just says whether scrolling was needed
}

void uiAreaBeginUserWindowMove(uiArea *a)
{
	libuiNSWindow *w;

	w = (libuiNSWindow *) [a->area window];
	if (w == nil)
		return;		// TODO
	if (a->dragevent == nil)
		return;		// TODO
	[w libui_doMove:a->dragevent];
}

void uiAreaBeginUserWindowResize(uiArea *a, uiWindowResizeEdge edge)
{
	libuiNSWindow *w;

	w = (libuiNSWindow *) [a->area window];
	if (w == nil)
		return;		// TODO
	if (a->dragevent == nil)
		return;		// TODO
	[w libui_doResize:a->dragevent on:edge];
}

uiArea *uiNewArea(uiAreaHandler *ah)
{
	uiArea *a;

	uiDarwinNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = NO;

	a->area = [[areaView alloc] initWithFrame:NSZeroRect area:a];

	a->view = a->area;

	return a;
}

uiArea *uiNewScrollingArea(uiAreaHandler *ah, int width, int height)
{
	uiArea *a;
	struct scrollViewCreateParams p;

	uiDarwinNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = YES;

	a->area = [[areaView alloc] initWithFrame:NSMakeRect(0, 0, width, height)
		area:a];

	memset(&p, 0, sizeof (struct scrollViewCreateParams));
	p.DocumentView = a->area;
	p.BackgroundColor = [NSColor controlColor];
	p.DrawsBackground = 1;
	p.Bordered = NO;
	p.HScroll = YES;
	p.VScroll = YES;
	a->sv = mkScrollView(&p, &(a->d));

	a->view = a->sv;

	return a;
}
