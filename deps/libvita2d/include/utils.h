#ifndef UTILS_H
#define UTILS_H

#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>

/* Misc utils */
#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))
#define	UNUSED(a)	(void)(a)
#define SCREEN_DPI	220

/* GPU utils */
void *gpu_alloc(SceKernelMemBlockType type, unsigned int size, unsigned int alignment, unsigned int attribs, SceUID *uid);
void gpu_free(SceUID uid);
void *vertex_usse_alloc(unsigned int size, SceUID *uid, unsigned int *usse_offset);
void vertex_usse_free(SceUID uid);
void *fragment_usse_alloc(unsigned int size, SceUID *uid, unsigned int *usse_offset);
void fragment_usse_free(SceUID uid);

/* Math utils */

#define _PI_OVER_180 0.0174532925199432957692369076849f
#define _180_OVER_PI 57.2957795130823208767981548141f

#define DEG_TO_RAD(x) (x * _PI_OVER_180)
#define RAD_TO_DEG(x) (x * _180_OVER_PI)

void matrix_init_orthographic(float *m, float left, float right, float bottom, float top, float near, float far);

/* Text utils */

#endif
