// 20 may 2015
#include "uipriv_windows.hpp"

struct uiSlider {
	uiWindowsControl c;
	HWND hwnd;
	void (*onChanged)(uiSlider *, void *);
	void *onChangedData;
};

static BOOL onWM_HSCROLL(uiControl *c, HWND hwnd, WORD code, LRESULT *lResult)
{
	uiSlider *s = uiSlider(c);

	(*(s->onChanged))(s, s->onChangedData);
	*lResult = 0;
	return TRUE;
}

static void uiSliderDestroy(uiControl *c)
{
	uiSlider *s = uiSlider(c);

	uiWindowsUnregisterWM_HSCROLLHandler(s->hwnd);
	uiWindowsEnsureDestroyWindow(s->hwnd);
	uiFreeControl(uiControl(s));
}

uiWindowsControlAllDefaultsExceptDestroy(uiSlider);

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define sliderWidth 107 /* this is actually the shorter progress bar width, but Microsoft doesn't indicate a width */
#define sliderHeight 15

static void uiSliderMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiSlider *s = uiSlider(c);
	uiWindowsSizing sizing;
	int x, y;

	x = sliderWidth;
	y = sliderHeight;
	uiWindowsGetSizing(s->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, &x, &y);
	*width = x;
	*height = y;
}

static void defaultOnChanged(uiSlider *s, void *data)
{
	// do nothing
}

int uiSliderValue(uiSlider *s)
{
	return SendMessageW(s->hwnd, TBM_GETPOS, 0, 0);
}

void uiSliderSetValue(uiSlider *s, int value)
{
	// don't use TBM_SETPOSNOTIFY; that triggers an event
	SendMessageW(s->hwnd, TBM_SETPOS, (WPARAM) TRUE, (LPARAM) value);
}

void uiSliderOnChanged(uiSlider *s, void (*f)(uiSlider *, void *), void *data)
{
	s->onChanged = f;
	s->onChangedData = data;
}

uiSlider *uiNewSlider(int min, int max)
{
	uiSlider *s;
	int temp;

	if (min >= max) {
		temp = min;
		min = max;
		max = temp;
	}

	uiWindowsNewControl(uiSlider, s);

	s->hwnd = uiWindowsEnsureCreateControlHWND(0,
		TRACKBAR_CLASSW, L"",
		TBS_HORZ | TBS_TOOLTIPS | TBS_TRANSPARENTBKGND | WS_TABSTOP,
		hInstance, NULL,
		TRUE);

	uiWindowsRegisterWM_HSCROLLHandler(s->hwnd, onWM_HSCROLL, uiControl(s));
	uiSliderOnChanged(s, defaultOnChanged, NULL);

	SendMessageW(s->hwnd, TBM_SETRANGEMIN, (WPARAM) TRUE, (LPARAM) min);
	SendMessageW(s->hwnd, TBM_SETRANGEMAX, (WPARAM) TRUE, (LPARAM) max);
	SendMessageW(s->hwnd, TBM_SETPOS, (WPARAM) TRUE, (LPARAM) min);

	return s;
}
