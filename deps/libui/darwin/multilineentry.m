// 8 december 2015
#import "uipriv_darwin.h"

// NSTextView has no intrinsic content size by default, which wreaks havoc on a pure-Auto Layout system
// we'll have to take over to get it to work
// see also http://stackoverflow.com/questions/24210153/nstextview-not-properly-resizing-with-auto-layout and http://stackoverflow.com/questions/11237622/using-autolayout-with-expanding-nstextviews
@interface intrinsicSizeTextView : NSTextView {
	uiMultilineEntry *libui_e;
}
- (id)initWithFrame:(NSRect)r e:(uiMultilineEntry *)e;
@end

struct uiMultilineEntry {
	uiDarwinControl c;
	NSScrollView *sv;
	intrinsicSizeTextView *tv;
	struct scrollViewData *d;
	void (*onChanged)(uiMultilineEntry *, void *);
	void *onChangedData;
	BOOL changing;
};

@implementation intrinsicSizeTextView

- (id)initWithFrame:(NSRect)r e:(uiMultilineEntry *)e
{
	self = [super initWithFrame:r];
	if (self)
		self->libui_e = e;
	return self;
}

- (NSSize)intrinsicContentSize
{
	NSTextContainer *textContainer;
	NSLayoutManager *layoutManager;
	NSRect rect;

	textContainer = [self textContainer];
	layoutManager = [self layoutManager];
	[layoutManager ensureLayoutForTextContainer:textContainer];
	rect = [layoutManager usedRectForTextContainer:textContainer];
	return rect.size;
}

- (void)didChangeText
{
	[super didChangeText];
	[self invalidateIntrinsicContentSize];
	if (!self->libui_e->changing)
		(*(self->libui_e->onChanged))(self->libui_e, self->libui_e->onChangedData);
}

@end

uiDarwinControlAllDefaultsExceptDestroy(uiMultilineEntry, sv)

static void uiMultilineEntryDestroy(uiControl *c)
{
	uiMultilineEntry *e = uiMultilineEntry(c);

	scrollViewFreeData(e->sv, e->d);
	[e->tv release];
	[e->sv release];
	uiFreeControl(uiControl(e));
}

static void defaultOnChanged(uiMultilineEntry *e, void *data)
{
	// do nothing
}

char *uiMultilineEntryText(uiMultilineEntry *e)
{
	return uiDarwinNSStringToText([e->tv string]);
}

void uiMultilineEntrySetText(uiMultilineEntry *e, const char *text)
{
	[[e->tv textStorage] replaceCharactersInRange:NSMakeRange(0, [[e->tv string] length])
		withString:toNSString(text)];
	// must be called explicitly according to the documentation of shouldChangeTextInRange:replacementString:
	e->changing = YES;
	[e->tv didChangeText];
	e->changing = NO;
}

// TODO scroll to end?
void uiMultilineEntryAppend(uiMultilineEntry *e, const char *text)
{
	[[e->tv textStorage] replaceCharactersInRange:NSMakeRange([[e->tv string] length], 0)
		withString:toNSString(text)];
	e->changing = YES;
	[e->tv didChangeText];
	e->changing = NO;
}

void uiMultilineEntryOnChanged(uiMultilineEntry *e, void (*f)(uiMultilineEntry *e, void *data), void *data)
{
	e->onChanged = f;
	e->onChangedData = data;
}

int uiMultilineEntryReadOnly(uiMultilineEntry *e)
{
	return [e->tv isEditable] == NO;
}

void uiMultilineEntrySetReadOnly(uiMultilineEntry *e, int readonly)
{
	BOOL editable;

	editable = YES;
	if (readonly)
		editable = NO;
	[e->tv setEditable:editable];
}

static uiMultilineEntry *finishMultilineEntry(BOOL hscroll)
{
	uiMultilineEntry *e;
	NSFont *font;
	struct scrollViewCreateParams p;

	uiDarwinNewControl(uiMultilineEntry, e);

	e->tv = [[intrinsicSizeTextView alloc] initWithFrame:NSZeroRect e:e];

	// verified against Interface Builder for a sufficiently customized text view

	// NSText properties:
	// this is what Interface Builder sets the background color to
	[e->tv setBackgroundColor:[NSColor colorWithCalibratedWhite:1.0 alpha:1.0]];
	[e->tv setDrawsBackground:YES];
	[e->tv setEditable:YES];
	[e->tv setSelectable:YES];
	[e->tv setFieldEditor:NO];
	[e->tv setRichText:NO];
	[e->tv setImportsGraphics:NO];
	[e->tv setUsesFontPanel:NO];
	[e->tv setRulerVisible:NO];
	// we'll handle font last
	// while setAlignment: has been around since 10.0, the named constant "NSTextAlignmentNatural" seems to have only been introduced in 10.11
#define ourNSTextAlignmentNatural 4
	[e->tv setAlignment:ourNSTextAlignmentNatural];
	// textColor is set to nil, just keep the dfault
	[e->tv setBaseWritingDirection:NSWritingDirectionNatural];
	[e->tv setHorizontallyResizable:NO];
	[e->tv setVerticallyResizable:YES];

	// NSTextView properties:
	[e->tv setAllowsDocumentBackgroundColorChange:NO];
	[e->tv setAllowsUndo:YES];
	// default paragraph style is nil; keep default
	[e->tv setAllowsImageEditing:NO];
	[e->tv setAutomaticQuoteSubstitutionEnabled:NO];
	[e->tv setAutomaticLinkDetectionEnabled:NO];
	[e->tv setDisplaysLinkToolTips:YES];
	[e->tv setUsesRuler:NO];
	[e->tv setUsesInspectorBar:NO];
	[e->tv setSelectionGranularity:NSSelectByCharacter];
	// there is a dedicated named insertion point color but oh well
	[e->tv setInsertionPointColor:[NSColor controlTextColor]];
	// typing attributes is nil; keep default (we change it below for fonts though)
	[e->tv setSmartInsertDeleteEnabled:NO];
	[e->tv setContinuousSpellCheckingEnabled:NO];
	[e->tv setGrammarCheckingEnabled:NO];
	[e->tv setUsesFindPanel:YES];
	[e->tv setEnabledTextCheckingTypes:0];
	[e->tv setAutomaticDashSubstitutionEnabled:NO];
	[e->tv setAutomaticDataDetectionEnabled:NO];
	[e->tv setAutomaticSpellingCorrectionEnabled:NO];
	[e->tv setAutomaticTextReplacementEnabled:NO];
	[e->tv setUsesFindBar:NO];
	[e->tv setIncrementalSearchingEnabled:NO];

	// NSTextContainer properties:
	[[e->tv textContainer] setWidthTracksTextView:YES];
	[[e->tv textContainer] setHeightTracksTextView:NO];

	// NSLayoutManager properties:
	[[e->tv layoutManager] setAllowsNonContiguousLayout:YES];

	// now just to be safe; this will do some of the above but whatever
	disableAutocorrect(e->tv);

	// see https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/TextUILayer/Tasks/TextInScrollView.html
	// notice we don't use the Auto Layout code; see scrollview.m for more details
	[e->tv setMaxSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];
	[e->tv setVerticallyResizable:YES];
	[e->tv setHorizontallyResizable:hscroll];
	if (hscroll) {
		[e->tv setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
		[[e->tv textContainer] setWidthTracksTextView:NO];
	} else {
		[e->tv setAutoresizingMask:NSViewWidthSizable];
		[[e->tv textContainer] setWidthTracksTextView:YES];
	}
	[[e->tv textContainer] setContainerSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];

	// don't use uiDarwinSetControlFont() directly; we have to do a little extra work to set the font
	font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSRegularControlSize]];
	[e->tv setTypingAttributes:[NSDictionary
		dictionaryWithObject:font
		forKey:NSFontAttributeName]];
	// e->tv font from Interface Builder is nil, but setFont:nil throws an exception
	// let's just set it to the standard control font anyway, just to be safe
	[e->tv setFont:font];

	memset(&p, 0, sizeof (struct scrollViewCreateParams));
	p.DocumentView = e->tv;
	// this is what Interface Builder sets it to
	p.BackgroundColor = [NSColor colorWithCalibratedWhite:1.0 alpha:1.0];
	p.DrawsBackground = YES;
	p.Bordered = YES;
	p.HScroll = hscroll;
	p.VScroll = YES;
	e->sv = mkScrollView(&p, &(e->d));

	uiMultilineEntryOnChanged(e, defaultOnChanged, NULL);

	return e;
}

uiMultilineEntry *uiNewMultilineEntry(void)
{
	return finishMultilineEntry(NO);
}

uiMultilineEntry *uiNewNonWrappingMultilineEntry(void)
{
	return finishMultilineEntry(YES);
}
