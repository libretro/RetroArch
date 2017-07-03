// 17 april 2016
#include "uipriv_windows.hpp"

// The Direct2D scratch window is a utility for libui internal use to do quick things with Direct2D.
// To use, call newD2DScratch() passing in a subclass procedure. This subclass procedure should handle the msgD2DScratchPaint message, which has the following usage:
// - wParam - 0
// - lParam - ID2D1RenderTarget *
// - lResult - 0
// You can optionally also handle msgD2DScratchLButtonDown, which is sent when the left mouse button is either pressed for the first time or held while the mouse is moving.
// - wParam - position in DIPs, as D2D1_POINT_2F *
// - lParam - size of render target in DIPs, as D2D1_SIZE_F *
// - lResult - 0
// Other messages can also be handled here.

// TODO allow resize

#define d2dScratchClass L"libui_d2dScratchClass"

// TODO clip rect
static HRESULT d2dScratchDoPaint(HWND hwnd, ID2D1RenderTarget *rt)
{
	COLORREF bgcolorref;
	D2D1_COLOR_F bgcolor;

	rt->BeginDraw();

	// TODO only clear the clip area
	// TODO clear with actual background brush
	bgcolorref = GetSysColor(COLOR_BTNFACE);
	bgcolor.r = ((float) GetRValue(bgcolorref)) / 255.0;
	// due to utter apathy on Microsoft's part, GetGValue() does not work with MSVC's Run-Time Error Checks
	// it has not worked since 2008 and they have *never* fixed it
	// TODO now that -RTCc has just been deprecated entirely, should we switch back?
	bgcolor.g = ((float) ((BYTE) ((bgcolorref & 0xFF00) >> 8))) / 255.0;
	bgcolor.b = ((float) GetBValue(bgcolorref)) / 255.0;
	bgcolor.a = 1.0;
	rt->Clear(&bgcolor);

	SendMessageW(hwnd, msgD2DScratchPaint, 0, (LPARAM) rt);

	return rt->EndDraw(NULL, NULL);
}

static void d2dScratchDoLButtonDown(HWND hwnd, ID2D1RenderTarget *rt, LPARAM lParam)
{
	double xpix, ypix;
	FLOAT dpix, dpiy;
	D2D1_POINT_2F pos;
	D2D1_SIZE_F size;

	xpix = (double) GET_X_LPARAM(lParam);
	ypix = (double) GET_Y_LPARAM(lParam);
	// these are in pixels; we need points
	// TODO separate the function from areautil.cpp?
	rt->GetDpi(&dpix, &dpiy);
	pos.x = (xpix * 96) / dpix;
	pos.y = (ypix * 96) / dpiy;

	size = realGetSize(rt);

	SendMessageW(hwnd, msgD2DScratchLButtonDown, (WPARAM) (&pos), (LPARAM) (&size));
}

static LRESULT CALLBACK d2dScratchWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG_PTR init;
	ID2D1HwndRenderTarget *rt;
	ID2D1DCRenderTarget *dcrt;
	RECT client;
	HRESULT hr;

	init = GetWindowLongPtrW(hwnd, 0);
	if (!init) {
		if (uMsg == WM_CREATE)
			SetWindowLongPtrW(hwnd, 0, (LONG_PTR) TRUE);
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}

	rt = (ID2D1HwndRenderTarget *) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (rt == NULL) {
		rt = makeHWNDRenderTarget(hwnd);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) rt);
	}

	switch (uMsg) {
	case WM_DESTROY:
		rt->Release();
		SetWindowLongPtrW(hwnd, 0, (LONG_PTR) FALSE);
		break;
	case WM_PAINT:
		hr = d2dScratchDoPaint(hwnd, rt);
		switch (hr) {
		case S_OK:
			if (ValidateRect(hwnd, NULL) == 0)
				logLastError(L"error validating D2D scratch control rect");
			break;
		case D2DERR_RECREATE_TARGET:
			// DON'T validate the rect
			// instead, simply drop the render target
			// we'll get another WM_PAINT and make the render target again
			// TODO would this require us to invalidate the entire client area?
			rt->Release();
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) NULL);
			break;
		default:
			logHRESULT(L"error drawing D2D scratch window", hr);
		}
		return 0;
	case WM_PRINTCLIENT:
		uiWindowsEnsureGetClientRect(hwnd, &client);
		dcrt = makeHDCRenderTarget((HDC) wParam, &client);
		hr = d2dScratchDoPaint(hwnd, dcrt);
		if (hr != S_OK)
			logHRESULT(L"error printing D2D scratch window client area", hr);
		dcrt->Release();
		return 0;
	case WM_LBUTTONDOWN:
		d2dScratchDoLButtonDown(hwnd, rt, lParam);
		return 0;
	case WM_MOUSEMOVE:
		// also send LButtonDowns when dragging
		if ((wParam & MK_LBUTTON) != 0)
			d2dScratchDoLButtonDown(hwnd, rt, lParam);
		return 0;
	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

ATOM registerD2DScratchClass(HICON hDefaultIcon, HCURSOR hDefaultCursor)
{
	WNDCLASSW wc;

	ZeroMemory(&wc, sizeof (WNDCLASSW));
	wc.lpszClassName = d2dScratchClass;
	wc.lpfnWndProc = d2dScratchWndProc;
	wc.hInstance = hInstance;
	wc.hIcon = hDefaultIcon;
	wc.hCursor = hDefaultCursor;
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	wc.cbWndExtra = sizeof (LONG_PTR);			// for the init status
	return RegisterClassW(&wc);
}

void unregisterD2DScratchClass(void)
{
	if (UnregisterClassW(d2dScratchClass, hInstance) == 0)
		logLastError(L"error unregistering D2D scratch window class");
}

HWND newD2DScratch(HWND parent, RECT *rect, HMENU controlID, SUBCLASSPROC subclass, DWORD_PTR subclassData)
{
	HWND hwnd;

	hwnd = CreateWindowExW(0,
		d2dScratchClass, L"",
		WS_CHILD | WS_VISIBLE,
		rect->left, rect->top,
		rect->right - rect->left, rect->bottom - rect->top,
		parent, controlID, hInstance, NULL);
	if (hwnd == NULL)
		// TODO return decoy window
		logLastError(L"error creating D2D scratch window");
	if (SetWindowSubclass(hwnd, subclass, 0, subclassData) == FALSE)
		logLastError(L"error subclassing D2D scratch window");
	return hwnd;
}
