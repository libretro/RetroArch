// 7 june 2016
#import "uipriv_darwin.h"

// TODO in the test program, sometimes one of the radio buttons can disappear (try when spaced)

@interface formChild : NSView
@property uiControl *c;
@property (strong) NSTextField *label;
@property BOOL stretchy;
@property NSLayoutPriority oldHorzHuggingPri;
@property NSLayoutPriority oldVertHuggingPri;
@property (strong) NSLayoutConstraint *baseline;
@property (strong) NSLayoutConstraint *leading;
@property (strong) NSLayoutConstraint *top;
@property (strong) NSLayoutConstraint *trailing;
@property (strong) NSLayoutConstraint *bottom;
- (id)initWithLabel:(NSTextField *)l;
- (void)onDestroy;
- (NSView *)view;
@end

@interface formView : NSView {
	uiForm *f;
	NSMutableArray *children;
	int padded;

	NSLayoutConstraint *first;
	NSMutableArray *inBetweens;
	NSLayoutConstraint *last;
	NSMutableArray *widths;
	NSMutableArray *leadings;
	NSMutableArray *middles;
	NSMutableArray *trailings;
}
- (id)initWithF:(uiForm *)ff;
- (void)onDestroy;
- (void)removeOurConstraints;
- (void)syncEnableStates:(int)enabled;
- (CGFloat)paddingAmount;
- (void)establishOurConstraints;
- (void)append:(NSString *)label c:(uiControl *)c stretchy:(int)stretchy;
- (void)delete:(int)n;
- (int)isPadded;
- (void)setPadded:(int)p;
- (BOOL)hugsTrailing;
- (BOOL)hugsBottom;
- (int)nStretchy;
@end

struct uiForm {
	uiDarwinControl c;
	formView *view;
};

@implementation formChild

- (id)initWithLabel:(NSTextField *)l
{
	self = [super initWithFrame:NSZeroRect];
	if (self) {
		self.label = l;
		[self.label setTranslatesAutoresizingMaskIntoConstraints:NO];
		[self.label setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
		[self.label setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationVertical];
		[self.label setContentCompressionResistancePriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
		[self.label setContentCompressionResistancePriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationVertical];
		[self addSubview:self.label];

		self.leading = mkConstraint(self.label, NSLayoutAttributeLeading,
			NSLayoutRelationGreaterThanOrEqual,
			self, NSLayoutAttributeLeading,
			1, 0,
			@"uiForm label leading");
		[self addConstraint:self.leading];
		self.top = mkConstraint(self.label, NSLayoutAttributeTop,
			NSLayoutRelationEqual,
			self, NSLayoutAttributeTop,
			1, 0,
			@"uiForm label top");
		[self addConstraint:self.top];
		self.trailing = mkConstraint(self.label, NSLayoutAttributeTrailing,
			NSLayoutRelationEqual,
			self, NSLayoutAttributeTrailing,
			1, 0,
			@"uiForm label trailing");
		[self addConstraint:self.trailing];
		self.bottom = mkConstraint(self.label, NSLayoutAttributeBottom,
			NSLayoutRelationEqual,
			self, NSLayoutAttributeBottom,
			1, 0,
			@"uiForm label bottom");
		[self addConstraint:self.bottom];
	}
	return self;
}

- (void)onDestroy
{
	[self removeConstraint:self.trailing];
	self.trailing = nil;
	[self removeConstraint:self.top];
	self.top = nil;
	[self removeConstraint:self.bottom];
	self.bottom = nil;

	[self.label removeFromSuperview];
	self.label = nil;
}

- (NSView *)view
{
	return (NSView *) uiControlHandle(self.c);
}

@end

@implementation formView

- (id)initWithF:(uiForm *)ff
{
	self = [super initWithFrame:NSZeroRect];
	if (self != nil) {
		self->f = ff;
		self->padded = 0;
		self->children = [NSMutableArray new];

		self->inBetweens = [NSMutableArray new];
		self->widths = [NSMutableArray new];
		self->leadings = [NSMutableArray new];
		self->middles = [NSMutableArray new];
		self->trailings = [NSMutableArray new];
	}
	return self;
}

- (void)onDestroy
{
	formChild *fc;

	[self removeOurConstraints];
	[self->inBetweens release];
	[self->widths release];
	[self->leadings release];
	[self->middles release];
	[self->trailings release];

	for (fc in self->children) {
		[self removeConstraint:fc.baseline];
		fc.baseline = nil;
		uiControlSetParent(fc.c, NULL);
		uiDarwinControlSetSuperview(uiDarwinControl(fc.c), nil);
		uiControlDestroy(fc.c);
		[fc onDestroy];
		[fc removeFromSuperview];
	}
	[self->children release];
}

- (void)removeOurConstraints
{
	if (self->first != nil) {
		[self removeConstraint:self->first];
		[self->first release];
		self->first = nil;
	}
	if ([self->inBetweens count] != 0) {
		[self removeConstraints:self->inBetweens];
		[self->inBetweens removeAllObjects];
	}
	if (self->last != nil) {
		[self removeConstraint:self->last];
		[self->last release];
		self->last = nil;
	}
	if ([self->widths count] != 0) {
		[self removeConstraints:self->widths];
		[self->widths removeAllObjects];
	}
	if ([self->leadings count] != 0) {
		[self removeConstraints:self->leadings];
		[self->leadings removeAllObjects];
	}
	if ([self->middles count] != 0) {
		[self removeConstraints:self->middles];
		[self->middles removeAllObjects];
	}
	if ([self->trailings count] != 0) {
		[self removeConstraints:self->trailings];
		[self->trailings removeAllObjects];
	}
}

- (void)syncEnableStates:(int)enabled
{
	formChild *fc;

	for (fc in self->children)
		uiDarwinControlSyncEnableState(uiDarwinControl(fc.c), enabled);
}

- (CGFloat)paddingAmount
{
	if (!self->padded)
		return 0.0;
	return uiDarwinPaddingAmount(NULL);
}

- (void)establishOurConstraints
{
	formChild *fc;
	CGFloat padding;
	NSView *prev, *prevlabel;
	NSLayoutConstraint *c;

	[self removeOurConstraints];
	if ([self->children count] == 0)
		return;
	padding = [self paddingAmount];

	// first arrange the children vertically and make them the same width
	prev = nil;
	for (fc in self->children) {
		[fc setHidden:!uiControlVisible(fc.c)];
		if (!uiControlVisible(fc.c))
			continue;
		if (prev == nil) {			// first view
			self->first = mkConstraint(self, NSLayoutAttributeTop,
				NSLayoutRelationEqual,
				[fc view], NSLayoutAttributeTop,
				1, 0,
				@"uiForm first vertical constraint");
			[self addConstraint:self->first];
			[self->first retain];
			prev = [fc view];
			prevlabel = fc;
			continue;
		}
		// not the first; link it
		c = mkConstraint(prev, NSLayoutAttributeBottom,
			NSLayoutRelationEqual,
			[fc view], NSLayoutAttributeTop,
			1, -padding,
			@"uiForm in-between vertical constraint");
		[self addConstraint:c];
		[self->inBetweens addObject:c];
		// and make the same width
		c = mkConstraint(prev, NSLayoutAttributeWidth,
			NSLayoutRelationEqual,
			[fc view], NSLayoutAttributeWidth,
			1, 0,
			@"uiForm control width constraint");
		[self addConstraint:c];
		[self->widths addObject:c];
		c = mkConstraint(prevlabel, NSLayoutAttributeWidth,
			NSLayoutRelationEqual,
			fc, NSLayoutAttributeWidth,
			1, 0,
			@"uiForm label lwidth constraint");
		[self addConstraint:c];
		[self->widths addObject:c];
		prev = [fc view];
		prevlabel = fc;
	}
	if (prev == nil)		// all hidden; act as if nothing there
		return;
	self->last = mkConstraint(prev, NSLayoutAttributeBottom,
		NSLayoutRelationEqual,
		self, NSLayoutAttributeBottom,
		1, 0,
		@"uiForm last vertical constraint");
	[self addConstraint:self->last];
	[self->last retain];

	// now arrange the controls horizontally
	for (fc in self->children) {
		if (!uiControlVisible(fc.c))
			continue;
		c = mkConstraint(self, NSLayoutAttributeLeading,
			NSLayoutRelationEqual,
			fc, NSLayoutAttributeLeading,
			1, 0,
			@"uiForm leading constraint");
		[self addConstraint:c];
		[self->leadings addObject:c];
		// coerce the control to be as wide as possible
		// see http://stackoverflow.com/questions/37710892/in-auto-layout-i-set-up-labels-that-shouldnt-grow-horizontally-and-controls-th
		c = mkConstraint(self, NSLayoutAttributeLeading,
			NSLayoutRelationEqual,
			[fc view], NSLayoutAttributeLeading,
			1, 0,
			@"uiForm leading constraint");
		[c setPriority:NSLayoutPriorityDefaultHigh];
		[self addConstraint:c];
		[self->leadings addObject:c];
		c = mkConstraint(fc, NSLayoutAttributeTrailing,
			NSLayoutRelationEqual,
			[fc view], NSLayoutAttributeLeading,
			1, -padding,
			@"uiForm middle constraint");
		[self addConstraint:c];
		[self->middles addObject:c];
		c = mkConstraint([fc view], NSLayoutAttributeTrailing,
			NSLayoutRelationEqual,
			self, NSLayoutAttributeTrailing,
			1, 0,
			@"uiForm trailing constraint");
		[self addConstraint:c];
		[self->trailings addObject:c];
		// TODO
		c = mkConstraint(fc, NSLayoutAttributeBottom,
			NSLayoutRelationLessThanOrEqual,
			self, NSLayoutAttributeBottom,
			1, 0,
			@"TODO");
		[self addConstraint:c];
		[self->trailings addObject:c];
	}

	// and make all stretchy controls have the same height
	prev = nil;
	for (fc in self->children) {
		if (!uiControlVisible(fc.c))
			continue;
		if (!fc.stretchy)
			continue;
		if (prev == nil) {
			prev = [fc view];
			continue;
		}
		c = mkConstraint([fc view], NSLayoutAttributeHeight,
			NSLayoutRelationEqual,
			prev, NSLayoutAttributeHeight,
			1, 0,
			@"uiForm stretchy constraint");
		[self addConstraint:c];
		// TODO make a dedicated array for this
		[self->leadings addObject:c];
	}

	// we don't arrange the labels vertically; that's done when we add the control since those constraints don't need to change (they just need to be at their baseline)
}

- (void)append:(NSString *)label c:(uiControl *)c stretchy:(int)stretchy
{
	formChild *fc;
	NSLayoutPriority priority;
	NSLayoutAttribute attribute;
	int oldnStretchy;

	fc = [[formChild alloc] initWithLabel:newLabel(label)];
	fc.c = c;
	fc.stretchy = stretchy;
	fc.oldHorzHuggingPri = uiDarwinControlHuggingPriority(uiDarwinControl(fc.c), NSLayoutConstraintOrientationHorizontal);
	fc.oldVertHuggingPri = uiDarwinControlHuggingPriority(uiDarwinControl(fc.c), NSLayoutConstraintOrientationVertical);
	[fc setTranslatesAutoresizingMaskIntoConstraints:NO];
	[self addSubview:fc];

	uiControlSetParent(fc.c, uiControl(self->f));
	uiDarwinControlSetSuperview(uiDarwinControl(fc.c), self);
	uiDarwinControlSyncEnableState(uiDarwinControl(fc.c), uiControlEnabledToUser(uiControl(self->f)));

	// if a control is stretchy, it should not hug vertically
	// otherwise, it should *forcibly* hug
	if (fc.stretchy)
		priority = NSLayoutPriorityDefaultLow;
	else
		// LONGTERM will default high work?
		priority = NSLayoutPriorityRequired;
	uiDarwinControlSetHuggingPriority(uiDarwinControl(fc.c), priority, NSLayoutConstraintOrientationVertical);
	// make sure controls don't hug their horizontal direction so they fill the width of the view
	uiDarwinControlSetHuggingPriority(uiDarwinControl(fc.c), NSLayoutPriorityDefaultLow, NSLayoutConstraintOrientationHorizontal);

	// and constrain the baselines to position the label vertically
	// if the view is a scroll view, align tops, not baselines
	// this is what Interface Builder does
	attribute = NSLayoutAttributeBaseline;
	if ([[fc view] isKindOfClass:[NSScrollView class]])
		attribute = NSLayoutAttributeTop;
	fc.baseline = mkConstraint(fc.label, attribute,
		NSLayoutRelationEqual,
		[fc view], attribute,
		1, 0,
		@"uiForm baseline constraint");
	[self addConstraint:fc.baseline];

	oldnStretchy = [self nStretchy];
	[self->children addObject:fc];

	[self establishOurConstraints];
	if (fc.stretchy)
		if (oldnStretchy == 0)
			uiDarwinNotifyEdgeHuggingChanged(uiDarwinControl(self->f));

	[fc release];		// we don't need the initial reference now
}

- (void)delete:(int)n
{
	formChild *fc;
	int stretchy;

	fc = (formChild *) [self->children objectAtIndex:n];
	stretchy = fc.stretchy;

	uiControlSetParent(fc.c, NULL);
	uiDarwinControlSetSuperview(uiDarwinControl(fc.c), nil);

	uiDarwinControlSetHuggingPriority(uiDarwinControl(fc.c), fc.oldHorzHuggingPri, NSLayoutConstraintOrientationHorizontal);
	uiDarwinControlSetHuggingPriority(uiDarwinControl(fc.c), fc.oldVertHuggingPri, NSLayoutConstraintOrientationVertical);

	[fc onDestroy];
	[self->children removeObjectAtIndex:n];

	[self establishOurConstraints];
	if (stretchy)
		if ([self nStretchy] == 0)
			uiDarwinNotifyEdgeHuggingChanged(uiDarwinControl(self->f));
}

- (int)isPadded
{
	return self->padded;
}

- (void)setPadded:(int)p
{
	CGFloat padding;
	NSLayoutConstraint *c;

	self->padded = p;
	padding = [self paddingAmount];
	for (c in self->inBetweens)
		[c setConstant:-padding];
	for (c in self->middles)
		[c setConstant:-padding];
}

- (BOOL)hugsTrailing
{
	return YES;			// always hug trailing
}

- (BOOL)hugsBottom
{
	// only hug if we have stretchy
	return [self nStretchy] != 0;
}

- (int)nStretchy
{
	formChild *fc;
	int n;

	n = 0;
	for (fc in self->children) {
		if (!uiControlVisible(fc.c))
			continue;
		if (fc.stretchy)
			n++;
	}
	return n;
}

@end

static void uiFormDestroy(uiControl *c)
{
	uiForm *f = uiForm(c);

	[f->view onDestroy];
	[f->view release];
	uiFreeControl(uiControl(f));
}

uiDarwinControlDefaultHandle(uiForm, view)
uiDarwinControlDefaultParent(uiForm, view)
uiDarwinControlDefaultSetParent(uiForm, view)
uiDarwinControlDefaultToplevel(uiForm, view)
uiDarwinControlDefaultVisible(uiForm, view)
uiDarwinControlDefaultShow(uiForm, view)
uiDarwinControlDefaultHide(uiForm, view)
uiDarwinControlDefaultEnabled(uiForm, view)
uiDarwinControlDefaultEnable(uiForm, view)
uiDarwinControlDefaultDisable(uiForm, view)

static void uiFormSyncEnableState(uiDarwinControl *c, int enabled)
{
	uiForm *f = uiForm(c);

	if (uiDarwinShouldStopSyncEnableState(uiDarwinControl(f), enabled))
		return;
	[f->view syncEnableStates:enabled];
}

uiDarwinControlDefaultSetSuperview(uiForm, view)

static BOOL uiFormHugsTrailingEdge(uiDarwinControl *c)
{
	uiForm *f = uiForm(c);

	return [f->view hugsTrailing];
}

static BOOL uiFormHugsBottom(uiDarwinControl *c)
{
	uiForm *f = uiForm(c);

	return [f->view hugsBottom];
}

static void uiFormChildEdgeHuggingChanged(uiDarwinControl *c)
{
	uiForm *f = uiForm(c);

	[f->view establishOurConstraints];
}

uiDarwinControlDefaultHuggingPriority(uiForm, view)
uiDarwinControlDefaultSetHuggingPriority(uiForm, view)

static void uiFormChildVisibilityChanged(uiDarwinControl *c)
{
	uiForm *f = uiForm(c);

	[f->view establishOurConstraints];
}

void uiFormAppend(uiForm *f, const char *label, uiControl *c, int stretchy)
{
	// LONGTERM on other platforms
	// or at leat allow this and implicitly turn it into a spacer
	if (c == NULL)
		userbug("You cannot add NULL to a uiForm.");
	[f->view append:toNSString(label) c:c stretchy:stretchy];
}

void uiFormDelete(uiForm *f, int n)
{
	[f->view delete:n];
}

int uiFormPadded(uiForm *f)
{
	return [f->view isPadded];
}

void uiFormSetPadded(uiForm *f, int padded)
{
	[f->view setPadded:padded];
}

uiForm *uiNewForm(void)
{
	uiForm *f;

	uiDarwinNewControl(uiForm, f);

	f->view = [[formView alloc] initWithF:f];

	return f;
}
