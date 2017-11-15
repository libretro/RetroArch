// 14 august 2015
#import "uipriv_darwin.h"

struct uiDateTimePicker {
	uiDarwinControl c;
	NSDatePicker *dp;
};

uiDarwinControlAllDefaults(uiDateTimePicker, dp)

static uiDateTimePicker *finishNewDateTimePicker(NSDatePickerElementFlags elements)
{
	uiDateTimePicker *d;

	uiDarwinNewControl(uiDateTimePicker, d);

	d->dp = [[NSDatePicker alloc] initWithFrame:NSZeroRect];
	[d->dp setBordered:NO];
	[d->dp setBezeled:YES];
	[d->dp setDrawsBackground:YES];
	[d->dp setDatePickerStyle:NSTextFieldAndStepperDatePickerStyle];
	[d->dp setDatePickerElements:elements];
	[d->dp setDatePickerMode:NSSingleDateMode];
	uiDarwinSetControlFont(d->dp, NSRegularControlSize);

	return d;
}

uiDateTimePicker *uiNewDateTimePicker(void)
{
	return finishNewDateTimePicker(NSYearMonthDayDatePickerElementFlag | NSHourMinuteSecondDatePickerElementFlag);
}

uiDateTimePicker *uiNewDatePicker(void)
{
	return finishNewDateTimePicker(NSYearMonthDayDatePickerElementFlag);
}

uiDateTimePicker *uiNewTimePicker(void)
{
	return finishNewDateTimePicker(NSHourMinuteSecondDatePickerElementFlag);
}
