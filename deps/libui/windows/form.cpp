// 8 june 2016
#include "uipriv_windows.hpp"

struct formChild {
	uiControl *c;
	HWND label;
	int stretchy;
	int height;
};

struct uiForm {
	uiWindowsControl c;
	HWND hwnd;
	std::vector<struct formChild> *controls;
	int padded;
};

static void formPadding(uiForm *f, int *xpadding, int *ypadding)
{
	uiWindowsSizing sizing;

	*xpadding = 0;
	*ypadding = 0;
	if (f->padded) {
		uiWindowsGetSizing(f->hwnd, &sizing);
		uiWindowsSizingStandardPadding(&sizing, xpadding, ypadding);
	}
}

// via http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define labelHeight 8
#define labelYOffset 3

static void formRelayout(uiForm *f)
{
	RECT r;
	int x, y, width, height;
	int xpadding, ypadding;
	int nStretchy;
	int labelwid, stretchyht;
	int thiswid;
	int i;
	int minimumWidth, minimumHeight;
	uiWindowsSizing sizing;
	int labelht, labelyoff;
	int nVisible;

	if (f->controls->size() == 0)
		return;

	uiWindowsEnsureGetClientRect(f->hwnd, &r);
	x = r.left;
	y = r.top;
	width = r.right - r.left;
	height = r.bottom - r.top;

	// 0) get this Form's padding
	formPadding(f, &xpadding, &ypadding);

	// 1) get width of labels and height of non-stretchy controls
	// this will tell us how much space will be left for controls
	labelwid = 0;
	stretchyht = height;
	nStretchy = 0;
	nVisible = 0;
	for (struct formChild &fc : *(f->controls)) {
		if (!uiControlVisible(fc.c)) {
			ShowWindow(fc.label, SW_HIDE);
			continue;
		}
		ShowWindow(fc.label, SW_SHOW);
		nVisible++;
		thiswid = uiWindowsWindowTextWidth(fc.label);
		if (labelwid < thiswid)
			labelwid = thiswid;
		if (fc.stretchy) {
			nStretchy++;
			continue;
		}
		uiWindowsControlMinimumSize(uiWindowsControl(fc.c), &minimumWidth, &minimumHeight);
		fc.height = minimumHeight;
		stretchyht -= minimumHeight;
	}
	if (nVisible == 0)		// nothing to do
		return;

	// 2) inset the available rect by the needed padding
	width -= xpadding;
	height -= (nVisible - 1) * ypadding;
	stretchyht -= (nVisible - 1) * ypadding;

	// 3) now get the width of controls and the height of stretchy controls
	width -= labelwid;
	if (nStretchy != 0) {
		stretchyht /= nStretchy;
		for (struct formChild &fc : *(f->controls)) {
			if (!uiControlVisible(fc.c))
				continue;
			if (fc.stretchy)
				fc.height = stretchyht;
		}
	}

	// 4) get the y offset
	labelyoff = labelYOffset;
	uiWindowsGetSizing(f->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, NULL, &labelyoff);

	// 5) now we can position controls
	// first, make relative to the top-left corner of the container
	// also prefer left alignment on Windows
	x = labelwid + xpadding;
	y = 0;
	for (const struct formChild &fc : *(f->controls)) {
		if (!uiControlVisible(fc.c))
			continue;
		labelht = labelHeight;
		uiWindowsGetSizing(f->hwnd, &sizing);
		uiWindowsSizingDlgUnitsToPixels(&sizing, NULL, &labelht);
		uiWindowsEnsureMoveWindowDuringResize(fc.label, 0, y + labelyoff - sizing.InternalLeading, labelwid, labelht);
		uiWindowsEnsureMoveWindowDuringResize((HWND) uiControlHandle(fc.c), x, y, width, fc.height);
		y += fc.height + ypadding;
	}
}

static void uiFormDestroy(uiControl *c)
{
	uiForm *f = uiForm(c);

	for (const struct formChild &fc : *(f->controls)) {
		uiControlSetParent(fc.c, NULL);
		uiControlDestroy(fc.c);
		uiWindowsEnsureDestroyWindow(fc.label);
	}
	delete f->controls;
	uiWindowsEnsureDestroyWindow(f->hwnd);
	uiFreeControl(uiControl(f));
}

uiWindowsControlDefaultHandle(uiForm)
uiWindowsControlDefaultParent(uiForm)
uiWindowsControlDefaultSetParent(uiForm)
uiWindowsControlDefaultToplevel(uiForm)
uiWindowsControlDefaultVisible(uiForm)
uiWindowsControlDefaultShow(uiForm)
uiWindowsControlDefaultHide(uiForm)
uiWindowsControlDefaultEnabled(uiForm)
uiWindowsControlDefaultEnable(uiForm)
uiWindowsControlDefaultDisable(uiForm)

static void uiFormSyncEnableState(uiWindowsControl *c, int enabled)
{
	uiForm *f = uiForm(c);

	if (uiWindowsShouldStopSyncEnableState(uiWindowsControl(f), enabled))
		return;
	for (const struct formChild &fc : *(f->controls))
		uiWindowsControlSyncEnableState(uiWindowsControl(fc.c), enabled);
}

uiWindowsControlDefaultSetParentHWND(uiForm)

static void uiFormMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiForm *f = uiForm(c);
	int xpadding, ypadding;
	int nStretchy;
	// these two contain the largest minimum width and height of all stretchy controls in the form
	// all stretchy controls will use this value to determine the final minimum size
	int maxLabelWidth, maxControlWidth;
	int maxStretchyHeight;
	int labelwid;
	int i;
	int minimumWidth, minimumHeight;
	int nVisible;
	uiWindowsSizing sizing;

	*width = 0;
	*height = 0;
	if (f->controls->size() == 0)
		return;

	// 0) get this Form's padding
	formPadding(f, &xpadding, &ypadding);

	// 1) determine the longest width of all controls and labels; add in the height of non-stretchy controls and get (but not add in) the largest heights of stretchy controls
	// we still add in like direction of stretchy controls
	nStretchy = 0;
	maxLabelWidth = 0;
	maxControlWidth = 0;
	maxStretchyHeight = 0;
	nVisible = 0;
	for (const struct formChild &fc : *(f->controls)) {
		if (!uiControlVisible(fc.c))
			continue;
		nVisible++;
		labelwid = uiWindowsWindowTextWidth(fc.label);
		if (maxLabelWidth < labelwid)
			maxLabelWidth = labelwid;
		uiWindowsControlMinimumSize(uiWindowsControl(fc.c), &minimumWidth, &minimumHeight);
		if (fc.stretchy) {
			nStretchy++;
			if (maxStretchyHeight < minimumHeight)
				maxStretchyHeight = minimumHeight;
		}
		if (maxControlWidth < minimumWidth)
			maxControlWidth = minimumWidth;
		if (!fc.stretchy)
			*height += minimumHeight;
	}
	if (nVisible == 0)		// nothing to show; return 0x0
		return;
	*width += maxLabelWidth + maxControlWidth;

	// 2) outset the desired rect with the needed padding
	*width += xpadding;
	*height += (nVisible - 1) * ypadding;

	// 3) and now we can add in stretchy controls
	*height += nStretchy * maxStretchyHeight;
}

static void uiFormMinimumSizeChanged(uiWindowsControl *c)
{
	uiForm *f = uiForm(c);

	if (uiWindowsControlTooSmall(uiWindowsControl(f))) {
		uiWindowsControlContinueMinimumSizeChanged(uiWindowsControl(f));
		return;
	}
	formRelayout(f);
}

uiWindowsControlDefaultLayoutRect(uiForm)
uiWindowsControlDefaultAssignControlIDZOrder(uiForm)

static void uiFormChildVisibilityChanged(uiWindowsControl *c)
{
	// TODO eliminate the redundancy
	uiWindowsControlMinimumSizeChanged(c);
}

static void formArrangeChildren(uiForm *f)
{
	LONG_PTR controlID;
	HWND insertAfter;
	int i;

	controlID = 100;
	insertAfter = NULL;
	for (const struct formChild &fc : *(f->controls)) {
		// TODO assign label ID and z-order
		uiWindowsControlAssignControlIDZOrder(uiWindowsControl(fc.c), &controlID, &insertAfter);
	}
}

void uiFormAppend(uiForm *f, const char *label, uiControl *c, int stretchy)
{
	struct formChild fc;
	WCHAR *wlabel;

	fc.c = c;
	wlabel = toUTF16(label);
	fc.label = uiWindowsEnsureCreateControlHWND(0,
		L"STATIC", wlabel,
		SS_LEFT | SS_NOPREFIX,
		hInstance, NULL,
		TRUE);
	uiFree(wlabel);
	uiWindowsEnsureSetParentHWND(fc.label, f->hwnd);
	fc.stretchy = stretchy;
	uiControlSetParent(fc.c, uiControl(f));
	uiWindowsControlSetParentHWND(uiWindowsControl(fc.c), f->hwnd);
	f->controls->push_back(fc);
	formArrangeChildren(f);
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(f));
}

void uiFormDelete(uiForm *f, int index)
{
	struct formChild fc;

	fc = (*(f->controls))[index];
	uiControlSetParent(fc.c, NULL);
	uiWindowsControlSetParentHWND(uiWindowsControl(fc.c), NULL);
	uiWindowsEnsureDestroyWindow(fc.label);
	f->controls->erase(f->controls->begin() + index);
	formArrangeChildren(f);
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(f));
}

int uiFormPadded(uiForm *f)
{
	return f->padded;
}

void uiFormSetPadded(uiForm *f, int padded)
{
	f->padded = padded;
	uiWindowsControlMinimumSizeChanged(uiWindowsControl(f));
}

static void onResize(uiWindowsControl *c)
{
	formRelayout(uiForm(c));
}

uiForm *uiNewForm(void)
{
	uiForm *f;

	uiWindowsNewControl(uiForm, f);

	f->hwnd = uiWindowsMakeContainer(uiWindowsControl(f), onResize);

	f->controls = new std::vector<struct formChild>;

	return f;
}
