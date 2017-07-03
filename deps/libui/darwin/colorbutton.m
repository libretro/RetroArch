// 15 may 2016
#import "uipriv_darwin.h"

// TODO no intrinsic height?

@interface colorButton : NSColorWell {
	uiColorButton *libui_b;
	BOOL libui_changing;
	BOOL libui_setting;
}
- (id)initWithFrame:(NSRect)frame libuiColorButton:(uiColorButton *)b;
- (void)deactivateOnClose:(NSNotification *)note;
- (void)libuiColor:(double *)r g:(double *)g b:(double *)b a:(double *)a;
- (void)libuiSetColor:(double)r g:(double)g b:(double)b a:(double)a;
@end

// only one may be active at one time
static colorButton *activeColorButton = nil;

struct uiColorButton {
	uiDarwinControl c;
	colorButton *button;
	void (*onChanged)(uiColorButton *, void *);
	void *onChangedData;
};

@implementation colorButton

- (id)initWithFrame:(NSRect)frame libuiColorButton:(uiColorButton *)b
{
	self = [super initWithFrame:frame];
	if (self) {
		// the default color is white; set it to black first (see -setColor: below for why we do it first)
		[self libuiSetColor:0.0 g:0.0 b:0.0 a:1.0];

		self->libui_b = b;
		self->libui_changing = NO;
	}
	return self;
}

- (void)activate:(BOOL)exclusive
{
	if (activeColorButton != nil)
		activeColorButton->libui_changing = YES;
	[NSColorPanel setPickerMask:NSColorPanelAllModesMask];
	[[NSColorPanel sharedColorPanel] setShowsAlpha:YES];
	[super activate:YES];
	activeColorButton = self;
	// see stddialogs.m for details
	[[NSColorPanel sharedColorPanel] setWorksWhenModal:NO];
	[[NSNotificationCenter defaultCenter] addObserver:self
		selector:@selector(deactivateOnClose:)
		name:NSWindowWillCloseNotification
		object:[NSColorPanel sharedColorPanel]];
}

- (void)deactivate
{
	[super deactivate];
	activeColorButton = nil;
	if (!self->libui_changing)
		[[NSColorPanel sharedColorPanel] orderOut:nil];
	[[NSNotificationCenter defaultCenter] removeObserver:self
		name:NSWindowWillCloseNotification
		object:[NSColorPanel sharedColorPanel]];
	self->libui_changing = NO;
}

- (void)deactivateOnClose:(NSNotification *)note
{
	[self deactivate];
}

- (void)setColor:(NSColor *)color
{
	uiColorButton *b = self->libui_b;

	[super setColor:color];
	// this is called by NSColorWell's init, so we have to guard
	// also don't signal during a programmatic change
	if (b != nil && !self->libui_setting)
		(*(b->onChanged))(b, b->onChangedData);
}

- (void)libuiColor:(double *)r g:(double *)g b:(double *)b a:(double *)a
{
	NSColor *rgba;
	CGFloat cr, cg, cb, ca;

	// the given color may not be an RGBA color, which will cause the -getRed:green:blue:alpha: call to throw an exception
	rgba = [[self color] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	[rgba getRed:&cr green:&cg blue:&cb alpha:&ca];
	*r = cr;
	*g = cg;
	*b = cb;
	*a = ca;
	// rgba will be autoreleased since it isn't a new or init call
}

- (void)libuiSetColor:(double)r g:(double)g b:(double)b a:(double)a
{
	self->libui_setting = YES;
	[self setColor:[NSColor colorWithSRGBRed:r green:g blue:b alpha:a]];
	self->libui_setting = NO;
}

// NSColorWell has no intrinsic size by default; give it the default Interface Builder size.
- (NSSize)intrinsicContentSize
{
	return NSMakeSize(44, 23);
}

@end

uiDarwinControlAllDefaults(uiColorButton, button)

// we do not want color change events to be sent to any controls other than the color buttons
// see main.m for more details
BOOL colorButtonInhibitSendAction(SEL sel, id from, id to)
{
	if (sel != @selector(changeColor:))
		return NO;
	return ![to isKindOfClass:[colorButton class]];
}

static void defaultOnChanged(uiColorButton *b, void *data)
{
	// do nothing
}

void uiColorButtonColor(uiColorButton *b, double *r, double *g, double *bl, double *a)
{
	[b->button libuiColor:r g:g b:bl a:a];
}

void uiColorButtonSetColor(uiColorButton *b, double r, double g, double bl, double a)
{
	[b->button libuiSetColor:r g:g b:bl a:a];
}

void uiColorButtonOnChanged(uiColorButton *b, void (*f)(uiColorButton *, void *), void *data)
{
	b->onChanged = f;
	b->onChangedData = data;
}

uiColorButton *uiNewColorButton(void)
{
	uiColorButton *b;

	uiDarwinNewControl(uiColorButton, b);

	b->button = [[colorButton alloc] initWithFrame:NSZeroRect libuiColorButton:b];

	uiColorButtonOnChanged(b, defaultOnChanged, NULL);

	return b;
}
