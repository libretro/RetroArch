// 25 june 2016
#import "uipriv_darwin.h"

struct uiImage {
	NSImage *i;
	NSSize size;
	NSMutableArray *swizzled;
};

uiImage *uiNewImage(double width, double height)
{
	uiImage *i;

	i = uiNew(uiImage);
	i->size = NSMakeSize(width, height);
	i->i = [[NSImage alloc] initWithSize:i->size];
	i->swizzled = [NSMutableArray new];
	return i;
}

void uiFreeImage(uiImage *i)
{
	NSValue *v;

	[i->i release];
	// to be safe, do this after releasing the image
	for (v in i->swizzled)
		uiFree([v pointerValue]);
	[i->swizzled release];
	uiFree(i);
}

void uiImageAppend(uiImage *i, void *pixels, int pixelWidth, int pixelHeight, int pixelStride)
{
	NSBitmapImageRep *repCalibrated, *repsRGB;
	uint8_t *swizzled, *bp, *sp;
	int x, y;
	unsigned char *pix[1];

	// OS X demands that R and B are in the opposite order from what we expect
	// we must swizzle :(
	// LONGTERM test on a big-endian system
	swizzled = (uint8_t *) uiAlloc((pixelStride * pixelHeight * 4) * sizeof (uint8_t), "uint8_t[]");
	bp = (uint8_t *) pixels;
	sp = swizzled;
	for (y = 0; y < pixelHeight * pixelStride; y += pixelStride)
		for (x = 0; x < pixelStride; x++) {
			sp[0] = bp[2];
			sp[1] = bp[1];
			sp[2] = bp[0];
			sp[3] = bp[3];
			sp += 4;
			bp += 4;
		}

	pix[0] = (unsigned char *) swizzled;
	repCalibrated = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:pix
		pixelsWide:pixelWidth
		pixelsHigh:pixelHeight
		bitsPerSample:8
		samplesPerPixel:4
		hasAlpha:YES
		isPlanar:NO
		colorSpaceName:NSCalibratedRGBColorSpace
		bitmapFormat:0
		bytesPerRow:pixelStride
		bitsPerPixel:32];
	repsRGB = [repCalibrated bitmapImageRepByRetaggingWithColorSpace:[NSColorSpace sRGBColorSpace]];
	[repCalibrated release];

	[i->i addRepresentation:repsRGB];
	[repsRGB setSize:i->size];
	[repsRGB release];

	// we need to keep swizzled alive for NSBitmapImageRep
	[i->swizzled addObject:[NSValue valueWithPointer:swizzled]];
}

NSImage *imageImage(uiImage *i)
{
	return i->i;
}
