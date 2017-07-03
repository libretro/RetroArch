// 9 april 2015
#include "uipriv_windows.hpp"

WCHAR *windowTextAndLen(HWND hwnd, LRESULT *len)
{
	LRESULT n;
	WCHAR *text;

	n = SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
	if (len != NULL)
		*len = n;
	// WM_GETTEXTLENGTH does not include the null terminator
	text = (WCHAR *) uiAlloc((n + 1) * sizeof (WCHAR), "WCHAR[]");
	// note the comparison: the size includes the null terminator, but the return does not
	if (GetWindowTextW(hwnd, text, n + 1) != n) {
		logLastError(L"error getting window text");
		// on error, return an empty string to be safe
		*text = L'\0';
		if (len != NULL)
			*len = 0;
	}
	return text;
}

WCHAR *windowText(HWND hwnd)
{
	return windowTextAndLen(hwnd, NULL);
}

void setWindowText(HWND hwnd, WCHAR *wtext)
{
	if (SetWindowTextW(hwnd, wtext) == 0)
		logLastError(L"error setting window text");
}

void uiFreeText(char *text)
{
	uiFree(text);
}

int uiWindowsWindowTextWidth(HWND hwnd)
{
	LRESULT len;
	WCHAR *text;
	HDC dc;
	HFONT prevfont;
	SIZE size;

	size.cx = 0;
	size.cy = 0;

	text = windowTextAndLen(hwnd, &len);
	if (len == 0)		// no text; nothing to do
		goto noTextOrError;

	// now we can do the calculations
	dc = GetDC(hwnd);
	if (dc == NULL) {
		logLastError(L"error getting DC");
		// on any error, assume no text
		goto noTextOrError;
	}
	prevfont = (HFONT) SelectObject(dc, hMessageFont);
	if (prevfont == NULL) {
		logLastError(L"error loading control font into device context");
		ReleaseDC(hwnd, dc);
		goto noTextOrError;
	}
	if (GetTextExtentPoint32W(dc, text, len, &size) == 0) {
		logLastError(L"error getting text extent point");
		// continue anyway, assuming size is 0
		size.cx = 0;
		size.cy = 0;
	}
	// continue on errors; we got what we want
	if (SelectObject(dc, prevfont) != hMessageFont)
		logLastError(L"error restoring previous font into device context");
	if (ReleaseDC(hwnd, dc) == 0)
		logLastError(L"error releasing DC");

	uiFree(text);
	return size.cx;

noTextOrError:
	uiFree(text);
	return 0;
}

char *uiWindowsWindowText(HWND hwnd)
{
	WCHAR *wtext;
	char *text;

	wtext = windowText(hwnd);
	text = toUTF8(wtext);
	uiFree(wtext);
	return text;
}

void uiWindowsSetWindowText(HWND hwnd, const char *text)
{
	WCHAR *wtext;

	wtext = toUTF16(text);
	setWindowText(hwnd, wtext);
	uiFree(wtext);
}
