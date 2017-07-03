// 7 september 2015
#include "uipriv_windows.hpp"
#include "draw.hpp"

ID2D1Factory *d2dfactory = NULL;

HRESULT initDraw(void)
{
	D2D1_FACTORY_OPTIONS opts;

	ZeroMemory(&opts, sizeof (D2D1_FACTORY_OPTIONS));
	// TODO make this an option
	opts.debugLevel = D2D1_DEBUG_LEVEL_NONE;
	return D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
		IID_ID2D1Factory,
		&opts,
		(void **) (&d2dfactory));
}

void uninitDraw(void)
{
	d2dfactory->Release();
}

ID2D1HwndRenderTarget *makeHWNDRenderTarget(HWND hwnd)
{
	D2D1_RENDER_TARGET_PROPERTIES props;
	D2D1_HWND_RENDER_TARGET_PROPERTIES hprops;
	HDC dc;
	RECT r;
	ID2D1HwndRenderTarget *rt;
	HRESULT hr;

	// we need a DC for the DPI
	// we *could* just use the screen DPI but why when we have a window handle and its DC has a DPI
	dc = GetDC(hwnd);
	if (dc == NULL)
		logLastError(L"error getting DC to find DPI");

	ZeroMemory(&props, sizeof (D2D1_RENDER_TARGET_PROPERTIES));
	props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
	props.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
	props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_UNKNOWN;
	props.dpiX = GetDeviceCaps(dc, LOGPIXELSX);
	props.dpiY = GetDeviceCaps(dc, LOGPIXELSY);
	props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
	props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

	if (ReleaseDC(hwnd, dc) == 0)
		logLastError(L"error releasing DC for finding DPI");

	uiWindowsEnsureGetClientRect(hwnd, &r);

	ZeroMemory(&hprops, sizeof (D2D1_HWND_RENDER_TARGET_PROPERTIES));
	hprops.hwnd = hwnd;
	hprops.pixelSize.width = r.right - r.left;
	hprops.pixelSize.height = r.bottom - r.top;
	// according to Rick Brewster, some drivers will misbehave if we don't specify this (see http://stackoverflow.com/a/33222983/3408572)
	hprops.presentOptions = D2D1_PRESENT_OPTIONS_RETAIN_CONTENTS;

	hr = d2dfactory->CreateHwndRenderTarget(
		&props,
		&hprops,
		&rt);
	if (hr != S_OK)
		logHRESULT(L"error creating HWND render target", hr);
	return rt;
}

ID2D1DCRenderTarget *makeHDCRenderTarget(HDC dc, RECT *r)
{
	D2D1_RENDER_TARGET_PROPERTIES props;
	ID2D1DCRenderTarget *rt;
	HRESULT hr;

	ZeroMemory(&props, sizeof (D2D1_RENDER_TARGET_PROPERTIES));
	props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
	props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	props.dpiX = GetDeviceCaps(dc, LOGPIXELSX);
	props.dpiY = GetDeviceCaps(dc, LOGPIXELSY);
	props.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
	props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

	hr = d2dfactory->CreateDCRenderTarget(&props, &rt);
	if (hr != S_OK)
		logHRESULT(L"error creating DC render target", hr);
	hr = rt->BindDC(dc, r);
	if (hr != S_OK)
		logHRESULT(L"error binding DC to DC render target", hr);
	return rt;
}

static void resetTarget(ID2D1RenderTarget *rt)
{
	D2D1_MATRIX_3X2_F dm;

	// transformations persist
	// reset to the identity matrix
	ZeroMemory(&dm, sizeof (D2D1_MATRIX_3X2_F));
	dm._11 = 1;
	dm._22 = 1;
	rt->SetTransform(&dm);
}

uiDrawContext *newContext(ID2D1RenderTarget *rt)
{
	uiDrawContext *c;

	c = uiNew(uiDrawContext);
	c->rt = rt;
	c->states = new std::vector<struct drawState>;
	resetTarget(c->rt);
	return c;
}

void freeContext(uiDrawContext *c)
{
	if (c->currentClip != NULL)
		c->currentClip->Release();
	if (c->states->size() != 0)
		// TODO do this on other platforms
		userbug("You did not balance uiDrawSave() and uiDrawRestore() calls.");
	delete c->states;
	uiFree(c);
}

static ID2D1Brush *makeSolidBrush(uiDrawBrush *b, ID2D1RenderTarget *rt, D2D1_BRUSH_PROPERTIES *props)
{
	D2D1_COLOR_F color;
	ID2D1SolidColorBrush *brush;
	HRESULT hr;

	color.r = b->R;
	color.g = b->G;
	color.b = b->B;
	color.a = b->A;

	hr = rt->CreateSolidColorBrush(
		&color,
		props,
		&brush);
	if (hr != S_OK)
		logHRESULT(L"error creating solid brush", hr);
	return brush;
}

static ID2D1GradientStopCollection *mkstops(uiDrawBrush *b, ID2D1RenderTarget *rt)
{
	ID2D1GradientStopCollection *s;
	D2D1_GRADIENT_STOP *stops;
	size_t i;
	HRESULT hr;

	stops = (D2D1_GRADIENT_STOP *) uiAlloc(b->NumStops * sizeof (D2D1_GRADIENT_STOP), "D2D1_GRADIENT_STOP[]");
	for (i = 0; i < b->NumStops; i++) {
		stops[i].position = b->Stops[i].Pos;
		stops[i].color.r = b->Stops[i].R;
		stops[i].color.g = b->Stops[i].G;
		stops[i].color.b = b->Stops[i].B;
		stops[i].color.a = b->Stops[i].A;
	}

	hr = rt->CreateGradientStopCollection(
		stops,
		b->NumStops,
		D2D1_GAMMA_2_2,			// this is the default for the C++-only overload of ID2D1RenderTarget::GradientStopCollection()
		D2D1_EXTEND_MODE_CLAMP,
		&s);
	if (hr != S_OK)
		logHRESULT(L"error creating stop collection", hr);

	uiFree(stops);
	return s;
}

static ID2D1Brush *makeLinearBrush(uiDrawBrush *b, ID2D1RenderTarget *rt, D2D1_BRUSH_PROPERTIES *props)
{
	ID2D1LinearGradientBrush *brush;
	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gprops;
	ID2D1GradientStopCollection *stops;
	HRESULT hr;

	ZeroMemory(&gprops, sizeof (D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES));
	gprops.startPoint.x = b->X0;
	gprops.startPoint.y = b->Y0;
	gprops.endPoint.x = b->X1;
	gprops.endPoint.y = b->Y1;

	stops = mkstops(b, rt);

	hr = rt->CreateLinearGradientBrush(
		&gprops,
		props,
		stops,
		&brush);
	if (hr != S_OK)
		logHRESULT(L"error creating gradient brush", hr);

	// the example at https://msdn.microsoft.com/en-us/library/windows/desktop/dd756682%28v=vs.85%29.aspx says this is safe to do now
	stops->Release();
	return brush;
}

static ID2D1Brush *makeRadialBrush(uiDrawBrush *b, ID2D1RenderTarget *rt, D2D1_BRUSH_PROPERTIES *props)
{
	ID2D1RadialGradientBrush *brush;
	D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES gprops;
	ID2D1GradientStopCollection *stops;
	HRESULT hr;

	ZeroMemory(&gprops, sizeof (D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES));
	gprops.gradientOriginOffset.x = b->X0 - b->X1;
	gprops.gradientOriginOffset.y = b->Y0 - b->Y1;
	gprops.center.x = b->X1;
	gprops.center.y = b->Y1;
	gprops.radiusX = b->OuterRadius;
	gprops.radiusY = b->OuterRadius;

	stops = mkstops(b, rt);

	hr = rt->CreateRadialGradientBrush(
		&gprops,
		props,
		stops,
		&brush);
	if (hr != S_OK)
		logHRESULT(L"error creating gradient brush", hr);

	stops->Release();
	return brush;
}

static ID2D1Brush *makeBrush(uiDrawBrush *b, ID2D1RenderTarget *rt)
{
	D2D1_BRUSH_PROPERTIES props;

	ZeroMemory(&props, sizeof (D2D1_BRUSH_PROPERTIES));
	props.opacity = 1.0;
	// identity matrix
	props.transform._11 = 1;
	props.transform._22 = 1;

	switch (b->Type) {
	case uiDrawBrushTypeSolid:
		return makeSolidBrush(b, rt, &props);
	case uiDrawBrushTypeLinearGradient:
		return makeLinearBrush(b, rt, &props);
	case uiDrawBrushTypeRadialGradient:
		return makeRadialBrush(b, rt, &props);
//	case uiDrawBrushTypeImage:
//		TODO
	}

	// TODO do this on all platforms
	userbug("Invalid brush type %d given to drawing operation.", b->Type);
	// TODO dummy brush?
	return NULL;		// make compiler happy
}

// how clipping works:
// every fill and stroke is done on a temporary layer with the clip geometry applied to it
// this is really the only way to clip in Direct2D that doesn't involve opacity images
// reference counting:
// - initially the clip is NULL, which means do not use a layer
// - the first time uiDrawClip() is called, we take a reference on the path passed in (this is also why uiPathEnd() is needed)
// - every successive time, we create a new PathGeometry and merge the current clip with the new path, releasing the reference we took earlier and taking a reference to the new one
// - in Save, we take another reference; in Restore we drop the refernece to the existing path geometry and transfer that saved ref to the new path geometry over to the context
// uiDrawFreePath() doesn't destroy the path geometry, it just drops the reference count, so a clip can exist independent of its path

static ID2D1Layer *applyClip(uiDrawContext *c)
{
	ID2D1Layer *layer;
	D2D1_LAYER_PARAMETERS params;
	HRESULT hr;

	// if no clip, don't do anything
	if (c->currentClip == NULL)
		return NULL;

	// create a layer for clipping
	// we have to explicitly make the layer because we're still targeting Windows 7
	hr = c->rt->CreateLayer(NULL, &layer);
	if (hr != S_OK)
		logHRESULT(L"error creating clip layer", hr);

	// apply it as the clip
	ZeroMemory(&params, sizeof (D2D1_LAYER_PARAMETERS));
	// this is the equivalent of InfiniteRect() in d2d1helper.h
	params.contentBounds.left = -FLT_MAX;
	params.contentBounds.top = -FLT_MAX;
	params.contentBounds.right = FLT_MAX;
	params.contentBounds.bottom = FLT_MAX;
	params.geometricMask = (ID2D1Geometry *) (c->currentClip);
	// TODO is this correct?
	params.maskAntialiasMode = c->rt->GetAntialiasMode();
	// identity matrix
	params.maskTransform._11 = 1;
	params.maskTransform._22 = 1;
	params.opacity = 1.0;
	params.opacityBrush = NULL;
	params.layerOptions = D2D1_LAYER_OPTIONS_NONE;
	// TODO is this correct?
	if (c->rt->GetTextAntialiasMode() == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE)
		params.layerOptions = D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE;
	c->rt->PushLayer(&params, layer);

	// return the layer so it can be freed later
	return layer;
}

static void unapplyClip(uiDrawContext *c, ID2D1Layer *layer)
{
	if (layer == NULL)
		return;
	c->rt->PopLayer();
	layer->Release();
}

void uiDrawStroke(uiDrawContext *c, uiDrawPath *p, uiDrawBrush *b, uiDrawStrokeParams *sp)
{
	ID2D1Brush *brush;
	ID2D1StrokeStyle *style;
	D2D1_STROKE_STYLE_PROPERTIES dsp;
	FLOAT *dashes;
	size_t i;
	ID2D1Layer *cliplayer;
	HRESULT hr;

	brush = makeBrush(b, c->rt);

	ZeroMemory(&dsp, sizeof (D2D1_STROKE_STYLE_PROPERTIES));
	switch (sp->Cap) {
	case uiDrawLineCapFlat:
		dsp.startCap = D2D1_CAP_STYLE_FLAT;
		dsp.endCap = D2D1_CAP_STYLE_FLAT;
		dsp.dashCap = D2D1_CAP_STYLE_FLAT;
		break;
	case uiDrawLineCapRound:
		dsp.startCap = D2D1_CAP_STYLE_ROUND;
		dsp.endCap = D2D1_CAP_STYLE_ROUND;
		dsp.dashCap = D2D1_CAP_STYLE_ROUND;
		break;
	case uiDrawLineCapSquare:
		dsp.startCap = D2D1_CAP_STYLE_SQUARE;
		dsp.endCap = D2D1_CAP_STYLE_SQUARE;
		dsp.dashCap = D2D1_CAP_STYLE_SQUARE;
		break;
	}
	switch (sp->Join) {
	case uiDrawLineJoinMiter:
		dsp.lineJoin = D2D1_LINE_JOIN_MITER_OR_BEVEL;
		dsp.miterLimit = sp->MiterLimit;
		break;
	case uiDrawLineJoinRound:
		dsp.lineJoin = D2D1_LINE_JOIN_ROUND;
		break;
	case uiDrawLineJoinBevel:
		dsp.lineJoin = D2D1_LINE_JOIN_BEVEL;
		break;
	}
	dsp.dashStyle = D2D1_DASH_STYLE_SOLID;
	dashes = NULL;
	// note that dash widths and the dash phase are scaled up by the thickness by Direct2D
	// TODO be sure to formally document this
	if (sp->NumDashes != 0) {
		dsp.dashStyle = D2D1_DASH_STYLE_CUSTOM;
		dashes = (FLOAT *) uiAlloc(sp->NumDashes * sizeof (FLOAT), "FLOAT[]");
		for (i = 0; i < sp->NumDashes; i++)
			dashes[i] = sp->Dashes[i] / sp->Thickness;
	}
	dsp.dashOffset = sp->DashPhase / sp->Thickness;
	hr = d2dfactory->CreateStrokeStyle(
		&dsp,
		dashes,
		sp->NumDashes,
		&style);
	if (hr != S_OK)
		logHRESULT(L"error creating stroke style", hr);
	if (sp->NumDashes != 0)
		uiFree(dashes);

	cliplayer = applyClip(c);
	c->rt->DrawGeometry(
		pathGeometry(p),
		brush,
		sp->Thickness,
		style);
	unapplyClip(c, cliplayer);

	style->Release();
	brush->Release();
}

void uiDrawFill(uiDrawContext *c, uiDrawPath *p, uiDrawBrush *b)
{
	ID2D1Brush *brush;
	ID2D1Layer *cliplayer;

	brush = makeBrush(b, c->rt);
	cliplayer = applyClip(c);
	c->rt->FillGeometry(
		pathGeometry(p),
		brush,
		NULL);
	unapplyClip(c, cliplayer);
	brush->Release();
}

void uiDrawTransform(uiDrawContext *c, uiDrawMatrix *m)
{
	D2D1_MATRIX_3X2_F dm, cur;

	c->rt->GetTransform(&cur);
	m2d(m, &dm);
	// you would think we have to do already * m, right?
	// WRONG! we have to do m * already
	// why? a few reasons
	// a) this lovely comment in cairo's source - http://cgit.freedesktop.org/cairo/tree/src/cairo-matrix.c?id=0537479bd1d4c5a3bc0f6f41dec4deb98481f34a#n330
	// 	Direct2D uses column vectors and I don't know if this is even documented
	// b) that's what Core Graphics does
	// TODO see if Microsoft says to do this
	dm = dm * cur;		// for whatever reason operator * is defined but not operator *=
	c->rt->SetTransform(&dm);
}

void uiDrawClip(uiDrawContext *c, uiDrawPath *path)
{
	ID2D1PathGeometry *newPath;
	ID2D1GeometrySink *newSink;
	HRESULT hr;

	// if there's no current clip, borrow the path
	if (c->currentClip == NULL) {
		c->currentClip = pathGeometry(path);
		// we have to take our own reference to that clip
		c->currentClip->AddRef();
		return;
	}

	// otherwise we have to intersect the current path with the new one
	// we do that into a new path, and then replace c->currentClip with that new path
	hr = d2dfactory->CreatePathGeometry(&newPath);
	if (hr != S_OK)
		logHRESULT(L"error creating new path", hr);
	hr = newPath->Open(&newSink);
	if (hr != S_OK)
		logHRESULT(L"error opening new path", hr);
	hr = c->currentClip->CombineWithGeometry(
		pathGeometry(path),
		D2D1_COMBINE_MODE_INTERSECT,
		NULL,
		// TODO is this correct or can this be set per target?
		D2D1_DEFAULT_FLATTENING_TOLERANCE,
		newSink);
	if (hr != S_OK)
		logHRESULT(L"error intersecting old path with new path", hr);
	hr = newSink->Close();
	if (hr != S_OK)
		logHRESULT(L"error closing new path", hr);
	newSink->Release();

	// okay we have the new clip; we just need to replace the old one with it
	c->currentClip->Release();
	c->currentClip = newPath;
	// we have a reference already; no need for another
}

struct drawState {
	ID2D1DrawingStateBlock *dsb;
	ID2D1PathGeometry *clip;
};

void uiDrawSave(uiDrawContext *c)
{
	struct drawState state;
	HRESULT hr;

	hr = d2dfactory->CreateDrawingStateBlock(
		// TODO verify that these are correct
		NULL,
		NULL,
		&(state.dsb));
	if (hr != S_OK)
		logHRESULT(L"error creating drawing state block", hr);
	c->rt->SaveDrawingState(state.dsb);

	// if we have a clip, we need to hold another reference to it
	if (c->currentClip != NULL)
		c->currentClip->AddRef();
	state.clip = c->currentClip;		// even if NULL assign it

	c->states->push_back(state);
}

void uiDrawRestore(uiDrawContext *c)
{
	struct drawState state;

	state = (*(c->states))[c->states->size() - 1];
	c->states->pop_back();

	c->rt->RestoreDrawingState(state.dsb);
	state.dsb->Release();

	// if we have a current clip, we need to drop it
	if (c->currentClip != NULL)
		c->currentClip->Release();
	// no need to explicitly addref or release; just transfer the ref
	c->currentClip = state.clip;
}
