// 1 november 2016
#import "uipriv_darwin.h"

// because we are changing the window frame each time the mouse moves, the successive -[NSEvent locationInWindow]s cannot be meaningfully used together
// make sure they are all following some sort of standard to avoid this problem; the screen is the most obvious possibility since it requires only one conversion (the only one that a NSWindow provides)
static NSPoint makeIndependent(NSPoint p, NSWindow *w)
{
	NSRect r;

	r.origin = p;
	// mikeash in irc.freenode.net/#macdev confirms both that any size will do and that we can safely ignore the resultant size
	r.size = NSZeroSize;
	return [w convertRectToScreen:r].origin;
}

struct onMoveDragParams {
	NSWindow *w;
	// using the previous point causes weird issues like the mouse seeming to fall behind the window edge... so do this instead
	// TODO will this make things like the menubar and dock easier too?
	NSRect initialFrame;
	NSPoint initialPoint;
};

void onMoveDrag(struct onMoveDragParams *p, NSEvent *e)
{
	NSPoint new;
	NSRect frame;
	CGFloat offx, offy;

	new = makeIndependent([e locationInWindow], p->w);
	frame = p->initialFrame;

	offx = new.x - p->initialPoint.x;
	offy = new.y - p->initialPoint.y;
	frame.origin.x += offx;
	frame.origin.y += offy;

	// TODO handle the menubar
	// TODO wait the system does this for us already?!

	[p->w setFrameOrigin:frame.origin];
}

void doManualMove(NSWindow *w, NSEvent *initialEvent)
{
	__block struct onMoveDragParams mdp;
	struct nextEventArgs nea;
	BOOL (^handleEvent)(NSEvent *e);
	__block BOOL done;

	// this is only available on 10.11 and newer (LONGTERM FUTURE)
	// but use it if available; this lets us use the real OS dragging code, which means we can take advantage of OS features like Spaces
	if ([w respondsToSelector:@selector(performWindowDragWithEvent:)]) {
		[((id) w) performWindowDragWithEvent:initialEvent];
		return;
	}

	mdp.w = w;
	mdp.initialFrame = [mdp.w frame];
	mdp.initialPoint = makeIndependent([initialEvent locationInWindow], mdp.w);

	nea.mask = NSLeftMouseDraggedMask | NSLeftMouseUpMask;
	nea.duration = [NSDate distantFuture];
	nea.mode = NSEventTrackingRunLoopMode;		// nextEventMatchingMask: docs suggest using this for manual mouse tracking
	nea.dequeue = YES;
	handleEvent = ^(NSEvent *e) {
		if ([e type] == NSLeftMouseUp) {
			done = YES;
			return YES;	// do not send
		}
		onMoveDrag(&mdp, e);
		return YES;		// do not send
	};
	done = NO;
	while (mainStep(&nea, handleEvent))
		if (done)
			break;
}

// see http://stackoverflow.com/a/40352996/3408572
static void minMaxAutoLayoutSizes(NSWindow *w, NSSize *min, NSSize *max)
{
	NSLayoutConstraint *cw, *ch;
	NSView *contentView;
	NSRect prevFrame;

	// if adding these constraints causes the window to change size somehow, don't show it to the user and change it back afterwards
	NSDisableScreenUpdates();
	prevFrame = [w frame];

	// minimum: encourage the window to be as small as possible
	contentView = [w contentView];
	cw = mkConstraint(contentView, NSLayoutAttributeWidth,
		NSLayoutRelationEqual,
		nil, NSLayoutAttributeNotAnAttribute,
		0, 0,
		@"window minimum width finding constraint");
	[cw setPriority:NSLayoutPriorityDragThatCanResizeWindow];
	[contentView addConstraint:cw];
	ch = mkConstraint(contentView, NSLayoutAttributeHeight,
		NSLayoutRelationEqual,
		nil, NSLayoutAttributeNotAnAttribute,
		0, 0,
		@"window minimum height finding constraint");
	[ch setPriority:NSLayoutPriorityDragThatCanResizeWindow];
	[contentView addConstraint:ch];
	*min = [contentView fittingSize];
	[contentView removeConstraint:cw];
	[contentView removeConstraint:ch];

	// maximum: encourage the window to be as large as possible
	contentView = [w contentView];
	cw = mkConstraint(contentView, NSLayoutAttributeWidth,
		NSLayoutRelationEqual,
		nil, NSLayoutAttributeNotAnAttribute,
		0, CGFLOAT_MAX,
		@"window maximum width finding constraint");
	[cw setPriority:NSLayoutPriorityDragThatCanResizeWindow];
	[contentView addConstraint:cw];
	ch = mkConstraint(contentView, NSLayoutAttributeHeight,
		NSLayoutRelationEqual,
		nil, NSLayoutAttributeNotAnAttribute,
		0, CGFLOAT_MAX,
		@"window maximum height finding constraint");
	[ch setPriority:NSLayoutPriorityDragThatCanResizeWindow];
	[contentView addConstraint:ch];
	*max = [contentView fittingSize];
	[contentView removeConstraint:cw];
	[contentView removeConstraint:ch];

	[w setFrame:prevFrame display:YES];		// TODO really YES?
	NSEnableScreenUpdates();
}

static void handleResizeLeft(NSRect *frame, NSPoint old, NSPoint new)
{
	frame->origin.x += new.x - old.x;
	frame->size.width -= new.x - old.x;
}

// TODO properly handle the menubar
// TODO wait, OS X does it for us?!
static void handleResizeTop(NSRect *frame, NSPoint old, NSPoint new)
{
	frame->size.height += new.y - old.y;
}

static void handleResizeRight(NSRect *frame, NSPoint old, NSPoint new)
{
	frame->size.width += new.x - old.x;
}


// TODO properly handle the menubar
static void handleResizeBottom(NSRect *frame, NSPoint old, NSPoint new)
{
	frame->origin.y += new.y - old.y;
	frame->size.height -= new.y - old.y;
}

struct onResizeDragParams {
	NSWindow *w;
	// using the previous point causes weird issues like the mouse seeming to fall behind the window edge... so do this instead
	// TODO will this make things like the menubar and dock easier too?
	NSRect initialFrame;
	NSPoint initialPoint;
	uiWindowResizeEdge edge;
	NSSize min;
	NSSize max;
};

static void onResizeDrag(struct onResizeDragParams *p, NSEvent *e)
{
	NSPoint new;
	NSRect frame;

	new = makeIndependent([e locationInWindow], p->w);
	frame = p->initialFrame;

	// horizontal
	switch (p->edge) {
	case uiWindowResizeEdgeLeft:
	case uiWindowResizeEdgeTopLeft:
	case uiWindowResizeEdgeBottomLeft:
		handleResizeLeft(&frame, p->initialPoint, new);
		break;
	case uiWindowResizeEdgeRight:
	case uiWindowResizeEdgeTopRight:
	case uiWindowResizeEdgeBottomRight:
		handleResizeRight(&frame, p->initialPoint, new);
		break;
	}
	// vertical
	switch (p->edge) {
	case uiWindowResizeEdgeTop:
	case uiWindowResizeEdgeTopLeft:
	case uiWindowResizeEdgeTopRight:
		handleResizeTop(&frame, p->initialPoint, new);
		break;
	case uiWindowResizeEdgeBottom:
	case uiWindowResizeEdgeBottomLeft:
	case uiWindowResizeEdgeBottomRight:
		handleResizeBottom(&frame, p->initialPoint, new);
		break;
	}

	// constrain
	// TODO should we constrain against anything else as well? minMaxAutoLayoutSizes() already gives us nonnegative sizes, but...
	if (frame.size.width < p->min.width)
		frame.size.width = p->min.width;
	if (frame.size.height < p->min.height)
		frame.size.height = p->min.height;
	// TODO > or >= ?
	if (frame.size.width > p->max.width)
		frame.size.width = p->max.width;
	if (frame.size.height > p->max.height)
		frame.size.height = p->max.height;

	[p->w setFrame:frame display:YES];			// and do reflect the new frame immediately
}

// TODO do our events get fired with this? *should* they?
void doManualResize(NSWindow *w, NSEvent *initialEvent, uiWindowResizeEdge edge)
{
	__block struct onResizeDragParams rdp;
	struct nextEventArgs nea;
	BOOL (^handleEvent)(NSEvent *e);
	__block BOOL done;

	rdp.w = w;
	rdp.initialFrame = [rdp.w frame];
	rdp.initialPoint = makeIndependent([initialEvent locationInWindow], rdp.w);
	rdp.edge = edge;
	// TODO what happens if these change during the loop?
	minMaxAutoLayoutSizes(rdp.w, &(rdp.min), &(rdp.max));

	nea.mask = NSLeftMouseDraggedMask | NSLeftMouseUpMask;
	nea.duration = [NSDate distantFuture];
	nea.mode = NSEventTrackingRunLoopMode;		// nextEventMatchingMask: docs suggest using this for manual mouse tracking
	nea.dequeue = YES;
	handleEvent = ^(NSEvent *e) {
		if ([e type] == NSLeftMouseUp) {
			done = YES;
			return YES;	// do not send
		}
		onResizeDrag(&rdp, e);
		return YES;		// do not send
	};
	done = NO;
	while (mainStep(&nea, handleEvent))
		if (done)
			break;
}
