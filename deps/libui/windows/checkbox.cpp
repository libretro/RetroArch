// 7 april 2015
#include "uipriv_windows.hpp"

struct uiCheckbox {
	uiWindowsControl c;
	HWND hwnd;
	void (*onToggled)(uiCheckbox *, void *);
	void *onToggledData;
};

static BOOL onWM_COMMAND(uiControl *cc, HWND hwnd, WORD code, LRESULT *lResult)
{
	uiCheckbox *c = uiCheckbox(cc);
	WPARAM check;

	if (code != BN_CLICKED)
		return FALSE;

	// we didn't use BS_AUTOCHECKBOX (http://blogs.msdn.com/b/oldnewthing/archive/2014/05/22/10527522.aspx) so we have to manage the check state ourselves
	check = BST_CHECKED;
	if (SendMessage(c->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED)
		check = BST_UNCHECKED;
	SendMessage(c->hwnd, BM_SETCHECK, check, 0);

	(*(c->onToggled))(c, c->onToggledData);
	*lResult = 0;
	return TRUE;
}

static void uiCheckboxDestroy(uiControl *cc)
{
	uiCheckbox *c = uiCheckbox(cc);

	uiWindowsUnregisterWM_COMMANDHandler(c->hwnd);
	uiWindowsEnsureDestroyWindow(c->hwnd);
	uiFreeControl(uiControl(c));
}

uiWindowsControlAllDefaultsExceptDestroy(uiCheckbox)

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define checkboxHeight 10
// from http://msdn.microsoft.com/en-us/library/windows/desktop/bb226818%28v=vs.85%29.aspx
#define checkboxXFromLeftOfBoxToLeftOfLabel 12

static void uiCheckboxMinimumSize(uiWindowsControl *cc, int *width, int *height)
{
	uiCheckbox *c = uiCheckbox(cc);
	uiWindowsSizing sizing;
	int x, y;

	x = checkboxXFromLeftOfBoxToLeftOfLabel;
	y = checkboxHeight;
	uiWindowsGetSizing(c->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, &x, &y);
	*width = x + uiWindowsWindowTextWidth(c->hwnd);
	*height = y;
}

static void defaultOnToggled(uiCheckbox *c, void *data)
{
	// do nothing
}

char *uiCheckboxText(uiCheckbox *c)
{
	return uiWindowsWindowText(c->hwnd);
}

void uiCheckboxSetText(uiCheckbox *c, const char *text)
{
	uiWindowsSetWindowText(c->hwnd, text);
	// changing the text might necessitate a change in the checkbox's size
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(c));
}

void uiCheckboxOnToggled(uiCheckbox *c, void (*f)(uiCheckbox *, void *), void *data)
{
	c->onToggled = f;
	c->onToggledData = data;
}

int uiCheckboxChecked(uiCheckbox *c)
{
	return SendMessage(c->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void uiCheckboxSetChecked(uiCheckbox *c, int checked)
{
	WPARAM check;

	check = BST_CHECKED;
	if (!checked)
		check = BST_UNCHECKED;
	SendMessage(c->hwnd, BM_SETCHECK, check, 0);
}

uiCheckbox *uiNewCheckbox(const char *text)
{
	uiCheckbox *c;
	WCHAR *wtext;

	uiWindowsNewControl(uiCheckbox, c);

	wtext = toUTF16(text);
	c->hwnd = uiWindowsEnsureCreateControlHWND(0,
		L"button", wtext,
		BS_CHECKBOX | WS_TABSTOP,
		hInstance, NULL,
		TRUE);
	uiFree(wtext);

	uiWindowsRegisterWM_COMMANDHandler(c->hwnd, onWM_COMMAND, uiControl(c));
	uiCheckboxOnToggled(c, defaultOnToggled, NULL);

	return c;
}
