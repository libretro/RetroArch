#ifndef VG_OPENVG_STUB_H
#define VG_OPENVG_STUB_H
/* Generated stand-in: only the OpenVG surface gfx/drivers/vg.c
 * names, so the driver can be syntax-checked where the real
 * headers are not installed. */
typedef float VGfloat;
typedef int VGint;
typedef unsigned VGuint;
typedef unsigned VGbitfield;
typedef unsigned VGenum;
typedef unsigned VGHandle;
typedef unsigned VGboolean;
#define VG_FALSE 0
#define VG_TRUE 1
typedef VGHandle VGFont;
typedef VGHandle VGImage;
typedef VGHandle VGImageFormat;
typedef VGHandle VGPaint;
#define VG_CLEAR_COLOR 0
#define VG_EXTENSIONS 0
#define VG_FALSE 0
#define VG_IMAGE_QUALITY_BETTER 0
#define VG_IMAGE_QUALITY_NONANTIALIASED 0
#define VG_INVALID_HANDLE 0
#define VG_MATRIX_IMAGE_USER_TO_SURFACE 0
#define VG_MATRIX_MODE 0
#define VG_PAINT_COLOR 0
#define VG_PAINT_TYPE 0
#define VG_PAINT_TYPE_COLOR 0
#define VG_SCISSORING 0
#define VG_SCISSOR_RECTS 0
#define VG_TRUE 0
typedef VGHandle (*PFNVGCREATEEGLIMAGETARGETKHRPROC)(VGeglImageKHR);
extern VGHandle vgClear();
extern VGHandle vgCreateFont();
extern VGHandle vgCreateImage();
extern VGHandle vgCreatePaint();
extern VGHandle vgDestroyFont();
extern VGHandle vgDestroyImage();
extern VGHandle vgDestroyPaint();
extern VGHandle vgDrawImage();
extern VGHandle vgGetError();
extern VGHandle vgGetString();
extern VGHandle vgImageSubData();
extern VGHandle vgLoadMatrix();
extern VGHandle vgSetParameterfv();
extern VGHandle vgSetParameteri();
extern VGHandle vgSetfv();
extern VGHandle vgSeti();
extern VGHandle vgSetiv();
#define VG_sXRGB_8888 0
#define VG_sRGB_565 0
typedef VGHandle VGeglImageKHR;
typedef VGHandle VGImageFormat;
#endif
