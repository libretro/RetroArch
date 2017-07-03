// 27 april 2015
#include "uipriv_windows.hpp"

#define windowClass L"libui_uiWindowClass"

struct uiWindow {
	uiWindowsControl c;
	HWND hwnd;
	HMENU menubar;
	uiControl *child;
	BOOL shownOnce;
	int visible;
	int (*onClosing)(uiWindow *, void *);
	void *onClosingData;
	int margined;
	BOOL hasMenubar;
	void (*onContentSizeChanged)(uiWindow *, void *);
	void *onContentSizeChangedData;
	BOOL changingSize;
	int fullscreen;
	WINDOWPLACEMENT fsPrevPlacement;
	int borderless;
};

// from https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define windowMargin 7

static void windowMargins(uiWindow *w, int *mx, int *my)
{
	uiWindowsSizing sizing;

	*mx = 0;
	*my = 0;
	if (!w->margined)
		return;
	uiWindowsGetSizing(w->hwnd, &sizing);
	*mx = windowMargin;
	*my = windowMargin;
	uiWindowsSizingDlgUnitsToPixels(&sizing, mx, my);
}

static void windowRelayout(uiWindow *w)
{
	int x, y, width, height;
	RECT r;
	int mx, my;
	HWND child;

	if (w->child == NULL)
		return;
	x = 0;
	y = 0;
	uiWindowsEnsureGetClientRect(w->hwnd, &r);
	width = r.right - r.left;
	height = r.bottom - r.top;
	windowMargins(w, &mx, &my);
	x += mx;
	y += my;
	width -= 2 * mx;
	height -= 2 * my;
	child = (HWND) uiControlHandle(w->child);
	uiWindowsEnsureMoveWindowDuringResize(child, x, y, width, height);
}

static LRESULT CALLBACK windowWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG_PTR ww;
	uiWindow *w;
	CREATESTRUCTW *cs = (CREATESTRUCTW *) lParam;
	WINDOWPOS *wp = (WINDOWPOS *) lParam;
	MINMAXINFO *mmi = (MINMAXINFO *) lParam;
	int width, height;
	LRESULT lResult;

	ww = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (ww == 0) {
		if (uMsg == WM_CREATE)
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) (cs->lpCreateParams));
		// fall through to DefWindowProc() anyway
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
	w = uiWindow((void *) ww);
	if (handleParentMessages(hwnd, uMsg, wParam, lParam, &lResult) != FALSE)
		return lResult;
	switch (uMsg) {
	case WM_COMMAND:
		// not a menu
		if (lParam != 0)
			break;
		if (HIWORD(wParam) != 0)
			break;
		runMenuEvent(LOWORD(wParam), uiWindow(w));
		return 0;
	case WM_WINDOWPOSCHANGED:
		if ((wp->flags & SWP_NOSIZE) != 0)
			break;
		if (w->onContentSizeChanged != NULL)		// TODO figure out why this is happening too early
			if (!w->changingSize)
				(*(w->onContentSizeChanged))(w, w->onContentSizeChangedData);
		windowRelayout(w);
		return 0;
	case WM_GETMINMAXINFO:
		// ensure the user cannot resize the window smaller than its minimum size
		lResult = DefWindowProcW(hwnd, uMsg, wParam, lParam);
		uiWindowsControlMinimumSize(uiWindowsControl(w), &width, &height);
		// width and height are in client coordinates; ptMinTrackSize is in window coordinates
		clientSizeToWindowSize(w->hwnd, &width, &height, w->hasMenubar);
		mmi->ptMinTrackSize.x = width;
		mmi->ptMinTrackSize.y = height;
		return lResult;
	case WM_PRINTCLIENT:
		// we do no special painting; just erase the background
		// don't worry about the return value; we let DefWindowProcW() handle this message
		SendMessageW(hwnd, WM_ERASEBKGND, wParam, lParam);
		return 0;
	case WM_CLOSE:
		if ((*(w->onClosing))(w, w->onClosingData))
			uiControlDestroy(uiControl(w));
		return 0;		// we destroyed it already
	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

ATOM registerWindowClass(HICON hDefaultIcon, HCURSOR hDefaultCursor)
{
	WNDCLASSW wc;

	ZeroMemory(&wc, sizeof (WNDCLASSW));
	wc.lpszClassName = windowClass;
	wc.lpfnWndProc = windowWndProc;
	wc.hInstance = hInstance;
	wc.hIcon = hDefaultIcon;
	wc.hCursor = hDefaultCursor;
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	return RegisterClassW(&wc);
}

void unregisterWindowClass(void)
{
	if (UnregisterClassW(windowClass, hInstance) == 0)
		logLastError(L"error unregistering uiWindow window class");
}

static int defaultOnClosing(uiWindow *w, void *data)
{
	return 0;
}

static void defaultOnPositionContentSizeChanged(uiWindow *w, void *data)
{
	// do nothing
}

static std::map<uiWindow *, bool> windows;

static void uiWindowDestroy(uiControl *c)
{
	uiWindow *w = uiWindow(c);

	// first hide ourselves
	ShowWindow(w->hwnd, SW_HIDE);
	// now destroy the child
	if (w->child != NULL) {
		uiControlSetParent(w->child, NULL);
		uiControlDestroy(w->child);
	}
	// now free the menubar, if any
	if (w->menubar != NULL)
		freeMenubar(w->menubar);
	// and finally free ourselves
	windows.erase(w);
	uiWindowsEnsureDestroyWindow(w->hwnd);
	uiFreeControl(uiControl(w));
}

uiWindowsControlDefaultHandle(uiWindow)

uiControl *uiWindowParent(uiControl *c)
{
	return NULL;
}

void uiWindowSetParent(uiControl *c, uiControl *parent)
{
	uiUserBugCannotSetParentOnToplevel("uiWindow");
}

static int uiWindowToplevel(uiControl *c)
{
	return 1;
}

// TODO initial state of windows is hidden; ensure this here and make it so on other platforms
static int uiWindowVisible(uiControl *c)
{
	uiWindow *w = uiWindow(c);

	return w->visible;
}

static void uiWindowShow(uiControl *c)
{
	uiWindow *w = uiWindow(c);

	w->visible = 1;
	// just in case the window's minimum size wasn't recalculated already
	ensureMinimumWindowSize(w);
	if (w->shownOnce) {
		ShowWindow(w->hwnd, SW_SHOW);
		return;
	}
	w->shownOnce = TRUE;
	// make sure the child is the correct size
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(w));
	ShowWindow(w->hwnd, nCmdShow);
	if (UpdateWindow(w->hwnd) == 0)
		logLastError(L"error calling UpdateWindow() after showing uiWindow for the first time");
}

static void uiWindowHide(uiControl *c)
{
	uiWindow *w = uiWindow(c);

	w->visible = 0;
	ShowWindow(w->hwnd, SW_HIDE);
}

// TODO we don't want the window to be disabled completely; that would prevent it from being moved! ...would it?
uiWindowsControlDefaultEnabled(uiWindow)
uiWindowsControlDefaultEnable(uiWindow)
uiWindowsControlDefaultDisable(uiWindow)
// TODO we need to do something about undocumented fields in the OS control types
uiWindowsControlDefaultSyncEnableState(uiWindow)
// TODO
uiWindowsControlDefaultSetParentHWND(uiWindow)

static void uiWindowMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiWindow *w = uiWindow(c);
	int mx, my;

	*width = 0;
	*height = 0;
	if (w->child != NULL)
		uiWindowsControlMinimumSize(uiWindowsControl(w->child), width, height);
	windowMargins(w, &mx, &my);
	*width += 2 * mx;
	*height += 2 * my;
}

static void uiWindowMinimumSizeChanged(uiWindowsControl *c)
{
	uiWindow *w = uiWindow(c);

	if (uiWindowsControlTooSmall(uiWindowsControl(w))) {
		// TODO figure out what to do with this function
		// maybe split it into two so WM_GETMINMAXINFO can use it?
		ensureMinimumWindowSize(w);
		return;
	}
	// otherwise we only need to re-layout everything
	windowRelayout(w);
}

static void uiWindowLayoutRect(uiWindowsControl *c, RECT *r)
{
	uiWindow *w = uiWindow(c);

	// the layout rect is the client rect in this case
	uiWindowsEnsureGetClientRect(w->hwnd, r);
}

uiWindowsControlDefaultAssignControlIDZOrder(uiWindow)

static void uiWindowChildVisibilityChanged(uiWindowsControl *c)
{
	// TODO eliminate the redundancy
	uiWindowsControlMinimumSizeChanged(c);
}

char *uiWindowTitle(uiWindow *w)
{
	return uiWindowsWindowText(w->hwnd);
}

void uiWindowSetTitle(uiWindow *w, const char *title)
{
	uiWindowsSetWindowText(w->hwnd, title);
	// don't queue resize; the caption isn't part of what affects layout and sizing of the client area (it'll be ellipsized if too long)
}

// this is used for both fullscreening and centering
// see also https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353 and https://blogs.msdn.microsoft.com/oldnewthing/20050505-04/?p=35703
static void windowMonitorRect(HWND hwnd, RECT *r)
{
	HMONITOR monitor;
	MONITORINFO mi;

	monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
	ZeroMemory(&mi, sizeof (MONITORINFO));
	mi.cbSize = sizeof (MONITORINFO);
	if (GetMonitorInfoW(monitor, &mi) == 0) {
		logLastError(L"error getting window monitor rect");
		// default to SM_CXSCREEN x SM_CYSCREEN to be safe
		r->left = 0;
		r->top = 0;
		r->right = GetSystemMetrics(SM_CXSCREEN);
		r->bottom = GetSystemMetrics(SM_CYSCREEN);
		return;
	}
	*r = mi.rcMonitor;
}

void uiWindowContentSize(uiWindow *w, int *width, int *height)
{
	RECT r;

	uiWindowsEnsureGetClientRect(w->hwnd, &r);
	*width = r.right - r.left;
	*height = r.bottom - r.top;
}

// TODO should this disallow too small?
void uiWindowSetContentSize(uiWindow *w, int width, int height)
{
	w->changingSize = TRUE;
	clientSizeToWindowSize(w->hwnd, &width, &height, w->hasMenubar);
	if (SetWindowPos(w->hwnd, NULL, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER) == 0)
		logLastError(L"error resizing window");
	w->changingSize = FALSE;
}

int uiWindowFullscreen(uiWindow *w)
{
	return w->fullscreen;
}

void uiWindowSetFullscreen(uiWindow *w, int fullscreen)
{
	RECT r;

	if (w->fullscreen && fullscreen)
		return;
	if (!w->fullscreen && !fullscreen)
		return;
	w->fullscreen = fullscreen;
	w->changingSize = TRUE;
	if (w->fullscreen) {
		ZeroMemory(&(w->fsPrevPlacement), sizeof (WINDOWPLACEMENT));
		w->fsPrevPlacement.length = sizeof (WINDOWPLACEMENT);
		if (GetWindowPlacement(w->hwnd, &(w->fsPrevPlacement)) == 0)
			logLastError(L"error getting old window placement");
		windowMonitorRect(w->hwnd, &r);
		setStyle(w->hwnd, getStyle(w->hwnd) & ~WS_OVERLAPPEDWINDOW);
		if (SetWindowPos(w->hwnd, HWND_TOP,
			r.left, r.top,
			r.right - r.left, r.bottom - r.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER) == 0)
			logLastError(L"error making window fullscreen");
	} else {
		if (!w->borderless)		// keep borderless until that is turned off
			setStyle(w->hwnd, getStyle(w->hwnd) | WS_OVERLAPPEDWINDOW);
		if (SetWindowPlacement(w->hwnd, &(w->fsPrevPlacement)) == 0)
			logLastError(L"error leaving fullscreen");
		if (SetWindowPos(w->hwnd, NULL,
			0, 0, 0, 0,
			SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER) == 0)
			logLastError(L"error restoring window border after fullscreen");
	}
	w->changingSize = FALSE;
}

void uiWindowOnContentSizeChanged(uiWindow *w, void (*f)(uiWindow *, void *), void *data)
{
	w->onContentSizeChanged = f;
	w->onContentSizeChangedData = data;
}

void uiWindowOnClosing(uiWindow *w, int (*f)(uiWindow *, void *), void *data)
{
	w->onClosing = f;
	w->onClosingData = data;
}

int uiWindowBorderless(uiWindow *w)
{
	return w->borderless;
}

// TODO window should move to the old client position and should not have the extra space the borders left behind
// TODO extract the relevant styles from WS_OVERLAPPEDWINDOW?
void uiWindowSetBorderless(uiWindow *w, int borderless)
{
	w->borderless = borderless;
	if (w->borderless)
		setStyle(w->hwnd, getStyle(w->hwnd) & ~WS_OVERLAPPEDWINDOW);
	else
		if (!w->fullscreen)		// keep borderless until leaving fullscreen
			setStyle(w->hwnd, getStyle(w->hwnd) | WS_OVERLAPPEDWINDOW);
}

void uiWindowSetChild(uiWindow *w, uiControl *child)
{
	if (w->child != NULL) {
		uiControlSetParent(w->child, NULL);
		uiWindowsControlSetParentHWND(uiWindowsControl(w->child), NULL);
	}
	w->child = child;
	if (w->child != NULL) {
		uiControlSetParent(w->child, uiControl(w));
		uiWindowsControlSetParentHWND(uiWindowsControl(w->child), w->hwnd);
		uiWindowsControlAssignSoleControlIDZOrder(uiWindowsControl(w->child));
		windowRelayout(w);
	}
}

int uiWindowMargined(uiWindow *w)
{
	return w->margined;
}

void uiWindowSetMargined(uiWindow *w, int margined)
{
	w->margined = margined;
	windowRelayout(w);
}

// see http://blogs.msdn.com/b/oldnewthing/archive/2003/09/11/54885.aspx and http://blogs.msdn.com/b/oldnewthing/archive/2003/09/13/54917.aspx
// TODO use clientSizeToWindowSize()
static void setClientSize(uiWindow *w, int width, int height, BOOL hasMenubar, DWORD style, DWORD exstyle)
{
	RECT window;

	window.left = 0;
	window.top = 0;
	window.right = width;
	window.bottom = height;
	if (AdjustWindowRectEx(&window, style, hasMenubar, exstyle) == 0)
		logLastError(L"error getting real window coordinates");
	if (hasMenubar) {
		RECT temp;

		temp = window;
		temp.bottom = 0x7FFF;		// infinite height
		SendMessageW(w->hwnd, WM_NCCALCSIZE, (WPARAM) FALSE, (LPARAM) (&temp));
		window.bottom += temp.top;
	}
	if (SetWindowPos(w->hwnd, NULL, 0, 0, window.right - window.left, window.bottom - window.top, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER) == 0)
		logLastError(L"error resizing window");
}

uiWindow *uiNewWindow(const char *title, int width, int height, int hasMenubar)
{
	uiWindow *w;
	WCHAR *wtitle;
	BOOL hasMenubarBOOL;

	uiWindowsNewControl(uiWindow, w);

	hasMenubarBOOL = FALSE;
	if (hasMenubar)
		hasMenubarBOOL = TRUE;
	w->hasMenubar = hasMenubarBOOL;

#define style WS_OVERLAPPEDWINDOW
#define exstyle 0

	wtitle = toUTF16(title);
	w->hwnd = CreateWindowExW(exstyle,
		windowClass, wtitle,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		// use the raw width and height for now
		// this will get CW_USEDEFAULT (hopefully) predicting well
		// even if it doesn't, we're adjusting it later
		width, height,
		NULL, NULL, hInstance, w);
	if (w->hwnd == NULL)
		logLastError(L"error creating window");
	uiFree(wtitle);

	if (hasMenubar) {
		w->menubar = makeMenubar();
		if (SetMenu(w->hwnd, w->menubar) == 0)
			logLastError(L"error giving menu to window");
	}

	// and use the proper size
	setClientSize(w, width, height, hasMenubarBOOL, style, exstyle);

	uiWindowOnClosing(w, defaultOnClosing, NULL);
	uiWindowOnContentSizeChanged(w, defaultOnPositionContentSizeChanged, NULL);

	windows[w] = true;
	return w;
}

// this cannot queue a resize because it's called by the resize handler
void ensureMinimumWindowSize(uiWindow *w)
{
	int width, height;
	RECT r;

	uiWindowsControlMinimumSize(uiWindowsControl(w), &width, &height);
	uiWindowsEnsureGetClientRect(w->hwnd, &r);
	if (width < (r.right - r.left))		// preserve width if larger
		width = r.right - r.left;
	if (height < (r.bottom - r.top))		// preserve height if larger
		height = r.bottom - r.top;
	clientSizeToWindowSize(w->hwnd, &width, &height, w->hasMenubar);
	if (SetWindowPos(w->hwnd, NULL, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER) == 0)
		logLastError(L"error resizing window");
}

void disableAllWindowsExcept(uiWindow *which)
{
	for (auto &w : windows) {
		if (w.first == which)
			continue;
		EnableWindow(w.first->hwnd, FALSE);
	}
}

void enableAllWindowsExcept(uiWindow *which)
{
	for (auto &w : windows) {
		if (w.first == which)
			continue;
		if (!uiControlEnabled(uiControl(w.first)))
			continue;
		EnableWindow(w.first->hwnd, TRUE);
	}
}
