// 8 september 2015
#include "uipriv_windows.hpp"
#include "area.hpp"

// TODO handle WM_DESTROY/WM_NCDESTROY
// TODO same for other Direct2D stuff
static LRESULT CALLBACK areaWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	uiArea *a;
	CREATESTRUCTW *cs = (CREATESTRUCTW *) lParam;
	RECT client;
	WINDOWPOS *wp = (WINDOWPOS *) lParam;
	LRESULT lResult;

	a = (uiArea *) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (a == NULL) {
		if (uMsg == WM_CREATE) {
			a = (uiArea *) (cs->lpCreateParams);
			// assign a->hwnd here so we can use it immediately
			a->hwnd = hwnd;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) a);
		}
		// fall through to DefWindowProcW() anyway
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}

	// always recreate the render target if necessary
	if (a->rt == NULL)
		a->rt = makeHWNDRenderTarget(a->hwnd);

	if (areaDoDraw(a, uMsg, wParam, lParam, &lResult) != FALSE)
		return lResult;

	if (uMsg == WM_WINDOWPOSCHANGED) {
		if ((wp->flags & SWP_NOSIZE) != 0)
			return DefWindowProcW(hwnd, uMsg, wParam, lParam);
		uiWindowsEnsureGetClientRect(a->hwnd, &client);
		areaDrawOnResize(a, &client);
		areaScrollOnResize(a, &client);
		return 0;
	}

	if (areaDoScroll(a, uMsg, wParam, lParam, &lResult) != FALSE)
		return lResult;
	if (areaDoEvents(a, uMsg, wParam, lParam, &lResult) != FALSE)
		return lResult;

	// nothing done
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// control implementation

uiWindowsControlAllDefaults(uiArea)

static void uiAreaMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	// TODO
	*width = 0;
	*height = 0;
}

ATOM registerAreaClass(HICON hDefaultIcon, HCURSOR hDefaultCursor)
{
	WNDCLASSW wc;

	ZeroMemory(&wc, sizeof (WNDCLASSW));
	wc.lpszClassName = areaClass;
	wc.lpfnWndProc = areaWndProc;
	wc.hInstance = hInstance;
	wc.hIcon = hDefaultIcon;
	wc.hCursor = hDefaultCursor;
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	// this is just to be safe; see the InvalidateRect() call in the WM_WINDOWPOSCHANGED handler for more details
	wc.style = CS_HREDRAW | CS_VREDRAW;
	return RegisterClassW(&wc);
}

void unregisterArea(void)
{
	if (UnregisterClassW(areaClass, hInstance) == 0)
		logLastError(L"error unregistering uiArea window class");
}

void uiAreaSetSize(uiArea *a, int width, int height)
{
	a->scrollWidth = width;
	a->scrollHeight = height;
	areaUpdateScroll(a);
}

void uiAreaQueueRedrawAll(uiArea *a)
{
	// don't erase the background; we do that ourselves in doPaint()
	invalidateRect(a->hwnd, NULL, FALSE);
}

void uiAreaScrollTo(uiArea *a, double x, double y, double width, double height)
{
	// TODO
}

void uiAreaBeginUserWindowMove(uiArea *a)
{
	HWND toplevel;

	// TODO restrict execution
	ReleaseCapture();		// TODO use properly and reset internal data structures
	toplevel = parentToplevel(a->hwnd);
	if (toplevel == NULL) {
		// TODO
		return;
	}
	// see http://stackoverflow.com/questions/40249940/how-do-i-initiate-a-user-mouse-driven-move-or-resize-for-custom-window-borders-o#40250654
	SendMessageW(toplevel, WM_SYSCOMMAND,
		SC_MOVE | 2, 0);
}

void uiAreaBeginUserWindowResize(uiArea *a, uiWindowResizeEdge edge)
{
	HWND toplevel;
	WPARAM wParam;

	// TODO restrict execution
	ReleaseCapture();		// TODO use properly and reset internal data structures
	toplevel = parentToplevel(a->hwnd);
	if (toplevel == NULL) {
		// TODO
		return;
	}
	// see http://stackoverflow.com/questions/40249940/how-do-i-initiate-a-user-mouse-driven-move-or-resize-for-custom-window-borders-o#40250654
	wParam = SC_SIZE;
	switch (edge) {
	case uiWindowResizeEdgeLeft:
		wParam |= 1;
		break;
	case uiWindowResizeEdgeTop:
		wParam |= 3;
		break;
	case uiWindowResizeEdgeRight:
		wParam |= 2;
		break;
	case uiWindowResizeEdgeBottom:
		wParam |= 6;
		break;
	case uiWindowResizeEdgeTopLeft:
		wParam |= 4;
		break;
	case uiWindowResizeEdgeTopRight:
		wParam |= 5;
		break;
	case uiWindowResizeEdgeBottomLeft:
		wParam |= 7;
		break;
	case uiWindowResizeEdgeBottomRight:
		wParam |= 8;
		break;
	}
	SendMessageW(toplevel, WM_SYSCOMMAND,
		wParam, 0);
}

uiArea *uiNewArea(uiAreaHandler *ah)
{
	uiArea *a;

	uiWindowsNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = FALSE;
	clickCounterReset(&(a->cc));

	// a->hwnd is assigned in areaWndProc()
	uiWindowsEnsureCreateControlHWND(0,
		areaClass, L"",
		0,
		hInstance, a,
		FALSE);

	return a;
}

uiArea *uiNewScrollingArea(uiAreaHandler *ah, int width, int height)
{
	uiArea *a;

	uiWindowsNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = TRUE;
	a->scrollWidth = width;
	a->scrollHeight = height;
	clickCounterReset(&(a->cc));

	// a->hwnd is assigned in areaWndProc()
	uiWindowsEnsureCreateControlHWND(0,
		areaClass, L"",
		WS_HSCROLL | WS_VSCROLL,
		hInstance, a,
		FALSE);

	// set initial scrolling parameters
	areaUpdateScroll(a);

	return a;
}
