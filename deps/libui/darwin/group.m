// 14 august 2015
#import "uipriv_darwin.h"

struct uiGroup {
	uiDarwinControl c;
	NSBox *box;
	uiControl *child;
	NSLayoutPriority oldHorzHuggingPri;
	NSLayoutPriority oldVertHuggingPri;
	int margined;
	struct singleChildConstraints constraints;
	NSLayoutPriority horzHuggingPri;
	NSLayoutPriority vertHuggingPri;
};

static void removeConstraints(uiGroup *g)
{
	// set to contentView instead of to the box itself, otherwise we get clipping underneath the label
	singleChildConstraintsRemove(&(g->constraints), [g->box contentView]);
}

static void uiGroupDestroy(uiControl *c)
{
	uiGroup *g = uiGroup(c);

	removeConstraints(g);
	if (g->child != NULL) {
		uiControlSetParent(g->child, NULL);
		uiDarwinControlSetSuperview(uiDarwinControl(g->child), nil);
		uiControlDestroy(g->child);
	}
	[g->box release];
	uiFreeControl(uiControl(g));
}

uiDarwinControlDefaultHandle(uiGroup, box)
uiDarwinControlDefaultParent(uiGroup, box)
uiDarwinControlDefaultSetParent(uiGroup, box)
uiDarwinControlDefaultToplevel(uiGroup, box)
uiDarwinControlDefaultVisible(uiGroup, box)
uiDarwinControlDefaultShow(uiGroup, box)
uiDarwinControlDefaultHide(uiGroup, box)
uiDarwinControlDefaultEnabled(uiGroup, box)
uiDarwinControlDefaultEnable(uiGroup, box)
uiDarwinControlDefaultDisable(uiGroup, box)

static void uiGroupSyncEnableState(uiDarwinControl *c, int enabled)
{
	uiGroup *g = uiGroup(c);

	if (uiDarwinShouldStopSyncEnableState(uiDarwinControl(g), enabled))
		return;
	if (g->child != NULL)
		uiDarwinControlSyncEnableState(uiDarwinControl(g->child), enabled);
}

uiDarwinControlDefaultSetSuperview(uiGroup, box)

static void groupRelayout(uiGroup *g)
{
	NSView *childView;

	removeConstraints(g);
	if (g->child == NULL)
		return;
	childView = (NSView *) uiControlHandle(g->child);
	singleChildConstraintsEstablish(&(g->constraints),
		[g->box contentView], childView,
		uiDarwinControlHugsTrailingEdge(uiDarwinControl(g->child)),
		uiDarwinControlHugsBottom(uiDarwinControl(g->child)),
		g->margined,
		@"uiGroup");
	// needed for some very rare drawing errors...
	jiggleViewLayout(g->box);
}

// TODO rename these since I'm starting to get confused by what they mean by hugging
BOOL uiGroupHugsTrailingEdge(uiDarwinControl *c)
{
	uiGroup *g = uiGroup(c);

	// TODO make a function?
	return g->horzHuggingPri < NSLayoutPriorityWindowSizeStayPut;
}

BOOL uiGroupHugsBottom(uiDarwinControl *c)
{
	uiGroup *g = uiGroup(c);

	return g->vertHuggingPri < NSLayoutPriorityWindowSizeStayPut;
}

static void uiGroupChildEdgeHuggingChanged(uiDarwinControl *c)
{
	uiGroup *g = uiGroup(c);

	groupRelayout(g);
}

static NSLayoutPriority uiGroupHuggingPriority(uiDarwinControl *c, NSLayoutConstraintOrientation orientation)
{
	uiGroup *g = uiGroup(c);

	if (orientation == NSLayoutConstraintOrientationHorizontal)
		return g->horzHuggingPri;
	return g->vertHuggingPri;
}

static void uiGroupSetHuggingPriority(uiDarwinControl *c, NSLayoutPriority priority, NSLayoutConstraintOrientation orientation)
{
	uiGroup *g = uiGroup(c);

	if (orientation == NSLayoutConstraintOrientationHorizontal)
		g->horzHuggingPri = priority;
	else
		g->vertHuggingPri = priority;
	uiDarwinNotifyEdgeHuggingChanged(uiDarwinControl(g));
}

static void uiGroupChildVisibilityChanged(uiDarwinControl *c)
{
	uiGroup *g = uiGroup(c);

	groupRelayout(g);
}

char *uiGroupTitle(uiGroup *g)
{
	return uiDarwinNSStringToText([g->box title]);
}

void uiGroupSetTitle(uiGroup *g, const char *title)
{
	[g->box setTitle:toNSString(title)];
}

void uiGroupSetChild(uiGroup *g, uiControl *child)
{
	NSView *childView;

	if (g->child != NULL) {
		removeConstraints(g);
		uiDarwinControlSetHuggingPriority(uiDarwinControl(g->child), g->oldHorzHuggingPri, NSLayoutConstraintOrientationHorizontal);
		uiDarwinControlSetHuggingPriority(uiDarwinControl(g->child), g->oldVertHuggingPri, NSLayoutConstraintOrientationVertical);
		uiControlSetParent(g->child, NULL);
		uiDarwinControlSetSuperview(uiDarwinControl(g->child), nil);
	}
	g->child = child;
	if (g->child != NULL) {
		childView = (NSView *) uiControlHandle(g->child);
		uiControlSetParent(g->child, uiControl(g));
		uiDarwinControlSetSuperview(uiDarwinControl(g->child), [g->box contentView]);
		uiDarwinControlSyncEnableState(uiDarwinControl(g->child), uiControlEnabledToUser(uiControl(g)));
		// don't hug, just in case we're a stretchy group
		g->oldHorzHuggingPri = uiDarwinControlHuggingPriority(uiDarwinControl(g->child), NSLayoutConstraintOrientationHorizontal);
		g->oldVertHuggingPri = uiDarwinControlHuggingPriority(uiDarwinControl(g->child), NSLayoutConstraintOrientationVertical);
		uiDarwinControlSetHuggingPriority(uiDarwinControl(g->child), NSLayoutPriorityDefaultLow, NSLayoutConstraintOrientationHorizontal);
		uiDarwinControlSetHuggingPriority(uiDarwinControl(g->child), NSLayoutPriorityDefaultLow, NSLayoutConstraintOrientationVertical);
	}
	groupRelayout(g);
}

int uiGroupMargined(uiGroup *g)
{
	return g->margined;
}

void uiGroupSetMargined(uiGroup *g, int margined)
{
	g->margined = margined;
	singleChildConstraintsSetMargined(&(g->constraints), g->margined);
}

uiGroup *uiNewGroup(const char *title)
{
	uiGroup *g;

	uiDarwinNewControl(uiGroup, g);

	g->box = [[NSBox alloc] initWithFrame:NSZeroRect];
	[g->box setTitle:toNSString(title)];
	[g->box setBoxType:NSBoxPrimary];
	[g->box setBorderType:NSLineBorder];
	[g->box setTransparent:NO];
	[g->box setTitlePosition:NSAtTop];
	// we can't use uiDarwinSetControlFont() because the selector is different
	[g->box setTitleFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]]];

	// default to low hugging to not hug edges
	g->horzHuggingPri = NSLayoutPriorityDefaultLow;
	g->vertHuggingPri = NSLayoutPriorityDefaultLow;

	return g;
}
