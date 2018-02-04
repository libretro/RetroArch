// 14 august 2015
#import "uipriv_darwin.h"

// TODO resizing the controlgallery vertically causes the third button to still resize :|

// In the old days you would use a NSMatrix for this; as of OS X 10.8 this was deprecated and now you need just a bunch of NSButtons with the same superview AND same action method.
// This is documented on the NSMatrix page, but the rest of the OS X documentation says to still use NSMatrix.
// NSMatrix has weird quirks anyway...

// LONGTERM 6 units of spacing between buttons, as suggested by Interface Builder?

@interface radioButtonsDelegate : NSObject {
	uiRadioButtons *libui_r;
}
- (id)initWithR:(uiRadioButtons *)r;
- (IBAction)onClicked:(id)sender;
@end

struct uiRadioButtons {
	uiDarwinControl c;
	NSView *view;
	NSMutableArray *buttons;
	NSMutableArray *constraints;
	NSLayoutConstraint *lastv;
	radioButtonsDelegate *delegate;
	void (*onSelected)(uiRadioButtons *, void *);
	void *onSelectedData;
};

@implementation radioButtonsDelegate

- (id)initWithR:(uiRadioButtons *)r
{
	self = [super init];
	if (self)
		self->libui_r = r;
	return self;
}

- (IBAction)onClicked:(id)sender
{
	uiRadioButtons *r = self->libui_r;

	(*(r->onSelected))(r, r->onSelectedData);
}

@end

uiDarwinControlAllDefaultsExceptDestroy(uiRadioButtons, view)

static void defaultOnSelected(uiRadioButtons *r, void *data)
{
	// do nothing
}

static void uiRadioButtonsDestroy(uiControl *c)
{
	uiRadioButtons *r = uiRadioButtons(c);
	NSButton *b;

	// drop the constraints
	[r->view removeConstraints:r->constraints];
	[r->constraints release];
	if (r->lastv != nil)
		[r->lastv release];
	// destroy the buttons
	for (b in r->buttons) {
		[b setTarget:nil];
		[b removeFromSuperview];
	}
	[r->buttons release];
	// destroy the delegate
	[r->delegate release];
	// and destroy ourselves
	[r->view release];
	uiFreeControl(uiControl(r));
}

static NSButton *buttonAt(uiRadioButtons *r, int n)
{
	return (NSButton *) [r->buttons objectAtIndex:n];
}

void uiRadioButtonsAppend(uiRadioButtons *r, const char *text)
{
	NSButton *b, *b2;
	NSLayoutConstraint *constraint;

	b = [[NSButton alloc] initWithFrame:NSZeroRect];
	[b setTitle:toNSString(text)];
	[b setButtonType:NSRadioButton];
	// doesn't seem to have an associated bezel style
	[b setBordered:NO];
	[b setTransparent:NO];
	uiDarwinSetControlFont(b, NSRegularControlSize);
	[b setTranslatesAutoresizingMaskIntoConstraints:NO];

	[b setTarget:r->delegate];
	[b setAction:@selector(onClicked:)];

	[r->buttons addObject:b];
	[r->view addSubview:b];

	// pin horizontally to the edges of the superview
	constraint = mkConstraint(b, NSLayoutAttributeLeading,
		NSLayoutRelationEqual,
		r->view, NSLayoutAttributeLeading,
		1, 0,
		@"uiRadioButtons button leading constraint");
	[r->view addConstraint:constraint];
	[r->constraints addObject:constraint];
	constraint = mkConstraint(b, NSLayoutAttributeTrailing,
		NSLayoutRelationEqual,
		r->view, NSLayoutAttributeTrailing,
		1, 0,
		@"uiRadioButtons button trailing constraint");
	[r->view addConstraint:constraint];
	[r->constraints addObject:constraint];

	// if this is the first view, pin it to the top
	// otherwise pin to the bottom of the last
	if ([r->buttons count] == 1)
		constraint = mkConstraint(b, NSLayoutAttributeTop,
			NSLayoutRelationEqual,
			r->view, NSLayoutAttributeTop,
			1, 0,
			@"uiRadioButtons first button top constraint");
	else {
		b2 = buttonAt(r, [r->buttons count] - 2);
		constraint = mkConstraint(b, NSLayoutAttributeTop,
			NSLayoutRelationEqual,
			b2, NSLayoutAttributeBottom,
			1, 0,
			@"uiRadioButtons non-first button top constraint");
	}
	[r->view addConstraint:constraint];
	[r->constraints addObject:constraint];

	// if there is a previous bottom constraint, remove it
	if (r->lastv != nil) {
		[r->view removeConstraint:r->lastv];
		[r->constraints removeObject:r->lastv];
		[r->lastv release];
	}

	// and make the new bottom constraint
	r->lastv = mkConstraint(b, NSLayoutAttributeBottom,
		NSLayoutRelationEqual,
		r->view, NSLayoutAttributeBottom,
		1, 0,
		@"uiRadioButtons last button bottom constraint");
	[r->view addConstraint:r->lastv];
	[r->constraints addObject:r->lastv];
	[r->lastv retain];
}

int uiRadioButtonsSelected(uiRadioButtons *r)
{
	NSButton *b;
	NSUInteger i;

	for (i = 0; i < [r->buttons count]; i++) {
		b = (NSButton *) [r->buttons objectAtIndex:i];
		if ([b state] == NSOnState)
			return i;
	}
	return -1;
}

void uiRadioButtonsSetSelected(uiRadioButtons *r, int n)
{
	NSButton *b;
	NSInteger state;

	state = NSOnState;
	if (n == -1) {
		n = uiRadioButtonsSelected(r);
		if (n == -1)		// from nothing to nothing; do nothing
			return;
		state = NSOffState;
	}
	b = (NSButton *) [r->buttons objectAtIndex:n];
	[b setState:state];
}

void uiRadioButtonsOnSelected(uiRadioButtons *r, void (*f)(uiRadioButtons *, void *), void *data)
{
	r->onSelected = f;
	r->onSelectedData = data;
}

uiRadioButtons *uiNewRadioButtons(void)
{
	uiRadioButtons *r;

	uiDarwinNewControl(uiRadioButtons, r);

	r->view = [[NSView alloc] initWithFrame:NSZeroRect];
	r->buttons = [NSMutableArray new];
	r->constraints = [NSMutableArray new];

	r->delegate = [[radioButtonsDelegate alloc] initWithR:r];

	uiRadioButtonsOnSelected(r, defaultOnSelected, NULL);

	return r;
}
