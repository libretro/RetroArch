/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * gpu_utils.c:
 * Utilities for GPU usage
 */

#include "../shared.h"
#include "stb_dxt.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))
#define CEIL(a) ((a - (int)a) == 0 ? (int)a : (int)a + 1)
#endif

// VRAM usage setting
uint8_t use_vram = 0;
uint8_t use_vram_for_usse = 0;

// Newlib mempool usage setting
GLboolean use_extra_mem = GL_TRUE;

// vitaGL memory pool setup
static void *pool_addr = NULL;
static unsigned int pool_index = 0;
static unsigned int pool_size = 0;

// USSE memory settings
vglMemType frag_usse_type;
vglMemType vert_usse_type;

uint64_t morton_1(uint64_t x) {
	x = x & 0x5555555555555555;
	x = (x | (x >> 1)) & 0x3333333333333333;
	x = (x | (x >> 2)) & 0x0F0F0F0F0F0F0F0F;
	x = (x | (x >> 4)) & 0x00FF00FF00FF00FF;
	x = (x | (x >> 8)) & 0x0000FFFF0000FFFF;
	x = (x | (x >> 16)) & 0xFFFFFFFFFFFFFFFF;
	return x;
}

void d2xy_morton(uint64_t d, uint64_t *x, uint64_t *y) {
	*x = morton_1(d);
	*y = morton_1(d >> 1);
}

void extract_block(const uint8_t *src, int width, uint8_t *block) {
	int j;
	for (j = 0; j < 4; j++) {
		memcpy_neon(&block[j * 4 * 4], src, 16);
		src += width * 4;
	}
}

void dxt_compress(uint8_t *dst, uint8_t *src, int w, int h, int isdxt5) {
	uint8_t block[64];
	int s = MAX(w, h);
	uint32_t num_blocks = (s * s) / 16;
	uint64_t d, offs_x, offs_y;
	for (d = 0; d < num_blocks; d++) {
		d2xy_morton(d, &offs_x, &offs_y);
		if (offs_x * 4 >= h)
			continue;
		if (offs_y * 4 >= w)
			continue;
		extract_block(src + offs_y * 16 + offs_x * w * 16, w, block);
		stb_compress_dxt_block(dst, block, isdxt5, fast_texture_compression ? STB_DXT_NORMAL : STB_DXT_HIGHQUAL);
		dst += isdxt5 ? 16 : 8;
	}
}

void swizzle_compressed_texture(uint8_t *dst, uint8_t *src, int w, int h, int isdxt5, int ispvrt2bpp) {
	int blocksize = isdxt5 ? 16 : 8;

	int s = MAX(w, h);
	uint32_t num_blocks = (s * s) / 16;
	uint64_t d, offs_x, offs_y;
	for (d = 0; d < num_blocks; d++) {
		d2xy_morton(d, &offs_x, &offs_y);
		if (offs_x * 4 >= h)
			continue;
		if (offs_y * (ispvrt2bpp ? 8 : 4) >= w)
			continue;
		memcpy(dst, src + offs_y * blocksize + offs_x * (w / (ispvrt2bpp ? 8 : 4)) * blocksize, blocksize);
		dst += isdxt5 ? 16 : 8;
	}
}

void *gpu_alloc_mapped(size_t size, vglMemType *type) {
	// Allocating requested memblock
	void *res = mempool_alloc(size, *type);

	// Requested memory type finished, using other one
	if (res == NULL) {
		*type = *type == VGL_MEM_VRAM ? VGL_MEM_RAM : VGL_MEM_VRAM;
		res = mempool_alloc(size, *type);
	}

	// Even the other one failed, using our last resort
	if (res == NULL) {
		*type = VGL_MEM_SLOW;
		res = mempool_alloc(size, *type);
	}

	if (res == NULL && use_extra_mem) {
		*type = VGL_MEM_EXTERNAL;
		res = malloc(size);
	}

	return res;
}

void *gpu_vertex_usse_alloc_mapped(size_t size, unsigned int *usse_offset) {
	// Allocating memblock
	vert_usse_type = use_vram_for_usse ? VGL_MEM_VRAM : VGL_MEM_RAM;
	void *addr = gpu_alloc_mapped(size, &vert_usse_type);

	// Mapping memblock into sceGxm as vertex USSE memory
	sceGxmMapVertexUsseMemory(addr, size, usse_offset);

	// Returning memblock starting address
	return addr;
}

void gpu_vertex_usse_free_mapped(void *addr) {
	// Unmapping memblock from sceGxm as vertex USSE memory
	sceGxmUnmapVertexUsseMemory(addr);

	// Deallocating memblock
	mempool_free(addr, vert_usse_type);
}

void *gpu_fragment_usse_alloc_mapped(size_t size, unsigned int *usse_offset) {
	// Allocating memblock
	frag_usse_type = use_vram_for_usse ? VGL_MEM_VRAM : VGL_MEM_RAM;
	void *addr = gpu_alloc_mapped(size, &frag_usse_type);

	// Mapping memblock into sceGxm as fragment USSE memory
	sceGxmMapFragmentUsseMemory(addr, size, usse_offset);

	// Returning memblock starting address
	return addr;
}

void gpu_fragment_usse_free_mapped(void *addr) {
	// Unmapping memblock from sceGxm as fragment USSE memory
	sceGxmUnmapFragmentUsseMemory(addr);

	// Deallocating memblock
	mempool_free(addr, frag_usse_type);
}

void *gpu_pool_malloc(unsigned int size) {
	// Reserving vitaGL mempool space
	if ((pool_index + size) < pool_size) {
		void *addr = (void *)((unsigned int)pool_addr + pool_index);
		pool_index += size;
		return addr;
	}

	return NULL;
}

void *gpu_pool_memalign(unsigned int size, unsigned int alignment) {
	// Aligning requested memory size
	unsigned int new_index = ALIGN(pool_index, alignment);

	// Reserving vitaGL mempool space
	if ((new_index + size) < pool_size) {
		void *addr = (void *)((unsigned int)pool_addr + new_index);
		pool_index = new_index + size;
		return addr;
	}
	return NULL;
}

unsigned int gpu_pool_free_space() {
	// Returning vitaGL available mempool space
	return pool_size - pool_index;
}

void gpu_pool_reset() {
	// Resetting vitaGL available mempool space
	pool_index = 0;
}

void gpu_pool_init(uint32_t temp_pool_size) {
	// Allocating vitaGL mempool
	pool_size = temp_pool_size;
	vglMemType type = VGL_MEM_RAM;
	pool_addr = gpu_alloc_mapped(temp_pool_size, &type);
}

int tex_format_to_bytespp(SceGxmTextureFormat format) {
	// Calculating bpp for the requested texture format
	switch (format & 0x9f000000U) {
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
		return 1;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
		return 2;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
		return 3;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
	default:
		return 4;
	}
}

int tex_format_to_alignment(SceGxmTextureFormat format) {
	switch (format & 0x9f000000U) {
	case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
		return 16;
	default:
		return 8;
	}
}

palette *gpu_alloc_palette(const void *data, uint32_t w, uint32_t bpe) {
	// Allocating a palette object
	palette *res = (palette *)malloc(sizeof(palette));
	res->type = use_vram ? VGL_MEM_VRAM : VGL_MEM_RAM;

	// Allocating palette data buffer
	void *texture_palette = gpu_alloc_mapped(256 * sizeof(uint32_t), &res->type);

	// Initializing palette
	if (data == NULL)
		memset(texture_palette, 0, 256 * sizeof(uint32_t));
	else if (bpe == 4)
		memcpy_neon(texture_palette, data, w * sizeof(uint32_t));
	res->data = texture_palette;

	// Returning palette
	return res;
}

void gpu_free_texture(texture *tex) {
	// Deallocating texture
	if (tex->data != NULL)
		mempool_free(tex->data, tex->mtype);

	// Invalidating texture object
	tex->valid = 0;
}

void gpu_alloc_texture(uint32_t w, uint32_t h, SceGxmTextureFormat format, const void *data, texture *tex, uint8_t src_bpp, uint32_t (*read_cb)(void *), void (*write_cb)(void *, uint32_t), uint8_t fast_store) {
	// If there's already a texture in passed texture object we first dealloc it
	if (tex->valid)
		gpu_free_texture(tex);

	// Getting texture format bpp
	uint8_t bpp = tex_format_to_bytespp(format);

	// Allocating texture data buffer
	tex->mtype = use_vram ? VGL_MEM_VRAM : VGL_MEM_RAM;
	const int tex_size = ALIGN(w, 8) * h * bpp;
	void *texture_data = gpu_alloc_mapped(tex_size, &tex->mtype);

	if (texture_data != NULL) {
		// Initializing texture data buffer
		if (data != NULL) {
			int i, j;
			uint8_t *src = (uint8_t *)data;
			uint8_t *dst;
			if (fast_store) { // Internal Format and Data Format are the same, we can just use memcpy_neon for better performance
				uint32_t line_size = w * bpp;
				for (i = 0; i < h; i++) {
					dst = ((uint8_t *)texture_data) + (ALIGN(w, 8) * bpp) * i;
					memcpy_neon(dst, src, line_size);
					src += line_size;
				}
			} else { // Different internal and data formats, we need to go with slower callbacks system
				for (i = 0; i < h; i++) {
					dst = ((uint8_t *)texture_data) + (ALIGN(w, 8) * bpp) * i;
					for (j = 0; j < w; j++) {
						uint32_t clr = read_cb(src);
						write_cb(dst, clr);
						src += src_bpp;
						dst += bpp;
					}
				}
			}
		} else
			memset(texture_data, 0, tex_size);

		// Initializing texture and validating it
		sceGxmTextureInitLinear(&tex->gxm_tex, texture_data, format, w, h, 0);
		if ((format & 0x9f000000U) == SCE_GXM_TEXTURE_BASE_FORMAT_P8)
			tex->palette_UID = 1;
		else
			tex->palette_UID = 0;
		tex->valid = 1;
		tex->data = texture_data;
	}
}

void gpu_alloc_compressed_texture(uint32_t w, uint32_t h, SceGxmTextureFormat format, uint32_t image_size, const void *data, texture *tex, uint8_t src_bpp, uint32_t (*read_cb)(void *)) {
	// If there's already a texture in passed texture object we first dealloc it
	if (tex->valid)
		gpu_free_texture(tex);

	// Getting texture format alignment
	uint8_t alignment = tex_format_to_alignment(format);

	// Calculating swizzled compressed texture size on memory
	tex->mtype = use_vram ? VGL_MEM_VRAM : VGL_MEM_RAM;

	int tex_size = 0;
	switch (format) {
	case SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_1BGR:
	case SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_ABGR:
		tex_size = (MAX(w, 8) * MAX(h, 8) * 2 + 7) / 8;
		break;
	case SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_1BGR:
	case SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_ABGR:
		tex_size = (MAX(w, 8) * MAX(h, 8) * 4 + 7) / 8;
		break;
	case SCE_GXM_TEXTURE_FORMAT_PVRTII2BPP_ABGR:
		tex_size = CEIL(w / 8.0) * CEIL(h / 4.0) * 8.0;
		break;
	case SCE_GXM_TEXTURE_FORMAT_PVRTII4BPP_ABGR:
		tex_size = CEIL(w / 4.0) * CEIL(h / 4.0) * 8.0;
		break;
	case SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR:
		tex_size = (w * h) / 2;
		break;
	case SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR:
		tex_size = w * h;
		break;
	}

#ifndef SKIP_ERROR_HANDLING
	// Check the given texture data size.
	if (image_size != 0 && image_size != tex_size) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
#endif

	// Allocating texture data buffer
	void *texture_data = gpu_alloc_mapped(tex_size, &tex->mtype);

	// Initializing texture data buffer
	if (texture_data != NULL) {
		if (data != NULL) {
			if (read_cb != NULL) {
				void *temp = (void *)data;

				// stb_dxt expects input as RGBA8888, so we convert input texture if necessary
				if (read_cb != readRGBA) {
					temp = malloc(w * h * 4);
					uint8_t *src = (uint8_t *)data;
					uint32_t *dst = (uint32_t *)temp;
					int i;
					for (i = 0; i < w * h; i++) {
						uint32_t clr = read_cb(src);
						writeRGBA(dst++, clr);
						src += src_bpp;
					}
				}

				// Performing swizzling and DXT compression
				dxt_compress(texture_data, temp, w, h, alignment == 16);

				// Freeing temporary data if necessary
				if (read_cb != readRGBA)
					free(temp);
			} else {
				// Perform swizzling if necessary.
				switch (format) {
				case SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_1BGR:
				case SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_ABGR:
				case SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_1BGR:
				case SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_ABGR:
					memcpy_neon(texture_data, data, tex_size);
					break;
				case SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR:
					swizzle_compressed_texture(texture_data, (void *)data, w, h, 1, 0);
					break;
				case SCE_GXM_TEXTURE_FORMAT_PVRTII2BPP_ABGR:
					swizzle_compressed_texture(texture_data, (void *)data, w, h, 0, 1);
					break;
				default:
					swizzle_compressed_texture(texture_data, (void *)data, w, h, 0, 0);
					break;
				}
			}

		} else
			memset(texture_data, 0, tex_size);

		// Initializing texture and validating it
		sceGxmTextureInitSwizzled(&tex->gxm_tex, texture_data, format, w, h, 0);
		tex->palette_UID = 0;
		tex->valid = 1;
		tex->data = texture_data;
	}
}

void gpu_alloc_mipmaps(int level, texture *tex) {
	// Getting current mipmap count in passed texture
	uint32_t count = sceGxmTextureGetMipmapCount(&tex->gxm_tex);

	// Getting textures info and calculating bpp
	uint32_t w, h, stride;
	uint32_t orig_w = sceGxmTextureGetWidth(&tex->gxm_tex);
	uint32_t orig_h = sceGxmTextureGetHeight(&tex->gxm_tex);
	SceGxmTextureFormat format = sceGxmTextureGetFormat(&tex->gxm_tex);
	uint32_t bpp = tex_format_to_bytespp(format);

	// Checking if we need at least one more new mipmap level
	if ((level > count) || (level < 0)) { // Note: level < 0 means we will use max possible mipmaps level

		uint32_t jumps[10];
		for (w = 1; w < orig_w; w <<= 1) {
		}
		for (h = 1; h < orig_h; h <<= 1) {
		}

		// Calculating new texture data buffer size
		uint32_t size = 0;
		int j;
		if (level > 0) {
			for (j = 0; j < level; j++) {
				jumps[j] = max(w, 8) * h * bpp;
				size += jumps[j];
				w /= 2;
				h /= 2;
			}
		} else {
			level = 0;
			while ((w > 1) && (h > 1)) {
				jumps[level] = max(w, 8) * h * bpp;
				size += jumps[level];
				w /= 2;
				h /= 2;
				level++;
			}
		}

		// Calculating needed sceGxmTransfer format for the downscale process
		SceGxmTransferFormat fmt;
		switch (tex->type) {
		case GL_RGBA:
			fmt = SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR;
			break;
		case GL_RGB:
			fmt = SCE_GXM_TRANSFER_FORMAT_U8U8U8_BGR;
		default:
			break;
		}

		// Moving texture data to heap and deallocating texture memblock
		GLboolean has_temp_buffer = GL_TRUE;
		stride = ALIGN(orig_w, 8);
		void *temp = (void *)malloc(stride * orig_h * bpp);
		if (temp == NULL) { // If we finished newlib heap, we delay texture free
			has_temp_buffer = GL_FALSE;
			temp = sceGxmTextureGetData(&tex->gxm_tex);
		} else {
			memcpy_neon(temp, sceGxmTextureGetData(&tex->gxm_tex), stride * orig_h * bpp);
			gpu_free_texture(tex);
		}

		// Allocating the new texture data buffer
		tex->mtype = use_vram ? VGL_MEM_VRAM : VGL_MEM_RAM;
		void *texture_data = gpu_alloc_mapped(size, &tex->mtype);

		// Moving back old texture data from heap to texture memblock
		memcpy_neon(texture_data, temp, stride * orig_h * bpp);
		if (has_temp_buffer)
			free(temp);
		else
			gpu_free_texture(tex);
		tex->valid = 1;

		// Performing a chain downscale process to generate requested mipmaps
		uint8_t *curPtr = (uint8_t *)texture_data;
		uint32_t curWidth = orig_w;
		uint32_t curHeight = orig_h;
		if (curWidth % 2)
			curWidth--;
		if (curHeight % 2)
			curHeight--;
		for (j = 0; j < level - 1; j++) {
			uint32_t curSrcStride = ALIGN(curWidth, 8);
			uint32_t curDstStride = ALIGN(curWidth >> 1, 8);
			uint8_t *dstPtr = curPtr + jumps[j];
			sceGxmTransferDownscale(
				fmt, curPtr, 0, 0,
				curWidth, curHeight,
				curSrcStride * bpp,
				fmt, dstPtr, 0, 0,
				curDstStride * bpp,
				NULL, SCE_GXM_TRANSFER_FRAGMENT_SYNC, NULL);
			curPtr = dstPtr;
			curWidth /= 2;
			curHeight /= 2;
		}

		// Initializing texture in sceGxm
		sceGxmTextureInitLinear(&tex->gxm_tex, texture_data, format, orig_w, orig_h, level);
		tex->data = texture_data;
	}
}

void gpu_free_palette(palette *pal) {
	// Deallocating palette memblock and object
	if (pal == NULL)
		return;
	mempool_free(pal->data, pal->type);
	free(pal);
}