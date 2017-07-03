// 14 may 2015
#include "uipriv_windows.hpp"

// TODO rework the error handling
void getSizing(HWND hwnd, uiWindowsSizing *sizing, HFONT font)
{
	HDC dc;
	HFONT prevfont;
	TEXTMETRICW tm;
	SIZE size;

	dc = GetDC(hwnd);
	if (dc == NULL)
		logLastError(L"error getting DC");
	prevfont = (HFONT) SelectObject(dc, font);
	if (prevfont == NULL)
		logLastError(L"error loading control font into device context");

	ZeroMemory(&tm, sizeof (TEXTMETRICW));
	if (GetTextMetricsW(dc, &tm) == 0)
		logLastError(L"error getting text metrics");
	if (GetTextExtentPoint32W(dc, L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &size) == 0)
		logLastError(L"error getting text extent point");

	sizing->BaseX = (int) ((size.cx / 26 + 1) / 2);
	sizing->BaseY = (int) tm.tmHeight;
	sizing->InternalLeading = tm.tmInternalLeading;

	if (SelectObject(dc, prevfont) != font)
		logLastError(L"error restoring previous font into device context");
	if (ReleaseDC(hwnd, dc) == 0)
		logLastError(L"error releasing DC");
}

void uiWindowsGetSizing(HWND hwnd, uiWindowsSizing *sizing)
{
	return getSizing(hwnd, sizing, hMessageFont);
}

#define dlgUnitsToX(dlg, baseX) MulDiv((dlg), (baseX), 4)
#define dlgUnitsToY(dlg, baseY) MulDiv((dlg), (baseY), 8)

void uiWindowsSizingDlgUnitsToPixels(uiWindowsSizing *sizing, int *x, int *y)
{
	if (x != NULL)
		*x = dlgUnitsToX(*x, sizing->BaseX);
	if (y != NULL)
		*y = dlgUnitsToY(*y, sizing->BaseY);
}

// from https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing and https://msdn.microsoft.com/en-us/library/windows/desktop/bb226818%28v=vs.85%29.aspx
// this X value is really only for buttons but I don't see a better one :/
#define winXPadding 4
#define winYPadding 4

void uiWindowsSizingStandardPadding(uiWindowsSizing *sizing, int *x, int *y)
{
	if (x != NULL)
		*x = dlgUnitsToX(winXPadding, sizing->BaseX);
	if (y != NULL)
		*y = dlgUnitsToY(winYPadding, sizing->BaseY);
}
