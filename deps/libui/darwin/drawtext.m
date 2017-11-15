// 6 september 2015
#import "uipriv_darwin.h"

// TODO
#define complain(...) implbug(__VA_ARGS__)

// TODO double-check that we are properly handling allocation failures (or just toll free bridge from cocoa)
struct uiDrawFontFamilies {
	CFArrayRef fonts;
};

uiDrawFontFamilies *uiDrawListFontFamilies(void)
{
	uiDrawFontFamilies *ff;

	ff = uiNew(uiDrawFontFamilies);
	ff->fonts = CTFontManagerCopyAvailableFontFamilyNames();
	if (ff->fonts == NULL)
		implbug("error getting available font names (no reason specified) (TODO)");
	return ff;
}

int uiDrawFontFamiliesNumFamilies(uiDrawFontFamilies *ff)
{
	return CFArrayGetCount(ff->fonts);
}

char *uiDrawFontFamiliesFamily(uiDrawFontFamilies *ff, int n)
{
	CFStringRef familystr;
	char *family;

	familystr = (CFStringRef) CFArrayGetValueAtIndex(ff->fonts, n);
	// toll-free bridge
	family = uiDarwinNSStringToText((NSString *) familystr);
	// Get Rule means we do not free familystr
	return family;
}

void uiDrawFreeFontFamilies(uiDrawFontFamilies *ff)
{
	CFRelease(ff->fonts);
	uiFree(ff);
}

struct uiDrawTextFont {
	CTFontRef f;
};

uiDrawTextFont *mkTextFont(CTFontRef f, BOOL retain)
{
	uiDrawTextFont *font;

	font = uiNew(uiDrawTextFont);
	font->f = f;
	if (retain)
		CFRetain(font->f);
	return font;
}

uiDrawTextFont *mkTextFontFromNSFont(NSFont *f)
{
	// toll-free bridging; we do retain, though
	return mkTextFont((CTFontRef) f, YES);
}

static CFMutableDictionaryRef newAttrList(void)
{
	CFMutableDictionaryRef attr;

	attr = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (attr == NULL)
		complain("error creating attribute dictionary in newAttrList()()");
	return attr;
}

static void addFontFamilyAttr(CFMutableDictionaryRef attr, const char *family)
{
	CFStringRef cfstr;

	cfstr = CFStringCreateWithCString(NULL, family, kCFStringEncodingUTF8);
	if (cfstr == NULL)
		complain("error creating font family name CFStringRef in addFontFamilyAttr()");
	CFDictionaryAddValue(attr, kCTFontFamilyNameAttribute, cfstr);
	CFRelease(cfstr);			// dictionary holds its own reference
}

static void addFontSizeAttr(CFMutableDictionaryRef attr, double size)
{
	CFNumberRef n;

	n = CFNumberCreate(NULL, kCFNumberDoubleType, &size);
	CFDictionaryAddValue(attr, kCTFontSizeAttribute, n);
	CFRelease(n);
}

#if 0
TODO
// See http://stackoverflow.com/questions/4810409/does-coretext-support-small-caps/4811371#4811371 and https://git.gnome.org/browse/pango/tree/pango/pangocoretext-fontmap.c for what these do
// And fortunately, unlike the traits (see below), unmatched features are simply ignored without affecting the other features :D
static void addFontSmallCapsAttr(CFMutableDictionaryRef attr)
{
	CFMutableArrayRef outerArray;
	CFMutableDictionaryRef innerDict;
	CFNumberRef numType, numSelector;
	int num;

	outerArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (outerArray == NULL)
		complain("error creating outer CFArray for adding small caps attributes in addFontSmallCapsAttr()");

	// Apple's headers say these are deprecated, but a few fonts still rely on them
	num = kLetterCaseType;
	numType = CFNumberCreate(NULL, kCFNumberIntType, &num);
	num = kSmallCapsSelector;
	numSelector = CFNumberCreate(NULL, kCFNumberIntType, &num);
	innerDict = newAttrList();
	CFDictionaryAddValue(innerDict, kCTFontFeatureTypeIdentifierKey, numType);
	CFRelease(numType);
	CFDictionaryAddValue(innerDict, kCTFontFeatureSelectorIdentifierKey, numSelector);
	CFRelease(numSelector);
	CFArrayAppendValue(outerArray, innerDict);
	CFRelease(innerDict);		// and likewise for CFArray

	// these are the non-deprecated versions of the above; some fonts have these instead
	num = kLowerCaseType;
	numType = CFNumberCreate(NULL, kCFNumberIntType, &num);
	num = kLowerCaseSmallCapsSelector;
	numSelector = CFNumberCreate(NULL, kCFNumberIntType, &num);
	innerDict = newAttrList();
	CFDictionaryAddValue(innerDict, kCTFontFeatureTypeIdentifierKey, numType);
	CFRelease(numType);
	CFDictionaryAddValue(innerDict, kCTFontFeatureSelectorIdentifierKey, numSelector);
	CFRelease(numSelector);
	CFArrayAppendValue(outerArray, innerDict);
	CFRelease(innerDict);		// and likewise for CFArray

	CFDictionaryAddValue(attr, kCTFontFeatureSettingsAttribute, outerArray);
	CFRelease(outerArray);
}
#endif

// Named constants for these were NOT added until 10.11, and even then they were added as external symbols instead of macros, so we can't use them directly :(
// kode54 got these for me before I had access to El Capitan; thanks to him.
#define ourNSFontWeightUltraLight -0.800000
#define ourNSFontWeightThin -0.600000
#define ourNSFontWeightLight -0.400000
#define ourNSFontWeightRegular 0.000000
#define ourNSFontWeightMedium 0.230000
#define ourNSFontWeightSemibold 0.300000
#define ourNSFontWeightBold 0.400000
#define ourNSFontWeightHeavy 0.560000
#define ourNSFontWeightBlack 0.620000
static const CGFloat ctWeights[] = {
	// yeah these two have their names swapped; blame Pango
	[uiDrawTextWeightThin] = ourNSFontWeightUltraLight,
	[uiDrawTextWeightUltraLight] = ourNSFontWeightThin,
	[uiDrawTextWeightLight] = ourNSFontWeightLight,
	// for this one let's go between Light and Regular
	// we're doing nearest so if there happens to be an exact value hopefully it's close enough
	[uiDrawTextWeightBook] = ourNSFontWeightLight + ((ourNSFontWeightRegular - ourNSFontWeightLight) / 2),
	[uiDrawTextWeightNormal] = ourNSFontWeightRegular,
	[uiDrawTextWeightMedium] = ourNSFontWeightMedium,
	[uiDrawTextWeightSemiBold] = ourNSFontWeightSemibold,
	[uiDrawTextWeightBold] = ourNSFontWeightBold,
	// for this one let's go between Bold and Heavy
	[uiDrawTextWeightUltraBold] = ourNSFontWeightBold + ((ourNSFontWeightHeavy - ourNSFontWeightBold) / 2),
	[uiDrawTextWeightHeavy] = ourNSFontWeightHeavy,
	[uiDrawTextWeightUltraHeavy] = ourNSFontWeightBlack,
};

// Unfortunately there are still no named constants for these.
// Let's just use normalized widths.
// As far as I can tell (OS X only ships with condensed fonts, not expanded fonts; TODO), regardless of condensed or expanded, negative means condensed and positive means expanded.
// TODO verify this is correct
static const CGFloat ctStretches[] = {
	[uiDrawTextStretchUltraCondensed] = -1.0,
	[uiDrawTextStretchExtraCondensed] = -0.75,
	[uiDrawTextStretchCondensed] = -0.5,
	[uiDrawTextStretchSemiCondensed] = -0.25,
	[uiDrawTextStretchNormal] = 0.0,
	[uiDrawTextStretchSemiExpanded] = 0.25,
	[uiDrawTextStretchExpanded] = 0.5,
	[uiDrawTextStretchExtraExpanded] = 0.75,
	[uiDrawTextStretchUltraExpanded] = 1.0,
};

struct closeness {
	CFIndex index;
	CGFloat weight;
	CGFloat italic;
	CGFloat stretch;
	CGFloat distance;
};

// Stupidity: CTFont requires an **exact match for the entire traits dictionary**, otherwise it will **drop ALL the traits**.
// We have to implement the closest match ourselves.
// Also we have to do this before adding the small caps flags, because the matching descriptors won't have those.
CTFontDescriptorRef matchTraits(CTFontDescriptorRef against, uiDrawTextWeight weight, uiDrawTextItalic italic, uiDrawTextStretch stretch)
{
	CGFloat targetWeight;
	CGFloat italicCloseness, obliqueCloseness, normalCloseness;
	CGFloat targetStretch;
	CFArrayRef matching;
	CFIndex i, n;
	struct closeness *closeness;
	CTFontDescriptorRef current;
	CTFontDescriptorRef out;

	targetWeight = ctWeights[weight];
	switch (italic) {
	case uiDrawTextItalicNormal:
		italicCloseness = 1;
		obliqueCloseness = 1;
		normalCloseness = 0;
		break;
	case uiDrawTextItalicOblique:
		italicCloseness = 0.5;
		obliqueCloseness = 0;
		normalCloseness = 1;
		break;
	case uiDrawTextItalicItalic:
		italicCloseness = 0;
		obliqueCloseness = 0.5;
		normalCloseness = 1;
		break;
	}
	targetStretch = ctStretches[stretch];

	matching = CTFontDescriptorCreateMatchingFontDescriptors(against, NULL);
	if (matching == NULL)
		// no matches; give the original back and hope for the best
		return against;
	n = CFArrayGetCount(matching);
	if (n == 0) {
		// likewise
		CFRelease(matching);
		return against;
	}

	closeness = (struct closeness *) uiAlloc(n * sizeof (struct closeness), "struct closeness[]");
	for (i = 0; i < n; i++) {
		CFDictionaryRef traits;
		CFNumberRef cfnum;
		CTFontSymbolicTraits symbolic;

		closeness[i].index = i;

		current = CFArrayGetValueAtIndex(matching, i);
		traits = CTFontDescriptorCopyAttribute(current, kCTFontTraitsAttribute);
		if (traits == NULL) {
			// couldn't get traits; be safe by ranking it lowest
			// LONGTERM figure out what the longest possible distances are
			closeness[i].weight = 3;
			closeness[i].italic = 2;
			closeness[i].stretch = 3;
			continue;
		}

		symbolic = 0;			// assume no symbolic traits if none are listed
		cfnum = CFDictionaryGetValue(traits, kCTFontSymbolicTrait);
		if (cfnum != NULL) {
			SInt32 s;

			if (CFNumberGetValue(cfnum, kCFNumberSInt32Type, &s) == false)
				complain("error getting symbolic traits in matchTraits()");
			symbolic = (CTFontSymbolicTraits) s;
			// Get rule; do not release cfnum
		}

		// now try weight
		cfnum = CFDictionaryGetValue(traits, kCTFontWeightTrait);
		if (cfnum != NULL) {
			CGFloat val;

			// LONGTERM instead of complaining for this and width and possibly also symbolic traits above, should we just fall through to the default?
			if (CFNumberGetValue(cfnum, kCFNumberCGFloatType, &val) == false)
				complain("error getting weight value in matchTraits()");
			closeness[i].weight = val - targetWeight;
		} else
			// okay there's no weight key; let's try the literal meaning of the symbolic constant
			// LONGTERM is the weight key guaranteed?
			if ((symbolic & kCTFontBoldTrait) != 0)
				closeness[i].weight = ourNSFontWeightBold - targetWeight;
			else
				closeness[i].weight = ourNSFontWeightRegular - targetWeight;

		// italics is a bit harder because Core Text doesn't expose a concept of obliqueness
		// Pango just does a g_strrstr() (backwards case-sensitive search) for "Oblique" in the font's style name (see https://git.gnome.org/browse/pango/tree/pango/pangocoretext-fontmap.c); let's do that too I guess
		if ((symbolic & kCTFontItalicTrait) != 0)
			closeness[i].italic = italicCloseness;
		else {
			CFStringRef styleName;
			BOOL isOblique;

			isOblique = NO;		// default value
			styleName = CTFontDescriptorCopyAttribute(current, kCTFontStyleNameAttribute);
			if (styleName != NULL) {
				CFRange range;

				// note the use of the toll-free bridge for the string literal, since CFSTR() *can* return NULL
				range = CFStringFind(styleName, (CFStringRef) @"Oblique", kCFCompareBackwards);
				if (range.location != kCFNotFound)
					isOblique = YES;
				CFRelease(styleName);
			}
			if (isOblique)
				closeness[i].italic = obliqueCloseness;
			else
				closeness[i].italic = normalCloseness;
		}

		// now try width
		// TODO this does not seem to be enough for Skia's extended variants; the width trait is 0 but the Expanded flag is on
		// TODO verify the rest of this matrix (what matrix?)
		cfnum = CFDictionaryGetValue(traits, kCTFontWidthTrait);
		if (cfnum != NULL) {
			CGFloat val;

			if (CFNumberGetValue(cfnum, kCFNumberCGFloatType, &val) == false)
				complain("error getting width value in matchTraits()");
			closeness[i].stretch = val - targetStretch;
		} else
			// okay there's no width key; let's try the literal meaning of the symbolic constant
			// LONGTERM is the width key guaranteed?
			if ((symbolic & kCTFontExpandedTrait) != 0)
				closeness[i].stretch = 1.0 - targetStretch;
			else if ((symbolic & kCTFontCondensedTrait) != 0)
				closeness[i].stretch = -1.0 - targetStretch;
			else
				closeness[i].stretch = 0.0 - targetStretch;

		CFRelease(traits);
	}

	// now figure out the 3-space difference between the three and sort by that
	for (i = 0; i < n; i++) {
		CGFloat weight, italic, stretch;

		weight = closeness[i].weight;
		weight *= weight;
		italic = closeness[i].italic;
		italic *= italic;
		stretch = closeness[i].stretch;
		stretch *= stretch;
		closeness[i].distance = sqrt(weight + italic + stretch);
	}
	qsort_b(closeness, n, sizeof (struct closeness), ^(const void *aa, const void *bb) {
		const struct closeness *a = (const struct closeness *) aa;
		const struct closeness *b = (const struct closeness *) bb;

		// via http://www.gnu.org/software/libc/manual/html_node/Comparison-Functions.html#Comparison-Functions
		// LONGTERM is this really the best way? isn't it the same as if (*a < *b) return -1; if (*a > *b) return 1; return 0; ?
		return (a->distance > b->distance) - (a->distance < b->distance);
	});
	// and the first element of the sorted array is what we want
	out = CFArrayGetValueAtIndex(matching, closeness[0].index);
	CFRetain(out);			// get rule

	// release everything
	uiFree(closeness);
	CFRelease(matching);
	// and release the original descriptor since we no longer need it
	CFRelease(against);

	return out;
}

// Now remember what I said earlier about having to add the small caps traits after calling the above? This gets a dictionary back so we can do so.
CFMutableDictionaryRef extractAttributes(CTFontDescriptorRef desc)
{
	CFDictionaryRef dict;
	CFMutableDictionaryRef mdict;

	dict = CTFontDescriptorCopyAttributes(desc);
	// this might not be mutable, so make a mutable copy
	mdict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	CFRelease(dict);
	return mdict;
}

uiDrawTextFont *uiDrawLoadClosestFont(const uiDrawTextFontDescriptor *desc)
{
	CTFontRef f;
	CFMutableDictionaryRef attr;
	CTFontDescriptorRef cfdesc;

	attr = newAttrList();
	addFontFamilyAttr(attr, desc->Family);
	addFontSizeAttr(attr, desc->Size);

	// now we have to do the traits matching, so create a descriptor, match the traits, and then get the attributes back
	cfdesc = CTFontDescriptorCreateWithAttributes(attr);
	// TODO release attr?
	cfdesc = matchTraits(cfdesc, desc->Weight, desc->Italic, desc->Stretch);

	// specify the initial size again just to be safe
	f = CTFontCreateWithFontDescriptor(cfdesc, desc->Size, NULL);
	// TODO release cfdesc?

	return mkTextFont(f, NO);		// we hold the initial reference; no need to retain again
}

void uiDrawFreeTextFont(uiDrawTextFont *font)
{
	CFRelease(font->f);
	uiFree(font);
}

uintptr_t uiDrawTextFontHandle(uiDrawTextFont *font)
{
	return (uintptr_t) (font->f);
}

void uiDrawTextFontDescribe(uiDrawTextFont *font, uiDrawTextFontDescriptor *desc)
{
	// TODO
}

// text sizes and user space points are identical:
// - https://developer.apple.com/library/mac/documentation/TextFonts/Conceptual/CocoaTextArchitecture/TypoFeatures/TextSystemFeatures.html#//apple_ref/doc/uid/TP40009459-CH6-51627-BBCCHIFF text points are 72 per inch
// - https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/CocoaDrawingGuide/Transforms/Transforms.html#//apple_ref/doc/uid/TP40003290-CH204-SW5 user space points are 72 per inch
void uiDrawTextFontGetMetrics(uiDrawTextFont *font, uiDrawTextFontMetrics *metrics)
{
	metrics->Ascent = CTFontGetAscent(font->f);
	metrics->Descent = CTFontGetDescent(font->f);
	metrics->Leading = CTFontGetLeading(font->f);
	metrics->UnderlinePos = CTFontGetUnderlinePosition(font->f);
	metrics->UnderlineThickness = CTFontGetUnderlineThickness(font->f);
}

struct uiDrawTextLayout {
	CFMutableAttributedStringRef mas;
	CFRange *charsToRanges;
	double width;
};

uiDrawTextLayout *uiDrawNewTextLayout(const char *str, uiDrawTextFont *defaultFont, double width)
{
	uiDrawTextLayout *layout;
	CFAttributedStringRef immutable;
	CFMutableDictionaryRef attr;
	CFStringRef backing;
	CFIndex i, j, n;

	layout = uiNew(uiDrawTextLayout);

	// TODO docs say we need to use a different set of key callbacks
	// TODO see if the font attribute key callbacks need to be the same
	attr = newAttrList();
	// this will retain defaultFont->f; no need to worry
	CFDictionaryAddValue(attr, kCTFontAttributeName, defaultFont->f);

	immutable = CFAttributedStringCreate(NULL, (CFStringRef) [NSString stringWithUTF8String:str], attr);
	if (immutable == NULL)
		complain("error creating immutable attributed string in uiDrawNewTextLayout()");
	CFRelease(attr);

	layout->mas = CFAttributedStringCreateMutableCopy(NULL, 0, immutable);
	if (layout->mas == NULL)
		complain("error creating attributed string in uiDrawNewTextLayout()");
	CFRelease(immutable);

	uiDrawTextLayoutSetWidth(layout, width);

	// unfortunately the CFRanges for attributes expect UTF-16 codepoints
	// we want graphemes
	// fortunately CFStringGetRangeOfComposedCharactersAtIndex() is here for us
	// https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/Strings/Articles/stringsClusters.html says that this does work on all multi-codepoint graphemes (despite the name), and that this is the preferred function for this particular job anyway
	backing = CFAttributedStringGetString(layout->mas);
	n = CFStringGetLength(backing);
	// allocate one extra, just to be safe
	layout->charsToRanges = (CFRange *) uiAlloc((n + 1) * sizeof (CFRange), "CFRange[]");
	i = 0;
	j = 0;
	while (i < n) {
		CFRange range;

		range = CFStringGetRangeOfComposedCharactersAtIndex(backing, i);
		i = range.location + range.length;
		layout->charsToRanges[j] = range;
		j++;
	}
	// and set the last one
	layout->charsToRanges[j].location = i;
	layout->charsToRanges[j].length = 0;

	return layout;
}

void uiDrawFreeTextLayout(uiDrawTextLayout *layout)
{
	uiFree(layout->charsToRanges);
	CFRelease(layout->mas);
	uiFree(layout);
}

void uiDrawTextLayoutSetWidth(uiDrawTextLayout *layout, double width)
{
	layout->width = width;
}

struct framesetter {
	CTFramesetterRef fs;
	CFMutableDictionaryRef frameAttrib;
	CGSize extents;
};

// TODO CTFrameProgression for RTL/LTR
// TODO kCTParagraphStyleSpecifierMaximumLineSpacing, kCTParagraphStyleSpecifierMinimumLineSpacing, kCTParagraphStyleSpecifierLineSpacingAdjustment for line spacing
static void mkFramesetter(uiDrawTextLayout *layout, struct framesetter *fs)
{
	CFRange fitRange;
	CGFloat width;

	fs->fs = CTFramesetterCreateWithAttributedString(layout->mas);
	if (fs->fs == NULL)
		complain("error creating CTFramesetter object in mkFramesetter()");

	// TODO kCTFramePathWidthAttributeName?
	fs->frameAttrib = NULL;

	width = layout->width;
	if (layout->width < 0)
		width = CGFLOAT_MAX;
	// TODO these seem to be floor()'d or truncated?
	fs->extents = CTFramesetterSuggestFrameSizeWithConstraints(fs->fs,
		CFRangeMake(0, 0),
		fs->frameAttrib,
		CGSizeMake(width, CGFLOAT_MAX),
		&fitRange);		// not documented as accepting NULL
}

static void freeFramesetter(struct framesetter *fs)
{
	if (fs->frameAttrib != NULL)
		CFRelease(fs->frameAttrib);
	CFRelease(fs->fs);
}

// LONGTERM allow line separation and leading to be factored into a wrapping text layout

// TODO reconcile differences in character wrapping on platforms
void uiDrawTextLayoutExtents(uiDrawTextLayout *layout, double *width, double *height)
{
	struct framesetter fs;

	mkFramesetter(layout, &fs);
	*width = fs.extents.width;
	*height = fs.extents.height;
	freeFramesetter(&fs);
}

// Core Text doesn't draw onto a flipped view correctly; we have to do this
// see the iOS bits of the first example at https://developer.apple.com/library/mac/documentation/StringsTextFonts/Conceptual/CoreText_Programming/LayoutOperations/LayoutOperations.html#//apple_ref/doc/uid/TP40005533-CH12-SW1 (iOS is naturally flipped)
// TODO how is this affected by the CTM?
static void prepareContextForText(CGContextRef c, CGFloat cheight, double *y)
{
	CGContextSaveGState(c);
	CGContextTranslateCTM(c, 0, cheight);
	CGContextScaleCTM(c, 1.0, -1.0);
	CGContextSetTextMatrix(c, CGAffineTransformIdentity);

	// wait, that's not enough; we need to offset y values to account for our new flipping
	*y = cheight - *y;
}

// TODO placement is incorrect for Helvetica
void doDrawText(CGContextRef c, CGFloat cheight, double x, double y, uiDrawTextLayout *layout)
{
	struct framesetter fs;
	CGRect rect;
	CGPathRef path;
	CTFrameRef frame;

	prepareContextForText(c, cheight, &y);
	mkFramesetter(layout, &fs);

	// oh, and since we're flipped, y is the bottom-left coordinate of the rectangle, not the top-left
	// since we are flipped, we subtract
	y -= fs.extents.height;

	rect.origin = CGPointMake(x, y);
	rect.size = fs.extents;
	path = CGPathCreateWithRect(rect, NULL);

	frame = CTFramesetterCreateFrame(fs.fs,
		CFRangeMake(0, 0),
		path,
		fs.frameAttrib);
	if (frame == NULL)
		complain("error creating CTFrame object in doDrawText()");
	CTFrameDraw(frame, c);
	CFRelease(frame);

	CFRelease(path);

	freeFramesetter(&fs);
	CGContextRestoreGState(c);
}

// LONGTERM provide an equivalent to CTLineGetTypographicBounds() on uiDrawTextLayout?

// LONGTERM keep this for later features and documentation purposes
#if 0
		w = CTLineGetTypographicBounds(line, &ascent, &descent, NULL);
		// though CTLineGetTypographicBounds() returns 0 on error, it also returns 0 on an empty string, so we can't reasonably check for error
		CFRelease(line);

	// LONGTERM provide a way to get the image bounds as a separate function later
	bounds = CTLineGetImageBounds(line, c);
	// though CTLineGetImageBounds() returns CGRectNull on error, it also returns CGRectNull on an empty string, so we can't reasonably check for error

	// CGContextSetTextPosition() positions at the baseline in the case of CTLineDraw(); we need the top-left corner instead
	CTLineGetTypographicBounds(line, &yoff, NULL, NULL);
	// remember that we're flipped, so we subtract
	y -= yoff;
	CGContextSetTextPosition(c, x, y);
#endif

static CFRange charsToRange(uiDrawTextLayout *layout, int startChar, int endChar)
{
	CFRange start, end;
	CFRange out;

	start = layout->charsToRanges[startChar];
	end = layout->charsToRanges[endChar];
	out.location = start.location;
	out.length = end.location - start.location;
	return out;
}

#define rangeToCFRange() charsToRange(layout, startChar, endChar)

void uiDrawTextLayoutSetColor(uiDrawTextLayout *layout, int startChar, int endChar, double r, double g, double b, double a)
{
	CGColorSpaceRef colorspace;
	CGFloat components[4];
	CGColorRef color;

	// for consistency with windows, use sRGB
	colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	components[0] = r;
	components[1] = g;
	components[2] = b;
	components[3] = a;
	color = CGColorCreate(colorspace, components);
	CGColorSpaceRelease(colorspace);

	CFAttributedStringSetAttribute(layout->mas,
		rangeToCFRange(),
		kCTForegroundColorAttributeName,
		color);
	CGColorRelease(color);		// TODO safe?
}
