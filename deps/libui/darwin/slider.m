// 14 august 2015
#import "uipriv_darwin.h"

// Horizontal sliders have no intrinsic width; we'll use the default Interface Builder width for them.
// This will also be used for the initial frame size, to ensure the slider is always horizontal (see below).
#define sliderWidth 92

@interface libui_intrinsicWidthNSSlider : NSSlider
@end

@implementation libui_intrinsicWidthNSSlider

- (NSSize)intrinsicContentSize
{
	NSSize s;

	s = [super intrinsicContentSize];
	s.width = sliderWidth;
	return s;
}

@end

struct uiSlider {
	uiDarwinControl c;
	NSSlider *slider;
	void (*onChanged)(uiSlider *, void *);
	void *onChangedData;
};

@interface sliderDelegateClass : NSObject {
	struct mapTable *sliders;
}
- (IBAction)onChanged:(id)sender;
- (void)registerSlider:(uiSlider *)b;
- (void)unregisterSlider:(uiSlider *)b;
@end

@implementation sliderDelegateClass

- (id)init
{
	self = [super init];
	if (self)
		self->sliders = newMap();
	return self;
}

- (void)dealloc
{
	mapDestroy(self->sliders);
	[super dealloc];
}

- (IBAction)onChanged:(id)sender
{
	uiSlider *s;

	s = (uiSlider *) mapGet(self->sliders, sender);
	(*(s->onChanged))(s, s->onChangedData);
}

- (void)registerSlider:(uiSlider *)s
{
	mapSet(self->sliders, s->slider, s);
	[s->slider setTarget:self];
	[s->slider setAction:@selector(onChanged:)];
}

- (void)unregisterSlider:(uiSlider *)s
{
	[s->slider setTarget:nil];
	mapDelete(self->sliders, s->slider);
}

@end

static sliderDelegateClass *sliderDelegate = nil;

uiDarwinControlAllDefaultsExceptDestroy(uiSlider, slider)

static void uiSliderDestroy(uiControl *c)
{
	uiSlider *s = uiSlider(c);

	[sliderDelegate unregisterSlider:s];
	[s->slider release];
	uiFreeControl(uiControl(s));
}

int uiSliderValue(uiSlider *s)
{
	return [s->slider integerValue];
}

void uiSliderSetValue(uiSlider *s, int value)
{
	[s->slider setIntegerValue:value];
}

void uiSliderOnChanged(uiSlider *s, void (*f)(uiSlider *, void *), void *data)
{
	s->onChanged = f;
	s->onChangedData = data;
}

static void defaultOnChanged(uiSlider *s, void *data)
{
	// do nothing
}

uiSlider *uiNewSlider(int min, int max)
{
	uiSlider *s;
	NSSliderCell *cell;
	int temp;

	if (min >= max) {
		temp = min;
		min = max;
		max = temp;
	}

	uiDarwinNewControl(uiSlider, s);

	// a horizontal slider is defined as one where the width > height, not by a flag
	// to be safe, don't use NSZeroRect, but make it horizontal from the get-go
	s->slider = [[libui_intrinsicWidthNSSlider alloc]
		initWithFrame:NSMakeRect(0, 0, sliderWidth, 2)];
	[s->slider setMinValue:min];
	[s->slider setMaxValue:max];
	[s->slider setAllowsTickMarkValuesOnly:NO];
	[s->slider setNumberOfTickMarks:0];
	[s->slider setTickMarkPosition:NSTickMarkAbove];

	cell = (NSSliderCell *) [s->slider cell];
	[cell setSliderType:NSLinearSlider];

	if (sliderDelegate == nil) {
		sliderDelegate = [[sliderDelegateClass new] autorelease];
		[delegates addObject:sliderDelegate];
	}
	[sliderDelegate registerSlider:s];
	uiSliderOnChanged(s, defaultOnChanged, NULL);

	return s;
}
