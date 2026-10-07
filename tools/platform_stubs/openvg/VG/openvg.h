#ifndef VG_OPENVG_STUB_H
#define VG_OPENVG_STUB_H
/* Stand-in for the OpenVG 1.1 header: the types, enumerants and
 * prototypes gfx/drivers/vg.c uses, with the values and signatures of
 * the Khronos header, so the driver is compiled - and its calls are
 * checked - where the real headers are not installed. */
typedef float          VGfloat;
typedef signed char    VGbyte;
typedef unsigned char  VGubyte;
typedef short          VGshort;
typedef int            VGint;
typedef unsigned int   VGuint;
typedef unsigned int   VGbitfield;
typedef VGuint         VGHandle;

typedef enum
{
   VG_FALSE = 0,
   VG_TRUE  = 1
} VGboolean;

#define VG_INVALID_HANDLE ((VGHandle)0)

typedef VGHandle VGFont;
typedef VGHandle VGImage;
typedef VGHandle VGPaint;

typedef enum
{
   VG_NO_ERROR                       = 0,
   VG_BAD_HANDLE_ERROR               = 0x1000,
   VG_ILLEGAL_ARGUMENT_ERROR         = 0x1001,
   VG_OUT_OF_MEMORY_ERROR            = 0x1002
} VGErrorCode;

typedef enum
{
   VG_MATRIX_MODE                    = 0x1100,
   VG_FILL_RULE                      = 0x1101,
   VG_IMAGE_QUALITY                  = 0x1102,
   VG_RENDERING_QUALITY              = 0x1103,
   VG_BLEND_MODE                     = 0x1104,
   VG_IMAGE_MODE                     = 0x1105,
   VG_SCISSOR_RECTS                  = 0x1106,
   VG_CLEAR_COLOR                    = 0x1121,
   VG_SCISSORING                     = 0x1130,
   VG_MAX_IMAGE_WIDTH                = 0x1165,
   VG_MAX_IMAGE_HEIGHT               = 0x1166
} VGParamType;

typedef enum
{
   VG_MATRIX_PATH_USER_TO_SURFACE    = 0x1400,
   VG_MATRIX_IMAGE_USER_TO_SURFACE   = 0x1401,
   VG_MATRIX_FILL_PAINT_TO_USER      = 0x1402,
   VG_MATRIX_STROKE_PAINT_TO_USER    = 0x1403,
   VG_MATRIX_GLYPH_USER_TO_SURFACE   = 0x1404
} VGMatrixMode;

typedef enum
{
   VG_PAINT_TYPE                     = 0x1A00,
   VG_PAINT_COLOR                    = 0x1A01
} VGPaintParamType;

typedef enum
{
   VG_PAINT_TYPE_COLOR               = 0x1B00
} VGPaintType;

typedef enum
{
   VG_STROKE_PATH                    = (1 << 0),
   VG_FILL_PATH                      = (1 << 1)
} VGPaintMode;

typedef enum
{
   VG_sRGBX_8888                     = 0,
   VG_sRGBA_8888                     = 1,
   VG_sRGBA_8888_PRE                 = 2,
   VG_sRGB_565                       = 3,
   VG_sRGBA_5551                     = 4,
   VG_sRGBA_4444                     = 5,
   VG_sL_8                           = 6,
   VG_lRGBX_8888                     = 7,
   VG_lRGBA_8888                     = 8,
   VG_lRGBA_8888_PRE                 = 9,
   VG_lL_8                           = 10,
   VG_A_8                            = 11,
   VG_BW_1                           = 12,
   VG_A_1                            = 13,
   VG_A_4                            = 14,
   VG_sXRGB_8888                     = 0 | (1 << 6)
} VGImageFormat;

typedef enum
{
   VG_IMAGE_QUALITY_NONANTIALIASED   = (1 << 0),
   VG_IMAGE_QUALITY_FASTER           = (1 << 1),
   VG_IMAGE_QUALITY_BETTER           = (1 << 2)
} VGImageQuality;

typedef enum
{
   VG_DRAW_IMAGE_NORMAL              = 0x1F00,
   VG_DRAW_IMAGE_MULTIPLY            = 0x1F01,
   VG_DRAW_IMAGE_STENCIL             = 0x1F02
} VGImageMode;

typedef enum
{
   VG_BLEND_SRC                      = 0x2000,
   VG_BLEND_SRC_OVER                 = 0x2001
} VGBlendMode;

typedef enum
{
   VG_VENDOR                         = 0x2300,
   VG_RENDERER                       = 0x2301,
   VG_VERSION                        = 0x2302,
   VG_EXTENSIONS                     = 0x2303
} VGStringID;

VGErrorCode    vgGetError(void);
void           vgSeti(VGParamType type, VGint value);
void           vgSetfv(VGParamType type, VGint count, const VGfloat *values);
void           vgSetiv(VGParamType type, VGint count, const VGint *values);
VGint          vgGeti(VGParamType type);
void           vgSetParameteri(VGHandle object, VGint paramType, VGint value);
void           vgSetParameterfv(VGHandle object, VGint paramType,
                     VGint count, const VGfloat *values);
void           vgLoadIdentity(void);
void           vgLoadMatrix(const VGfloat *m);
void           vgTranslate(VGfloat tx, VGfloat ty);
void           vgScale(VGfloat sx, VGfloat sy);
VGPaint        vgCreatePaint(void);
void           vgDestroyPaint(VGPaint paint);
void           vgSetPaint(VGPaint paint, VGbitfield paintModes);
VGImage        vgCreateImage(VGImageFormat format, VGint width,
                     VGint height, VGbitfield allowedQuality);
void           vgDestroyImage(VGImage image);
void           vgClear(VGint x, VGint y, VGint width, VGint height);
void           vgImageSubData(VGImage image, const void *data,
                     VGint dataStride, VGImageFormat dataFormat,
                     VGint x, VGint y, VGint width, VGint height);
VGImage        vgChildImage(VGImage parent, VGint x, VGint y,
                     VGint width, VGint height);
void           vgDrawImage(VGImage image);
VGFont         vgCreateFont(VGint glyphCapacityHint);
void           vgDestroyFont(VGFont font);
const VGubyte *vgGetString(VGStringID name);
#endif
