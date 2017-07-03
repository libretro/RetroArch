// 30 may 2015
#include "uipriv_windows.hpp"

// TODO refine error handling

// from http://msdn.microsoft.com/en-us/library/windows/desktop/bb226818%28v=vs.85%29.aspx
#define tabMargin 7

static void tabPageMargins(struct tabPage *tp, int *mx, int *my)
{
	uiWindowsSizing sizing;

	*mx = 0;
	*my = 0;
	if (!tp->margined)
		return;
	uiWindowsGetSizing(tp->hwnd, &sizing);
	*mx = tabMargin;
	*my = tabMargin;
	uiWindowsSizingDlgUnitsToPixels(&sizing, mx, my);
}

static void tabPageRelayout(struct tabPage *tp)
{
	RECT r;
	int mx, my;
	HWND child;

	if (tp->child == NULL)
		return;
	uiWindowsEnsureGetClientRect(tp->hwnd, &r);
	tabPageMargins(tp, &mx, &my);
	r.left += mx;
	r.top += my;
	r.right -= mx;
	r.bottom -= my;
	child = (HWND) uiControlHandle(tp->child);
	uiWindowsEnsureMoveWindowDuringResize(child, r.left, r.top, r.right - r.left, r.bottom - r.top);
}

// dummy dialog procedure; see below for details
// let's handle parent messages here to avoid needing to subclass
// TODO do we need to handle DM_GETDEFID/DM_SETDEFID here too because of the ES_WANTRETURN stuff at http://blogs.msdn.com/b/oldnewthing/archive/2007/08/20/4470527.aspx? what about multiple default buttons (TODO)?
// TODO we definitely need to do something about edit message handling; it does a fake close of our parent on pressing escape, causing uiWindow to stop responding to maximizes but still respond to events and then die horribly on destruction
static INT_PTR CALLBACK dlgproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct tabPage *tp;
	LRESULT lResult;

	if (uMsg == WM_INITDIALOG) {
		tp = (struct tabPage *) lParam;
		tp->hwnd = hwnd;
		SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR) tp);
		return TRUE;
	}
	if (handleParentMessages(hwnd, uMsg, wParam, lParam, &lResult) != FALSE) {
		SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, (LONG_PTR) lResult);
		return TRUE;
	}
	if (uMsg == WM_WINDOWPOSCHANGED) {
		tp = (struct tabPage *) GetWindowLongPtrW(hwnd, DWLP_USER);
		tabPageRelayout(tp);
		// pretend the dialog hasn't handled this just in case it needs to do something special
		return FALSE;
	}

	// unthemed dialogs don't respond to WM_PRINTCLIENT
	// fortunately they don't have any special painting
	if (uMsg == WM_PRINTCLIENT) {
		// don't worry about the return value; hopefully DefWindowProcW() caught it (if not the dialog procedure itself)
		// we COULD paint the dialog background brush ourselves but meh, it works
		SendMessageW(hwnd, WM_ERASEBKGND, wParam, lParam);
		// and pretend we did nothing just so the themed dialog can still paint its content
		// TODO see if w ecan avoid erasing the background in this case in the first place, or if we even need to
		return FALSE;
	}

	return FALSE;
}

struct tabPage *newTabPage(uiControl *child)
{
	struct tabPage *tp;
	HRESULT hr;

	tp = uiNew(struct tabPage);

	// unfortunately this needs to be a proper dialog for EnableThemeDialogTexture() to work; CreateWindowExW() won't suffice
	if (CreateDialogParamW(hInstance, MAKEINTRESOURCE(rcTabPageDialog),
		utilWindow, dlgproc, (LPARAM) tp) == NULL)
		logLastError(L"error creating tab page");

	tp->child = child;
	if (tp->child != NULL) {
		uiWindowsEnsureSetParentHWND((HWND) uiControlHandle(tp->child), tp->hwnd);
		uiWindowsControlAssignSoleControlIDZOrder(uiWindowsControl(tp->child));
	}

	hr = EnableThemeDialogTexture(tp->hwnd, ETDT_ENABLE | ETDT_USETABTEXTURE | ETDT_ENABLETAB);
	if (hr != S_OK)
		logHRESULT(L"error setting tab page background", hr);
		// continue anyway; it'll look wrong but eh

	// and start the tab page hidden
	ShowWindow(tp->hwnd, SW_HIDE);

	return tp;
}

void tabPageDestroy(struct tabPage *tp)
{
	// don't destroy the child with the page
	if (tp->child != NULL)
		uiWindowsControlSetParentHWND(uiWindowsControl(tp->child), NULL);
	// don't call EndDialog(); that's for the DialogBox() family of functions instead of CreateDialog()
	uiWindowsEnsureDestroyWindow(tp->hwnd);
	uiFree(tp);
}

void tabPageMinimumSize(struct tabPage *tp, int *width, int *height)
{
	int mx, my;

	*width = 0;
	*height = 0;
	if (tp->child != NULL)
		uiWindowsControlMinimumSize(uiWindowsControl(tp->child), width, height);
	tabPageMargins(tp, &mx, &my);
	*width += 2 * mx;
	*height += 2 * my;
}
