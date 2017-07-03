// 14 august 2015
#import "uipriv_darwin.h"

// NSProgressIndicator has no intrinsic width by default; use the default width in Interface Builder
#define progressIndicatorWidth 100

@interface intrinsicWidthNSProgressIndicator : NSProgressIndicator
@end

@implementation intrinsicWidthNSProgressIndicator

- (NSSize)intrinsicContentSize
{
	NSSize s;

	s = [super intrinsicContentSize];
	s.width = progressIndicatorWidth;
	return s;
}

@end

struct uiProgressBar {
	uiDarwinControl c;
	NSProgressIndicator *pi;
};

uiDarwinControlAllDefaults(uiProgressBar, pi)

int uiProgressBarValue(uiProgressBar *p)
{
	if ([p->pi isIndeterminate])
		return -1;
	return [p->pi doubleValue];
}

void uiProgressBarSetValue(uiProgressBar *p, int value)
{
	if (value == -1) {
		[p->pi setIndeterminate:YES];
		[p->pi startAnimation:p->pi];
		return;
	}

	if ([p->pi isIndeterminate]) {
		[p->pi setIndeterminate:NO];
		[p->pi stopAnimation:p->pi];
	}

	if (value < 0 || value > 100)
		userbug("Value %d out of range for a uiProgressBar.", value);

	// on 10.8 there's an animation when the progress bar increases, just like with Aero
	if (value == 100) {
		[p->pi setMaxValue:101];
		[p->pi setDoubleValue:101];
		[p->pi setDoubleValue:100];
		[p->pi setMaxValue:100];
		return;
	}
	[p->pi setDoubleValue:((double) (value + 1))];
	[p->pi setDoubleValue:((double) value)];
}

uiProgressBar *uiNewProgressBar(void)
{
	uiProgressBar *p;

	uiDarwinNewControl(uiProgressBar, p);

	p->pi = [[intrinsicWidthNSProgressIndicator alloc] initWithFrame:NSZeroRect];
	[p->pi setControlSize:NSRegularControlSize];
	[p->pi setBezeled:YES];
	[p->pi setStyle:NSProgressIndicatorBarStyle];
	[p->pi setIndeterminate:NO];

	return p;
}
