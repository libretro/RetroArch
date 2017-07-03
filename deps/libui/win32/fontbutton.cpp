// 14 april 2016
#include "uipriv_windows.hpp"

struct uiFontButton {
	uiWindowsControl c;
	HWND hwnd;
	struct fontDialogParams params;
	BOOL already;
	void (*onChanged)(uiFontButton *, void *);
	void *onChangedData;
};

static void uiFontButtonDestroy(uiControl *c)
{
	uiFontButton *b = uiFontButton(c);

	uiWindowsUnregisterWM_COMMANDHandler(b->hwnd);
	destroyFontDialogParams(&(b->params));
	uiWindowsEnsureDestroyWindow(b->hwnd);
	uiFreeControl(uiControl(b));
}

static void updateFontButtonLabel(uiFontButton *b)
{
	WCHAR *text;

	text = fontDialogParamsToString(&(b->params));
	setWindowText(b->hwnd, text);
	uiFree(text);

	// changing the text might necessitate a change in the button's size
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(b));
}

static BOOL onWM_COMMAND(uiControl *c, HWND hwnd, WORD code, LRESULT *lResult)
{
	uiFontButton *b = uiFontButton(c);
	HWND parent;

	if (code != BN_CLICKED)
		return FALSE;

	parent = parentToplevel(b->hwnd);
	if (showFontDialog(parent, &(b->params))) {
		updateFontButtonLabel(b);
		(*(b->onChanged))(b, b->onChangedData);
	}

	*lResult = 0;
	return TRUE;
}

uiWindowsControlAllDefaultsExceptDestroy(uiFontButton)

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define buttonHeight 14

static void uiFontButtonMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiFontButton *b = uiFontButton(c);
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

static void defaultOnChanged(uiFontButton *b, void *data)
{
	// do nothing
}

uiDrawTextFont *uiFontButtonFont(uiFontButton *b)
{
	// we don't own b->params.font; we have to add a reference
	// we don't own b->params.familyName either; we have to copy it
	return mkTextFont(b->params.font, TRUE, b->params.familyName, TRUE, b->params.size);
}

void uiFontButtonOnChanged(uiFontButton *b, void (*f)(uiFontButton *, void *), void *data)
{
	b->onChanged = f;
	b->onChangedData = data;
}

uiFontButton *uiNewFontButton(void)
{
	uiFontButton *b;

	uiWindowsNewControl(uiFontButton, b);

	b->hwnd = uiWindowsEnsureCreateControlHWND(0,
		L"button", L"you should not be seeing this",
		BS_PUSHBUTTON | WS_TABSTOP,
		hInstance, NULL,
		TRUE);

	loadInitialFontDialogParams(&(b->params));

	uiWindowsRegisterWM_COMMANDHandler(b->hwnd, onWM_COMMAND, uiControl(b));
	uiFontButtonOnChanged(b, defaultOnChanged, NULL);

	updateFontButtonLabel(b);

	return b;
}
