// 8 april 2015
#include "uipriv_windows.hpp"

struct uiSpinbox {
	uiWindowsControl c;
	HWND hwnd;
	HWND edit;
	HWND updown;
	void (*onChanged)(uiSpinbox *, void *);
	void *onChangedData;
	BOOL inhibitChanged;
};

// utility functions

static int value(uiSpinbox *s)
{
	BOOL neededCap = FALSE;
	LRESULT val;

	// This verifies the value put in, capping it automatically.
	// We don't need to worry about checking for an error; that flag should really be called "did we have to cap?".
	// We DO need to set the value in case of a cap though.
	val = SendMessageW(s->updown, UDM_GETPOS32, 0, (LPARAM) (&neededCap));
	if (neededCap) {
		s->inhibitChanged = TRUE;
		SendMessageW(s->updown, UDM_SETPOS32, 0, (LPARAM) val);
		s->inhibitChanged = FALSE;
	}
	return val;
}

// control implementation

static BOOL onWM_COMMAND(uiControl *c, HWND hwnd, WORD code, LRESULT *lResult)
{
	uiSpinbox *s = (uiSpinbox *) c;
	WCHAR *wtext;

	if (code != EN_CHANGE)
		return FALSE;
	if (s->inhibitChanged)
		return FALSE;
	// We want to allow typing negative numbers; the natural way to do so is to start with a -.
	// However, if we just have the code below, the up-down will catch the bare - and reject it.
	// Let's fix that.
	// This won't handle leading spaces, but spaces aren't allowed *anyway*.
	wtext = windowText(s->edit);
	if (wcscmp(wtext, L"-") == 0) {
		uiFree(wtext);
		return TRUE;
	}
	uiFree(wtext);
	// value() does the work for us
	value(s);
	(*(s->onChanged))(s, s->onChangedData);
	return TRUE;
}

static void uiSpinboxDestroy(uiControl *c)
{
	uiSpinbox *s = uiSpinbox(c);

	uiWindowsUnregisterWM_COMMANDHandler(s->edit);
	uiWindowsEnsureDestroyWindow(s->updown);
	uiWindowsEnsureDestroyWindow(s->edit);
	uiWindowsEnsureDestroyWindow(s->hwnd);
	uiFreeControl(uiControl(s));
}

// TODO SyncEnableState
uiWindowsControlAllDefaultsExceptDestroy(uiSpinbox)

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
// TODO reduce this?
#define entryWidth 107 /* this is actually the shorter progress bar width, but Microsoft only indicates as wide as necessary */
#define entryHeight 14

static void uiSpinboxMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiSpinbox *s = uiSpinbox(c);
	uiWindowsSizing sizing;
	int x, y;

	x = entryWidth;
	y = entryHeight;
	// note that we go by the edit here
	uiWindowsGetSizing(s->edit, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, &x, &y);
	*width = x;
	*height = y;
}

static void spinboxArrangeChildren(uiSpinbox *s)
{
	LONG_PTR controlID;
	HWND insertAfter;

	controlID = 100;
	insertAfter = NULL;
	uiWindowsEnsureAssignControlIDZOrder(s->edit, &controlID, &insertAfter);
	uiWindowsEnsureAssignControlIDZOrder(s->updown, &controlID, &insertAfter);
}

// an up-down control will only properly position itself the first time
// stupidly, there are no messages to force a size calculation, nor can I seem to reset the buddy window to force a new position
// alas, we have to make a new up/down control each time :(
static void recreateUpDown(uiSpinbox *s)
{
	BOOL preserve = FALSE;
	int current;
	// Microsoft's commctrl.h says to use this type
	INT min, max;

	if (s->updown != NULL) {
		preserve = TRUE;
		current = value(s);
		SendMessageW(s->updown, UDM_GETRANGE32, (WPARAM) (&min), (LPARAM) (&max));
		uiWindowsEnsureDestroyWindow(s->updown);
	}
	s->inhibitChanged = TRUE;
	s->updown = CreateWindowExW(0,
		UPDOWN_CLASSW, L"",
		// no WS_VISIBLE; we set visibility ourselves
		// up-down control should not be a tab stop
		WS_CHILD | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_HOTTRACK | UDS_NOTHOUSANDS | UDS_SETBUDDYINT,
		// this is important; it's necessary for autosizing to work
		0, 0, 0, 0,
		s->hwnd, NULL, hInstance, NULL);
	if (s->updown == NULL)
		logLastError(L"error creating updown");
	SendMessageW(s->updown, UDM_SETBUDDY, (WPARAM) (s->edit), 0);
	if (preserve) {
		SendMessageW(s->updown, UDM_SETRANGE32, (WPARAM) min, (LPARAM) max);
		SendMessageW(s->updown, UDM_SETPOS32, 0, (LPARAM) current);
	}
	// preserve the Z-order
	spinboxArrangeChildren(s);
	// TODO properly show/enable
	ShowWindow(s->updown, SW_SHOW);
	s->inhibitChanged = FALSE;
}

static void spinboxRelayout(uiSpinbox *s)
{
	RECT r;

	// make the edit fill the container first; the new updown will resize it
	uiWindowsEnsureGetClientRect(s->hwnd, &r);
	uiWindowsEnsureMoveWindowDuringResize(s->edit, r.left, r.top, r.right - r.left, r.bottom - r.top);
	recreateUpDown(s);
}

static void defaultOnChanged(uiSpinbox *s, void *data)
{
	// do nothing
}

int uiSpinboxValue(uiSpinbox *s)
{
	return value(s);
}

void uiSpinboxSetValue(uiSpinbox *s, int value)
{
	s->inhibitChanged = TRUE;
	SendMessageW(s->updown, UDM_SETPOS32, 0, (LPARAM) value);
	s->inhibitChanged = FALSE;
}

void uiSpinboxOnChanged(uiSpinbox *s, void (*f)(uiSpinbox *, void *), void *data)
{
	s->onChanged = f;
	s->onChangedData = data;
}

static void onResize(uiWindowsControl *c)
{
	spinboxRelayout(uiSpinbox(c));
}

uiSpinbox *uiNewSpinbox(int min, int max)
{
	uiSpinbox *s;
	int temp;

	if (min >= max) {
		temp = min;
		min = max;
		max = temp;
	}

	uiWindowsNewControl(uiSpinbox, s);

	s->hwnd = uiWindowsMakeContainer(uiWindowsControl(s), onResize);

	s->edit = uiWindowsEnsureCreateControlHWND(WS_EX_CLIENTEDGE,
		L"edit", L"",
		// don't use ES_NUMBER; it doesn't allow typing in a leading -
		ES_AUTOHSCROLL | ES_LEFT | ES_NOHIDESEL | WS_TABSTOP,
		hInstance, NULL,
		TRUE);
	uiWindowsEnsureSetParentHWND(s->edit, s->hwnd);

	uiWindowsRegisterWM_COMMANDHandler(s->edit, onWM_COMMAND, uiControl(s));
	uiSpinboxOnChanged(s, defaultOnChanged, NULL);

	recreateUpDown(s);
	s->inhibitChanged = TRUE;
	SendMessageW(s->updown, UDM_SETRANGE32, (WPARAM) min, (LPARAM) max);
	SendMessageW(s->updown, UDM_SETPOS32, 0, (LPARAM) min);
	s->inhibitChanged = FALSE;

	return s;
}
