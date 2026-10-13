/* Stand-in for the macOS SDK umbrella header: only the CoreGraphics
 * and CoreText declarations gfx/drivers_font_renderer/coretext.c uses. */
#ifndef STUB_AS_H
#define STUB_AS_H
#include <CoreFoundation/CFString.h>
typedef double CGFloat;
typedef unsigned short CGGlyph;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef struct { CGFloat a,b,c,d,tx,ty; } CGAffineTransform;
extern const CGAffineTransform CGAffineTransformIdentity;
typedef struct CGContext *CGContextRef;
typedef struct CGColorSpace *CGColorSpaceRef;
typedef struct CGDataProvider *CGDataProviderRef;
typedef struct CGFont *CGFontRef;
typedef const struct __CTFont *CTFontRef;
typedef const struct __CTLine *CTLineRef;
typedef uint32_t CTFontOrientation;
typedef uint32_t CGBitmapInfo;
enum { kCGImageAlphaNone = 0, kCGImageAlphaOnly = 7, kCGBitmapByteOrder16Host = 0x1000 };
extern const CFStringRef kCTFontAttributeName;
extern const CFStringRef kCTForegroundColorFromContextAttributeName;
typedef void (*CGDataProviderReleaseDataCallback)(void *info, const void *data, size_t size);
CGDataProviderRef CGDataProviderCreateWithData(void *, const void *, size_t, CGDataProviderReleaseDataCallback);
CGFontRef CGFontCreateWithDataProvider(CGDataProviderRef);
CTFontRef CTFontCreateWithGraphicsFont(CGFontRef, CGFloat, const CGAffineTransform *, void *);
bool CTFontGetGlyphsForCharacters(CTFontRef, const UniChar *, CGGlyph *, CFIndex);
CGRect CTFontGetBoundingRectsForGlyphs(CTFontRef, CTFontOrientation, const CGGlyph *, CGRect *, CFIndex);
double CTFontGetAdvancesForGlyphs(CTFontRef, CTFontOrientation, const CGGlyph *, CGSize *, CFIndex);
CGFloat CTFontGetAscent(CTFontRef);
CGFloat CTFontGetDescent(CTFontRef);
CGFloat CTFontGetLeading(CTFontRef);
CGColorSpaceRef CGColorSpaceCreateDeviceGray(void);
void CGColorSpaceRelease(CGColorSpaceRef);
CGContextRef CGBitmapContextCreate(void *, size_t, size_t, size_t, size_t, CGColorSpaceRef, uint32_t);
void CGContextRelease(CGContextRef);
void CGContextSetGrayFillColor(CGContextRef, CGFloat, CGFloat);
void CGContextSetTextMatrix(CGContextRef, CGAffineTransform);
void CGContextSetTextPosition(CGContextRef, CGFloat, CGFloat);
CTLineRef CTLineCreateWithAttributedString(CFAttributedStringRef);
void CTLineDraw(CTLineRef, CGContextRef);
#endif
