// 19 may 2015
#include "uipriv_windows.hpp"

struct uiProgressBar {
	uiWindowsControl c;
	HWND hwnd;
};

uiWindowsControlAllDefaults(uiProgressBar)

// via http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define pbarWidth 237
#define pbarHeight 8

static void uiProgressBarMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiProgressBar *p = uiProgressBar(c);
	uiWindowsSizing sizing;
	int x, y;

	x = pbarWidth;
	y = pbarHeight;
	uiWindowsGetSizing(p->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, &x, &y);
	*width = x;
	*height = y;
}

#define indeterminate(p) ((getStyle(p->hwnd) & PBS_MARQUEE) != 0)

int uiProgressBarValue(uiProgressBar *p)
{
	if (indeterminate(p))
		return -1;
	return SendMessage(p->hwnd, PBM_GETPOS, 0, 0);
}

// unfortunately, as of Vista progress bars have a forced animation on increase
// we have to set the progress bar to value + 1 and decrease it back to value if we want an "instant" change
// see http://stackoverflow.com/questions/2217688/windows-7-aero-theme-progress-bar-bug
// it's not ideal/perfect, but it will have to do
void uiProgressBarSetValue(uiProgressBar *p, int value)
{
	if (value == -1) {
		if (!indeterminate(p)) {
			setStyle(p->hwnd, getStyle(p->hwnd) | PBS_MARQUEE);
			SendMessageW(p->hwnd, PBM_SETMARQUEE, (WPARAM) TRUE, 0);
		}
		return;
	}
	if (indeterminate(p)) {
		SendMessageW(p->hwnd, PBM_SETMARQUEE, (WPARAM) FALSE, 0);
		setStyle(p->hwnd, getStyle(p->hwnd) & ~PBS_MARQUEE);
	}

	if (value < 0 || value > 100)
		userbug("Value %d is out of range for uiProgressBars.", value);

	if (value == 100) {			// because we can't 101
		SendMessageW(p->hwnd, PBM_SETRANGE32, 0, 101);
		SendMessageW(p->hwnd, PBM_SETPOS, 101, 0);
		SendMessageW(p->hwnd, PBM_SETPOS, 100, 0);
		SendMessageW(p->hwnd, PBM_SETRANGE32, 0, 100);
		return;
	}
	SendMessageW(p->hwnd, PBM_SETPOS, (WPARAM) (value + 1), 0);
	SendMessageW(p->hwnd, PBM_SETPOS, (WPARAM) value, 0);
}

uiProgressBar *uiNewProgressBar(void)
{
	uiProgressBar *p;

	uiWindowsNewControl(uiProgressBar, p);

	p->hwnd = uiWindowsEnsureCreateControlHWND(0,
		PROGRESS_CLASSW, L"",
		PBS_SMOOTH,
		hInstance, NULL,
		FALSE);

	return p;
}
