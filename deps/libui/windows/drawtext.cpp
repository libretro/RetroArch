// 22 december 2015
#include "uipriv_windows.hpp"
#include "draw.hpp"
// TODO really migrate

// notes:
// only available in windows 8 and newer:
// - character spacing
// - kerning control
// - justficiation (how could I possibly be making this up?!)
// - vertical text (SERIOUSLY?! WHAT THE ACTUAL FUCK, MICROSOFT?!?!?!? DID YOU NOT THINK ABOUT THIS THE FIRST TIME, TRYING TO IMPROVE THE INTERNATIONALIZATION OF WINDOWS 7?!?!?! bonus: some parts of MSDN even say 8.1 and up only!)

struct uiDrawFontFamilies {
	fontCollection *fc;
};

uiDrawFontFamilies *uiDrawListFontFamilies(void)
{
	struct uiDrawFontFamilies *ff;

	ff = uiNew(struct uiDrawFontFamilies);
	ff->fc = loadFontCollection();
	return ff;
}

int uiDrawFontFamiliesNumFamilies(uiDrawFontFamilies *ff)
{
	return ff->fc->fonts->GetFontFamilyCount();
}

char *uiDrawFontFamiliesFamily(uiDrawFontFamilies *ff, int n)
{
	IDWriteFontFamily *family;
	WCHAR *wname;
	char *name;
	HRESULT hr;

	hr = ff->fc->fonts->GetFontFamily(n, &family);
	if (hr != S_OK)
		logHRESULT(L"error getting font out of collection", hr);
	wname = fontCollectionFamilyName(ff->fc, family);
	name = toUTF8(wname);
	uiFree(wname);
	family->Release();
	return name;
}

void uiDrawFreeFontFamilies(uiDrawFontFamilies *ff)
{
	fontCollectionFree(ff->fc);
	uiFree(ff);
}

struct uiDrawTextFont {
	IDWriteFont *f;
	WCHAR *family;		// save for convenience in uiDrawNewTextLayout()
	double size;
};

uiDrawTextFont *mkTextFont(IDWriteFont *df, BOOL addRef, WCHAR *family, BOOL copyFamily, double size)
{
	uiDrawTextFont *font;
	WCHAR *copy;
	HRESULT hr;

	font = uiNew(uiDrawTextFont);
	font->f = df;
	if (addRef)
		font->f->AddRef();
	if (copyFamily) {
		copy = (WCHAR *) uiAlloc((wcslen(family) + 1) * sizeof (WCHAR), "WCHAR[]");
		wcscpy(copy, family);
		font->family = copy;
	} else
		font->family = family;
	font->size = size;
	return font;
}

// TODO consider moving these all to dwrite.cpp

// TODO MinGW-w64 is missing this one
#define DWRITE_FONT_WEIGHT_SEMI_LIGHT (DWRITE_FONT_WEIGHT(350))
static const struct {
	bool lastOne;
	uiDrawTextWeight uival;
	DWRITE_FONT_WEIGHT dwval;
} dwriteWeights[] = {
	{ false, uiDrawTextWeightThin, DWRITE_FONT_WEIGHT_THIN },
	{ false, uiDrawTextWeightUltraLight, DWRITE_FONT_WEIGHT_ULTRA_LIGHT },
	{ false, uiDrawTextWeightLight, DWRITE_FONT_WEIGHT_LIGHT },
	{ false, uiDrawTextWeightBook, DWRITE_FONT_WEIGHT_SEMI_LIGHT },
	{ false, uiDrawTextWeightNormal, DWRITE_FONT_WEIGHT_NORMAL },
	{ false, uiDrawTextWeightMedium, DWRITE_FONT_WEIGHT_MEDIUM },
	{ false, uiDrawTextWeightSemiBold, DWRITE_FONT_WEIGHT_SEMI_BOLD },
	{ false, uiDrawTextWeightBold, DWRITE_FONT_WEIGHT_BOLD },
	{ false, uiDrawTextWeightUltraBold, DWRITE_FONT_WEIGHT_ULTRA_BOLD },
	{ false, uiDrawTextWeightHeavy, DWRITE_FONT_WEIGHT_HEAVY },
	{ true, uiDrawTextWeightUltraHeavy, DWRITE_FONT_WEIGHT_ULTRA_BLACK, },
};

static const struct {
	bool lastOne;
	uiDrawTextItalic uival;
	DWRITE_FONT_STYLE dwval;
} dwriteItalics[] = {
	{ false, uiDrawTextItalicNormal, DWRITE_FONT_STYLE_NORMAL },
	{ false, uiDrawTextItalicOblique, DWRITE_FONT_STYLE_OBLIQUE },
	{ true, uiDrawTextItalicItalic, DWRITE_FONT_STYLE_ITALIC },
};

static const struct {
	bool lastOne;
	uiDrawTextStretch uival;
	DWRITE_FONT_STRETCH dwval;
} dwriteStretches[] = {
	{ false, uiDrawTextStretchUltraCondensed, DWRITE_FONT_STRETCH_ULTRA_CONDENSED },
	{ false, uiDrawTextStretchExtraCondensed, DWRITE_FONT_STRETCH_EXTRA_CONDENSED },
	{ false, uiDrawTextStretchCondensed, DWRITE_FONT_STRETCH_CONDENSED },
	{ false, uiDrawTextStretchSemiCondensed, DWRITE_FONT_STRETCH_SEMI_CONDENSED },
	{ false, uiDrawTextStretchNormal, DWRITE_FONT_STRETCH_NORMAL },
	{ false, uiDrawTextStretchSemiExpanded, DWRITE_FONT_STRETCH_SEMI_EXPANDED },
	{ false, uiDrawTextStretchExpanded, DWRITE_FONT_STRETCH_EXPANDED },
	{ false, uiDrawTextStretchExtraExpanded, DWRITE_FONT_STRETCH_EXTRA_EXPANDED },
	{ true, uiDrawTextStretchUltraExpanded, DWRITE_FONT_STRETCH_ULTRA_EXPANDED },
};

void attrToDWriteAttr(struct dwriteAttr *attr)
{
	bool found;
	int i;

	found = false;
	for (i = 0; ; i++) {
		if (dwriteWeights[i].uival == attr->weight) {
			attr->dweight = dwriteWeights[i].dwval;
			found = true;
			break;
		}
		if (dwriteWeights[i].lastOne)
			break;
	}
	if (!found)
		userbug("Invalid text weight %d passed to text function.", attr->weight);

	found = false;
	for (i = 0; ; i++) {
		if (dwriteItalics[i].uival == attr->italic) {
			attr->ditalic = dwriteItalics[i].dwval;
			found = true;
			break;
		}
		if (dwriteItalics[i].lastOne)
			break;
	}
	if (!found)
		userbug("Invalid text italic %d passed to text function.", attr->italic);

	found = false;
	for (i = 0; ; i++) {
		if (dwriteStretches[i].uival == attr->stretch) {
			attr->dstretch = dwriteStretches[i].dwval;
			found = true;
			break;
		}
		if (dwriteStretches[i].lastOne)
			break;
	}
	if (!found)
		// TODO on other platforms too
		userbug("Invalid text stretch %d passed to text function.", attr->stretch);
}

void dwriteAttrToAttr(struct dwriteAttr *attr)
{
	int weight, against, n;
	int curdiff, curindex;
	bool found;
	int i;

	// weight is scaled; we need to test to see what's nearest
	weight = (int) (attr->dweight);
	against = (int) (dwriteWeights[0].dwval);
	curdiff = abs(against - weight);
	curindex = 0;
	for (i = 1; ; i++) {
		against = (int) (dwriteWeights[i].dwval);
		n = abs(against - weight);
		if (n < curdiff) {
			curdiff = n;
			curindex = i;
		}
		if (dwriteWeights[i].lastOne)
			break;
	}
	attr->weight = dwriteWeights[i].uival;

	// italic and stretch are simple values; we can just do a matching search
	found = false;
	for (i = 0; ; i++) {
		if (dwriteItalics[i].dwval == attr->ditalic) {
			attr->italic = dwriteItalics[i].uival;
			found = true;
			break;
		}
		if (dwriteItalics[i].lastOne)
			break;
	}
	if (!found)
		// these are implbug()s because users shouldn't be able to get here directly; TODO?
		implbug("invalid italic %d passed to dwriteAttrToAttr()", attr->ditalic);

	found = false;
	for (i = 0; ; i++) {
		if (dwriteStretches[i].dwval == attr->dstretch) {
			attr->stretch = dwriteStretches[i].uival;
			found = true;
			break;
		}
		if (dwriteStretches[i].lastOne)
			break;
	}
	if (!found)
		implbug("invalid stretch %d passed to dwriteAttrToAttr()", attr->dstretch);
}

uiDrawTextFont *uiDrawLoadClosestFont(const uiDrawTextFontDescriptor *desc)
{
	uiDrawTextFont *font;
	IDWriteFontCollection *collection;
	UINT32 index;
	BOOL exists;
	struct dwriteAttr attr;
	IDWriteFontFamily *family;
	WCHAR *wfamily;
	IDWriteFont *match;
	HRESULT hr;

	// always get the latest available font information
	hr = dwfactory->GetSystemFontCollection(&collection, TRUE);
	if (hr != S_OK)
		logHRESULT(L"error getting system font collection", hr);

	wfamily = toUTF16(desc->Family);
	hr = collection->FindFamilyName(wfamily, &index, &exists);
	if (hr != S_OK)
		logHRESULT(L"error finding font family", hr);
	if (!exists)
		implbug("LONGTERM family not found in uiDrawLoadClosestFont()", hr);
	hr = collection->GetFontFamily(index, &family);
	if (hr != S_OK)
		logHRESULT(L"error loading font family", hr);

	attr.weight = desc->Weight;
	attr.italic = desc->Italic;
	attr.stretch = desc->Stretch;
	attrToDWriteAttr(&attr);
	hr = family->GetFirstMatchingFont(
		attr.dweight,
		attr.dstretch,
		attr.ditalic,
		&match);
	if (hr != S_OK)
		logHRESULT(L"error loading font", hr);

	font = mkTextFont(match,
		FALSE,				// we own the initial reference; no need to add another one
		wfamily, FALSE,		// will be freed with font
		desc->Size);

	family->Release();
	collection->Release();

	return font;
}

void uiDrawFreeTextFont(uiDrawTextFont *font)
{
	font->f->Release();
	uiFree(font->family);
	uiFree(font);
}

uintptr_t uiDrawTextFontHandle(uiDrawTextFont *font)
{
	return (uintptr_t) (font->f);
}

void uiDrawTextFontDescribe(uiDrawTextFont *font, uiDrawTextFontDescriptor *desc)
{
	// TODO

	desc->Size = font->size;

	// TODO
}

// text sizes are 1/72 of an inch
// points in Direct2D are 1/96 of an inch (https://msdn.microsoft.com/en-us/library/windows/desktop/ff684173%28v=vs.85%29.aspx, https://msdn.microsoft.com/en-us/library/windows/desktop/hh447022%28v=vs.85%29.aspx)
// As for the actual conversion from design units, see:
// - http://cboard.cprogramming.com/windows-programming/136733-directwrite-font-height-issues.html
// - https://sourceforge.net/p/vstgui/mailman/message/32483143/
// - http://xboxforums.create.msdn.com/forums/t/109445.aspx
// - https://msdn.microsoft.com/en-us/library/dd183564%28v=vs.85%29.aspx
// - http://www.fontbureau.com/blog/the-em/
// TODO make points here about how DIPs in DirectWrite == DIPs in Direct2D; if not, figure out what they really are? for the width and layout functions later
static double scaleUnits(double what, double designUnitsPerEm, double size)
{
	return (what / designUnitsPerEm) * (size * (96.0 / 72.0));
}

void uiDrawTextFontGetMetrics(uiDrawTextFont *font, uiDrawTextFontMetrics *metrics)
{
	DWRITE_FONT_METRICS dm;

	font->f->GetMetrics(&dm);
	metrics->Ascent = scaleUnits(dm.ascent, dm.designUnitsPerEm, font->size);
	metrics->Descent = scaleUnits(dm.descent, dm.designUnitsPerEm, font->size);
	// TODO what happens if dm.xxx is negative?
	// TODO remember what this was for
	metrics->Leading = scaleUnits(dm.lineGap, dm.designUnitsPerEm, font->size);
	metrics->UnderlinePos = scaleUnits(dm.underlinePosition, dm.designUnitsPerEm, font->size);
	metrics->UnderlineThickness = scaleUnits(dm.underlineThickness, dm.designUnitsPerEm, font->size);
}

// some attributes, such as foreground color, can't be applied until after we establish a Direct2D context :/ so we have to prepare all attributes in advance
// also since there's no way to clear the attributes from a layout en masse (apart from overwriting them all), we'll play it safe by creating a new layout each time
enum layoutAttrType {
	layoutAttrColor,
};

struct layoutAttr {
	enum layoutAttrType type;
	int start;
	int end;
	double components[4];
};

struct uiDrawTextLayout {
	WCHAR *text;
	size_t textlen;
	size_t *graphemes;
	double width;
	IDWriteTextFormat *format;
	std::vector<struct layoutAttr> *attrs;
};

uiDrawTextLayout *uiDrawNewTextLayout(const char *text, uiDrawTextFont *defaultFont, double width)
{
	uiDrawTextLayout *layout;
	HRESULT hr;

	layout = uiNew(uiDrawTextLayout);

	hr = dwfactory->CreateTextFormat(defaultFont->family,
		NULL,
		defaultFont->f->GetWeight(),
		defaultFont->f->GetStyle(),
		defaultFont->f->GetStretch(),
		// typographic points are 1/72 inch; this parameter is 1/96 inch
		// fortunately Microsoft does this too, in https://msdn.microsoft.com/en-us/library/windows/desktop/dd371554%28v=vs.85%29.aspx
		defaultFont->size * (96.0 / 72.0),
		// see http://stackoverflow.com/questions/28397971/idwritefactorycreatetextformat-failing and https://msdn.microsoft.com/en-us/library/windows/desktop/dd368203.aspx
		// TODO use the current locale again?
		L"",
		&(layout->format));
	if (hr != S_OK)
		logHRESULT(L"error creating IDWriteTextFormat", hr);

	layout->text = toUTF16(text);
	layout->textlen = wcslen(layout->text);
	layout->graphemes = graphemes(layout->text);

	uiDrawTextLayoutSetWidth(layout, width);

	layout->attrs = new std::vector<struct layoutAttr>;

	return layout;
}

void uiDrawFreeTextLayout(uiDrawTextLayout *layout)
{
	delete layout->attrs;
	layout->format->Release();
	uiFree(layout->graphemes);
	uiFree(layout->text);
	uiFree(layout);
}

static ID2D1SolidColorBrush *mkSolidBrush(ID2D1RenderTarget *rt, double r, double g, double b, double a)
{
	D2D1_BRUSH_PROPERTIES props;
	D2D1_COLOR_F color;
	ID2D1SolidColorBrush *brush;
	HRESULT hr;

	ZeroMemory(&props, sizeof (D2D1_BRUSH_PROPERTIES));
	props.opacity = 1.0;
	// identity matrix
	props.transform._11 = 1;
	props.transform._22 = 1;
	color.r = r;
	color.g = g;
	color.b = b;
	color.a = a;
	hr = rt->CreateSolidColorBrush(
		&color,
		&props,
		&brush);
	if (hr != S_OK)
		logHRESULT(L"error creating solid brush", hr);
	return brush;
}

IDWriteTextLayout *prepareLayout(uiDrawTextLayout *layout, ID2D1RenderTarget *rt)
{
	IDWriteTextLayout *dl;
	DWRITE_TEXT_RANGE range;
	IUnknown *unkBrush;
	DWRITE_WORD_WRAPPING wrap;
	FLOAT maxWidth;
	HRESULT hr;

	hr = dwfactory->CreateTextLayout(layout->text, layout->textlen,
		layout->format,
		// FLOAT is float, not double, so this should work... TODO
		FLT_MAX, FLT_MAX,
		&dl);
	if (hr != S_OK)
		logHRESULT(L"error creating IDWriteTextLayout", hr);

	for (const struct layoutAttr &attr : *(layout->attrs)) {
		range.startPosition = layout->graphemes[attr.start];
		range.length = layout->graphemes[attr.end] - layout->graphemes[attr.start];
		switch (attr.type) {
		case layoutAttrColor:
			if (rt == NULL)		// determining extents, not drawing
				break;
			unkBrush = mkSolidBrush(rt,
				attr.components[0],
				attr.components[1],
				attr.components[2],
				attr.components[3]);
			hr = dl->SetDrawingEffect(unkBrush, range);
			unkBrush->Release();		// associated with dl
			break;
		default:
			hr = E_FAIL;
			logHRESULT(L"invalid text attribute type", hr);
		}
		if (hr != S_OK)
			logHRESULT(L"error adding attribute to text layout", hr);
	}

	// and set the width
	// this is the only wrapping mode (apart from "no wrap") available prior to Windows 8.1
	wrap = DWRITE_WORD_WRAPPING_WRAP;
	maxWidth = layout->width;
	if (layout->width < 0) {
		wrap = DWRITE_WORD_WRAPPING_NO_WRAP;
		// setting the max width in this case technically isn't needed since the wrap mode will simply ignore the max width, but let's do it just to be safe
		maxWidth = FLT_MAX;		// see TODO above
	}
	hr = dl->SetWordWrapping(wrap);
	if (hr != S_OK)
		logHRESULT(L"error setting word wrapping mode", hr);
	hr = dl->SetMaxWidth(maxWidth);
	if (hr != S_OK)
		logHRESULT(L"error setting max layout width", hr);

	return dl;
}


void uiDrawTextLayoutSetWidth(uiDrawTextLayout *layout, double width)
{
	layout->width = width;
}

// TODO for a single line the height includes the leading; it should not
void uiDrawTextLayoutExtents(uiDrawTextLayout *layout, double *width, double *height)
{
	IDWriteTextLayout *dl;
	DWRITE_TEXT_METRICS metrics;
	HRESULT hr;

	dl = prepareLayout(layout, NULL);
	hr = dl->GetMetrics(&metrics);
	if (hr != S_OK)
		logHRESULT(L"error getting layout metrics", hr);
	*width = metrics.width;
	// TODO make sure the behavior of this on empty strings is the same on all platforms
	*height = metrics.height;
	dl->Release();
}

void uiDrawText(uiDrawContext *c, double x, double y, uiDrawTextLayout *layout)
{
	IDWriteTextLayout *dl;
	D2D1_POINT_2F pt;
	ID2D1Brush *black;
	HRESULT hr;

	// TODO document that fully opaque black is the default text color; figure out whether this is upheld in various scenarios on other platforms
	black = mkSolidBrush(c->rt, 0.0, 0.0, 0.0, 1.0);

	dl = prepareLayout(layout, c->rt);
	pt.x = x;
	pt.y = y;
	// TODO D2D1_DRAW_TEXT_OPTIONS_NO_SNAP?
	// TODO D2D1_DRAW_TEXT_OPTIONS_CLIP?
	// TODO when setting 8.1 as minimum, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT?
	c->rt->DrawTextLayout(pt, dl, black, D2D1_DRAW_TEXT_OPTIONS_NONE);
	dl->Release();

	black->Release();
}

void uiDrawTextLayoutSetColor(uiDrawTextLayout *layout, int startChar, int endChar, double r, double g, double b, double a)
{
	struct layoutAttr attr;

	attr.type = layoutAttrColor;
	attr.start = startChar;
	attr.end = endChar;
	attr.components[0] = r;
	attr.components[1] = g;
	attr.components[2] = b;
	attr.components[3] = a;
	layout->attrs->push_back(attr);
}
