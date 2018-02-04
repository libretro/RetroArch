// 16 may 2016
#include "uipriv_windows.hpp"

// TODO should the d2dscratch programs capture mouse?

struct colorDialog {
	HWND hwnd;

	HWND svChooser;
	HWND hSlider;
	HWND preview;
	HWND opacitySlider;
	HWND editH;
	HWND editS;
	HWND editV;
	HWND editRDouble, editRInt;
	HWND editGDouble, editGInt;
	HWND editBDouble, editBInt;
	HWND editADouble, editAInt;
	HWND editHex;

	double h;
	double s;
	double v;
	double a;
	struct colorDialogRGBA *out;

	BOOL updating;
};

// both of these are from the wikipedia page on HSV
// TODO what to do about negative h?
static void rgb2HSV(double r, double g, double b, double *h, double *s, double *v)
{
	double M, m;
	int whichmax;
	double c;

	M = r;
	whichmax = 0;
	if (M < g) {
		M = g;
		whichmax = 1;
	}
	if (M < b) {
		M = b;
		whichmax = 2;
	}
	m = r;
	if (m > g)
		m = g;
	if (m > b)
		m = b;
	c = M - m;

	if (c == 0)
		*h = 0;
	else {
		switch (whichmax) {
		case 0:
			*h = ((g - b) / c);
			*h = fmod(*h, 6);
			break;
		case 1:
			*h = ((b - r) / c) + 2;
			break;
		case 2:
			*h = ((r - g) / c) + 4;
			break;
		}
		*h /= 6;		// put in range [0,1)
	}

	*v = M;

	if (c == 0)
		*s = 0;
	else
		*s = c / *v;
}

// TODO negative R values?
static void hsv2RGB(double h, double s, double v, double *r, double *g, double *b)
{
	double c;
	double hPrime;
	int h60;
	double x;
	double m;
	double c1, c2;

	c = v * s;
	hPrime = h * 6;
	h60 = (int) hPrime;		// equivalent to splitting into 60° chunks
	x = c * (1.0 - fabs(fmod(hPrime, 2) - 1.0));
	m = v - c;
	switch (h60) {
	case 0:
		*r = c + m;
		*g = x + m;
		*b = m;
		return;
	case 1:
		*r = x + m;
		*g = c + m;
		*b = m;
		return;
	case 2:
		*r = m;
		*g = c + m;
		*b = x + m;
		return;
	case 3:
		*r = m;
		*g = x + m;
		*b = c + m;
		return;
	case 4:
		*r = x + m;
		*g = m;
		*b = c + m;
		return;
	case 5:
		*r = c + m;
		*g = m;
		*b = x + m;
		return;
	}
	// TODO
}

#define hexd L"0123456789ABCDEF"

static void rgba2Hex(uint8_t r, uint8_t g, uint8_t b, uint8_t a, WCHAR *buf)
{
	buf[0] = L'#';
	buf[1] = hexd[(a >> 4) & 0xF];
	buf[2] = hexd[a & 0xF];
	buf[3] = hexd[(r >> 4) & 0xF];
	buf[4] = hexd[r & 0xF];
	buf[5] = hexd[(g >> 4) & 0xF];
	buf[6] = hexd[g & 0xF];
	buf[7] = hexd[(b >> 4) & 0xF];
	buf[8] = hexd[b & 0xF];
	buf[9] = L'\0';
}

static int convHexDigit(WCHAR c)
{
	if (c >= L'0' && c <= L'9')
		return c - L'0';
	if (c >= L'A' && c <= L'F')
		return c - L'A' + 0xA;
	if (c >= L'a' && c <= L'f')
		return c - L'a' + 0xA;
	return -1;
}

// TODO allow #NNN shorthand
static BOOL hex2RGBA(WCHAR *buf, double *r, double *g, double *b, double *a)
{
	uint8_t component;
	int i;

	if (*buf == L'#')
		buf++;

	component = 0;
	i = convHexDigit(*buf++);
	if (i < 0)
		return FALSE;
	component |= ((uint8_t) i) << 4;
	i = convHexDigit(*buf++);
	if (i < 0)
		return FALSE;
	component |= ((uint8_t) i);
	*a = ((double) component) / 255;

	component = 0;
	i = convHexDigit(*buf++);
	if (i < 0)
		return FALSE;
	component |= ((uint8_t) i) << 4;
	i = convHexDigit(*buf++);
	if (i < 0)
		return FALSE;
	component |= ((uint8_t) i);
	*r = ((double) component) / 255;

	component = 0;
	i = convHexDigit(*buf++);
	if (i < 0)
		return FALSE;
	component |= ((uint8_t) i) << 4;
	i = convHexDigit(*buf++);
	if (i < 0)
		return FALSE;
	component |= ((uint8_t) i);
	*g = ((double) component) / 255;

	if (*buf == L'\0') {		// #NNNNNN syntax
		*b = *g;
		*g = *r;
		*r = *a;
		*a = 1;
		return TRUE;
	}

	component = 0;
	i = convHexDigit(*buf++);
	if (i < 0)
		return FALSE;
	component |= ((uint8_t) i) << 4;
	i = convHexDigit(*buf++);
	if (i < 0)
		return FALSE;
	component |= ((uint8_t) i);
	*b = ((double) component) / 255;

	return *buf == L'\0';
}

static void updateDouble(HWND hwnd, double d, HWND whichChanged)
{
	WCHAR *str;

	if (whichChanged == hwnd)
		return;
	str = ftoutf16(d);
	setWindowText(hwnd, str);
	uiFree(str);
}

static void updateDialog(struct colorDialog *c, HWND whichChanged)
{
	double r, g, b;
	uint8_t rb, gb, bb, ab;
	WCHAR *str;
	WCHAR hexbuf[16];		// more than enough

	c->updating = TRUE;

	updateDouble(c->editH, c->h, whichChanged);
	updateDouble(c->editS, c->s, whichChanged);
	updateDouble(c->editV, c->v, whichChanged);

	hsv2RGB(c->h, c->s, c->v, &r, &g, &b);

	updateDouble(c->editRDouble, r, whichChanged);
	updateDouble(c->editGDouble, g, whichChanged);
	updateDouble(c->editBDouble, b, whichChanged);
	updateDouble(c->editADouble, c->a, whichChanged);

	rb = (uint8_t) (r * 255);
	gb = (uint8_t) (g * 255);
	bb = (uint8_t) (b * 255);
	ab = (uint8_t) (c->a * 255);

	if (whichChanged != c->editRInt) {
		str = itoutf16(rb);
		setWindowText(c->editRInt, str);
		uiFree(str);
	}
	if (whichChanged != c->editGInt) {
		str = itoutf16(gb);
		setWindowText(c->editGInt, str);
		uiFree(str);
	}
	if (whichChanged != c->editBInt) {
		str = itoutf16(bb);
		setWindowText(c->editBInt, str);
		uiFree(str);
	}
	if (whichChanged != c->editAInt) {
		str = itoutf16(ab);
		setWindowText(c->editAInt, str);
		uiFree(str);
	}

	if (whichChanged != c->editHex) {
		rgba2Hex(rb, gb, bb, ab, hexbuf);
		setWindowText(c->editHex, hexbuf);
	}

	// TODO TRUE?
	invalidateRect(c->svChooser, NULL, TRUE);
	invalidateRect(c->hSlider, NULL, TRUE);
	invalidateRect(c->preview, NULL, TRUE);
	invalidateRect(c->opacitySlider, NULL, TRUE);

	c->updating = FALSE;
}

// this imitates http://blogs.msdn.com/b/wpfsdk/archive/2006/10/26/uncommon-dialogs--font-chooser-and-color-picker-dialogs.aspx
static void drawGrid(ID2D1RenderTarget *rt, D2D1_RECT_F *fillRect)
{
	D2D1_SIZE_F size;
	D2D1_PIXEL_FORMAT pformat;
	ID2D1BitmapRenderTarget *brt;
	D2D1_COLOR_F color;
	D2D1_BRUSH_PROPERTIES bprop;
	ID2D1SolidColorBrush *brush;
	D2D1_RECT_F rect;
	ID2D1Bitmap *bitmap;
	D2D1_BITMAP_BRUSH_PROPERTIES bbp;
	ID2D1BitmapBrush *bb;
	HRESULT hr;

	// mind the divisions; they represent the fact the original uses a viewport
	size.width = 100 / 10;
	size.height = 100 / 10;
	// yay more ABI bugs
#ifdef _MSC_VER
	pformat = rt->GetPixelFormat();
#else
	{
		typedef D2D1_PIXEL_FORMAT *(__stdcall ID2D1RenderTarget::* GetPixelFormatF)(D2D1_PIXEL_FORMAT *);
		GetPixelFormatF gpf;

		gpf = (GetPixelFormatF) (&(rt->GetPixelFormat));
		(rt->*gpf)(&pformat);
	}
#endif
	hr = rt->CreateCompatibleRenderTarget(&size, NULL,
		&pformat, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
		&brt);
	if (hr != S_OK)
		logHRESULT(L"error creating render target for grid", hr);

	brt->BeginDraw();

	color.r = 1.0;
	color.g = 1.0;
	color.b = 1.0;
	color.a = 1.0;
	brt->Clear(&color);

	color = D2D1::ColorF(D2D1::ColorF::LightGray, 1.0);
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = 1.0;
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = brt->CreateSolidColorBrush(&color, &bprop, &brush);
	if (hr != S_OK)
		logHRESULT(L"error creating brush for grid", hr);
	rect.left = 0;
	rect.top = 0;
	rect.right = 50 / 10;
	rect.bottom = 50 / 10;
	brt->FillRectangle(&rect, brush);
	rect.left = 50 / 10;
	rect.top = 50 / 10;
	rect.right = 100 / 10;
	rect.bottom = 100 / 10;
	brt->FillRectangle(&rect, brush);
	brush->Release();

	hr = brt->EndDraw(NULL, NULL);
	if (hr != S_OK)
		logHRESULT(L"error finalizing render target for grid", hr);
	hr = brt->GetBitmap(&bitmap);
	if (hr != S_OK)
		logHRESULT(L"error getting bitmap for grid", hr);
	brt->Release();

	ZeroMemory(&bbp, sizeof (D2D1_BITMAP_BRUSH_PROPERTIES));
	bbp.extendModeX = D2D1_EXTEND_MODE_WRAP;
	bbp.extendModeY = D2D1_EXTEND_MODE_WRAP;
	bbp.interpolationMode = D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
	hr = rt->CreateBitmapBrush(bitmap, &bbp, &bprop, &bb);
	if (hr != S_OK)
		logHRESULT(L"error creating bitmap brush for grid", hr);
	rt->FillRectangle(fillRect, bb);
	bb->Release();
	bitmap->Release();
}

// this interesting approach comes from http://blogs.msdn.com/b/wpfsdk/archive/2006/10/26/uncommon-dialogs--font-chooser-and-color-picker-dialogs.aspx
static void drawSVChooser(struct colorDialog *c, ID2D1RenderTarget *rt)
{
	D2D1_SIZE_F size;
	D2D1_RECT_F rect;
	double rTop, gTop, bTop;
	D2D1_GRADIENT_STOP stops[2];
	ID2D1GradientStopCollection *collection;
	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lprop;
	D2D1_BRUSH_PROPERTIES bprop;
	ID2D1LinearGradientBrush *brush;
	ID2D1LinearGradientBrush *opacity;
	ID2D1Layer *layer;
	D2D1_LAYER_PARAMETERS layerparams;
	D2D1_ELLIPSE mparam;
	D2D1_COLOR_F mcolor;
	ID2D1SolidColorBrush *markerBrush;
	HRESULT hr;

	size = realGetSize(rt);
	rect.left = 0;
	rect.top = 0;
	rect.right = size.width;
	rect.bottom = size.height;

	drawGrid(rt, &rect);

	// first, draw a vertical gradient from the current hue at max S/V to black
	// the source example draws it upside down; let's do so too just to be safe
	hsv2RGB(c->h, 1.0, 1.0, &rTop, &gTop, &bTop);
	stops[0].position = 0;
	stops[0].color.r = 0.0;
	stops[0].color.g = 0.0;
	stops[0].color.b = 0.0;
	stops[0].color.a = 1.0;
	stops[1].position = 1;
	stops[1].color.r = rTop;
	stops[1].color.g = gTop;
	stops[1].color.b = bTop;
	stops[1].color.a = 1.0;
	hr = rt->CreateGradientStopCollection(stops, 2,
		D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
		&collection);
	if (hr != S_OK)
		logHRESULT(L"error making gradient stop collection for first gradient in SV chooser", hr);
	ZeroMemory(&lprop, sizeof (D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES));
	lprop.startPoint.x = size.width / 2;
	lprop.startPoint.y = size.height;
	lprop.endPoint.x = size.width / 2;
	lprop.endPoint.y = 0;
	// TODO decide what to do about the duplication of this
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = c->a;		// note this part; we also use it below for the layer
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = rt->CreateLinearGradientBrush(&lprop, &bprop,
		collection, &brush);
	if (hr != S_OK)
		logHRESULT(L"error making gradient brush for first gradient in SV chooser", hr);
	rt->FillRectangle(&rect, brush);
	brush->Release();
	collection->Release();

	// second, create an opacity mask for the third step: a horizontal gradientthat goes from opaque to translucent
	stops[0].position = 0;
	stops[0].color.r = 0.0;
	stops[0].color.g = 0.0;
	stops[0].color.b = 0.0;
	stops[0].color.a = 1.0;
	stops[1].position = 1;
	stops[1].color.r = 0.0;
	stops[1].color.g = 0.0;
	stops[1].color.b = 0.0;
	stops[1].color.a = 0.0;
	hr = rt->CreateGradientStopCollection(stops, 2,
		D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
		&collection);
	if (hr != S_OK)
		logHRESULT(L"error making gradient stop collection for opacity mask gradient in SV chooser", hr);
	ZeroMemory(&lprop, sizeof (D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES));
	lprop.startPoint.x = 0;
	lprop.startPoint.y = size.height / 2;
	lprop.endPoint.x = size.width;
	lprop.endPoint.y = size.height / 2;
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = 1.0;
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = rt->CreateLinearGradientBrush(&lprop, &bprop,
		collection, &opacity);
	if (hr != S_OK)
		logHRESULT(L"error making gradient brush for opacity mask gradient in SV chooser", hr);
	collection->Release();

	// finally, make a vertical gradient from white at the top to black at the bottom (right side up this time) and with the previous opacity mask
	stops[0].position = 0;
	stops[0].color.r = 1.0;
	stops[0].color.g = 1.0;
	stops[0].color.b = 1.0;
	stops[0].color.a = 1.0;
	stops[1].position = 1;
	stops[1].color.r = 0.0;
	stops[1].color.g = 0.0;
	stops[1].color.b = 0.0;
	stops[1].color.a = 1.0;
	hr = rt->CreateGradientStopCollection(stops, 2,
		D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
		&collection);
	if (hr != S_OK)
		logHRESULT(L"error making gradient stop collection for second gradient in SV chooser", hr);
	ZeroMemory(&lprop, sizeof (D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES));
	lprop.startPoint.x = size.width / 2;
	lprop.startPoint.y = 0;
	lprop.endPoint.x = size.width / 2;
	lprop.endPoint.y = size.height;
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = 1.0;
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = rt->CreateLinearGradientBrush(&lprop, &bprop,
		collection, &brush);
	if (hr != S_OK)
		logHRESULT(L"error making gradient brush for second gradient in SV chooser", hr);
	// oh but wait we can't use FillRectangle() with an opacity mask
	// and we can't use FillGeometry() with both an opacity mask and a non-bitmap
	// layers it is!
	hr = rt->CreateLayer(&size, &layer);
	if (hr != S_OK)
		logHRESULT(L"error making layer for second gradient in SV chooser", hr);
	ZeroMemory(&layerparams, sizeof (D2D1_LAYER_PARAMETERS));
	layerparams.contentBounds = rect;
	// TODO make sure these are right
	layerparams.geometricMask = NULL;
	layerparams.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
	layerparams.maskTransform._11 = 1;
	layerparams.maskTransform._22 = 1;
	layerparams.opacity = c->a;			// here's the other use of c->a to note
	layerparams.opacityBrush = opacity;
	layerparams.layerOptions = D2D1_LAYER_OPTIONS_NONE;
	rt->PushLayer(&layerparams, layer);
	rt->FillRectangle(&rect, brush);
	rt->PopLayer();
	layer->Release();
	brush->Release();
	collection->Release();
	opacity->Release();

	// and now we just draw the marker
	ZeroMemory(&mparam, sizeof (D2D1_ELLIPSE));
	mparam.point.x = c->s * size.width;
	mparam.point.y = (1 - c->v) * size.height;
	mparam.radiusX = 7;
	mparam.radiusY = 7;
	// TODO make the color contrast?
	mcolor.r = 1.0;
	mcolor.g = 1.0;
	mcolor.b = 1.0;
	mcolor.a = 1.0;
	bprop.opacity = 1.0;		// the marker should always be opaque
	hr = rt->CreateSolidColorBrush(&mcolor, &bprop, &markerBrush);
	if (hr != S_OK)
		logHRESULT(L"error creating brush for SV chooser marker", hr);
	rt->DrawEllipse(&mparam, markerBrush, 2, NULL);
	markerBrush->Release();
}

static LRESULT CALLBACK svChooserSubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	ID2D1RenderTarget *rt;
	struct colorDialog *c;
	D2D1_POINT_2F *pos;
	D2D1_SIZE_F *size;

	c = (struct colorDialog *) dwRefData;
	switch (uMsg) {
	case msgD2DScratchPaint:
		rt = (ID2D1RenderTarget *) lParam;
		drawSVChooser(c, rt);
		return 0;
	case msgD2DScratchLButtonDown:
		pos = (D2D1_POINT_2F *) wParam;
		size = (D2D1_SIZE_F *) lParam;
		c->s = pos->x / size->width;
		c->v = 1 - (pos->y / size->height);
		updateDialog(c, NULL);
		return 0;
	case WM_NCDESTROY:
		if (RemoveWindowSubclass(hwnd, svChooserSubProc, uIdSubclass) == FALSE)
			logLastError(L"error removing color dialog SV chooser subclass");
		break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

static void drawArrow(ID2D1RenderTarget *rt, D2D1_POINT_2F center, double hypot)
{
	double leg;
	D2D1_RECT_F rect;
	D2D1_MATRIX_3X2_F oldtf, rotate;
	D2D1_COLOR_F color;
	D2D1_BRUSH_PROPERTIES bprop;
	ID2D1SolidColorBrush *brush;
	HRESULT hr;

	// to avoid needing a geometry, this will just be a rotated square
	// compute the length of each side; the diagonal of the square is 2 * offset to gradient
	// a^2 + a^2 = c^2 -> 2a^2 = c^2
	// a = sqrt(c^2/2)
	hypot *= hypot;
	hypot /= 2;
	leg = sqrt(hypot);
	rect.left = center.x - leg;
	rect.top = center.y - leg;
	rect.right = center.x + leg;
	rect.bottom = center.y + leg;

	// now we need to rotate the render target 45° (either way works) about the center point
	rt->GetTransform(&oldtf);
	rotate = oldtf * D2D1::Matrix3x2F::Rotation(45, center);
	rt->SetTransform(&rotate);

	// and draw
	color.r = 0.0;
	color.g = 0.0;
	color.b = 0.0;
	color.a = 1.0;
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = 1.0;
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = rt->CreateSolidColorBrush(&color, &bprop, &brush);
	if (hr != S_OK)
		logHRESULT(L"error creating brush for arrow", hr);
	rt->FillRectangle(&rect, brush);
	brush->Release();

	// clean up
	rt->SetTransform(&oldtf);
}

// the gradient stuff also comes from http://blogs.msdn.com/b/wpfsdk/archive/2006/10/26/uncommon-dialogs--font-chooser-and-color-picker-dialogs.aspx
#define nStops (30)
#define degPerStop (360 / nStops)
#define stopIncr (1.0 / ((double) nStops))

static void drawHSlider(struct colorDialog *c, ID2D1RenderTarget *rt)
{
	D2D1_SIZE_F size;
	D2D1_RECT_F rect;
	D2D1_GRADIENT_STOP stops[nStops];
	double r, g, b;
	int i;
	double h;
	ID2D1GradientStopCollection *collection;
	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lprop;
	D2D1_BRUSH_PROPERTIES bprop;
	ID2D1LinearGradientBrush *brush;
	double hypot;
	D2D1_POINT_2F center;
	HRESULT hr;

	size = realGetSize(rt);
	rect.left = size.width / 6;		// leftmost sixth for arrow
	rect.top = 0;
	rect.right = size.width;
	rect.bottom = size.height;

	for (i = 0; i < nStops; i++) {
		h = ((double) (i * degPerStop)) / 360.0;
		if (i == (nStops - 1))
			h = 0;
		hsv2RGB(h, 1.0, 1.0, &r, &g, &b);
		stops[i].position = ((double) i) * stopIncr;
		stops[i].color.r = r;
		stops[i].color.g = g;
		stops[i].color.b = b;
		stops[i].color.a = 1.0;
	}
	// and pin the last one
	stops[i - 1].position = 1.0;

	hr = rt->CreateGradientStopCollection(stops, nStops,
		// note that in this case this gamma is explicitly specified by the original
		D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
		&collection);
	if (hr != S_OK)
		logHRESULT(L"error creating stop collection for H slider gradient", hr);
	ZeroMemory(&lprop, sizeof (D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES));
	lprop.startPoint.x = (rect.right - rect.left) / 2;
	lprop.startPoint.y = 0;
	lprop.endPoint.x = (rect.right - rect.left) / 2;
	lprop.endPoint.y = size.height;
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = 1.0;
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = rt->CreateLinearGradientBrush(&lprop, &bprop,
		collection, &brush);
	if (hr != S_OK)
		logHRESULT(L"error creating gradient brush for H slider", hr);
	rt->FillRectangle(&rect, brush);
	brush->Release();
	collection->Release();

	// now draw a black arrow
	center.x = 0;
	center.y = c->h * size.height;
	hypot = rect.left;
	drawArrow(rt, center, hypot);
}

static LRESULT CALLBACK hSliderSubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	ID2D1RenderTarget *rt;
	struct colorDialog *c;
	D2D1_POINT_2F *pos;
	D2D1_SIZE_F *size;

	c = (struct colorDialog *) dwRefData;
	switch (uMsg) {
	case msgD2DScratchPaint:
		rt = (ID2D1RenderTarget *) lParam;
		drawHSlider(c, rt);
		return 0;
	case msgD2DScratchLButtonDown:
		pos = (D2D1_POINT_2F *) wParam;
		size = (D2D1_SIZE_F *) lParam;
		c->h = pos->y / size->height;
		updateDialog(c, NULL);
		return 0;
	case WM_NCDESTROY:
		if (RemoveWindowSubclass(hwnd, hSliderSubProc, uIdSubclass) == FALSE)
			logLastError(L"error removing color dialog H slider subclass");
		break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

static void drawPreview(struct colorDialog *c, ID2D1RenderTarget *rt)
{
	D2D1_SIZE_F size;
	D2D1_RECT_F rect;
	double r, g, b;
	D2D1_COLOR_F color;
	D2D1_BRUSH_PROPERTIES bprop;
	ID2D1SolidColorBrush *brush;
	HRESULT hr;

	size = realGetSize(rt);
	rect.left = 0;
	rect.top = 0;
	rect.right = size.width;
	rect.bottom = size.height;

	drawGrid(rt, &rect);

	hsv2RGB(c->h, c->s, c->v, &r, &g, &b);
	color.r = r;
	color.g = g;
	color.b = b;
	color.a = c->a;
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = 1.0;
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = rt->CreateSolidColorBrush(&color, &bprop, &brush);
	if (hr != S_OK)
		logHRESULT(L"error creating brush for preview", hr);
	rt->FillRectangle(&rect, brush);
	brush->Release();
}

static LRESULT CALLBACK previewSubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	ID2D1RenderTarget *rt;
	struct colorDialog *c;

	c = (struct colorDialog *) dwRefData;
	switch (uMsg) {
	case msgD2DScratchPaint:
		rt = (ID2D1RenderTarget *) lParam;
		drawPreview(c, rt);
		return 0;
	case WM_NCDESTROY:
		if (RemoveWindowSubclass(hwnd, previewSubProc, uIdSubclass) == FALSE)
			logLastError(L"error removing color dialog previewer subclass");
		break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// once again, this is based on the Microsoft sample above
static void drawOpacitySlider(struct colorDialog *c, ID2D1RenderTarget *rt)
{
	D2D1_SIZE_F size;
	D2D1_RECT_F rect;
	D2D1_GRADIENT_STOP stops[2];
	ID2D1GradientStopCollection *collection;
	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lprop;
	D2D1_BRUSH_PROPERTIES bprop;
	ID2D1LinearGradientBrush *brush;
	double hypot;
	D2D1_POINT_2F center;
	HRESULT hr;

	size = realGetSize(rt);
	rect.left = 0;
	rect.top = 0;
	rect.right = size.width;
	rect.bottom = size.height * (5.0 / 6.0);		// bottommost sixth for arrow

	drawGrid(rt, &rect);

	stops[0].position = 0.0;
	stops[0].color.r = 0.0;
	stops[0].color.g = 0.0;
	stops[0].color.b = 0.0;
	stops[0].color.a = 1.0;
	stops[1].position = 1.0;
	stops[1].color.r = 1.0;		// this is the XAML color Transparent, as in the source
	stops[1].color.g = 1.0;
	stops[1].color.b = 1.0;
	stops[1].color.a = 0.0;
	hr = rt->CreateGradientStopCollection(stops, 2,
		// note that in this case this gamma is explicitly specified by the original
		D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
		&collection);
	if (hr != S_OK)
		logHRESULT(L"error creating stop collection for opacity slider gradient", hr);
	ZeroMemory(&lprop, sizeof (D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES));
	lprop.startPoint.x = 0;
	lprop.startPoint.y = (rect.bottom - rect.top) / 2;
	lprop.endPoint.x = size.width;
	lprop.endPoint.y = (rect.bottom - rect.top) / 2;
	ZeroMemory(&bprop, sizeof (D2D1_BRUSH_PROPERTIES));
	bprop.opacity = 1.0;
	bprop.transform._11 = 1;
	bprop.transform._22 = 1;
	hr = rt->CreateLinearGradientBrush(&lprop, &bprop,
		collection, &brush);
	if (hr != S_OK)
		logHRESULT(L"error creating gradient brush for opacity slider", hr);
	rt->FillRectangle(&rect, brush);
	brush->Release();
	collection->Release();

	// now draw a black arrow
	center.x = (1 - c->a) * size.width;
	center.y = size.height;
	hypot = size.height - rect.bottom;
	drawArrow(rt, center, hypot);
}

static LRESULT CALLBACK opacitySliderSubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	ID2D1RenderTarget *rt;
	struct colorDialog *c;
	D2D1_POINT_2F *pos;
	D2D1_SIZE_F *size;

	c = (struct colorDialog *) dwRefData;
	switch (uMsg) {
	case msgD2DScratchPaint:
		rt = (ID2D1RenderTarget *) lParam;
		drawOpacitySlider(c, rt);
		return 0;
	case msgD2DScratchLButtonDown:
		pos = (D2D1_POINT_2F *) wParam;
		size = (D2D1_SIZE_F *) lParam;
		c->a = 1 - (pos->x / size->width);
		updateDialog(c, NULL);
		return 0;
	case WM_NCDESTROY:
		if (RemoveWindowSubclass(hwnd, opacitySliderSubProc, uIdSubclass) == FALSE)
			logLastError(L"error removing color dialog opacity slider subclass");
		break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// TODO extract into d2dscratch.cpp, use in font dialog
HWND replaceWithD2DScratch(HWND parent, int id, SUBCLASSPROC subproc, void *data)
{
	HWND replace;
	RECT r;

	replace = getDlgItem(parent, id);
	uiWindowsEnsureGetWindowRect(replace, &r);
	mapWindowRect(NULL, parent, &r);
	uiWindowsEnsureDestroyWindow(replace);
	return newD2DScratch(parent, &r, (HMENU) id, subproc, (DWORD_PTR) data);
	// TODO preserve Z-order
}

// a few issues:
// - some controls are positioned wrong; see http://stackoverflow.com/questions/37263267/why-are-some-of-my-controls-positioned-slightly-off-in-a-dialog-template-in-a-re
// - labels are too low; need to adjust them by the font's internal leading
// fixupControlPositions() and the following helper routines fix that for us

static LONG offsetTo(HWND a, HWND b)
{
	RECT ra, rb;

	uiWindowsEnsureGetWindowRect(a, &ra);
	uiWindowsEnsureGetWindowRect(b, &rb);
	return rb.top - ra.bottom;
}

static void moveWindowsUp(struct colorDialog *c, LONG by, ...)
{
	va_list ap;
	HWND cur;
	RECT r;

	va_start(ap, by);
	for (;;) {
		cur = va_arg(ap, HWND);
		if (cur == NULL)
			break;
		uiWindowsEnsureGetWindowRect(cur, &r);
		mapWindowRect(NULL, c->hwnd, &r);
		r.top -= by;
		r.bottom -= by;
		// TODO this isn't technically during a resize
		uiWindowsEnsureMoveWindowDuringResize(cur,
			r.left, r.top,
			r.right - r.left, r.bottom - r.top);
	}
	va_end(ap);
}

static void fixupControlPositions(struct colorDialog *c)
{
	HWND labelH;
	HWND labelS;
	HWND labelV;
	HWND labelR;
	HWND labelG;
	HWND labelB;
	HWND labelA;
	HWND labelHex;
	LONG offset;
	uiWindowsSizing sizing;

	labelH = getDlgItem(c->hwnd, rcHLabel);
	labelS = getDlgItem(c->hwnd, rcSLabel);
	labelV = getDlgItem(c->hwnd, rcVLabel);
	labelR = getDlgItem(c->hwnd, rcRLabel);
	labelG = getDlgItem(c->hwnd, rcGLabel);
	labelB = getDlgItem(c->hwnd, rcBLabel);
	labelA = getDlgItem(c->hwnd, rcALabel);
	labelHex = getDlgItem(c->hwnd, rcHexLabel);

	offset = offsetTo(c->editH, c->editS);
	moveWindowsUp(c, offset,
		labelS, c->editS,
		labelG, c->editGDouble, c->editGInt,
		NULL);
	offset = offsetTo(c->editS, c->editV);
	moveWindowsUp(c, offset,
		labelV, c->editV,
		labelB, c->editBDouble, c->editBInt,
		NULL);
	offset = offsetTo(c->editBDouble, c->editADouble);
	moveWindowsUp(c, offset,
		labelA, c->editADouble, c->editAInt,
		NULL);

	getSizing(c->hwnd, &sizing, (HFONT) SendMessageW(labelH, WM_GETFONT, 0, 0));
	offset = sizing.InternalLeading;
	moveWindowsUp(c, offset,
		labelH, labelS, labelV,
		labelR, labelG, labelB, labelA,
		labelHex,
		NULL);
}

static struct colorDialog *beginColorDialog(HWND hwnd, LPARAM lParam)
{
	struct colorDialog *c;

	c = uiNew(struct colorDialog);
	c->hwnd = hwnd;
	c->out = (struct colorDialogRGBA *) lParam;
	// load initial values now
	rgb2HSV(c->out->r, c->out->g, c->out->b, &(c->h), &(c->s), &(c->v));
	c->a = c->out->a;

	// TODO set up d2dscratches

	// TODO prefix all these with rcColor instead of just rc
	c->editH = getDlgItem(c->hwnd, rcH);
	c->editS = getDlgItem(c->hwnd, rcS);
	c->editV = getDlgItem(c->hwnd, rcV);
	c->editRDouble = getDlgItem(c->hwnd, rcRDouble);
	c->editRInt = getDlgItem(c->hwnd, rcRInt);
	c->editGDouble = getDlgItem(c->hwnd, rcGDouble);
	c->editGInt = getDlgItem(c->hwnd, rcGInt);
	c->editBDouble = getDlgItem(c->hwnd, rcBDouble);
	c->editBInt = getDlgItem(c->hwnd, rcBInt);
	c->editADouble = getDlgItem(c->hwnd, rcADouble);
	c->editAInt = getDlgItem(c->hwnd, rcAInt);
	c->editHex = getDlgItem(c->hwnd, rcHex);

	c->svChooser = replaceWithD2DScratch(c->hwnd, rcColorSVChooser, svChooserSubProc, c);
	c->hSlider = replaceWithD2DScratch(c->hwnd, rcColorHSlider, hSliderSubProc, c);
	c->preview = replaceWithD2DScratch(c->hwnd, rcPreview, previewSubProc, c);
	c->opacitySlider = replaceWithD2DScratch(c->hwnd, rcOpacitySlider, opacitySliderSubProc, c);

	fixupControlPositions(c);

	// and get the ball rolling
	updateDialog(c, NULL);
	return c;
}

static void endColorDialog(struct colorDialog *c, INT_PTR code)
{
	if (EndDialog(c->hwnd, code) == 0)
		logLastError(L"error ending color dialog");
	uiFree(c);
}

// TODO make this void on the font dialog too
static void tryFinishDialog(struct colorDialog *c, WPARAM wParam)
{
	// cancelling
	if (LOWORD(wParam) != IDOK) {
		endColorDialog(c, 1);
		return;
	}

	// OK
	hsv2RGB(c->h, c->s, c->v, &(c->out->r), &(c->out->g), &(c->out->b));
	c->out->a = c->a;
	endColorDialog(c, 2);
}

static double editDouble(HWND hwnd)
{
	WCHAR *s;
	double d;

	s = windowText(hwnd);
	d = _wtof(s);
	uiFree(s);
	return d;
}

static void hChanged(struct colorDialog *c)
{
	double h;

	h = editDouble(c->editH);
	if (h < 0 || h >= 1.0)		// note the >=
		return;
	c->h = h;
	updateDialog(c, c->editH);
}

static void sChanged(struct colorDialog *c)
{
	double s;

	s = editDouble(c->editS);
	if (s < 0 || s > 1)
		return;
	c->s = s;
	updateDialog(c, c->editS);
}

static void vChanged(struct colorDialog *c)
{
	double v;

	v = editDouble(c->editV);
	if (v < 0 || v > 1)
		return;
	c->v = v;
	updateDialog(c, c->editV);
}

static void rDoubleChanged(struct colorDialog *c)
{
	double r, g, b;

	hsv2RGB(c->h, c->s, c->v, &r, &g, &b);
	r = editDouble(c->editRDouble);
	if (r < 0 || r > 1)
		return;
	rgb2HSV(r, g, b, &(c->h), &(c->s), &(c->v));
	updateDialog(c, c->editRDouble);
}

static void gDoubleChanged(struct colorDialog *c)
{
	double r, g, b;

	hsv2RGB(c->h, c->s, c->v, &r, &g, &b);
	g = editDouble(c->editGDouble);
	if (g < 0 || g > 1)
		return;
	rgb2HSV(r, g, b, &(c->h), &(c->s), &(c->v));
	updateDialog(c, c->editGDouble);
}

static void bDoubleChanged(struct colorDialog *c)
{
	double r, g, b;

	hsv2RGB(c->h, c->s, c->v, &r, &g, &b);
	b = editDouble(c->editBDouble);
	if (b < 0 || b > 1)
		return;
	rgb2HSV(r, g, b, &(c->h), &(c->s), &(c->v));
	updateDialog(c, c->editBDouble);
}

static void aDoubleChanged(struct colorDialog *c)
{
	double a;

	a = editDouble(c->editADouble);
	if (a < 0 || a > 1)
		return;
	c->a = a;
	updateDialog(c, c->editADouble);
}

static int editInt(HWND hwnd)
{
	WCHAR *s;
	int i;

	s = windowText(hwnd);
	i = _wtoi(s);
	uiFree(s);
	return i;
}

static void rIntChanged(struct colorDialog *c)
{
	double r, g, b;
	int i;

	hsv2RGB(c->h, c->s, c->v, &r, &g, &b);
	i = editInt(c->editRInt);
	if (i < 0 || i > 255)
		return;
	r = ((double) i) / 255.0;
	rgb2HSV(r, g, b, &(c->h), &(c->s), &(c->v));
	updateDialog(c, c->editRInt);
}

static void gIntChanged(struct colorDialog *c)
{
	double r, g, b;
	int i;

	hsv2RGB(c->h, c->s, c->v, &r, &g, &b);
	i = editInt(c->editGInt);
	if (i < 0 || i > 255)
		return;
	g = ((double) i) / 255.0;
	rgb2HSV(r, g, b, &(c->h), &(c->s), &(c->v));
	updateDialog(c, c->editGInt);
}

static void bIntChanged(struct colorDialog *c)
{
	double r, g, b;
	int i;

	hsv2RGB(c->h, c->s, c->v, &r, &g, &b);
	i = editInt(c->editBInt);
	if (i < 0 || i > 255)
		return;
	b = ((double) i) / 255.0;
	rgb2HSV(r, g, b, &(c->h), &(c->s), &(c->v));
	updateDialog(c, c->editBInt);
}

static void aIntChanged(struct colorDialog *c)
{
	int a;

	a = editInt(c->editAInt);
	if (a < 0 || a > 255)
		return;
	c->a = ((double) a) / 255;
	updateDialog(c, c->editAInt);
}

static void hexChanged(struct colorDialog *c)
{
	WCHAR *buf;
	double r, g, b, a;
	BOOL is;

	buf = windowText(c->editHex);
	is = hex2RGBA(buf, &r, &g, &b, &a);
	uiFree(buf);
	if (!is)
		return;
	rgb2HSV(r, g, b, &(c->h), &(c->s), &(c->v));
	c->a = a;
	updateDialog(c, c->editHex);
}

// TODO change fontdialog to use this
// note that if we make this const, we get lots of weird compiler errors
static std::map<int, void (*)(struct colorDialog *)> changed = {
	{ rcH, hChanged },
	{ rcS, sChanged },
	{ rcV, vChanged },
	{ rcRDouble, rDoubleChanged },
	{ rcGDouble, gDoubleChanged },
	{ rcBDouble, bDoubleChanged },
	{ rcADouble, aDoubleChanged },
	{ rcRInt, rIntChanged },
	{ rcGInt, gIntChanged },
	{ rcBInt, bIntChanged },
	{ rcAInt, aIntChanged },
	{ rcHex, hexChanged },
};

static INT_PTR CALLBACK colorDialogDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct colorDialog *c;

	c = (struct colorDialog *) GetWindowLongPtrW(hwnd, DWLP_USER);
	if (c == NULL) {
		if (uMsg == WM_INITDIALOG) {
			c = beginColorDialog(hwnd, lParam);
			SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR) c);
			return TRUE;
		}
		return FALSE;
	}

	switch (uMsg) {
	case WM_COMMAND:
		SetWindowLongPtrW(c->hwnd, DWLP_MSGRESULT, 0);		// just in case
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			if (HIWORD(wParam) != BN_CLICKED)
				return FALSE;
			tryFinishDialog(c, wParam);
			return TRUE;
		case rcH:
		case rcS:
		case rcV:
		case rcRDouble:
		case rcGDouble:
		case rcBDouble:
		case rcADouble:
		case rcRInt:
		case rcGInt:
		case rcBInt:
		case rcAInt:
		case rcHex:
			if (HIWORD(wParam) != EN_CHANGE)
				return FALSE;
			if (c->updating)		// prevent infinite recursion during an update
				return FALSE;
			(*(changed[LOWORD(wParam)]))(c);
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

BOOL showColorDialog(HWND parent, struct colorDialogRGBA *c)
{
	switch (DialogBoxParamW(hInstance, MAKEINTRESOURCE(rcColorDialog), parent, colorDialogDlgProc, (LPARAM) c)) {
	case 1:			// cancel
		return FALSE;
	case 2:			// ok
		// make the compiler happy by putting the return after the switch
		break;
	default:
		logLastError(L"error running color dialog");
	}
	return TRUE;
}
