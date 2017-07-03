// 14 august 2015
#import "uipriv_darwin.h"

// NSComboBoxes have no intrinsic width; we'll use the default Interface Builder width for them.
// NSPopUpButton is fine.
#define comboboxWidth 96

struct uiCombobox {
	uiDarwinControl c;
	NSPopUpButton *pb;
	NSArrayController *pbac;
	void (*onSelected)(uiCombobox *, void *);
	void *onSelectedData;
};

@interface comboboxDelegateClass : NSObject {
	struct mapTable *comboboxes;
}
- (IBAction)onSelected:(id)sender;
- (void)registerCombobox:(uiCombobox *)c;
- (void)unregisterCombobox:(uiCombobox *)c;
@end

@implementation comboboxDelegateClass

- (id)init
{
	self = [super init];
	if (self)
		self->comboboxes = newMap();
	return self;
}

- (void)dealloc
{
	mapDestroy(self->comboboxes);
	[super dealloc];
}

- (IBAction)onSelected:(id)sender
{
	uiCombobox *c;

	c = uiCombobox(mapGet(self->comboboxes, sender));
	(*(c->onSelected))(c, c->onSelectedData);
}

- (void)registerCombobox:(uiCombobox *)c
{
	mapSet(self->comboboxes, c->pb, c);
	[c->pb setTarget:self];
	[c->pb setAction:@selector(onSelected:)];
}

- (void)unregisterCombobox:(uiCombobox *)c
{
	[c->pb setTarget:nil];
	mapDelete(self->comboboxes, c->pb);
}

@end

static comboboxDelegateClass *comboboxDelegate = nil;

uiDarwinControlAllDefaultsExceptDestroy(uiCombobox, pb)

static void uiComboboxDestroy(uiControl *cc)
{
	uiCombobox *c = uiCombobox(cc);

	[comboboxDelegate unregisterCombobox:c];
	[c->pb unbind:@"contentObjects"];
	[c->pb unbind:@"selectedIndex"];
	[c->pbac release];
	[c->pb release];
	uiFreeControl(uiControl(c));
}

void uiComboboxAppend(uiCombobox *c, const char *text)
{
	[c->pbac addObject:toNSString(text)];
}

int uiComboboxSelected(uiCombobox *c)
{
	return [c->pb indexOfSelectedItem];
}

void uiComboboxSetSelected(uiCombobox *c, int n)
{
	[c->pb selectItemAtIndex:n];
}

void uiComboboxOnSelected(uiCombobox *c, void (*f)(uiCombobox *c, void *data), void *data)
{
	c->onSelected = f;
	c->onSelectedData = data;
}

static void defaultOnSelected(uiCombobox *c, void *data)
{
	// do nothing
}

uiCombobox *uiNewCombobox(void)
{
	uiCombobox *c;
	NSPopUpButtonCell *pbcell;

	uiDarwinNewControl(uiCombobox, c);

	c->pb = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
	[c->pb setPreferredEdge:NSMinYEdge];
	pbcell = (NSPopUpButtonCell *) [c->pb cell];
	[pbcell setArrowPosition:NSPopUpArrowAtBottom];
	// the font defined by Interface Builder is Menu 13, which is lol
	// just use the regular control size for consistency
	uiDarwinSetControlFont(c->pb, NSRegularControlSize);

	// NSPopUpButton doesn't work like a combobox
	// - it automatically selects the first item
	// - it doesn't support duplicates
	// but we can use a NSArrayController and Cocoa bindings to bypass these restrictions
	c->pbac = [NSArrayController new];
	[c->pbac setAvoidsEmptySelection:NO];
	[c->pbac setSelectsInsertedObjects:NO];
	[c->pbac setAutomaticallyRearrangesObjects:NO];
	[c->pb bind:@"contentValues"
		toObject:c->pbac
		withKeyPath:@"arrangedObjects"
		options:nil];
	[c->pb bind:@"selectedIndex"
		toObject:c->pbac
		withKeyPath:@"selectionIndex"
		options:nil];

	if (comboboxDelegate == nil) {
		comboboxDelegate = [[comboboxDelegateClass new] autorelease];
		[delegates addObject:comboboxDelegate];
	}
	[comboboxDelegate registerCombobox:c];
	uiComboboxOnSelected(c, defaultOnSelected, NULL);

	return c;
}
