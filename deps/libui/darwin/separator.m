// 14 august 2015
#import "uipriv_darwin.h"

// TODO make this intrinsic
#define separatorWidth 96
#define separatorHeight 96

struct uiSeparator {
	uiDarwinControl c;
	NSBox *box;
};

uiDarwinControlAllDefaults(uiSeparator, box)

uiSeparator *uiNewHorizontalSeparator(void)
{
	uiSeparator *s;

	uiDarwinNewControl(uiSeparator, s);

	// make the initial width >= initial height to force horizontal
	s->box = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, 100, 1)];
	[s->box setBoxType:NSBoxSeparator];
	[s->box setBorderType:NSGrooveBorder];
	[s->box setTransparent:NO];
	[s->box setTitlePosition:NSNoTitle];

	return s;
}

uiSeparator *uiNewVerticalSeparator(void)
{
	uiSeparator *s;

	uiDarwinNewControl(uiSeparator, s);

	// make the initial height >= initial width to force vertical
	s->box = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, 1, 100)];
	[s->box setBoxType:NSBoxSeparator];
	[s->box setBorderType:NSGrooveBorder];
	[s->box setTransparent:NO];
	[s->box setTitlePosition:NSNoTitle];

	return s;
}
