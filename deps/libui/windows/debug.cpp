// 25 february 2015
#include "uipriv_windows.hpp"

// LONGTERM disable logging and stopping on no-debug builds

static void printDebug(const WCHAR *msg)
{
	OutputDebugStringW(msg);
}

HRESULT _logLastError(debugargs, const WCHAR *s)
{
	DWORD le;
	WCHAR *msg;
	WCHAR *formatted;
	BOOL useFormatted;

	le = GetLastError();

	useFormatted = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, le, 0, (LPWSTR) (&formatted), 0, NULL) != 0;
	if (!useFormatted)
		formatted = L"\n";
	msg = strf(L"[libui] %s:%s:%s() %s: GetLastError() == %I32u %s",
		file, line, func,
		s, le, formatted);
	if (useFormatted)
		LocalFree(formatted);		// ignore error
	printDebug(msg);
	uiFree(msg);
	DebugBreak();

	SetLastError(le);
	// a function does not have to set a last error
	// if the last error we get is actually 0, then HRESULT_FROM_WIN32(0) will return S_OK (0 cast to an HRESULT, since 0 <= 0), which we don't want
	// prevent this by returning E_FAIL
	if (le == 0)
		return E_FAIL;
	return HRESULT_FROM_WIN32(le);
}

HRESULT _logHRESULT(debugargs, const WCHAR *s, HRESULT hr)
{
	WCHAR *msg;
	WCHAR *formatted;
	BOOL useFormatted;

	useFormatted = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, hr, 0, (LPWSTR) (&formatted), 0, NULL) != 0;
	if (!useFormatted)
		formatted = L"\n";
	msg = strf(L"[libui] %s:%s:%s() %s: HRESULT == 0x%08I32X %s",
		file, line, func,
		s, hr, formatted);
	if (useFormatted)
		LocalFree(formatted);		// ignore error
	printDebug(msg);
	uiFree(msg);
	DebugBreak();

	return hr;
}

void realbug(const char *file, const char *line, const char *func, const char *prefix, const char *format, va_list ap)
{
	va_list ap2;
	char *msg;
	size_t n;
	WCHAR *final;

	va_copy(ap2, ap);
	n = _vscprintf(format, ap2);
	va_end(ap2);
	n++;		// terminating '\0'

	msg = (char *) uiAlloc(n * sizeof (char), "char[]");
	// includes terminating '\0' according to example in https://msdn.microsoft.com/en-us/library/xa1a1a6z.aspx
	vsprintf_s(msg, n, format, ap);

	final = strf(L"[libui] %hs:%hs:%hs() %hs%hs\n", file, line, func, prefix, msg);
	uiFree(msg);
	printDebug(final);
	uiFree(final);

	DebugBreak();
}
