// 15 august 2015
#import "uipriv_darwin.h"

// TODO need to jiggle on tab change too (second page disabled tab label initially ambiguous)

@interface tabPage : NSObject {
	struct singleChildConstraints constraints;
	int margined;
	NSView *view;		// the NSTabViewItem view itself
	NSObject *pageID;
}
@property uiControl *c;
@property NSLayoutPriority oldHorzHuggingPri;
@property NSLayoutPriority oldVertHuggingPri;
- (id)initWithView:(NSView *)v pageID:(NSObject *)o;
- (NSView *)childView;
- (void)establishChildConstraints;
- (void)removeChildConstraints;
- (int)isMargined;
- (void)setMargined:(int)m;
@end

struct uiTab {
	uiDarwinControl c;
	NSTabView *tabview;
	NSMutableArray *pages;
	NSLayoutPriority horzHuggingPri;
	NSLayoutPriority vertHuggingPri;
};

@implementation tabPage

- (id)initWithView:(NSView *)v pageID:(NSObject *)o
{
	self = [super init];
	if (self != nil) {
		self->view = [v retain];
		self->pageID = [o retain];
	}
	return self;
}

- (void)dealloc
{
	[self removeChildConstraints];
	[self->view release];
	[self->pageID release];
	[super dealloc];
}

- (NSView *)childView
{
	return (NSView *) uiControlHandle(self.c);
}

- (void)establishChildConstraints
{
	[self removeChildConstraints];
	if (self.c == NULL)
		return;
	singleChildConstraintsEstablish(&(self->constraints),
		self->view, [self childView],
		uiDarwinControlHugsTrailingEdge(uiDarwinControl(self.c)),
		uiDarwinControlHugsBottom(uiDarwinControl(self.c)),
		self->margined,
		@"uiTab page");
}

- (void)removeChildConstraints
{
	singleChildConstraintsRemove(&(self->constraints), self->view);
}

- (int)isMargined
{
	return self->margined;
}

- (void)setMargined:(int)m
{
	self->margined = m;
	singleChildConstraintsSetMargined(&(self->constraints), self->margined);
}

@end

static void uiTabDestroy(uiControl *c)
{
	uiTab *t = uiTab(c);
	tabPage *page;

	// first remove all tab pages so we can destroy all the children
	while ([t->tabview numberOfTabViewItems] != 0)
		[t->tabview removeTabViewItem:[t->tabview tabViewItemAtIndex:0]];
	// then destroy all the children
	for (page in t->pages) {
		[page removeChildConstraints];
		uiControlSetParent(page.c, NULL);
		uiDarwinControlSetSuperview(uiDarwinControl(page.c), nil);
		uiControlDestroy(page.c);
	}
	// and finally destroy ourselves
	[t->pages release];
	[t->tabview release];
	uiFreeControl(uiControl(t));
}

uiDarwinControlDefaultHandle(uiTab, tabview)
uiDarwinControlDefaultParent(uiTab, tabview)
uiDarwinControlDefaultSetParent(uiTab, tabview)
uiDarwinControlDefaultToplevel(uiTab, tabview)
uiDarwinControlDefaultVisible(uiTab, tabview)
uiDarwinControlDefaultShow(uiTab, tabview)
uiDarwinControlDefaultHide(uiTab, tabview)
uiDarwinControlDefaultEnabled(uiTab, tabview)
uiDarwinControlDefaultEnable(uiTab, tabview)
uiDarwinControlDefaultDisable(uiTab, tabview)

static void uiTabSyncEnableState(uiDarwinControl *c, int enabled)
{
	uiTab *t = uiTab(c);
	tabPage *page;

	if (uiDarwinShouldStopSyncEnableState(uiDarwinControl(t), enabled))
		return;
	for (page in t->pages)
		uiDarwinControlSyncEnableState(uiDarwinControl(page.c), enabled);
}

uiDarwinControlDefaultSetSuperview(uiTab, tabview)

static void tabRelayout(uiTab *t)
{
	tabPage *page;

	for (page in t->pages)
		[page establishChildConstraints];
	// and this gets rid of some weird issues with regards to box alignment
	jiggleViewLayout(t->tabview);
}

BOOL uiTabHugsTrailingEdge(uiDarwinControl *c)
{
	uiTab *t = uiTab(c);

	return t->horzHuggingPri < NSLayoutPriorityWindowSizeStayPut;
}

BOOL uiTabHugsBottom(uiDarwinControl *c)
{
	uiTab *t = uiTab(c);

	return t->vertHuggingPri < NSLayoutPriorityWindowSizeStayPut;
}

static void uiTabChildEdgeHuggingChanged(uiDarwinControl *c)
{
	uiTab *t = uiTab(c);

	tabRelayout(t);
}

static NSLayoutPriority uiTabHuggingPriority(uiDarwinControl *c, NSLayoutConstraintOrientation orientation)
{
	uiTab *t = uiTab(c);

	if (orientation == NSLayoutConstraintOrientationHorizontal)
		return t->horzHuggingPri;
	return t->vertHuggingPri;
}

static void uiTabSetHuggingPriority(uiDarwinControl *c, NSLayoutPriority priority, NSLayoutConstraintOrientation orientation)
{
	uiTab *t = uiTab(c);

	if (orientation == NSLayoutConstraintOrientationHorizontal)
		t->horzHuggingPri = priority;
	else
		t->vertHuggingPri = priority;
	uiDarwinNotifyEdgeHuggingChanged(uiDarwinControl(t));
}

static void uiTabChildVisibilityChanged(uiDarwinControl *c)
{
	uiTab *t = uiTab(c);

	tabRelayout(t);
}

void uiTabAppend(uiTab *t, const char *name, uiControl *child)
{
	uiTabInsertAt(t, name, [t->pages count], child);
}

void uiTabInsertAt(uiTab *t, const char *name, int n, uiControl *child)
{
	tabPage *page;
	NSView *view;
	NSTabViewItem *i;
	NSObject *pageID;

	uiControlSetParent(child, uiControl(t));

	view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
	// note: if we turn off the autoresizing mask, nothing shows up
	uiDarwinControlSetSuperview(uiDarwinControl(child), view);
	uiDarwinControlSyncEnableState(uiDarwinControl(child), uiControlEnabledToUser(uiControl(t)));

	// the documentation says these can be nil but the headers say these must not be; let's be safe and make them non-nil anyway
	pageID = [NSObject new];
	page = [[[tabPage alloc] initWithView:view pageID:pageID] autorelease];
	page.c = child;

	// don't hug, just in case we're a stretchy tab
	page.oldHorzHuggingPri = uiDarwinControlHuggingPriority(uiDarwinControl(page.c), NSLayoutConstraintOrientationHorizontal);
	page.oldVertHuggingPri = uiDarwinControlHuggingPriority(uiDarwinControl(page.c), NSLayoutConstraintOrientationVertical);
	uiDarwinControlSetHuggingPriority(uiDarwinControl(page.c), NSLayoutPriorityDefaultLow, NSLayoutConstraintOrientationHorizontal);
	uiDarwinControlSetHuggingPriority(uiDarwinControl(page.c), NSLayoutPriorityDefaultLow, NSLayoutConstraintOrientationVertical);

	[t->pages insertObject:page atIndex:n];

	i = [[[NSTabViewItem alloc] initWithIdentifier:pageID] autorelease];
	[i setLabel:toNSString(name)];
	[i setView:view];
	[t->tabview insertTabViewItem:i atIndex:n];

	tabRelayout(t);
}

void uiTabDelete(uiTab *t, int n)
{
	tabPage *page;
	uiControl *child;
	NSTabViewItem *i;

	page = (tabPage *) [t->pages objectAtIndex:n];

	uiDarwinControlSetHuggingPriority(uiDarwinControl(page.c), page.oldHorzHuggingPri, NSLayoutConstraintOrientationHorizontal);
	uiDarwinControlSetHuggingPriority(uiDarwinControl(page.c), page.oldVertHuggingPri, NSLayoutConstraintOrientationVertical);

	child = page.c;
	[page removeChildConstraints];
	[t->pages removeObjectAtIndex:n];

	uiControlSetParent(child, NULL);
	uiDarwinControlSetSuperview(uiDarwinControl(child), nil);

	i = [t->tabview tabViewItemAtIndex:n];
	[t->tabview removeTabViewItem:i];

	tabRelayout(t);
}

int uiTabNumPages(uiTab *t)
{
	return [t->pages count];
}

int uiTabMargined(uiTab *t, int n)
{
	tabPage *page;

	page = (tabPage *) [t->pages objectAtIndex:n];
	return [page isMargined];
}

void uiTabSetMargined(uiTab *t, int n, int margined)
{
	tabPage *page;

	page = (tabPage *) [t->pages objectAtIndex:n];
	[page setMargined:margined];
}

uiTab *uiNewTab(void)
{
	uiTab *t;

	uiDarwinNewControl(uiTab, t);

	t->tabview = [[NSTabView alloc] initWithFrame:NSZeroRect];
	// also good for NSTabView (same selector and everything)
	uiDarwinSetControlFont((NSControl *) (t->tabview), NSRegularControlSize);

	t->pages = [NSMutableArray new];

	// default to low hugging to not hug edges
	t->horzHuggingPri = NSLayoutPriorityDefaultLow;
	t->vertHuggingPri = NSLayoutPriorityDefaultLow;

	return t;
}
