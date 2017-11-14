// 26 april 2015
#include "uipriv_windows.hpp"

// This contains code used by all uiControls that contain other controls.
// It also contains the code to draw the background of a container.c container, as that is a variant of the WM_CTLCOLORxxx handler code.

static HBRUSH parentBrush = NULL;

static HWND parentWithBackground(HWND hwnd)
{
	HWND parent;
	int cls;

	parent = hwnd;
	for (;;) {
		parent = parentOf(parent);
		// skip groupboxes; they're (supposed to be) transparent
		// skip uiContainers; they don't draw anything
		cls = windowClassOf(parent, L"button", containerClass, NULL);
		if (cls != 0 && cls != 1)
			break;
	}
	return parent;
}

struct parentDraw {
	HDC cdc;
	HBITMAP bitmap;
	HBITMAP prevbitmap;
};

static HRESULT parentDraw(HDC dc, HWND parent, struct parentDraw *pd)
{
	RECT r;

	uiWindowsEnsureGetClientRect(parent, &r);
	pd->cdc = CreateCompatibleDC(dc);
	if (pd->cdc == NULL)
		return logLastError(L"error creating compatible DC");
	pd->bitmap = CreateCompatibleBitmap(dc, r.right - r.left, r.bottom - r.top);
	if (pd->bitmap == NULL)
		return logLastError(L"error creating compatible bitmap");
	pd->prevbitmap = (HBITMAP) SelectObject(pd->cdc, pd->bitmap);
	if (pd->prevbitmap == NULL)
		return logLastError(L"error selecting bitmap into compatible DC");
	SendMessageW(parent, WM_PRINTCLIENT, (WPARAM) (pd->cdc), PRF_CLIENT);
	return S_OK;
}

static void endParentDraw(struct parentDraw *pd)
{
	// continue in case of any error
	if (pd->prevbitmap != NULL)
		if (((HBITMAP) SelectObject(pd->cdc, pd->prevbitmap)) != pd->bitmap)
			logLastError(L"error selecting previous bitmap back into compatible DC");
	if (pd->bitmap != NULL)
		if (DeleteObject(pd->bitmap) == 0)
			logLastError(L"error deleting compatible bitmap");
	if (pd->cdc != NULL)
		if (DeleteDC(pd->cdc) == 0)
			logLastError(L"error deleting compatible DC");
}

// see http://www.codeproject.com/Articles/5978/Correctly-drawn-themed-dialogs-in-WinXP
static HBRUSH getControlBackgroundBrush(HWND hwnd, HDC dc)
{
	HWND parent;
	RECT hwndScreenRect;
	struct parentDraw pd;
	HBRUSH brush;
	HRESULT hr;

	parent = parentWithBackground(hwnd);

	hr = parentDraw(dc, parent, &pd);
	if (hr != S_OK)
		return NULL;
	brush = CreatePatternBrush(pd.bitmap);
	if (brush == NULL) {
		logLastError(L"error creating pattern brush");
		endParentDraw(&pd);
		return NULL;
	}
	endParentDraw(&pd);

	// now figure out where the control is relative to the parent so we can align the brush properly
	// if anything fails, give up and return the brush as-is
	uiWindowsEnsureGetWindowRect(hwnd, &hwndScreenRect);
	// this will be in screen coordinates; convert to parent coordinates
	mapWindowRect(NULL, parent, &hwndScreenRect);
	if (SetBrushOrgEx(dc, -hwndScreenRect.left, -hwndScreenRect.top, NULL) == 0)
		logLastError(L"error setting brush origin");

	return brush;
}

void paintContainerBackground(HWND hwnd, HDC dc, RECT *paintRect)
{
	HWND parent;
	RECT paintRectParent;
	struct parentDraw pd;
	HRESULT hr;

	parent = parentWithBackground(hwnd);
	hr = parentDraw(dc, parent, &pd);
	if (hr != S_OK)		// we couldn't get it; draw nothing
		return;

	paintRectParent = *paintRect;
	mapWindowRect(hwnd, parent, &paintRectParent);
	if (BitBlt(dc, paintRect->left, paintRect->top, paintRect->right - paintRect->left, paintRect->bottom - paintRect->top,
		pd.cdc, paintRectParent.left, paintRectParent.top,
		SRCCOPY) == 0)
		logLastError(L"error drawing parent background over uiContainer");

	endParentDraw(&pd);
}

// TODO make this public if we want custom containers
// why have this to begin with? http://blogs.msdn.com/b/oldnewthing/archive/2010/03/16/9979112.aspx
BOOL handleParentMessages(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *lResult)
{
	switch (uMsg) {
	case WM_COMMAND:
		return runWM_COMMAND(wParam, lParam, lResult);
	case WM_NOTIFY:
		return runWM_NOTIFY(wParam, lParam, lResult);
	case WM_HSCROLL:
		return runWM_HSCROLL(wParam, lParam, lResult);
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
		if (parentBrush != NULL)
			if (DeleteObject(parentBrush) == 0)
				logLastError(L"error deleting old background brush()");		// but continue anyway; we will leak a brush but whatever
		if (SetBkMode((HDC) wParam, TRANSPARENT) == 0)
			logLastError(L"error setting transparent background mode to controls");		// but continue anyway; text will be wrong
		parentBrush = getControlBackgroundBrush((HWND) lParam, (HDC) wParam);
		if (parentBrush == NULL)		// failed; just do default behavior
			return FALSE;
		*lResult = (LRESULT) parentBrush;
		return TRUE;
	}
	return FALSE;
}
