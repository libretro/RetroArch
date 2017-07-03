// 16 may 2015
#include "uipriv_windows.hpp"

struct uiGroup {
	uiWindowsControl c;
	HWND hwnd;
	struct uiControl *child;
	int margined;
};

// from https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define groupXMargin 6
#define groupYMarginTop 11 /* note this value /includes/ the groupbox label */
#define groupYMarginBottom 7

// unfortunately because the client area of a groupbox includes the frame and caption text, we have to apply some margins ourselves, even if we don't want "any"
// these were deduced by hand based on the standard DLU conversions; the X and Y top margins are the width and height, respectively, of one character cell
// they can be fine-tuned later
#define groupUnmarginedXMargin 4
#define groupUnmarginedYMarginTop 8
#define groupUnmarginedYMarginBottom 3

static void groupMargins(uiGroup *g, int *mx, int *mtop, int *mbottom)
{
	uiWindowsSizing sizing;

	*mx = groupUnmarginedXMargin;
	*mtop = groupUnmarginedYMarginTop;
	*mbottom = groupUnmarginedYMarginBottom;
	if (g->margined) {
		*mx = groupXMargin;
		*mtop = groupYMarginTop;
		*mbottom = groupYMarginBottom;
	}
	uiWindowsGetSizing(g->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, mx, mtop);
	uiWindowsSizingDlgUnitsToPixels(&sizing, NULL, mbottom);
}

static void groupRelayout(uiGroup *g)
{
	RECT r;
	int mx, mtop, mbottom;

	if (g->child == NULL)
		return;
	uiWindowsEnsureGetClientRect(g->hwnd, &r);
	groupMargins(g, &mx, &mtop, &mbottom);
	r.left += mx;
	r.top += mtop;
	r.right -= mx;
	r.bottom -= mbottom;
	uiWindowsEnsureMoveWindowDuringResize((HWND) uiControlHandle(g->child), r.left, r.top, r.right - r.left, r.bottom - r.top);
}

static void uiGroupDestroy(uiControl *c)
{
	uiGroup *g = uiGroup(c);

	if (g->child != NULL) {
		uiControlSetParent(g->child, NULL);
		uiControlDestroy(g->child);
	}
	uiWindowsEnsureDestroyWindow(g->hwnd);
	uiFreeControl(uiControl(g));
}

uiWindowsControlDefaultHandle(uiGroup)
uiWindowsControlDefaultParent(uiGroup)
uiWindowsControlDefaultSetParent(uiGroup)
uiWindowsControlDefaultToplevel(uiGroup)
uiWindowsControlDefaultVisible(uiGroup)
uiWindowsControlDefaultShow(uiGroup)
uiWindowsControlDefaultHide(uiGroup)
uiWindowsControlDefaultEnabled(uiGroup)
uiWindowsControlDefaultEnable(uiGroup)
uiWindowsControlDefaultDisable(uiGroup)

static void uiGroupSyncEnableState(uiWindowsControl *c, int enabled)
{
	uiGroup *g = uiGroup(c);

	if (uiWindowsShouldStopSyncEnableState(uiWindowsControl(g), enabled))
		return;
	EnableWindow(g->hwnd, enabled);
	if (g->child != NULL)
		uiWindowsControlSyncEnableState(uiWindowsControl(g->child), enabled);
}

uiWindowsControlDefaultSetParentHWND(uiGroup)

static void uiGroupMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiGroup *g = uiGroup(c);
	int mx, mtop, mbottom;
	int labelWidth;

	*width = 0;
	*height = 0;
	if (g->child != NULL)
		uiWindowsControlMinimumSize(uiWindowsControl(g->child), width, height);
	labelWidth = uiWindowsWindowTextWidth(g->hwnd);
	if (*width < labelWidth)		// don't clip the label; it doesn't ellipsize
		*width = labelWidth;
	groupMargins(g, &mx, &mtop, &mbottom);
	*width += 2 * mx;
	*height += mtop + mbottom;
}

static void uiGroupMinimumSizeChanged(uiWindowsControl *c)
{
	uiGroup *g = uiGroup(c);

	if (uiWindowsControlTooSmall(uiWindowsControl(g))) {
		uiWindowsControlContinueMinimumSizeChanged(uiWindowsControl(g));
		return;
	}
	groupRelayout(g);
}

uiWindowsControlDefaultLayoutRect(uiGroup)
uiWindowsControlDefaultAssignControlIDZOrder(uiGroup)

static void uiGroupChildVisibilityChanged(uiWindowsControl *c)
{
	// TODO eliminate the redundancy
	uiWindowsControlMinimumSizeChanged(c);
}

char *uiGroupTitle(uiGroup *g)
{
	return uiWindowsWindowText(g->hwnd);
}

void uiGroupSetTitle(uiGroup *g, const char *text)
{
	uiWindowsSetWindowText(g->hwnd, text);
	// changing the text might necessitate a change in the groupbox's size
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(g));
}

void uiGroupSetChild(uiGroup *g, uiControl *child)
{
	if (g->child != NULL) {
		uiControlSetParent(g->child, NULL);
		uiWindowsControlSetParentHWND(uiWindowsControl(g->child), NULL);
	}
	g->child = child;
	if (g->child != NULL) {
		uiControlSetParent(g->child, uiControl(g));
		uiWindowsControlSetParentHWND(uiWindowsControl(g->child), g->hwnd);
		uiWindowsControlAssignSoleControlIDZOrder(uiWindowsControl(g->child));
		uiWindowsControlMinimumSizeChanged(uiWindowsControl(g));
	}
}

int uiGroupMargined(uiGroup *g)
{
	return g->margined;
}

void uiGroupSetMargined(uiGroup *g, int margined)
{
	g->margined = margined;
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(g));
}

static LRESULT CALLBACK groupSubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	uiGroup *g = uiGroup(dwRefData);
	WINDOWPOS *wp = (WINDOWPOS *) lParam;
	MINMAXINFO *mmi = (MINMAXINFO *) lParam;
	int minwid, minht;
	LRESULT lResult;

	if (handleParentMessages(hwnd, uMsg, wParam, lParam, &lResult) != FALSE)
		return lResult;
	switch (uMsg) {
	case WM_WINDOWPOSCHANGED:
		if ((wp->flags & SWP_NOSIZE) != 0)
			break;
		groupRelayout(g);
		return 0;
	case WM_GETMINMAXINFO:
		lResult = DefWindowProcW(hwnd, uMsg, wParam, lParam);
		uiWindowsControlMinimumSize(uiWindowsControl(g), &minwid, &minht);
		mmi->ptMinTrackSize.x = minwid;
		mmi->ptMinTrackSize.y = minht;
		return lResult;
	case WM_NCDESTROY:
		if (RemoveWindowSubclass(hwnd, groupSubProc, uIdSubclass) == FALSE)
			logLastError(L"error removing groupbox subclass");
		break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

uiGroup *uiNewGroup(const char *text)
{
	uiGroup *g;
	WCHAR *wtext;

	uiWindowsNewControl(uiGroup, g);

	wtext = toUTF16(text);
	g->hwnd = uiWindowsEnsureCreateControlHWND(WS_EX_CONTROLPARENT,
		L"button", wtext,
		BS_GROUPBOX,
		hInstance, NULL,
		TRUE);
	uiFree(wtext);

	if (SetWindowSubclass(g->hwnd, groupSubProc, 0, (DWORD_PTR) g) == FALSE)
		logLastError(L"error subclassing groupbox to handle parent messages");

	return g;
}
