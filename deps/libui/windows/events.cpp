// 20 may 2015
#include "uipriv_windows.hpp"

struct handler {
	BOOL (*commandHandler)(uiControl *, HWND, WORD, LRESULT *);
	BOOL (*notifyHandler)(uiControl *, HWND, NMHDR *, LRESULT *);
	BOOL (*hscrollHandler)(uiControl *, HWND, WORD, LRESULT *);
	uiControl *c;

	// just to ensure handlers[new HWND] initializes properly
	// TODO gcc can't handle a struct keyword here? or is that a MSVC extension?
	handler()
	{
		this->commandHandler = NULL;
		this->notifyHandler = NULL;
		this->hscrollHandler = NULL;
		this->c = NULL;
	}
};

static std::map<HWND, struct handler> handlers;

void uiWindowsRegisterWM_COMMANDHandler(HWND hwnd, BOOL (*handler)(uiControl *, HWND, WORD, LRESULT *), uiControl *c)
{
	if (handlers[hwnd].commandHandler != NULL)
		implbug("already registered a WM_COMMAND handler to window handle %p", hwnd);
	handlers[hwnd].commandHandler = handler;
	handlers[hwnd].c = c;
}

void uiWindowsRegisterWM_NOTIFYHandler(HWND hwnd, BOOL (*handler)(uiControl *, HWND, NMHDR *, LRESULT *), uiControl *c)
{
	if (handlers[hwnd].notifyHandler != NULL)
		implbug("already registered a WM_NOTIFY handler to window handle %p", hwnd);
	handlers[hwnd].notifyHandler = handler;
	handlers[hwnd].c = c;
}

void uiWindowsRegisterWM_HSCROLLHandler(HWND hwnd, BOOL (*handler)(uiControl *, HWND, WORD, LRESULT *), uiControl *c)
{
	if (handlers[hwnd].hscrollHandler != NULL)
		implbug("already registered a WM_HSCROLL handler to window handle %p", hwnd);
	handlers[hwnd].hscrollHandler = handler;
	handlers[hwnd].c = c;
}

void uiWindowsUnregisterWM_COMMANDHandler(HWND hwnd)
{
	if (handlers[hwnd].commandHandler == NULL)
		implbug("window handle %p not registered to receive WM_COMMAND events", hwnd);
	handlers[hwnd].commandHandler = NULL;
}

void uiWindowsUnregisterWM_NOTIFYHandler(HWND hwnd)
{
	if (handlers[hwnd].notifyHandler == NULL)
		implbug("window handle %p not registered to receive WM_NOTIFY events", hwnd);
	handlers[hwnd].notifyHandler = NULL;
}

void uiWindowsUnregisterWM_HSCROLLHandler(HWND hwnd)
{
	if (handlers[hwnd].hscrollHandler == NULL)
		implbug("window handle %p not registered to receive WM_HSCROLL events", hwnd);
	handlers[hwnd].hscrollHandler = NULL;
}

template<typename T>
static BOOL shouldRun(HWND hwnd, T method)
{
	// not from a window
	if (hwnd == NULL)
		return FALSE;
	// don't bounce back if to the utility window, in which case act as if the message was ignored
	if (IsChild(utilWindow, hwnd) != 0)
		return FALSE;
	// registered?
	return method != NULL;
}

BOOL runWM_COMMAND(WPARAM wParam, LPARAM lParam, LRESULT *lResult)
{
	HWND hwnd;
	WORD arg3;
	BOOL (*handler)(uiControl *, HWND, WORD, LRESULT *);
	uiControl *c;

	hwnd = (HWND) lParam;
	arg3 = HIWORD(wParam);
	handler = handlers[hwnd].commandHandler;
	c = handlers[hwnd].c;
	if (shouldRun(hwnd, handler))
		return (*handler)(c, hwnd, arg3, lResult);
	return FALSE;
}

BOOL runWM_NOTIFY(WPARAM wParam, LPARAM lParam, LRESULT *lResult)
{
	HWND hwnd;
	NMHDR *arg3;
	BOOL (*handler)(uiControl *, HWND, NMHDR *, LRESULT *);
	uiControl *c;

	arg3 = (NMHDR *) lParam;
	hwnd = arg3->hwndFrom;
	handler = handlers[hwnd].notifyHandler;
	c = handlers[hwnd].c;
	if (shouldRun(hwnd, handler))
		return (*handler)(c, hwnd, arg3, lResult);
	return FALSE;
}

BOOL runWM_HSCROLL(WPARAM wParam, LPARAM lParam, LRESULT *lResult)
{
	HWND hwnd;
	WORD arg3;
	BOOL (*handler)(uiControl *, HWND, WORD, LRESULT *);
	uiControl *c;

	hwnd = (HWND) lParam;
	arg3 = LOWORD(wParam);
	handler = handlers[hwnd].hscrollHandler;
	c = handlers[hwnd].c;
	if (shouldRun(hwnd, handler))
		return (*handler)(c, hwnd, arg3, lResult);
	return FALSE;
}

static std::map<HWND, bool> wininichanges;

void uiWindowsRegisterReceiveWM_WININICHANGE(HWND hwnd)
{
	if (wininichanges[hwnd])
		implbug("window handle %p already subscribed to receive WM_WINICHANGEs", hwnd);
	wininichanges[hwnd] = true;
}

void uiWindowsUnregisterReceiveWM_WININICHANGE(HWND hwnd)
{
	if (!wininichanges[hwnd])
		implbug("window handle %p not registered to receive WM_WININICHANGEs", hwnd);
	wininichanges[hwnd] = false;
}

void issueWM_WININICHANGE(WPARAM wParam, LPARAM lParam)
{
	struct wininichange *ch;

	for (const auto &iter : wininichanges)
		SendMessageW(iter.first, WM_WININICHANGE, wParam, lParam);
}
