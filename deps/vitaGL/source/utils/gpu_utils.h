/* 
 * gpu_utils.h:
 * Header file for the GPU utilities exposed by gpu_utils.c
 */

#ifndef _GPU_UTILS_H_
#define _GPU_UTILS_H_

#include "mem_utils.h"

// Align a value to the requested alignment
#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))

// Texture object struct
typedef struct texture {
	SceGxmTexture gxm_tex;
	void *data;
	vglMemType mtype;
	SceUID palette_UID;
	SceUID depth_UID;
	uint8_t used;
	uint8_t valid;
	uint32_t type;
	void (*write_cb)(void *, uint32_t);
} texture;

// Palette object struct
typedef struct palette {
	void *data;
	vglMemType type;
} palette;

// Alloc a generic memblock into sceGxm mapped memory
void *gpu_alloc_mapped(size_t size, vglMemType *type);

// Alloc into sceGxm mapped memory a vertex USSE memblock
void *gpu_vertex_usse_alloc_mapped(size_t size, unsigned int *usse_offset);

// Dealloc from sceGxm mapped memory a vertex USSE memblock
void gpu_vertex_usse_free_mapped(void *addr);

// Alloc into sceGxm mapped memory a fragment USSE memblock
void *gpu_fragment_usse_alloc_mapped(size_t size, unsigned int *usse_offset);

// Dealloc from sceGxm mapped memory a fragment USSE memblock
void gpu_fragment_usse_free_mapped(void *addr);

// Reserve a memory space from vitaGL mempool
void *gpu_pool_malloc(unsigned int size);

// Reserve an aligned memory space from vitaGL mempool
void *gpu_pool_memalign(unsigned int size, unsigned int alignment);

// Returns available free space on vitaGL mempool
unsigned int gpu_pool_free_space();

// Resets vitaGL mempool
void gpu_pool_reset();

// Alloc vitaGL mempool
void gpu_pool_init(uint32_t temp_pool_size);

// Calculate bpp for a requested texture format
int tex_format_to_bytespp(SceGxmTextureFormat format);

// Alloc a texture
void gpu_alloc_texture(uint32_t w, uint32_t h, SceGxmTextureFormat format, const void *data, texture *tex, uint8_t src_bpp, uint32_t (*read_cb)(void *), void (*write_cb)(void *, uint32_t));

// Dealloc a texture
void gpu_free_texture(texture *tex);

// Alloc a palette
palette *gpu_alloc_palette(const void *data, uint32_t w, uint32_t bpe);

// Dealloc a palette
void gpu_free_palette(palette *pal);

// Generate mipmaps for a given texture
void gpu_alloc_mipmaps(int level, texture *tex);

#endif
