// 7 april 2015
#include "uipriv_windows.hpp"

struct uiButton {
	uiWindowsControl c;
	HWND hwnd;
	void (*onClicked)(uiButton *, void *);
	void *onClickedData;
};

static BOOL onWM_COMMAND(uiControl *c, HWND hwnd, WORD code, LRESULT *lResult)
{
	uiButton *b = uiButton(c);

	if (code != BN_CLICKED)
		return FALSE;
	(*(b->onClicked))(b, b->onClickedData);
	*lResult = 0;
	return TRUE;
}

static void uiButtonDestroy(uiControl *c)
{
	uiButton *b = uiButton(c);

	uiWindowsUnregisterWM_COMMANDHandler(b->hwnd);
	uiWindowsEnsureDestroyWindow(b->hwnd);
	uiFreeControl(uiControl(b));
}

uiWindowsControlAllDefaultsExceptDestroy(uiButton)

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define buttonHeight 14

static void uiButtonMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiButton *b = uiButton(c);
	SIZE size;
	uiWindowsSizing sizing;
	int y;

	// try the comctl32 version 6 way
	size.cx = 0;		// explicitly ask for ideal size
	size.cy = 0;
	if (SendMessageW(b->hwnd, BCM_GETIDEALSIZE, 0, (LPARAM) (&size)) != FALSE) {
		*width = size.cx;
		*height = size.cy;
		return;
	}

	// that didn't work; fall back to using Microsoft's metrics
	// Microsoft says to use a fixed width for all buttons; this isn't good enough
	// use the text width instead, with some edge padding
	*width = uiWindowsWindowTextWidth(b->hwnd) + (2 * GetSystemMetrics(SM_CXEDGE));
	y = buttonHeight;
	uiWindowsGetSizing(b->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, NULL, &y);
	*height = y;
}

static void defaultOnClicked(uiButton *b, void *data)
{
	// do nothing
}

char *uiButtonText(uiButton *b)
{
	return uiWindowsWindowText(b->hwnd);
}

void uiButtonSetText(uiButton *b, const char *text)
{
	uiWindowsSetWindowText(b->hwnd, text);
	// changing the text might necessitate a change in the button's size
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(b));
}

void uiButtonOnClicked(uiButton *b, void (*f)(uiButton *, void *), void *data)
{
	b->onClicked = f;
	b->onClickedData = data;
}

uiButton *uiNewButton(const char *text)
{
	uiButton *b;
	WCHAR *wtext;

	uiWindowsNewControl(uiButton, b);

	wtext = toUTF16(text);
	b->hwnd = uiWindowsEnsureCreateControlHWND(0,
		L"button", wtext,
		BS_PUSHBUTTON | WS_TABSTOP,
		hInstance, NULL,
		TRUE);
	uiFree(wtext);

	uiWindowsRegisterWM_COMMANDHandler(b->hwnd, onWM_COMMAND, uiControl(b));
	uiButtonOnClicked(b, defaultOnClicked, NULL);

	return b;
}
