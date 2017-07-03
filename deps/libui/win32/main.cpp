// 6 april 2015
#include "uipriv_windows.hpp"

static HHOOK filter;

static LRESULT CALLBACK filterProc(int code, WPARAM wParam, LPARAM lParam)
{
	MSG *msg = (MSG *) lParam;

	if (code < 0)
		goto callNext;

	if (areaFilter(msg))		// don't continue to our IsDialogMessage() hack if the area handled it
		goto discard;

	// TODO IsDialogMessage() hack here

	// otherwise keep going
	goto callNext;

discard:
	// we handled it; discard the message so the dialog manager doesn't see it
	return 1;

callNext:
	return CallNextHookEx(filter, code, wParam, lParam);
}

int registerMessageFilter(void)
{
	filter = SetWindowsHookExW(WH_MSGFILTER,
		filterProc,
		hInstance,
		GetCurrentThreadId());
	return filter != NULL;
}

void unregisterMessageFilter(void)
{
	if (UnhookWindowsHookEx(filter) == 0)
		logLastError(L"error unregistering libui message filter");
}

// LONGTERM http://blogs.msdn.com/b/oldnewthing/archive/2005/04/08/406509.aspx when adding accelerators, TranslateAccelerators() before IsDialogMessage()

static void processMessage(MSG *msg)
{
	HWND correctParent;

	if (msg->hwnd != NULL)
		correctParent = parentToplevel(msg->hwnd);
	else		// just to be safe
		correctParent = GetActiveWindow();
	if (correctParent != NULL)
		// this calls our mesage filter above for us
		if (IsDialogMessage(correctParent, msg) != 0)
			return;
	TranslateMessage(msg);
	DispatchMessageW(msg);
}

static int waitMessage(MSG *msg)
{
	int res;

	res = GetMessageW(msg, NULL, 0, 0);
	if (res < 0) {
		logLastError(L"error calling GetMessage()");
		return 0;		// bail out on error
	}
	return res != 0;		// returns false on WM_QUIT
}

void uiMain(void)
{
	while (uiMainStep(1))
		;
}

void uiMainSteps(void)
{
	// don't need to do anything here
}

static int peekMessage(MSG *msg)
{
	BOOL res;

	res = PeekMessageW(msg, NULL, 0, 0, PM_REMOVE);
	if (res == 0)
		return 2;		// no message available
	if (msg->message != WM_QUIT)
		return 1;		// a message
	return 0;			// WM_QUIT
}

int uiMainStep(int wait)
{
	MSG msg;

	if (wait) {
		if (!waitMessage(&msg))
			return 0;
		processMessage(&msg);
		return 1;
	}

	// don't wait for a message
	switch (peekMessage(&msg)) {
	case 0:		// quit
		// TODO PostQuitMessage() again?
		return 0;
	case 1:		// process a message
		processMessage(&msg);
		// fall out to the case for no message
	}
	return 1;		// no message
}

void uiQuit(void)
{
	PostQuitMessage(0);
}

void uiQueueMain(void (*f)(void *data), void *data)
{
	if (PostMessageW(utilWindow, msgQueued, (WPARAM) f, (LPARAM) data) == 0)
		// LONGTERM this is likely not safe to call across threads (allocates memory)
		logLastError(L"error queueing function to run on main thread");
}
