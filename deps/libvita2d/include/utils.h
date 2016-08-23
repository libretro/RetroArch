#ifndef UTILS_H
#define UTILS_H

#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>

/* Misc utils */
#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))
#define	UNUSED(a)	(void)(a)
#define SCREEN_DPI	220

/* Font utils */
uint32_t utf8_character(const char **unicode);

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

void matrix_copy(float *dst, const float *src);
void matrix_identity4x4(float *m);
void matrix_mult4x4(const float *src1, const float *src2, float *dst);
void matrix_set_x_rotation(float *m, float rad);
void matrix_set_y_rotation(float *m, float rad);
void matrix_set_z_rotation(float *m, float rad);
void matrix_rotate_x(float *m, float rad);
void matrix_rotate_y(float *m, float rad);
void matrix_rotate_z(float *m, float rad);
void matrix_set_xyz_translation(float *m, float x, float y, float z);
void matrix_translate_xyz(float *m, float x, float y, float z);
void matrix_set_scaling(float *m, float x_scale, float y_scale, float z_scale);
void matrix_swap_xy(float *m);
void matrix_init_orthographic(float *m, float left, float right, float bottom, float top, float near, float far);
void matrix_init_frustum(float *m, float left, float right, float bottom, float top, float near, float far);
void matrix_init_perspective(float *m, float fov, float aspect, float near, float far);

/* Text utils */

#endif
