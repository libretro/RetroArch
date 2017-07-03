// 6 april 2015
#import "uipriv_darwin.h"

static BOOL canQuit = NO;
static NSAutoreleasePool *globalPool;
static applicationClass *app;
static appDelegate *delegate;

static BOOL (^isRunning)(void);
static BOOL stepsIsRunning;

@implementation applicationClass

- (void)sendEvent:(NSEvent *)e
{
	if (sendAreaEvents(e) != 0)
		return;
	[super sendEvent:e];
}

// NSColorPanel always sends changeColor: to the first responder regardless of whether there's a target set on it
// we can override it here (see colorbutton.m)
// thanks to mikeash in irc.freenode.net/#macdev for informing me this is how the first responder chain is initiated
// it turns out NSFontManager also sends changeFont: through this; let's inhibit that here too (see fontbutton.m)
- (BOOL)sendAction:(SEL)sel to:(id)to from:(id)from
{
	if (colorButtonInhibitSendAction(sel, from, to))
		return NO;
	if (fontButtonInhibitSendAction(sel, from, to))
		return NO;
	return [super sendAction:sel to:to from:from];
}

// likewise, NSFontManager also sends NSFontPanelValidation messages to the first responder, however it does NOT use sendAction:from:to:!
// instead, it uses this one (thanks swillits in irc.freenode.net/#macdev)
// we also need to override it (see fontbutton.m)
- (id)targetForAction:(SEL)sel to:(id)to from:(id)from
{
	id override;

	if (fontButtonOverrideTargetForAction(sel, from, to, &override))
		return override;
	return [super targetForAction:sel to:to from:from];
}

// hey look! we're overriding terminate:!
// we're going to make sure we can go back to main() whether Cocoa likes it or not!
// and just how are we going to do that, hm?
// (note: this is called after applicationShouldTerminate:)
- (void)terminate:(id)sender
{
	// yes that's right folks: DO ABSOLUTELY NOTHING.
	// the magic is [NSApp run] will just... stop.

	// well let's not do nothing; let's actually quit our graceful way
	NSEvent *e;

	if (!canQuit)
		implbug("call to [NSApp terminate:] when not ready to terminate; definitely contact andlabs");

	[realNSApp() stop:realNSApp()];
	// stop: won't register until another event has passed; let's synthesize one
	e = [NSEvent otherEventWithType:NSApplicationDefined
		location:NSZeroPoint
		modifierFlags:0
		timestamp:[[NSProcessInfo processInfo] systemUptime]
		windowNumber:0
		context:[NSGraphicsContext currentContext]
		subtype:0
		data1:0
		data2:0];
	[realNSApp() postEvent:e atStart:NO];		// let pending events take priority (this is what PostQuitMessage() on Windows does so we have to do it here too for parity; thanks to mikeash in irc.freenode.net/#macdev for confirming that this parameter should indeed be NO)

	// and in case uiMainSteps() was called
	stepsIsRunning = NO;
}

@end

@implementation appDelegate

- (void)dealloc
{
	// Apple docs: "Don't Use Accessor Methods in Initializer Methods and dealloc"
	[_menuManager release];
	[super dealloc];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)app
{
	// for debugging
	NSLog(@"in applicationShouldTerminate:");
	if (shouldQuit()) {
		canQuit = YES;
		// this will call terminate:, which is the same as uiQuit()
		return NSTerminateNow;
	}
	return NSTerminateCancel;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
	return NO;
}

@end

uiInitOptions options;

const char *uiInit(uiInitOptions *o)
{
	@autoreleasepool {
		options = *o;
		app = [[applicationClass sharedApplication] retain];
		// don't check for a NO return; something (launch services?) causes running from application bundles to always return NO when asking to change activation policy, even if the change is to the same activation policy!
		// see https://github.com/andlabs/ui/issues/6
		[realNSApp() setActivationPolicy:NSApplicationActivationPolicyRegular];
		delegate = [appDelegate new];
		[realNSApp() setDelegate:delegate];

		initAlloc();

		// always do this so we always have an application menu
		appDelegate().menuManager = [[menuManager new] autorelease];
		[realNSApp() setMainMenu:[appDelegate().menuManager makeMenubar]];

		setupFontPanel();
	}

	globalPool = [[NSAutoreleasePool alloc] init];

	return NULL;
}

void uiUninit(void)
{
	if (!globalPool) {
		userbug("You must call uiInit() first!");
	}
	[globalPool release];

	@autoreleasepool {
		[delegate release];
		[realNSApp() setDelegate:nil];
		[app release];
		uninitAlloc();
	}
}

void uiFreeInitError(const char *err)
{
}

void uiMain(void)
{
	isRunning = ^{
		return [realNSApp() isRunning];
	};
	[realNSApp() run];
}

void uiMainSteps(void)
{
	// SDL does this and it seems to be necessary for the menubar to work (see #182)
	[realNSApp() finishLaunching];
	isRunning = ^{
		return stepsIsRunning;
	};
	stepsIsRunning = YES;
}

int uiMainStep(int wait)
{
	struct nextEventArgs nea;

	nea.mask = NSAnyEventMask;

	// ProPuke did this in his original PR requesting this
	// I'm not sure if this will work, but I assume it will...
	nea.duration = [NSDate distantPast];
	if (wait)		// but this is normal so it will work
		nea.duration = [NSDate distantFuture];

	nea.mode = NSDefaultRunLoopMode;
	nea.dequeue = YES;

	return mainStep(&nea, ^(NSEvent *e) {
		return NO;
	});
}

// see also:
// - http://www.cocoawithlove.com/2009/01/demystifying-nsapplication-by.html
// - https://github.com/gnustep/gui/blob/master/Source/NSApplication.m
int mainStep(struct nextEventArgs *nea, BOOL (^interceptEvent)(NSEvent *e))
{
	NSDate *expire;
	NSEvent *e;
	NSEventType type;

	@autoreleasepool {
		if (!isRunning())
			return 0;

		e = [realNSApp() nextEventMatchingMask:nea->mask
			untilDate:nea->duration
			inMode:nea->mode
			dequeue:nea->dequeue];
		if (e == nil)
			return 1;

		type = [e type];
		if (!interceptEvent(e))
			[realNSApp() sendEvent:e];
		[realNSApp() updateWindows];

		// GNUstep does this
		// it also updates the Services menu but there doesn't seem to be a public API for that so
		if (type != NSPeriodic && type != NSMouseMoved)
			[[realNSApp() mainMenu] update];

		return 1;
	}
}

void uiQuit(void)
{
	canQuit = YES;
	[realNSApp() terminate:realNSApp()];
}

// thanks to mikeash in irc.freenode.net/#macdev for suggesting the use of Grand Central Dispatch for this
// LONGTERM will dispatch_get_main_queue() break after _CFRunLoopSetCurrent()?
void uiQueueMain(void (*f)(void *data), void *data)
{
	// dispatch_get_main_queue() is a serial queue so it will not execute multiple uiQueueMain() functions concurrently
	// the signature of f matches dispatch_function_t
	dispatch_async_f(dispatch_get_main_queue(), data, f);
}
