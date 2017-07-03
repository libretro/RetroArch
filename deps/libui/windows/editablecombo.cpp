// 20 may 2015
#include "uipriv_windows.hpp"

// we as Common Controls 6 users don't need to worry about the height of comboboxes; see http://blogs.msdn.com/b/oldnewthing/archive/2006/03/10/548537.aspx

struct uiEditableCombobox {
	uiWindowsControl c;
	HWND hwnd;
	void (*onChanged)(uiEditableCombobox *, void *);
	void *onChangedData;
};

static BOOL onWM_COMMAND(uiControl *cc, HWND hwnd, WORD code, LRESULT *lResult)
{
	uiEditableCombobox *c = uiEditableCombobox(cc);

	if (code == CBN_SELCHANGE) {
		// like on OS X, this is sent before the edit has been updated :(
		if (PostMessage(parentOf(hwnd),
			WM_COMMAND,
			MAKEWPARAM(GetWindowLongPtrW(hwnd, GWLP_ID), CBN_EDITCHANGE),
			(LPARAM) hwnd) == 0)
			logLastError(L"error posting CBN_EDITCHANGE after CBN_SELCHANGE");
		*lResult = 0;
		return TRUE;
	}
	if (code != CBN_EDITCHANGE)
		return FALSE;
	(*(c->onChanged))(c, c->onChangedData);
	*lResult = 0;
	return TRUE;
}

void uiEditableComboboxDestroy(uiControl *cc)
{
	uiEditableCombobox *c = uiEditableCombobox(cc);

	uiWindowsUnregisterWM_COMMANDHandler(c->hwnd);
	uiWindowsEnsureDestroyWindow(c->hwnd);
	uiFreeControl(uiControl(c));
}

uiWindowsControlAllDefaultsExceptDestroy(uiEditableCombobox)

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define comboboxWidth 107	/* this is actually the shorter progress bar width, but Microsoft only indicates as wide as necessary; LONGTERM */
#define comboboxHeight 14	/* LONGTERM: is this too high? */

static void uiEditableComboboxMinimumSize(uiWindowsControl *cc, int *width, int *height)
{
	uiEditableCombobox *c = uiEditableCombobox(cc);
	uiWindowsSizing sizing;
	int x, y;

	x = comboboxWidth;
	y = comboboxHeight;
	uiWindowsGetSizing(c->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, &x, &y);
	*width = x;
	*height = y;
}

static void defaultOnChanged(uiEditableCombobox *c, void *data)
{
	// do nothing
}

void uiEditableComboboxAppend(uiEditableCombobox *c, const char *text)
{
	WCHAR *wtext;
	LRESULT res;

	wtext = toUTF16(text);
	res = SendMessageW(c->hwnd, CB_ADDSTRING, 0, (LPARAM) wtext);
	if (res == (LRESULT) CB_ERR)
		logLastError(L"error appending item to uiEditableCombobox");
	else if (res == (LRESULT) CB_ERRSPACE)
		logLastError(L"memory exhausted appending item to uiEditableCombobox");
	uiFree(wtext);
}

char *uiEditableComboboxText(uiEditableCombobox *c)
{
	return uiWindowsWindowText(c->hwnd);
}

void uiEditableComboboxSetText(uiEditableCombobox *c, const char *text)
{
	// does not trigger any notifications
	uiWindowsSetWindowText(c->hwnd, text);
}

void uiEditableComboboxOnChanged(uiEditableCombobox *c, void (*f)(uiEditableCombobox *c, void *data), void *data)
{
	c->onChanged = f;
	c->onChangedData = data;
}

uiEditableCombobox *uiNewEditableCombobox(void)
{
	uiEditableCombobox *c;

	uiWindowsNewControl(uiEditableCombobox, c);

	c->hwnd = uiWindowsEnsureCreateControlHWND(WS_EX_CLIENTEDGE,
		L"combobox", L"",
		CBS_DROPDOWN | WS_TABSTOP,
		hInstance, NULL,
		TRUE);

	uiWindowsRegisterWM_COMMANDHandler(c->hwnd, onWM_COMMAND, uiControl(c));
	uiEditableComboboxOnChanged(c, defaultOnChanged, NULL);

	return c;
}
