// 6 april 2015
#include "uipriv_windows.hpp"

void uiWindowsEnsureDestroyWindow(HWND hwnd)
{
	if (DestroyWindow(hwnd) == 0)
		logLastError(L"error destroying window");
}

void uiWindowsEnsureSetParentHWND(HWND hwnd, HWND parent)
{
	if (parent == NULL)
		parent = utilWindow;
	if (SetParent(hwnd, parent) == 0)
		logLastError(L"error setting window parent");
}

void uiWindowsEnsureAssignControlIDZOrder(HWND hwnd, LONG_PTR *controlID, HWND *insertAfter)
{
	SetWindowLongPtrW(hwnd, GWLP_ID, *controlID);
	(*controlID)++;
	setWindowInsertAfter(hwnd, *insertAfter);
	*insertAfter = hwnd;
}

void uiWindowsEnsureMoveWindowDuringResize(HWND hwnd, int x, int y, int width, int height)
{
	RECT r;

	r.left = x;
	r.top = y;
	r.right = x + width;
	r.bottom = y + height;
	if (SetWindowPos(hwnd, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER) == 0)
		logLastError(L"error moving window");
}

// do these function even error out in any case other than invalid parameters?! I thought all windows had rects
void uiWindowsEnsureGetClientRect(HWND hwnd, RECT *r)
{
	if (GetClientRect(hwnd, r) == 0) {
		logLastError(L"error getting window client rect");
		// zero out the rect on error just to be safe
		r->left = 0;
		r->top = 0;
		r->right = 0;
		r->bottom = 0;
	}
}

void uiWindowsEnsureGetWindowRect(HWND hwnd, RECT *r)
{
	if (GetWindowRect(hwnd, r) == 0) {
		logLastError(L"error getting window rect");
		// zero out the rect on error just to be safe
		r->left = 0;
		r->top = 0;
		r->right = 0;
		r->bottom = 0;
	}
}
