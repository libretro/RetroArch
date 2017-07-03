// 16 august 2015
#include "uipriv_windows.hpp"

void uiWindowsControlSyncEnableState(uiWindowsControl *c, int enabled)
{
	(*(c->SyncEnableState))(c, enabled);
}

void uiWindowsControlSetParentHWND(uiWindowsControl *c, HWND parent)
{
	(*(c->SetParentHWND))(c, parent);
}

void uiWindowsControlMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	(*(c->MinimumSize))(c, width, height);
}

void uiWindowsControlMinimumSizeChanged(uiWindowsControl *c)
{
	(*(c->MinimumSizeChanged))(c);
}

// TODO get rid of this
void uiWindowsControlLayoutRect(uiWindowsControl *c, RECT *r)
{
	(*(c->LayoutRect))(c, r);
}

void uiWindowsControlAssignControlIDZOrder(uiWindowsControl *c, LONG_PTR *controlID, HWND *insertAfter)
{
	(*(c->AssignControlIDZOrder))(c, controlID, insertAfter);
}

void uiWindowsControlChildVisibilityChanged(uiWindowsControl *c)
{
	(*(c->ChildVisibilityChanged))(c);
}

HWND uiWindowsEnsureCreateControlHWND(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, HINSTANCE hInstance, LPVOID lpParam, BOOL useStandardControlFont)
{
	HWND hwnd;

	// don't let using the arrow keys in a uiRadioButtons leave the radio buttons
	if ((dwStyle & WS_TABSTOP) != 0)
		dwStyle |= WS_GROUP;
	hwnd = CreateWindowExW(dwExStyle,
		lpClassName, lpWindowName,
		dwStyle | WS_CHILD | WS_VISIBLE,
		0, 0,
		// use a nonzero initial size just in case some control breaks with a zero initial size
		100, 100,
		utilWindow, NULL, hInstance, lpParam);
	if (hwnd == NULL) {
		logLastError(L"error creating window");
		// TODO return a decoy window
	}
	if (useStandardControlFont)
		SendMessageW(hwnd, WM_SETFONT, (WPARAM) hMessageFont, (LPARAM) TRUE);
	return hwnd;
}

// choose a value distinct from uiWindowSignature
#define uiWindowsControlSignature 0x4D53576E

uiWindowsControl *uiWindowsAllocControl(size_t n, uint32_t typesig, const char *typenamestr)
{
	return uiWindowsControl(uiAllocControl(n, uiWindowsControlSignature, typesig, typenamestr));
}

BOOL uiWindowsShouldStopSyncEnableState(uiWindowsControl *c, BOOL enabled)
{
	int ce;

	ce = uiControlEnabled(uiControl(c));
	// only stop if we're going from disabled back to enabled; don't stop under any other condition
	// (if we stop when going from enabled to disabled then enabled children of a disabled control won't get disabled at the OS level)
	if (!ce && enabled)
		return TRUE;
	return FALSE;
}

void uiWindowsControlAssignSoleControlIDZOrder(uiWindowsControl *c)
{
	LONG_PTR controlID;
	HWND insertAfter;

	controlID = 100;
	insertAfter = NULL;
	uiWindowsControlAssignControlIDZOrder(c, &controlID, &insertAfter);
}

BOOL uiWindowsControlTooSmall(uiWindowsControl *c)
{
	RECT r;
	int width, height;

	uiWindowsControlLayoutRect(c, &r);
	uiWindowsControlMinimumSize(c, &width, &height);
	if ((r.right - r.left) < width)
		return TRUE;
	if ((r.bottom - r.top) < height)
		return TRUE;
	return FALSE;
}

void uiWindowsControlContinueMinimumSizeChanged(uiWindowsControl *c)
{
	uiControl *parent;

	parent = uiControlParent(uiControl(c));
	if (parent != NULL)
		uiWindowsControlMinimumSizeChanged(uiWindowsControl(parent));
}

// TODO rename this nad the OS X this and hugging ones to NotifyChild
void uiWindowsControlNotifyVisibilityChanged(uiWindowsControl *c)
{
	// TODO we really need to figure this out; the duplication is a mess
	uiWindowsControlContinueMinimumSizeChanged(c);
}
