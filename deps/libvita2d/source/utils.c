#include "utils.h"
#include <math.h>
#include <string.h>

void *gpu_alloc(SceKernelMemBlockType type, unsigned int size, unsigned int alignment, unsigned int attribs, SceUID *uid)
{
	void *mem;

	if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW) {
		size = ALIGN(size, 256*1024);
	} else {
		size = ALIGN(size, 4*1024);
	}

	*uid = sceKernelAllocMemBlock("gpu_mem", type, size, NULL);

	if (*uid < 0)
		return NULL;

	if (sceKernelGetMemBlockBase(*uid, &mem) < 0)
		return NULL;

	if (sceGxmMapMemory(mem, size, attribs) < 0)
		return NULL;

	return mem;
}

void gpu_free(SceUID uid)
{
	void *mem = NULL;
	if (sceKernelGetMemBlockBase(uid, &mem) < 0)
		return;
	sceGxmUnmapMemory(mem);
	sceKernelFreeMemBlock(uid);
}

void *vertex_usse_alloc(unsigned int size, SceUID *uid, unsigned int *usse_offset)
{
	void *mem = NULL;

	size = ALIGN(size, 4096);
	*uid = sceKernelAllocMemBlock("vertex_usse", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);

	if (sceKernelGetMemBlockBase(*uid, &mem) < 0)
		return NULL;
	if (sceGxmMapVertexUsseMemory(mem, size, usse_offset) < 0)
		return NULL;

	return mem;
}

void vertex_usse_free(SceUID uid)
{
	void *mem = NULL;
	if (sceKernelGetMemBlockBase(uid, &mem) < 0)
		return;
	sceGxmUnmapVertexUsseMemory(mem);
	sceKernelFreeMemBlock(uid);
}

void *fragment_usse_alloc(unsigned int size, SceUID *uid, unsigned int *usse_offset)
{
	void *mem = NULL;

	size = ALIGN(size, 4096);
	*uid = sceKernelAllocMemBlock("fragment_usse", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);

	if (sceKernelGetMemBlockBase(*uid, &mem) < 0)
		return NULL;
	if (sceGxmMapFragmentUsseMemory(mem, size, usse_offset) < 0)
		return NULL;

	return mem;
}

void fragment_usse_free(SceUID uid)
{
	void *mem = NULL;
	if (sceKernelGetMemBlockBase(uid, &mem) < 0)
		return;
	sceGxmUnmapFragmentUsseMemory(mem);
	sceKernelFreeMemBlock(uid);
}


void matrix_copy(float *dst, const float *src)
{
	memcpy(dst, src, sizeof(float)*4*4);
}

void matrix_identity4x4(float *m)
{
	m[0] = m[5] = m[10] = m[15] = 1.0f;
	m[1] = m[2] = m[3] = 0.0f;
	m[4] = m[6] = m[7] = 0.0f;
	m[8] = m[9] = m[11] = 0.0f;
	m[12] = m[13] = m[14] = 0.0f;
}

void matrix_mult4x4(const float *src1, const float *src2, float *dst)
{
	int i, j, k;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			dst[i*4 + j] = 0.0f;
			for (k = 0; k < 4; k++) {
				dst[i*4 + j] += src1[i*4 + k]*src2[k*4 + j];
			}
		}
	}
}

void matrix_set_x_rotation(float *m, float rad)
{
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity4x4(m);

	m[0] = c;
	m[2] = -s;
	m[8] = s;
	m[10] = c;
}

void matrix_set_y_rotation(float *m, float rad)
{
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity4x4(m);

	m[5] = c;
	m[6] = s;
	m[9] = -s;
	m[10] = c;
}

void matrix_set_z_rotation(float *m, float rad)
{
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity4x4(m);

	m[0] = c;
	m[1] = s;
	m[4] = -s;
	m[5] = c;
}

void matrix_rotate_x(float *m, float rad)
{
	float mr[4*4], mt[4*4];
	matrix_set_y_rotation(mr, rad);
	matrix_mult4x4(m, mr, mt);
	matrix_copy(m, mt);
}


void matrix_rotate_y(float *m, float rad)
{
	float mr[4*4], mt[4*4];
	matrix_set_x_rotation(mr, rad);
	matrix_mult4x4(m, mr, mt);
	matrix_copy(m, mt);
}

void matrix_rotate_z(float *m, float rad)
{
	float mr[4*4], mt[4*4];
	matrix_set_z_rotation(mr, rad);
	matrix_mult4x4(m, mr, mt);
	matrix_copy(m, mt);
}

void matrix_set_xyz_translation(float *m, float x, float y, float z)
{
	matrix_identity4x4(m);

	m[12] = x;
	m[13] = y;
	m[14] = z;
}

void matrix_translate_xyz(float *m, float x, float y, float z)
{
	float mr[4*4], mt[4*4];
	matrix_set_xyz_translation(mr, x, y, z);
	matrix_mult4x4(m, mr, mt);
	matrix_copy(m, mt);
}

void matrix_set_scaling(float *m, float x_scale, float y_scale, float z_scale)
{
	matrix_identity4x4(m);
	m[0] = x_scale;
	m[5] = y_scale;
	m[10] = z_scale;
}

void matrix_swap_xy(float *m)
{
	float ms[4*4], mt[4*4];
	matrix_identity4x4(ms);

	ms[0] = 0.0f;
	ms[1] = 1.0f;
	ms[4] = 1.0f;
	ms[5] = 0.0f;

	matrix_mult4x4(m, ms, mt);
	matrix_copy(m, mt);
}

void matrix_init_orthographic(float *m, float left, float right, float bottom, float top, float near, float far)
{
	m[0x0] = 2.0f/(right-left);
	m[0x4] = 0.0f;
	m[0x8] = 0.0f;
	m[0xC] = -(right+left)/(right-left);

	m[0x1] = 0.0f;
	m[0x5] = 2.0f/(top-bottom);
	m[0x9] = 0.0f;
	m[0xD] = -(top+bottom)/(top-bottom);

	m[0x2] = 0.0f;
	m[0x6] = 0.0f;
	m[0xA] = -2.0f/(far-near);
	m[0xE] = (far+near)/(far-near);

	m[0x3] = 0.0f;
	m[0x7] = 0.0f;
	m[0xB] = 0.0f;
	m[0xF] = 1.0f;
}

void matrix_init_frustum(float *m, float left, float right, float bottom, float top, float near, float far)
{
	m[0x0] = (2.0f*near)/(right-left);
	m[0x4] = 0.0f;
	m[0x8] = (right+left)/(right-left);
	m[0xC] = 0.0f;

	m[0x1] = 0.0f;
	m[0x5] = (2.0f*near)/(top-bottom);
	m[0x9] = (top+bottom)/(top-bottom);
	m[0xD] = 0.0f;

	m[0x2] = 0.0f;
	m[0x6] = 0.0f;
	m[0xA] = -(far+near)/(far-near);
	m[0xE] = (-2.0f*far*near)/(far-near);

	m[0x3] = 0.0f;
	m[0x7] = 0.0f;
	m[0xB] = -1.0f;
	m[0xF] = 0.0f;
}

void matrix_init_perspective(float *m, float fov, float aspect, float near, float far)
{
	float half_height = near * tan(DEG_TO_RAD(fov) * 0.5f);
	float half_width = half_height * aspect;

	matrix_init_frustum(m, -half_width, half_width, -half_height, half_height, near, far);
}

uint32_t utf8_character(const char** unicode)
{
	char byte = **unicode;
	++*unicode;
	if (!(byte & 0x80)) {
		return byte;
	}
	uint32_t unichar;
	const static int tops[4] = { 0xC0, 0xE0, 0xF0, 0xF8 };
	size_t numBytes;
	for (numBytes = 0; numBytes < 3; ++numBytes) {
		if ((byte & tops[numBytes + 1]) == tops[numBytes]) {
			break;
		}
	}
	unichar = byte & ~tops[numBytes];
	if (numBytes == 3) {
		return 0;
	}
	++numBytes;
	size_t i;
	for (i = 0; i < numBytes; ++i) {
		unichar <<= 6;
		byte = **unicode;
		++*unicode;
		if ((byte & 0xC0) != 0x80) {
			return 0;
		}
		unichar |= byte & 0x3F;
	}
	return unichar;
}
