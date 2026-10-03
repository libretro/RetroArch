#ifndef VG_VGEXT_STUB_H
#define VG_VGEXT_STUB_H
/* Stand-in for the OpenVG extension header: KHR_EGL_image. */
#include <VG/openvg.h>
typedef void *VGeglImageKHR;
typedef VGImage (*PFNVGCREATEEGLIMAGETARGETKHRPROC)(VGeglImageKHR image);
#endif
