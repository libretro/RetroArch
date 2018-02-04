// 16 august 2015
#import "uipriv_darwin.h"

void uiDarwinControlSyncEnableState(uiDarwinControl *c, int state)
{
	(*(c->SyncEnableState))(c, state);
}

void uiDarwinControlSetSuperview(uiDarwinControl *c, NSView *superview)
{
	(*(c->SetSuperview))(c, superview);
}

BOOL uiDarwinControlHugsTrailingEdge(uiDarwinControl *c)
{
	return (*(c->HugsTrailingEdge))(c);
}

BOOL uiDarwinControlHugsBottom(uiDarwinControl *c)
{
	return (*(c->HugsBottom))(c);
}

void uiDarwinControlChildEdgeHuggingChanged(uiDarwinControl *c)
{
	(*(c->ChildEdgeHuggingChanged))(c);
}

NSLayoutPriority uiDarwinControlHuggingPriority(uiDarwinControl *c, NSLayoutConstraintOrientation orientation)
{
	return (*(c->HuggingPriority))(c, orientation);
}

void uiDarwinControlSetHuggingPriority(uiDarwinControl *c, NSLayoutPriority priority, NSLayoutConstraintOrientation orientation)
{
	(*(c->SetHuggingPriority))(c, priority, orientation);
}

void uiDarwinControlChildVisibilityChanged(uiDarwinControl *c)
{
	(*(c->ChildVisibilityChanged))(c);
}

void uiDarwinSetControlFont(NSControl *c, NSControlSize size)
{
	[c setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
}

#define uiDarwinControlSignature 0x44617277

uiDarwinControl *uiDarwinAllocControl(size_t n, uint32_t typesig, const char *typenamestr)
{
	return uiDarwinControl(uiAllocControl(n, uiDarwinControlSignature, typesig, typenamestr));
}

BOOL uiDarwinShouldStopSyncEnableState(uiDarwinControl *c, BOOL enabled)
{
	int ce;

	ce = uiControlEnabled(uiControl(c));
	// only stop if we're going from disabled back to enabled; don't stop under any other condition
	// (if we stop when going from enabled to disabled then enabled children of a disabled control won't get disabled at the OS level)
	if (!ce && enabled)
		return YES;
	return NO;
}

void uiDarwinNotifyEdgeHuggingChanged(uiDarwinControl *c)
{
	uiControl *parent;

	parent = uiControlParent(uiControl(c));
	if (parent != NULL)
		uiDarwinControlChildEdgeHuggingChanged(uiDarwinControl(parent));
}

void uiDarwinNotifyVisibilityChanged(uiDarwinControl *c)
{
	uiControl *parent;

	parent = uiControlParent(uiControl(c));
	if (parent != NULL)
		uiDarwinControlChildVisibilityChanged(uiDarwinControl(parent));
}
