// 11 june 2016
#import "uipriv_darwin.h"

// TODO the assorted test doesn't work right at all

@interface gridChild : NSView
@property uiControl *c;
@property int left;
@property int top;
@property int xspan;
@property int yspan;
@property int hexpand;
@property uiAlign halign;
@property int vexpand;
@property uiAlign valign;

@property (strong) NSLayoutConstraint *leadingc;
@property (strong) NSLayoutConstraint *topc;
@property (strong) NSLayoutConstraint *trailingc;
@property (strong) NSLayoutConstraint *bottomc;
@property (strong) NSLayoutConstraint *xcenterc;
@property (strong) NSLayoutConstraint *ycenterc;

@property NSLayoutPriority oldHorzHuggingPri;
@property NSLayoutPriority oldVertHuggingPri;
- (void)setC:(uiControl *)c grid:(uiGrid *)g;
- (void)onDestroy;
- (NSView *)view;
@end

@interface gridView : NSView {
	uiGrid *g;
	NSMutableArray *children;
	int padded;

	NSMutableArray *edges;
	NSMutableArray *inBetweens;

	NSMutableArray *emptyCellViews;
}
- (id)initWithG:(uiGrid *)gg;
- (void)onDestroy;
- (void)removeOurConstraints;
- (void)syncEnableStates:(int)enabled;
- (CGFloat)paddingAmount;
- (void)establishOurConstraints;
- (void)append:(gridChild *)gc;
- (void)insert:(gridChild *)gc after:(uiControl *)c at:(uiAt)at;
- (int)isPadded;
- (void)setPadded:(int)p;
- (BOOL)hugsTrailing;
- (BOOL)hugsBottom;
- (int)nhexpand;
- (int)nvexpand;
@end

struct uiGrid {
	uiDarwinControl c;
	gridView *view;
};

@implementation gridChild

- (void)setC:(uiControl *)c grid:(uiGrid *)g
{
	self.c = c;
	self.oldHorzHuggingPri = uiDarwinControlHuggingPriority(uiDarwinControl(self.c), NSLayoutConstraintOrientationHorizontal);
	self.oldVertHuggingPri = uiDarwinControlHuggingPriority(uiDarwinControl(self.c), NSLayoutConstraintOrientationVertical);

	uiControlSetParent(self.c, uiControl(g));
	uiDarwinControlSetSuperview(uiDarwinControl(self.c), self);
	uiDarwinControlSyncEnableState(uiDarwinControl(self.c), uiControlEnabledToUser(uiControl(g)));

	if (self.halign == uiAlignStart || self.halign == uiAlignFill) {
		self.leadingc = mkConstraint(self, NSLayoutAttributeLeading,
			NSLayoutRelationEqual,
			[self view], NSLayoutAttributeLeading,
			1, 0,
			@"uiGrid child horizontal alignment start constraint");
		[self addConstraint:self.leadingc];
	}
	if (self.halign == uiAlignCenter) {
		self.xcenterc = mkConstraint(self, NSLayoutAttributeCenterX,
			NSLayoutRelationEqual,
			[self view], NSLayoutAttributeCenterX,
			1, 0,
			@"uiGrid child horizontal alignment center constraint");
		[self addConstraint:self.xcenterc];
	}
	if (self.halign == uiAlignEnd || self.halign == uiAlignFill) {
		self.trailingc = mkConstraint(self, NSLayoutAttributeTrailing,
			NSLayoutRelationEqual,
			[self view], NSLayoutAttributeTrailing,
			1, 0,
			@"uiGrid child horizontal alignment end constraint");
		[self addConstraint:self.trailingc];
	}

	if (self.valign == uiAlignStart || self.valign == uiAlignFill) {
		self.topc = mkConstraint(self, NSLayoutAttributeTop,
			NSLayoutRelationEqual,
			[self view], NSLayoutAttributeTop,
			1, 0,
			@"uiGrid child vertical alignment start constraint");
		[self addConstraint:self.topc];
	}
	if (self.valign == uiAlignCenter) {
		self.ycenterc = mkConstraint(self, NSLayoutAttributeCenterY,
			NSLayoutRelationEqual,
			[self view], NSLayoutAttributeCenterY,
			1, 0,
			@"uiGrid child vertical alignment center constraint");
		[self addConstraint:self.ycenterc];
	}
	if (self.valign == uiAlignEnd || self.valign == uiAlignFill) {
		self.bottomc = mkConstraint(self, NSLayoutAttributeBottom,
			NSLayoutRelationEqual,
			[self view], NSLayoutAttributeBottom,
			1, 0,
			@"uiGrid child vertical alignment end constraint");
		[self addConstraint:self.bottomc];
	}
}

- (void)onDestroy
{
	if (self.leadingc != nil) {
		[self removeConstraint:self.leadingc];
		self.leadingc = nil;
	}
	if (self.topc != nil) {
		[self removeConstraint:self.topc];
		self.topc = nil;
	}
	if (self.trailingc != nil) {
		[self removeConstraint:self.trailingc];
		self.trailingc = nil;
	}
	if (self.bottomc != nil) {
		[self removeConstraint:self.bottomc];
		self.bottomc = nil;
	}
	if (self.xcenterc != nil) {
		[self removeConstraint:self.xcenterc];
		self.xcenterc = nil;
	}
	if (self.ycenterc != nil) {
		[self removeConstraint:self.ycenterc];
		self.ycenterc = nil;
	}

	uiControlSetParent(self.c, NULL);
	uiDarwinControlSetSuperview(uiDarwinControl(self.c), nil);
	uiDarwinControlSetHuggingPriority(uiDarwinControl(self.c), self.oldHorzHuggingPri, NSLayoutConstraintOrientationHorizontal);
	uiDarwinControlSetHuggingPriority(uiDarwinControl(self.c), self.oldVertHuggingPri, NSLayoutConstraintOrientationVertical);
}

- (NSView *)view
{
	return (NSView *) uiControlHandle(self.c);
}

@end

@implementation gridView

- (id)initWithG:(uiGrid *)gg
{
	self = [super initWithFrame:NSZeroRect];
	if (self != nil) {
		self->g = gg;
		self->padded = 0;
		self->children = [NSMutableArray new];

		self->edges = [NSMutableArray new];
		self->inBetweens = [NSMutableArray new];

		self->emptyCellViews = [NSMutableArray new];
	}
	return self;
}

- (void)onDestroy
{
	gridChild *gc;

	[self removeOurConstraints];
	[self->edges release];
	[self->inBetweens release];

	[self->emptyCellViews release];

	for (gc in self->children) {
		[gc onDestroy];
		uiControlDestroy(gc.c);
		[gc removeFromSuperview];
	}
	[self->children release];
}

- (void)removeOurConstraints
{
	NSView *v;

	if ([self->edges count] != 0) {
		[self removeConstraints:self->edges];
		[self->edges removeAllObjects];
	}
	if ([self->inBetweens count] != 0) {
		[self removeConstraints:self->inBetweens];
		[self->inBetweens removeAllObjects];
	}

	for (v in self->emptyCellViews)
		[v removeFromSuperview];
	[self->emptyCellViews removeAllObjects];
}

- (void)syncEnableStates:(int)enabled
{
	gridChild *gc;

	for (gc in self->children)
		uiDarwinControlSyncEnableState(uiDarwinControl(gc.c), enabled);
}

- (CGFloat)paddingAmount
{
	if (!self->padded)
		return 0.0;
	return uiDarwinPaddingAmount(NULL);
}

// LONGTERM stop early if all controls are hidden
- (void)establishOurConstraints
{
	gridChild *gc;
	CGFloat padding;
	int xmin, ymin;
	int xmax, ymax;
	int xcount, ycount;
	BOOL first;
	int **gg;
	NSView ***gv;
	BOOL **gspan;
	int x, y;
	int i;
	NSLayoutConstraint *c;
	int firstx, firsty;
	BOOL *hexpand, *vexpand;
	BOOL doit;
	BOOL onlyEmptyAndSpanning;

	[self removeOurConstraints];
	if ([self->children count] == 0)
		return;
	padding = [self paddingAmount];

	// first, figure out the minimum and maximum row and column numbers
	// ignore hidden controls
	first = YES;
	for (gc in self->children) {
		// this bit is important: it ensures row ymin and column xmin have at least one cell to draw, so the onlyEmptyAndSpanning logic below will never run on those rows
		if (!uiControlVisible(gc.c))
			continue;
		if (first) {
			xmin = gc.left;
			ymin = gc.top;
			xmax = gc.left + gc.xspan;
			ymax = gc.top + gc.yspan;
			first = NO;
			continue;
		}
		if (xmin > gc.left)
			xmin = gc.left;
		if (ymin > gc.top)
			ymin = gc.top;
		if (xmax < (gc.left + gc.xspan))
			xmax = gc.left + gc.xspan;
		if (ymax < (gc.top + gc.yspan))
			ymax = gc.top + gc.yspan;
	}
	if (first != NO)		// the entire grid is hidden; do nothing
		return;
	xcount = xmax - xmin;
	ycount = ymax - ymin;

	// now build a topological map of the grid gg[y][x]
	// also figure out which cells contain spanned views so they can be ignored later
	// treat hidden controls by keeping the indices -1
	gg = (int **) uiAlloc(ycount * sizeof (int *), "int[][]");
	gspan = (BOOL **) uiAlloc(ycount * sizeof (BOOL *), "BOOL[][]");
	for (y = 0; y < ycount; y++) {
		gg[y] = (int *) uiAlloc(xcount * sizeof (int), "int[]");
		gspan[y] = (BOOL *) uiAlloc(xcount * sizeof (BOOL), "BOOL[]");
		for (x = 0; x < xcount; x++)
			gg[y][x] = -1;		// empty
	}
	for (i = 0; i < [self->children count]; i++) {
		gc = (gridChild *) [self->children objectAtIndex:i];
		if (!uiControlVisible(gc.c))
			continue;
		for (y = gc.top; y < gc.top + gc.yspan; y++)
			for (x = gc.left; x < gc.left + gc.xspan; x++) {
				gg[y - ymin][x - xmin] = i;
				if (x != gc.left || y != gc.top)
					gspan[y - ymin][x - xmin] = YES;
			}
	}

	// if a row or column only contains emptys and spanning cells of a opposite-direction spannings, remove it by duplicating the previous row or column
	for (y = 0; y < ycount; y++) {
		onlyEmptyAndSpanning = YES;
		for (x = 0; x < xcount; x++)
			if (gg[y][x] != -1) {
				gc = (gridChild *) [self->children objectAtIndex:gg[y][x]];
				if (gc.yspan == 1 || gc.top - ymin == y) {
					onlyEmptyAndSpanning = NO;
					break;
				}
			}
		if (onlyEmptyAndSpanning)
			for (x = 0; x < xcount; x++) {
				gg[y][x] = gg[y - 1][x];
				gspan[y][x] = YES;
			}
	}
	for (x = 0; x < xcount; x++) {
		onlyEmptyAndSpanning = YES;
		for (y = 0; y < ycount; y++)
			if (gg[y][x] != -1) {
				gc = (gridChild *) [self->children objectAtIndex:gg[y][x]];
				if (gc.xspan == 1 || gc.left - xmin == x) {
					onlyEmptyAndSpanning = NO;
					break;
				}
			}
		if (onlyEmptyAndSpanning)
			for (y = 0; y < ycount; y++) {
				gg[y][x] = gg[y][x - 1];
				gspan[y][x] = YES;
			}
	}

	// now build a topological map of the grid's views gv[y][x]
	// for any empty cell, create a dummy view
	gv = (NSView ***) uiAlloc(ycount * sizeof (NSView **), "NSView *[][]");
	for (y = 0; y < ycount; y++) {
		gv[y] = (NSView **) uiAlloc(xcount * sizeof (NSView *), "NSView *[]");
		for (x = 0; x < xcount; x++)
			if (gg[y][x] == -1) {
				gv[y][x] = [[NSView alloc] initWithFrame:NSZeroRect];
				[gv[y][x] setTranslatesAutoresizingMaskIntoConstraints:NO];
				[self addSubview:gv[y][x]];
				[self->emptyCellViews addObject:gv[y][x]];
			} else {
				gc = (gridChild *) [self->children objectAtIndex:gg[y][x]];
				gv[y][x] = gc;
			}
	}

	// now figure out which rows and columns really expand
	hexpand = (BOOL *) uiAlloc(xcount * sizeof (BOOL), "BOOL[]");
	vexpand = (BOOL *) uiAlloc(ycount * sizeof (BOOL), "BOOL[]");
	// first, which don't span
	for (gc in self->children) {
		if (!uiControlVisible(gc.c))
			continue;
		if (gc.hexpand && gc.xspan == 1)
			hexpand[gc.left - xmin] = YES;
		if (gc.vexpand && gc.yspan == 1)
			vexpand[gc.top - ymin] = YES;
	}
	// second, which do span
	// the way we handle this is simple: if none of the spanned rows/columns expand, make all rows/columns expand
	for (gc in self->children) {
		if (!uiControlVisible(gc.c))
			continue;
		if (gc.hexpand && gc.xspan != 1) {
			doit = YES;
			for (x = gc.left; x < gc.left + gc.xspan; x++)
				if (hexpand[x - xmin]) {
					doit = NO;
					break;
				}
			if (doit)
				for (x = gc.left; x < gc.left + gc.xspan; x++)
					hexpand[x - xmin] = YES;
		}
		if (gc.vexpand && gc.yspan != 1) {
			doit = YES;
			for (y = gc.top; y < gc.top + gc.yspan; y++)
				if (vexpand[y - ymin]) {
					doit = NO;
					break;
				}
			if (doit)
				for (y = gc.top; y < gc.top + gc.yspan; y++)
					vexpand[y - ymin] = YES;
		}
	}

	// now establish all the edge constraints
	// leading and trailing edges
	for (y = 0; y < ycount; y++) {
		c = mkConstraint(self, NSLayoutAttributeLeading,
			NSLayoutRelationEqual,
			gv[y][0], NSLayoutAttributeLeading,
			1, 0,
			@"uiGrid leading edge constraint");
		[self addConstraint:c];
		[self->edges addObject:c];
		c = mkConstraint(self, NSLayoutAttributeTrailing,
			NSLayoutRelationEqual,
			gv[y][xcount - 1], NSLayoutAttributeTrailing,
			1, 0,
			@"uiGrid trailing edge constraint");
		[self addConstraint:c];
		[self->edges addObject:c];
	}
	// top and bottom edges
	for (x = 0; x < xcount; x++) {
		c = mkConstraint(self, NSLayoutAttributeTop,
			NSLayoutRelationEqual,
			gv[0][x], NSLayoutAttributeTop,
			1, 0,
			@"uiGrid top edge constraint");
		[self addConstraint:c];
		[self->edges addObject:c];
		c = mkConstraint(self, NSLayoutAttributeBottom,
			NSLayoutRelationEqual,
			gv[ycount - 1][x], NSLayoutAttributeBottom,
			1, 0,
			@"uiGrid bottom edge constraint");
		[self addConstraint:c];
		[self->edges addObject:c];
	}

	// now align leading and top edges
	// do NOT align spanning cells!
	for (x = 0; x < xcount; x++) {
		for (y = 0; y < ycount; y++)
			if (!gspan[y][x])
				break;
		firsty = y;
		for (y++; y < ycount; y++) {
			if (gspan[y][x])
				continue;
			c = mkConstraint(gv[firsty][x], NSLayoutAttributeLeading,
				NSLayoutRelationEqual,
				gv[y][x], NSLayoutAttributeLeading,
				1, 0,
				@"uiGrid column leading constraint");
			[self addConstraint:c];
			[self->edges addObject:c];
		}
	}
	for (y = 0; y < ycount; y++) {
		for (x = 0; x < xcount; x++)
			if (!gspan[y][x])
				break;
		firstx = x;
		for (x++; x < xcount; x++) {
			if (gspan[y][x])
				continue;
			c = mkConstraint(gv[y][firstx], NSLayoutAttributeTop,
				NSLayoutRelationEqual,
				gv[y][x], NSLayoutAttributeTop,
				1, 0,
				@"uiGrid row top constraint");
			[self addConstraint:c];
			[self->edges addObject:c];
		}
	}

	// now string adjacent views together
	for (y = 0; y < ycount; y++)
		for (x = 1; x < xcount; x++)
			if (gv[y][x - 1] != gv[y][x]) {
				c = mkConstraint(gv[y][x - 1], NSLayoutAttributeTrailing,
					NSLayoutRelationEqual,
					gv[y][x], NSLayoutAttributeLeading,
					1, -padding,
					@"uiGrid internal horizontal constraint");
				[self addConstraint:c];
				[self->inBetweens addObject:c];
			}
	for (x = 0; x < xcount; x++)
		for (y = 1; y < ycount; y++)
			if (gv[y - 1][x] != gv[y][x]) {
				c = mkConstraint(gv[y - 1][x], NSLayoutAttributeBottom,
					NSLayoutRelationEqual,
					gv[y][x], NSLayoutAttributeTop,
					1, -padding,
					@"uiGrid internal vertical constraint");
				[self addConstraint:c];
				[self->inBetweens addObject:c];
			}

	// now set priorities for all widgets that expand or not
	// if a cell is in an expanding row, OR If it spans, then it must be willing to stretch
	// otherwise, it tries not to
	// note we don't use NSLayoutPriorityRequired as that will cause things to squish when they shouldn't
	for (gc in self->children) {
		NSLayoutPriority priority;

		if (!uiControlVisible(gc.c))
			continue;
		if (hexpand[gc.left - xmin] || gc.xspan != 1)
			priority = NSLayoutPriorityDefaultLow;
		else
			priority = NSLayoutPriorityDefaultHigh;
		uiDarwinControlSetHuggingPriority(uiDarwinControl(gc.c), priority, NSLayoutConstraintOrientationHorizontal);
		// same for vertical direction
		if (vexpand[gc.top - ymin] || gc.yspan != 1)
			priority = NSLayoutPriorityDefaultLow;
		else
			priority = NSLayoutPriorityDefaultHigh;
		uiDarwinControlSetHuggingPriority(uiDarwinControl(gc.c), priority, NSLayoutConstraintOrientationVertical);
	}

	// TODO make all expanding rows/columns the same height/width

	// and finally clean up
	uiFree(hexpand);
	uiFree(vexpand);
	for (y = 0; y < ycount; y++) {
		uiFree(gg[y]);
		uiFree(gv[y]);
		uiFree(gspan[y]);
	}
	uiFree(gg);
	uiFree(gv);
	uiFree(gspan);
}

- (void)append:(gridChild *)gc
{
	BOOL update;
	int oldnh, oldnv;

	[gc setTranslatesAutoresizingMaskIntoConstraints:NO];
	[self addSubview:gc];

	// no need to set priority here; that's done in establishOurConstraints

	oldnh = [self nhexpand];
	oldnv = [self nvexpand];
	[self->children addObject:gc];

	[self establishOurConstraints];
	update = NO;
	if (gc.hexpand)
		if (oldnh == 0)
			update = YES;
	if (gc.vexpand)
		if (oldnv == 0)
			update = YES;
	if (update)
		uiDarwinNotifyEdgeHuggingChanged(uiDarwinControl(self->g));

	[gc release];		// we don't need the initial reference now
}

- (void)insert:(gridChild *)gc after:(uiControl *)c at:(uiAt)at
{
	gridChild *other;
	BOOL found;

	found = NO;
	for (other in self->children)
		if (other.c == c) {
			found = YES;
			break;
		}
	if (!found)
		userbug("Existing control %p is not in grid %p; you cannot add other controls next to it", c, self->g);

	switch (at) {
	case uiAtLeading:
		gc.left = other.left - gc.xspan;
		gc.top = other.top;
		break;
	case uiAtTop:
		gc.left = other.left;
		gc.top = other.top - gc.yspan;
		break;
	case uiAtTrailing:
		gc.left = other.left + other.xspan;
		gc.top = other.top;
		break;
	case uiAtBottom:
		gc.left = other.left;
		gc.top = other.top + other.yspan;
		break;
	// TODO add error checks to ALL enums
	}

	[self append:gc];
}

- (int)isPadded
{
	return self->padded;
}

- (void)setPadded:(int)p
{
	CGFloat padding;
	NSLayoutConstraint *c;

#if 0 /* TODO */
dispatch_after(
dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC),
dispatch_get_main_queue(),
^{ [[self window] visualizeConstraints:[self constraints]]; }
);
#endif
	self->padded = p;
	padding = [self paddingAmount];
	for (c in self->inBetweens)
		switch ([c firstAttribute]) {
		case NSLayoutAttributeLeading:
		case NSLayoutAttributeTop:
			[c setConstant:padding];
			break;
		case NSLayoutAttributeTrailing:
		case NSLayoutAttributeBottom:
			[c setConstant:-padding];
			break;
		}
}

- (BOOL)hugsTrailing
{
	// only hug if we have horizontally expanding
	return [self nhexpand] != 0;
}

- (BOOL)hugsBottom
{
	// only hug if we have vertically expanding
	return [self nvexpand] != 0;
}

- (int)nhexpand
{
	gridChild *gc;
	int n;

	n = 0;
	for (gc in self->children) {
		if (!uiControlVisible(gc.c))
			continue;
		if (gc.hexpand)
			n++;
	}
	return n;
}

- (int)nvexpand
{
	gridChild *gc;
	int n;

	n = 0;
	for (gc in self->children) {
		if (!uiControlVisible(gc.c))
			continue;
		if (gc.vexpand)
			n++;
	}
	return n;
}

@end

static void uiGridDestroy(uiControl *c)
{
	uiGrid *g = uiGrid(c);

	[g->view onDestroy];
	[g->view release];
	uiFreeControl(uiControl(g));
}

uiDarwinControlDefaultHandle(uiGrid, view)
uiDarwinControlDefaultParent(uiGrid, view)
uiDarwinControlDefaultSetParent(uiGrid, view)
uiDarwinControlDefaultToplevel(uiGrid, view)
uiDarwinControlDefaultVisible(uiGrid, view)
uiDarwinControlDefaultShow(uiGrid, view)
uiDarwinControlDefaultHide(uiGrid, view)
uiDarwinControlDefaultEnabled(uiGrid, view)
uiDarwinControlDefaultEnable(uiGrid, view)
uiDarwinControlDefaultDisable(uiGrid, view)

static void uiGridSyncEnableState(uiDarwinControl *c, int enabled)
{
	uiGrid *g = uiGrid(c);

	if (uiDarwinShouldStopSyncEnableState(uiDarwinControl(g), enabled))
		return;
	[g->view syncEnableStates:enabled];
}

uiDarwinControlDefaultSetSuperview(uiGrid, view)

static BOOL uiGridHugsTrailingEdge(uiDarwinControl *c)
{
	uiGrid *g = uiGrid(c);

	return [g->view hugsTrailing];
}

static BOOL uiGridHugsBottom(uiDarwinControl *c)
{
	uiGrid *g = uiGrid(c);

	return [g->view hugsBottom];
}

static void uiGridChildEdgeHuggingChanged(uiDarwinControl *c)
{
	uiGrid *g = uiGrid(c);

	[g->view establishOurConstraints];
}

uiDarwinControlDefaultHuggingPriority(uiGrid, view)
uiDarwinControlDefaultSetHuggingPriority(uiGrid, view)

static void uiGridChildVisibilityChanged(uiDarwinControl *c)
{
	uiGrid *g = uiGrid(c);

	[g->view establishOurConstraints];
}

static gridChild *toChild(uiControl *c, int xspan, int yspan, int hexpand, uiAlign halign, int vexpand, uiAlign valign, uiGrid *g)
{
	gridChild *gc;

	if (xspan < 0)
		userbug("You cannot have a negative xspan in a uiGrid cell.");
	if (yspan < 0)
		userbug("You cannot have a negative yspan in a uiGrid cell.");
	gc = [gridChild new];
	gc.xspan = xspan;
	gc.yspan = yspan;
	gc.hexpand = hexpand;
	gc.halign = halign;
	gc.vexpand = vexpand;
	gc.valign = valign;
	[gc setC:c grid:g];
	return gc;
}

void uiGridAppend(uiGrid *g, uiControl *c, int left, int top, int xspan, int yspan, int hexpand, uiAlign halign, int vexpand, uiAlign valign)
{
	gridChild *gc;

	// LONGTERM on other platforms
	// or at leat allow this and implicitly turn it into a spacer
	if (c == NULL)
		userbug("You cannot add NULL to a uiGrid.");
	gc = toChild(c, xspan, yspan, hexpand, halign, vexpand, valign, g);
	gc.left = left;
	gc.top = top;
	[g->view append:gc];
}

void uiGridInsertAt(uiGrid *g, uiControl *c, uiControl *existing, uiAt at, int xspan, int yspan, int hexpand, uiAlign halign, int vexpand, uiAlign valign)
{
	gridChild *gc;

	gc = toChild(c, xspan, yspan, hexpand, halign, vexpand, valign, g);
	[g->view insert:gc after:existing at:at];
}

int uiGridPadded(uiGrid *g)
{
	return [g->view isPadded];
}

void uiGridSetPadded(uiGrid *g, int padded)
{
	[g->view setPadded:padded];
}

uiGrid *uiNewGrid(void)
{
	uiGrid *g;

	uiDarwinNewControl(uiGrid, g);

	g->view = [[gridView alloc] initWithG:g];

	return g;
}
