// 20 may 2015
#include "uipriv_windows.hpp"

// references:
// - http://stackoverflow.com/questions/2892703/how-do-i-draw-separators
// - https://msdn.microsoft.com/en-us/library/windows/desktop/dn742405%28v=vs.85%29.aspx

struct uiSeparator {
	uiWindowsControl c;
	HWND hwnd;
	BOOL vertical;
};

uiWindowsControlAllDefaults(uiSeparator)

// via https://msdn.microsoft.com/en-us/library/windows/desktop/bb226818%28v=vs.85%29.aspx
#define separatorHeight 1

// TODO
#define separatorWidth 1

static void uiSeparatorMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiSeparator *s = uiSeparator(c);
	uiWindowsSizing sizing;
	int x, y;

	*width = 1;		// TODO
	*height = 1;
	x = separatorWidth;
	y = separatorHeight;
	uiWindowsGetSizing(s->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, &x, &y);
	if (s->vertical)
		*width = x;
	else
		*height = y;
}

uiSeparator *uiNewHorizontalSeparator(void)
{
	uiSeparator *s;

	uiWindowsNewControl(uiSeparator, s);

	s->hwnd = uiWindowsEnsureCreateControlHWND(0,
		L"static", L"",
		SS_ETCHEDHORZ,
		hInstance, NULL,
		TRUE);

	return s;
}

uiSeparator *uiNewVerticalSeparator(void)
{
	uiSeparator *s;

	uiWindowsNewControl(uiSeparator, s);

	s->hwnd = uiWindowsEnsureCreateControlHWND(0,
		L"static", L"",
		SS_ETCHEDHORZ,
		hInstance, NULL,
		TRUE);
	s->vertical = TRUE;

	return s;
}
