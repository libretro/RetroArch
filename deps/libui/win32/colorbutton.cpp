// 16 may 2016
#include "uipriv_windows.hpp"

struct uiColorButton {
	uiWindowsControl c;
	HWND hwnd;
	double r;
	double g;
	double b;
	double a;
	void (*onChanged)(uiColorButton *, void *);
	void *onChangedData;
};

static void uiColorButtonDestroy(uiControl *c)
{
	uiColorButton *b = uiColorButton(c);

	uiWindowsUnregisterWM_COMMANDHandler(b->hwnd);
	uiWindowsUnregisterWM_NOTIFYHandler(b->hwnd);
	uiWindowsEnsureDestroyWindow(b->hwnd);
	uiFreeControl(uiControl(b));
}

static BOOL onWM_COMMAND(uiControl *c, HWND hwnd, WORD code, LRESULT *lResult)
{
	uiColorButton *b = uiColorButton(c);
	HWND parent;
	struct colorDialogRGBA rgba;

	if (code != BN_CLICKED)
		return FALSE;

	parent = parentToplevel(b->hwnd);
	rgba.r = b->r;
	rgba.g = b->g;
	rgba.b = b->b;
	rgba.a = b->a;
	if (showColorDialog(parent, &rgba)) {
		b->r = rgba.r;
		b->g = rgba.g;
		b->b = rgba.b;
		b->a = rgba.a;
		invalidateRect(b->hwnd, NULL, TRUE);
		(*(b->onChanged))(b, b->onChangedData);
	}

	*lResult = 0;
	return TRUE;
}

static BOOL onWM_NOTIFY(uiControl *c, HWND hwnd, NMHDR *nmhdr, LRESULT *lResult)
{
	uiColorButton *b = uiColorButton(c);
	NMCUSTOMDRAW *nm = (NMCUSTOMDRAW *) nmhdr;
	RECT client;
	ID2D1DCRenderTarget *rt;
	D2D1_RECT_F r;
	D2D1_COLOR_F color;
	D2D1_BRUSH_PROPERTIES bprop;
	ID2D1SolidColorBrush *brush;
	uiWindowsSizing sizing;
	int x, y;
	HRESULT hr;

	if (nmhdr->code != NM_CUSTOMDRAW)
		return FALSE;
	// and allow the button to draw its background
	if (nm->dwDrawStage != CDDS_PREPAINT)
		return FALSE;

	uiWindowsEnsureGetClientRect(b->hwnd, &client);
	rt = makeHDCRenderTarget(nm->hdc, &client);
	rt->BeginDraw();

	uiWindowsGetSizing(b->hwnd, &sizing);
	x = 3;		// should be enough
	y = 3;
	uiWindowsSizingDlgUnitsToPixels(&sizing, &x, &y);
	r.left = client.left + x;
	r.top = client.top + y;
	r.right = client.right - x;
	r.bottom = client.bottom - y;

	color.r = b->r;
	color.g = b->g;
	color.b = b->b;
	color.a = b->a;
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = 1.0;
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = rt->CreateSolidColorBrush(&color, &bprop, &brush);
	if (hr != S_OK)
		logHRESULT(L"error creating brush for color button", hr);
	rt->FillRectangle(&r, brush);
	brush->Release();

	hr = rt->EndDraw(NULL, NULL);
	if (hr != S_OK)
		logHRESULT(L"error drawing color on color button", hr);
	rt->Release();

	// skip default processing (don't draw text)
	*lResult = CDRF_SKIPDEFAULT;
	return TRUE;
}

uiWindowsControlAllDefaultsExceptDestroy(uiColorButton)

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define buttonHeight 14

// TODO check widths
static void uiColorButtonMinimumSize(uiWindowsControl *c, int *width, int *height)
{
	uiColorButton *b = uiColorButton(c);
	SIZE size;
	uiWindowsSizing sizing;
	int y;

	// try the comctl32 version 6 way
	size.cx = 0;		// explicitly ask for ideal size
	size.cy = 0;
	if (SendMessageW(b->hwnd, BCM_GETIDEALSIZE, 0, (LPARAM) (&size)) != FALSE) {
		*width = size.cx;
		*height = size.cy;
		return;
	}

	// that didn't work; fall back to using Microsoft's metrics
	// Microsoft says to use a fixed width for all buttons; this isn't good enough
	// use the text width instead, with some edge padding
	*width = uiWindowsWindowTextWidth(b->hwnd) + (2 * GetSystemMetrics(SM_CXEDGE));
	y = buttonHeight;
	uiWindowsGetSizing(b->hwnd, &sizing);
	uiWindowsSizingDlgUnitsToPixels(&sizing, NULL, &y);
	*height = y;
}

static void defaultOnChanged(uiColorButton *b, void *data)
{
	// do nothing
}

void uiColorButtonColor(uiColorButton *b, double *r, double *g, double *bl, double *a)
{
	*r = b->r;
	*g = b->g;
	*bl = b->b;
	*a = b->a;
}

void uiColorButtonSetColor(uiColorButton *b, double r, double g, double bl, double a)
{
	b->r = r;
	b->g = g;
	b->b = bl;
	b->a = a;
	invalidateRect(b->hwnd, NULL, TRUE);
}

void uiColorButtonOnChanged(uiColorButton *b, void (*f)(uiColorButton *, void *), void *data)
{
	b->onChanged = f;
	b->onChangedData = data;
}

uiColorButton *uiNewColorButton(void)
{
	uiColorButton *b;

	uiWindowsNewControl(uiColorButton, b);

	// initial color is black
	b->r = 0.0;
	b->g = 0.0;
	b->b = 0.0;
	b->a = 1.0;

	b->hwnd = uiWindowsEnsureCreateControlHWND(0,
		L"button", L" ",		// TODO; can't use "" TODO
		BS_PUSHBUTTON | WS_TABSTOP,
		hInstance, NULL,
		TRUE);

	uiWindowsRegisterWM_COMMANDHandler(b->hwnd, onWM_COMMAND, uiControl(b));
	uiWindowsRegisterWM_NOTIFYHandler(b->hwnd, onWM_NOTIFY, uiControl(b));
	uiColorButtonOnChanged(b, defaultOnChanged, NULL);

	return b;
}
