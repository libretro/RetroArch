// 26 june 2015
#import "uipriv_darwin.h"

// LONGTERM restructure this whole file
// LONGTERM explicitly document this works as we want
// LONGTERM note that font and color buttons also do this

#define windowWindow(w) ((NSWindow *) uiControlHandle(uiControl(w)))

// source of code modal logic: http://stackoverflow.com/questions/604768/wait-for-nsalert-beginsheetmodalforwindow

// note: whether extensions are actually shown depends on a user setting in Finder; we can't control it here
static void setupSavePanel(NSSavePanel *s)
{
	[s setCanCreateDirectories:YES];
	[s setShowsHiddenFiles:YES];
	[s setExtensionHidden:NO];
	[s setCanSelectHiddenExtension:NO];
	[s setTreatsFilePackagesAsDirectories:YES];
}

static char *runSavePanel(NSWindow *parent, NSSavePanel *s)
{
	char *filename;

	[s beginSheetModalForWindow:parent completionHandler:^(NSInteger result) {
		[realNSApp() stopModalWithCode:result];
	}];
	if ([realNSApp() runModalForWindow:s] != NSFileHandlingPanelOKButton)
		return NULL;
	filename = uiDarwinNSStringToText([[s URL] path]);
	return filename;
}

char *uiOpenFile(uiWindow *parent)
{
	NSOpenPanel *o;

	o = [NSOpenPanel openPanel];
	[o setCanChooseFiles:YES];
	[o setCanChooseDirectories:NO];
	[o setResolvesAliases:NO];
	[o setAllowsMultipleSelection:NO];
	setupSavePanel(o);
	// panel is autoreleased
	return runSavePanel(windowWindow(parent), o);
}

char *uiSaveFile(uiWindow *parent)
{
	NSSavePanel *s;

	s = [NSSavePanel savePanel];
	setupSavePanel(s);
	// panel is autoreleased
	return runSavePanel(windowWindow(parent), s);
}

// I would use a completion handler for NSAlert as well, but alas NSAlert's are 10.9 and higher only
@interface libuiCodeModalAlertPanel : NSObject {
	NSAlert *panel;
	NSWindow *parent;
}
- (id)initWithPanel:(NSAlert *)p parent:(NSWindow *)w;
- (NSInteger)run;
- (void)panelEnded:(NSAlert *)panel result:(NSInteger)result data:(void *)data;
@end

@implementation libuiCodeModalAlertPanel

- (id)initWithPanel:(NSAlert *)p parent:(NSWindow *)w
{
	self = [super init];
	if (self) {
		self->panel = p;
		self->parent = w;
	}
	return self;
}

- (NSInteger)run
{
	[self->panel beginSheetModalForWindow:self->parent
		modalDelegate:self
		didEndSelector:@selector(panelEnded:result:data:)
		contextInfo:NULL];
	return [realNSApp() runModalForWindow:[self->panel window]];
}

- (void)panelEnded:(NSAlert *)panel result:(NSInteger)result data:(void *)data
{
	[realNSApp() stopModalWithCode:result];
}

@end

static void msgbox(NSWindow *parent, const char *title, const char *description, NSAlertStyle style)
{
	NSAlert *a;
	libuiCodeModalAlertPanel *cm;

	a = [NSAlert new];
	[a setAlertStyle:style];
	[a setShowsHelp:NO];
	[a setShowsSuppressionButton:NO];
	[a setMessageText:toNSString(title)];
	[a setInformativeText:toNSString(description)];
	[a addButtonWithTitle:@"OK"];
	cm = [[libuiCodeModalAlertPanel alloc] initWithPanel:a parent:parent];
	[cm run];
	[cm release];
	[a release];
}

void uiMsgBox(uiWindow *parent, const char *title, const char *description)
{
	msgbox(windowWindow(parent), title, description, NSInformationalAlertStyle);
}

void uiMsgBoxError(uiWindow *parent, const char *title, const char *description)
{
	msgbox(windowWindow(parent), title, description, NSCriticalAlertStyle);
}
