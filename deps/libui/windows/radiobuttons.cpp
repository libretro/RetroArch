// 20 may 2015
#include "uipriv_windows.hpp"

// desired behavior:
// - tab moves between the radio buttons and the adjacent controls
// - arrow keys navigate between radio buttons
// - arrow keys do not leave the radio buttons (this is done in control.c)
// - arrow keys wrap around bare groups (if the previous control has WS_GROUP but the first radio button doesn't, then it doesn't; since our radio buttons are all in their own child window we can't do that)
// - clicking on a radio button draws a focus rect (TODO)

struct uiRadioButtons {
	uiWindowsControl c;
	HWND hwnd;					// of the container
	std::vector<HWND> *hwnds;		// of the buttons
	void (*onSelected)(uiRadioButtons *, void *);
	void *onSelectedData;
};

static BOOL onWM_COMMAND(uiControl *c, HWND clicked, WORD code, LRESULT *lResult)
{
	uiRadioButtons *r = uiRadioButtons(c);
	WPARAM check;

	if (code != BN_CLICKED)
		return FALSE;
	for (const HWND &hwnd : *(r->hwnds)) {
		check = BST_UNCHECKED;
		if (clicked == hwnd)
			check = BST_CHECKED;
		SendMessage(hwnd, BM_SETCHECK, check, 0);
	}
	(*(r->onSelected))(r, r->onSelectedData);
	*lResult = 0;
	return TRUE;
}

static void defaultOnSelected(uiRadioButtons *r, void *data)
{
	// do nothing
}

static void uiRadioButtonsDestroy(uiControl *c)
{
	uiRadioButtons *r = uiRadioButtons(c);

	for (const HWND &hwnd : *(r->hwnds)) {
		uiWindowsUnregisterWM_COMMANDHandler(hwnd);
		uiWindowsEnsureDestroyWindow(hwnd);
	}
	delete r->hwnds;
	uiWindowsEnsureDestroyWindow(r->hwnd);
	uiFreeControl(uiControl(r));
}

// TODO SyncEnableState
uiWindowsControlAllDefaultsExceptDestroy(uiRadioButtons)

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define radiobuttonHeight 10
// from http://msdn.microsoft.com/en-us/library/windows/desktop/bb226818%28v=vs.85%29.aspx
#define radiobuttonXFromLeftOfBoxToLeftOfLabel 12

static void uiRadioButtonsMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiRadioButtons *r = uiRadioButtons(c);
	int wid, maxwid;
	uiWindowsSizing sizing;
	int x, y;

	if (r->hwnds->size() == 0) {
		*width = 0;
		*height = 0;
		return;
	}
	maxwid = 0;
	for (const HWND &hwnd : *(r->hwnds)) {
		wid = uiWindowsWindowTextWidth(hwnd);
		if (maxwid < wid)
			maxwid = wid;
	}

	x = radiobuttonXFromLeftOfBoxToLeftOfLabel;
	y = radiobuttonHeight;
	uiWindowsGetSizing((*(r->hwnds))[0], &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, &x, &y);

	*width = x + maxwid;
	*height = y * r->hwnds->size();
}

static void radiobuttonsRelayout(uiRadioButtons *r)
{
	RECT client;
	int x, y, width, height;
	int height1;
	uiWindowsSizing sizing;

	if (r->hwnds->size() == 0)
		return;
	uiWindowsEnsureGetClientRect(r->hwnd, &client);
	x = client.left;
	y = client.top;
	width = client.right - client.left;
	height1 = radiobuttonHeight;
	uiWindowsGetSizing((*(r->hwnds))[0], &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, NULL, &height1);
	height = height1;
	for (const HWND &hwnd : *(r->hwnds)) {
		uiWindowsEnsureMoveWindowDuringResize(hwnd, x, y, width, height);
		y += height;
	}
}

static void radiobuttonsArrangeChildren(uiRadioButtons *r)
{
	LONG_PTR controlID;
	HWND insertAfter;

	controlID = 100;
	insertAfter = NULL;
	for (const HWND &hwnd : *(r->hwnds))
		uiWindowsEnsureAssignControlIDZOrder(hwnd, &controlID, &insertAfter);
}

void uiRadioButtonsAppend(uiRadioButtons *r, const char *text)
{
	HWND hwnd;
	WCHAR *wtext;
	DWORD groupTabStop;

	// the first radio button gets both WS_GROUP and WS_TABSTOP
	// successive radio buttons get *neither*
	groupTabStop = 0;
	if (r->hwnds->size() == 0)
		groupTabStop = WS_GROUP | WS_TABSTOP;

	wtext = toUTF16(text);
	hwnd = uiWindowsEnsureCreateControlHWND(0,
		L"button", wtext,
		BS_RADIOBUTTON | groupTabStop,
		hInstance, NULL,
		TRUE);
	uiFree(wtext);
	uiWindowsEnsureSetParentHWND(hwnd, r->hwnd);
	uiWindowsRegisterWM_COMMANDHandler(hwnd, onWM_COMMAND, uiControl(r));
	r->hwnds->push_back(hwnd);
	radiobuttonsArrangeChildren(r);
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(r));
}

int uiRadioButtonsSelected(uiRadioButtons *r)
{
	size_t i;

	for (i = 0; i < r->hwnds->size(); i++)
		if (SendMessage((*(r->hwnds))[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
			return i;
	return -1;
}

void uiRadioButtonsSetSelected(uiRadioButtons *r, int n)
{
	int m;

	m = uiRadioButtonsSelected(r);
	if (m != -1)
		SendMessage((*(r->hwnds))[m], BM_SETCHECK, BST_UNCHECKED, 0);
	if (n != -1)
		SendMessage((*(r->hwnds))[n], BM_SETCHECK, BST_CHECKED, 0);
}

void uiRadioButtonsOnSelected(uiRadioButtons *r, void (*f)(uiRadioButtons *, void *), void *data)
{
	r->onSelected = f;
	r->onSelectedData = data;
}

static void onResize(uiWindowsControl *c)
{
	radiobuttonsRelayout(uiRadioButtons(c));
}

uiRadioButtons *uiNewRadioButtons(void)
{
	uiRadioButtons *r;

	uiWindowsNewControl(uiRadioButtons, r);

	r->hwnd = uiWindowsMakeContainer(uiWindowsControl(r), onResize);

	r->hwnds = new std::vector<HWND>;

	uiRadioButtonsOnSelected(r, defaultOnSelected, NULL);

	return r;
}
