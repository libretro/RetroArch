// 16 may 2015
#include "uipriv_windows.hpp"

// You don't add controls directly to a tab control on Windows; instead you make them siblings and swap between them on a TCN_SELCHANGING/TCN_SELCHANGE notification pair.
// In addition, you use dialogs because they can be textured properly; other controls cannot. (Things will look wrong if the tab background in the current theme is fancy if you just use the tab background by itself; see http://stackoverflow.com/questions/30087540/why-are-my-programss-tab-controls-rendering-their-background-in-a-blocky-way-b.)

struct uiTab {
	uiWindowsControl c;
	HWND hwnd;			// of the outer container
	HWND tabHWND;		// of the tab control itself
	std::vector<struct tabPage *> *pages;
	HWND parent;
};

// utility functions

static LRESULT curpage(uiTab *t)
{
	return SendMessageW(t->tabHWND, TCM_GETCURSEL, 0, 0);
}

static struct tabPage *tabPage(uiTab *t, int i)
{
	return (*(t->pages))[i];
}

static void tabPageRect(uiTab *t, RECT *r)
{
	// this rect needs to be in parent window coordinates, but TCM_ADJUSTRECT wants a window rect, which is screen coordinates
	// because we have each page as a sibling of the tab, use the tab's own rect as the input rect
	uiWindowsEnsureGetWindowRect(t->tabHWND, r);
	SendMessageW(t->tabHWND, TCM_ADJUSTRECT, (WPARAM) FALSE, (LPARAM) r);
	// and get it in terms of the container instead of the screen
	mapWindowRect(NULL, t->hwnd, r);
}

static void tabRelayout(uiTab *t)
{
	struct tabPage *page;
	RECT r;

	// first move the tab control itself
	uiWindowsEnsureGetClientRect(t->hwnd, &r);
	uiWindowsEnsureMoveWindowDuringResize(t->tabHWND, r.left, r.top, r.right - r.left, r.bottom - r.top);

	// then the current page
	if (t->pages->size() == 0)
		return;
	page = tabPage(t, curpage(t));
	tabPageRect(t, &r);
	uiWindowsEnsureMoveWindowDuringResize(page->hwnd, r.left, r.top, r.right - r.left, r.bottom - r.top);
}

static void showHidePage(uiTab *t, LRESULT which, int hide)
{
	struct tabPage *page;

	if (which == (LRESULT) (-1))
		return;
	page = tabPage(t, which);
	if (hide)
		ShowWindow(page->hwnd, SW_HIDE);
	else {
		ShowWindow(page->hwnd, SW_SHOW);
		// we only resize the current page, so we have to resize it; before we can do that, we need to make sure we are of the right size
		uiWindowsControlMinimumSizeChanged(uiWindowsControl(t));
	}
}

// control implementation

static BOOL onWM_NOTIFY(uiControl *c, HWND hwnd, NMHDR *nm, LRESULT *lResult)
{
	uiTab *t = uiTab(c);

	if (nm->code != TCN_SELCHANGING && nm->code != TCN_SELCHANGE)
		return FALSE;
	showHidePage(t, curpage(t), nm->code == TCN_SELCHANGING);
	*lResult = 0;
	if (nm->code == TCN_SELCHANGING)
		*lResult = FALSE;
	return TRUE;
}

static void uiTabDestroy(uiControl *c)
{
	uiTab *t = uiTab(c);
	uiControl *child;

	for (struct tabPage *&page : *(t->pages)) {
		child = page->child;
		tabPageDestroy(page);
		if (child != NULL) {
			uiControlSetParent(child, NULL);
			uiControlDestroy(child);
		}
	}
	delete t->pages;
	uiWindowsUnregisterWM_NOTIFYHandler(t->tabHWND);
	uiWindowsEnsureDestroyWindow(t->tabHWND);
	uiWindowsEnsureDestroyWindow(t->hwnd);
	uiFreeControl(uiControl(t));
}

uiWindowsControlDefaultHandle(uiTab)
uiWindowsControlDefaultParent(uiTab)
uiWindowsControlDefaultSetParent(uiTab)
uiWindowsControlDefaultToplevel(uiTab)
uiWindowsControlDefaultVisible(uiTab)
uiWindowsControlDefaultShow(uiTab)
uiWindowsControlDefaultHide(uiTab)
uiWindowsControlDefaultEnabled(uiTab)
uiWindowsControlDefaultEnable(uiTab)
uiWindowsControlDefaultDisable(uiTab)

static void uiTabSyncEnableState(uiWindowsControl *c, int enabled)
{
	uiTab *t = uiTab(c);

	if (uiWindowsShouldStopSyncEnableState(uiWindowsControl(t), enabled))
		return;
	EnableWindow(t->tabHWND, enabled);
	for (struct tabPage *&page : *(t->pages))
		if (page->child != NULL)
			uiWindowsControlSyncEnableState(uiWindowsControl(page->child), enabled);
}

uiWindowsControlDefaultSetParentHWND(uiTab)

static void uiTabMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiTab *t = uiTab(c);
	int pagewid, pageht;
	struct tabPage *page;
	RECT r;

	// only consider the current page
	pagewid = 0;
	pageht = 0;
	if (t->pages->size() != 0) {
		page = tabPage(t, curpage(t));
		tabPageMinimumSize(page, &pagewid, &pageht);
	}

	r.left = 0;
	r.top = 0;
	r.right = pagewid;
	r.bottom = pageht;
	// this also includes the tabs themselves
	SendMessageW(t->tabHWND, TCM_ADJUSTRECT, (WPARAM) TRUE, (LPARAM) (&r));
	*width = r.right - r.left;
	*height = r.bottom - r.top;
}

static void uiTabMinimumSizeChanged(uiWindowsControl *c)
{
	uiTab *t = uiTab(c);

	if (uiWindowsControlTooSmall(uiWindowsControl(t))) {
		uiWindowsControlContinueMinimumSizeChanged(uiWindowsControl(t));
		return;
	}
	tabRelayout(t);
}

uiWindowsControlDefaultLayoutRect(uiTab)
uiWindowsControlDefaultAssignControlIDZOrder(uiTab)

static void uiTabChildVisibilityChanged(uiWindowsControl *c)
{
	// TODO eliminate the redundancy
	uiWindowsControlMinimumSizeChanged(c);
}

static void tabArrangePages(uiTab *t)
{
	LONG_PTR controlID = 100;
	HWND insertAfter = NULL;

	// TODO is this first or last?
	uiWindowsEnsureAssignControlIDZOrder(t->tabHWND, &controlID, &insertAfter);
	for (struct tabPage *&page : *(t->pages))
		uiWindowsEnsureAssignControlIDZOrder(page->hwnd, &controlID, &insertAfter);
}

void uiTabAppend(uiTab *t, const char *name, uiControl *child)
{
	uiTabInsertAt(t, name, t->pages->size(), child);
}

void uiTabInsertAt(uiTab *t, const char *name, int n, uiControl *child)
{
	struct tabPage *page;
	LRESULT hide, show;
	TCITEMW item;
	WCHAR *wname;

	// see below
	hide = curpage(t);

	if (child != NULL)
		uiControlSetParent(child, uiControl(t));

	page = newTabPage(child);
	uiWindowsEnsureSetParentHWND(page->hwnd, t->hwnd);
	t->pages->insert(t->pages->begin() + n, page);
	tabArrangePages(t);

	ZeroMemory(&item, sizeof (TCITEMW));
	item.mask = TCIF_TEXT;
	wname = toUTF16(name);
	item.pszText = wname;
	if (SendMessageW(t->tabHWND, TCM_INSERTITEM, (WPARAM) n, (LPARAM) (&item)) == (LRESULT) -1)
		logLastError(L"error adding tab to uiTab");
	uiFree(wname);

	// we need to do this because adding the first tab doesn't send a TCN_SELCHANGE; it just shows the page
	show = curpage(t);
	if (show != hide) {
		showHidePage(t, hide, 1);
		showHidePage(t, show, 0);
	}
}

void uiTabDelete(uiTab *t, int n)
{
	struct tabPage *page;

	// first delete the tab from the tab control
	// if this is the current tab, no tab will be selected, which is good
	if (SendMessageW(t->tabHWND, TCM_DELETEITEM, (WPARAM) n, 0) == FALSE)
		logLastError(L"error deleting uiTab tab");

	// now delete the page itself
	page = tabPage(t, n);
	if (page->child != NULL)
		uiControlSetParent(page->child, NULL);
	tabPageDestroy(page);
	t->pages->erase(t->pages->begin() + n);
}

int uiTabNumPages(uiTab *t)
{
	return t->pages->size();
}

int uiTabMargined(uiTab *t, int n)
{
	return tabPage(t, n)->margined;
}

void uiTabSetMargined(uiTab *t, int n, int margined)
{
	struct tabPage *page;

	page = tabPage(t, n);
	page->margined = margined;
	// even if the page doesn't have a child it might still have a new minimum size with margins; this is the easiest way to verify it
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(t));
}

static void onResize(uiWindowsControl *c)
{
	tabRelayout(uiTab(c));
}

uiTab *uiNewTab(void)
{
	uiTab *t;

	uiWindowsNewControl(uiTab, t);

	t->hwnd = uiWindowsMakeContainer(uiWindowsControl(t), onResize);

	t->tabHWND = uiWindowsEnsureCreateControlHWND(0,
		WC_TABCONTROLW, L"",
		TCS_TOOLTIPS | WS_TABSTOP,
		hInstance, NULL,
		TRUE);
	uiWindowsEnsureSetParentHWND(t->tabHWND, t->hwnd);

	uiWindowsRegisterWM_NOTIFYHandler(t->tabHWND, onWM_NOTIFY, uiControl(t));

	t->pages = new std::vector<struct tabPage *>;

	return t;
}
