#include <psp2/kernel/sysmem.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../include/vita2d.h"
#include "../include/utils.h"
#include "../include/shared.h"

#define GXM_TEX_MAX_SIZE 4096
static SceKernelMemBlockType MemBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW;

static int tex_format_to_bytespp(SceGxmTextureFormat format)
{
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

void vita2d_texture_set_alloc_memblock_type(SceKernelMemBlockType type)
{
	MemBlockType = (type == 0) ? SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW : type;
}

SceKernelMemBlockType vita2d_texture_get_alloc_memblock_type()
{
	return MemBlockType;
}

vita2d_texture *vita2d_create_empty_texture(unsigned int w, unsigned int h)
{
	return vita2d_create_empty_texture_format(w, h, SCE_GXM_TEXTURE_FORMAT_A8B8G8R8);
}

static vita2d_texture *_vita2d_create_empty_texture_format_advanced(unsigned int w, unsigned int h, SceGxmTextureFormat format, unsigned int isRenderTarget)
{
	if (w > GXM_TEX_MAX_SIZE || h > GXM_TEX_MAX_SIZE)
		return NULL;

	vita2d_texture *texture = malloc(sizeof(*texture));
	if (!texture)
		return NULL;

	memset(texture, 0, sizeof(vita2d_texture));

	const int tex_size =  w * h * tex_format_to_bytespp(format);

	/* Allocate a GPU buffer for the texture */
	void *texture_data = gpu_alloc(
		MemBlockType,
		tex_size,
		SCE_GXM_TEXTURE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&texture->data_UID);

	if (!texture_data) {
		free(texture);
		return NULL;
	}

	/* Clear the texture */
	memset(texture_data, 0, tex_size);

	/* Create the gxm texture */
	sceGxmTextureInitLinear(
		&texture->gxm_tex,
		texture_data,
		format,
		w,
		h,
		0);

	if ((format & 0x9f000000U) == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {

		const int pal_size = 256 * sizeof(uint32_t);

		void *texture_palette = gpu_alloc(
			MemBlockType,
			pal_size,
			SCE_GXM_PALETTE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ,
			&texture->palette_UID);

		if (!texture_palette) {
			texture->palette_UID = 0;
			vita2d_free_texture(texture);
			return NULL;
		}

		memset(texture_palette, 0, pal_size);

		sceGxmTextureSetPalette(&texture->gxm_tex, texture_palette);
	} else {
		texture->palette_UID = 0;
	}

	if (isRenderTarget) {

		int err = sceGxmColorSurfaceInit(
			&texture->gxm_sfc,
			SCE_GXM_COLOR_FORMAT_A8B8G8R8,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			SCE_GXM_COLOR_SURFACE_SCALE_NONE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			w,
			h,
			w,
			texture_data
		);

		if (err < 0) {
			vita2d_free_texture(texture);
			return NULL;
		}

		// create the depth/stencil surface
		const uint32_t alignedWidth = ALIGN(w, SCE_GXM_TILE_SIZEX);
		const uint32_t alignedHeight = ALIGN(h, SCE_GXM_TILE_SIZEY);
		uint32_t sampleCount = alignedWidth*alignedHeight;
		uint32_t depthStrideInSamples = alignedWidth;

		// allocate it
		void *depthBufferData = gpu_alloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
			4*sampleCount,
			SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&texture->depth_UID);

		// create the SceGxmDepthStencilSurface structure
		err = sceGxmDepthStencilSurfaceInit(
			&texture->gxm_sfd,
			SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
			SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
			depthStrideInSamples,
			depthBufferData,
			NULL);

		if (err < 0) {
			vita2d_free_texture(texture);
			return NULL;
		}

		SceGxmRenderTarget *tgt = NULL;

		// set up parameters
		SceGxmRenderTargetParams renderTargetParams;
		memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
		renderTargetParams.flags = 0;
		renderTargetParams.width = w;
		renderTargetParams.height = h;
		renderTargetParams.scenesPerFrame = 1;
		renderTargetParams.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
		renderTargetParams.multisampleLocations = 0;
		renderTargetParams.driverMemBlock = -1;

		// create the render target
		err = sceGxmCreateRenderTarget(&renderTargetParams, &tgt);

		texture->gxm_rtgt = tgt;

		if (err < 0) {
			vita2d_free_texture(texture);
			return NULL;
		}

	}

	return texture;
}

vita2d_texture * vita2d_create_empty_texture_format(unsigned int w, unsigned int h, SceGxmTextureFormat format)
{
	return _vita2d_create_empty_texture_format_advanced(w, h, format, 0);
}

vita2d_texture * vita2d_create_empty_texture_rendertarget(unsigned int w, unsigned int h, SceGxmTextureFormat format)
{
	return _vita2d_create_empty_texture_format_advanced(w, h, format, 1);
}

void vita2d_free_texture(vita2d_texture *texture)
{
	if (texture) {
		if (texture->gxm_rtgt) {
			sceGxmDestroyRenderTarget(texture->gxm_rtgt);
		}
		if (texture->depth_UID) {
			gpu_free(texture->depth_UID);
		}
		if (texture->palette_UID) {
			gpu_free(texture->palette_UID);
		}
		gpu_free(texture->data_UID);
		free(texture);
	}
}

unsigned int vita2d_texture_get_width(const vita2d_texture *texture)
{
	return sceGxmTextureGetWidth(&texture->gxm_tex);
}

unsigned int vita2d_texture_get_height(const vita2d_texture *texture)
{
	return sceGxmTextureGetHeight(&texture->gxm_tex);
}

unsigned int vita2d_texture_get_stride(const vita2d_texture *texture)
{
	return ((vita2d_texture_get_width(texture) + 7) & ~7)
		* tex_format_to_bytespp(vita2d_texture_get_format(texture));
}

SceGxmTextureFormat vita2d_texture_get_format(const vita2d_texture *texture)
{
	return sceGxmTextureGetFormat(&texture->gxm_tex);
}

void *vita2d_texture_get_datap(const vita2d_texture *texture)
{
	return sceGxmTextureGetData(&texture->gxm_tex);
}

void *vita2d_texture_get_palette(const vita2d_texture *texture)
{
	return sceGxmTextureGetPalette(&texture->gxm_tex);
}

SceGxmTextureFilter vita2d_texture_get_min_filter(const vita2d_texture *texture)
{
	return sceGxmTextureGetMinFilter(&texture->gxm_tex);
}

SceGxmTextureFilter vita2d_texture_get_mag_filter(const vita2d_texture *texture)
{
	return sceGxmTextureGetMagFilter(&texture->gxm_tex);
}

void vita2d_texture_set_filters(vita2d_texture *texture, SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter)
{
	sceGxmTextureSetMinFilter(&texture->gxm_tex, min_filter);
	sceGxmTextureSetMagFilter(&texture->gxm_tex, mag_filter);
}

static inline void set_texture_program()
{
	sceGxmSetVertexProgram(_vita2d_context, _vita2d_textureVertexProgram);
	sceGxmSetFragmentProgram(_vita2d_context, _vita2d_textureFragmentProgram);
}

static inline void set_texture_tint_program()
{
	sceGxmSetVertexProgram(_vita2d_context, _vita2d_textureTintVertexProgram);
	sceGxmSetFragmentProgram(_vita2d_context, _vita2d_textureTintFragmentProgram);
}

static inline void set_texture_wvp_uniform()
{
	void *vertex_wvp_buffer;
	sceGxmReserveVertexDefaultUniformBuffer(_vita2d_context, &vertex_wvp_buffer);
	sceGxmSetUniformDataF(vertex_wvp_buffer, _vita2d_textureWvpParam, 0, 16, _vita2d_ortho_matrix);
}

static inline void set_texture_tint_wvp_uniform()
{
	void *vertex_wvp_buffer;
	sceGxmReserveVertexDefaultUniformBuffer(_vita2d_context, &vertex_wvp_buffer);
	sceGxmSetUniformDataF(vertex_wvp_buffer, _vita2d_textureTintWvpParam, 0, 16, _vita2d_ortho_matrix);
}

static inline void draw_texture_generic(const vita2d_texture *texture, float x, float y)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = x + w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = 1.0f;
	vertices[1].v = 0.0f;

	vertices[2].x = x;
	vertices[2].y = y + h;
	vertices[2].z = +0.5f;
	vertices[2].u = 0.0f;
	vertices[2].v = 1.0f;

	vertices[3].x = x + w;
	vertices[3].y = y + h;
	vertices[3].z = +0.5f;
	vertices[3].u = 1.0f;
	vertices[3].v = 1.0f;

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

void vita2d_draw_texture(const vita2d_texture *texture, float x, float y)
{
	set_texture_program();
	set_texture_wvp_uniform();
	draw_texture_generic(texture, x, y);
}

void vita2d_draw_texture_tint(const vita2d_texture *texture, float x, float y, unsigned int color)
{
	set_texture_tint_program();
	set_texture_wvp_uniform();
	draw_texture_generic(texture, x, y);
}

void vita2d_draw_texture_rotate(const vita2d_texture *texture, float x, float y, float rad)
{
	vita2d_draw_texture_rotate_hotspot(texture, x, y, rad,
		vita2d_texture_get_width(texture)/2.0f,
		vita2d_texture_get_height(texture)/2.0f);
}

void vita2d_draw_texture_tint_rotate(const vita2d_texture *texture, float x, float y, float rad, unsigned int color)
{
	vita2d_draw_texture_tint_rotate_hotspot(texture, x, y, rad,
		vita2d_texture_get_width(texture)/2.0f,
		vita2d_texture_get_height(texture)/2.0f,
		color);
}

static inline void draw_texture_rotate_hotspot_generic(const vita2d_texture *texture, float x, float y, float rad, float center_x, float center_y)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

	vertices[0].x = -center_x;
	vertices[0].y = -center_y;
	vertices[0].z = +0.5f;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = w - center_x;
	vertices[1].y = -center_y;
	vertices[1].z = +0.5f;
	vertices[1].u = 1.0f;
	vertices[1].v = 0.0f;

	vertices[2].x = -center_x;
	vertices[2].y = h - center_y;
	vertices[2].z = +0.5f;
	vertices[2].u = 0.0f;
	vertices[2].v = 1.0f;

	vertices[3].x = w - center_x;
	vertices[3].y = h - center_y;
	vertices[3].z = +0.5f;
	vertices[3].u = 1.0f;
	vertices[3].v = 1.0f;

	float c = cosf(rad);
	float s = sinf(rad);
	int i;
	for (i = 0; i < 4; ++i) { // Rotate and translate
		float _x = vertices[i].x;
		float _y = vertices[i].y;
		vertices[i].x = _x*c - _y*s + x;
		vertices[i].y = _x*s + _y*c + y;
	}

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

void vita2d_draw_texture_rotate_hotspot(const vita2d_texture *texture, float x, float y, float rad, float center_x, float center_y)
{
	set_texture_program();
	set_texture_wvp_uniform();
	draw_texture_rotate_hotspot_generic(texture, x, y, rad, center_x, center_y);
}

void vita2d_draw_texture_tint_rotate_hotspot(const vita2d_texture *texture, float x, float y, float rad, float center_x, float center_y, unsigned int color)
{
	set_texture_tint_program();
	set_texture_wvp_uniform();
	draw_texture_rotate_hotspot_generic(texture, x, y, rad, center_x, center_y);
}

static inline void draw_texture_scale_generic(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	const float w = x_scale * vita2d_texture_get_width(texture);
	const float h = y_scale * vita2d_texture_get_height(texture);

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = x + w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = 1.0f;
	vertices[1].v = 0.0f;

	vertices[2].x = x;
	vertices[2].y = y + h;
	vertices[2].z = +0.5f;
	vertices[2].u = 0.0f;
	vertices[2].v = 1.0f;

	vertices[3].x = x + w;
	vertices[3].y = y + h;
	vertices[3].z = +0.5f;
	vertices[3].u = 1.0f;
	vertices[3].v = 1.0f;

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

static inline void draw_texture_tint_scale_generic(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, const float *color)
{
	vita2d_texture_tint_vertex *vertices = (vita2d_texture_tint_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_tint_vertex), // 4 vertices
		sizeof(vita2d_texture_tint_vertex));

	const float w = x_scale * vita2d_texture_get_width(texture);
	const float h = y_scale * vita2d_texture_get_height(texture);

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = x + w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = 1.0f;
	vertices[1].v = 0.0f;

	vertices[2].x = x;
	vertices[2].y = y + h;
	vertices[2].z = +0.5f;
	vertices[2].u = 0.0f;
	vertices[2].v = 1.0f;

	vertices[3].x = x + w;
	vertices[3].y = y + h;
	vertices[3].z = +0.5f;
	vertices[3].u = 1.0f;
	vertices[3].v = 1.0f;

   for(int n = 0; n < 4; n++){
      int k = (n + 2) % 4 ;
      vertices[n].r = color[4*k+0]; 
      vertices[n].g = color[4*k+1];
      vertices[n].b = color[4*k+2];
      vertices[n].a = color[4*k+3];
   }

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

void vita2d_draw_texture_scale(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale)
{
	set_texture_program();
	set_texture_wvp_uniform();
	draw_texture_scale_generic(texture, x, y, x_scale, y_scale);
}

void vita2d_draw_texture_tint_scale(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, const float *color)
{
	set_texture_tint_program();
	set_texture_tint_wvp_uniform();
	draw_texture_tint_scale_generic(texture, x, y, x_scale, y_scale, color);
}


static inline void draw_texture_part_generic(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

	const float u0 = tex_x/w;
	const float v0 = tex_y/h;
	const float u1 = (tex_x+tex_w)/w;
	const float v1 = (tex_y+tex_h)/h;

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = u0;
	vertices[0].v = v0;

	vertices[1].x = x + tex_w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = u1;
	vertices[1].v = v0;

	vertices[2].x = x;
	vertices[2].y = y + tex_h;
	vertices[2].z = +0.5f;
	vertices[2].u = u0;
	vertices[2].v = v1;

	vertices[3].x = x + tex_w;
	vertices[3].y = y + tex_h;
	vertices[3].z = +0.5f;
	vertices[3].u = u1;
	vertices[3].v = v1;

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

void vita2d_draw_texture_part(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h)
{
	set_texture_program();
	set_texture_wvp_uniform();
	draw_texture_part_generic(texture, x, y, tex_x, tex_y, tex_w, tex_h);
}

void vita2d_draw_texture_tint_part(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, unsigned int color)
{
	set_texture_tint_program();
	set_texture_wvp_uniform();
	draw_texture_part_generic(texture, x, y, tex_x, tex_y, tex_w, tex_h);
}

static inline void draw_texture_part_scale_generic(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

	const float u0 = tex_x/w;
	const float v0 = tex_y/h;
	const float u1 = (tex_x+tex_w)/w;
	const float v1 = (tex_y+tex_h)/h;

	tex_w *= x_scale;
	tex_h *= y_scale;

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = u0;
	vertices[0].v = v0;

	vertices[1].x = x + tex_w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = u1;
	vertices[1].v = v0;

	vertices[2].x = x;
	vertices[2].y = y + tex_h;
	vertices[2].z = +0.5f;
	vertices[2].u = u0;
	vertices[2].v = v1;

	vertices[3].x = x + tex_w;
	vertices[3].y = y + tex_h;
	vertices[3].z = +0.5f;
	vertices[3].u = u1;
	vertices[3].v = v1;

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

static inline void draw_texture_tint_part_scale_generic(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, unsigned int color)
{
	vita2d_texture_tint_vertex *vertices = (vita2d_texture_tint_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_tint_vertex), // 4 vertices
		sizeof(vita2d_texture_tint_vertex));

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

	const float u0 = tex_x/w;
	const float v0 = tex_y/h;
	const float u1 = (tex_x+tex_w)/w;
	const float v1 = (tex_y+tex_h)/h;

	tex_w *= x_scale;
	tex_h *= y_scale;

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = u0;
	vertices[0].v = v0;

	vertices[1].x = x + tex_w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = u1;
	vertices[1].v = v0;

	vertices[2].x = x;
	vertices[2].y = y + tex_h;
	vertices[2].z = +0.5f;
	vertices[2].u = u0;
	vertices[2].v = v1;

	vertices[3].x = x + tex_w;
	vertices[3].y = y + tex_h;
	vertices[3].z = +0.5f;
	vertices[3].u = u1;
	vertices[3].v = v1;

   for(int n = 0; n < 4; n++){
      vertices[n].r = ((color >> 8*0) & 0xFF)/255.0f;
      vertices[n].g = ((color >> 8*1) & 0xFF)/255.0f;
      vertices[n].b = ((color >> 8*2) & 0xFF)/255.0f;
      vertices[n].a = ((color >> 8*3) & 0xFF)/255.0f;
   }

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

void vita2d_draw_texture_part_scale(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale)
{
	set_texture_program();
	set_texture_wvp_uniform();
	draw_texture_part_scale_generic(texture, x, y, tex_x, tex_y, tex_w, tex_h, x_scale, y_scale);
}

void vita2d_draw_texture_tint_part_scale(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, unsigned int color)
{
	set_texture_tint_program();
	set_texture_wvp_uniform();
	draw_texture_tint_part_scale_generic(texture, x, y, tex_x, tex_y, tex_w, tex_h, x_scale, y_scale, color);
}

static inline void draw_texture_scale_rotate_hotspot_generic(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad, float center_x, float center_y)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	const float w = x_scale * vita2d_texture_get_width(texture);
	const float h = y_scale * vita2d_texture_get_height(texture);
	const float center_x_scaled = x_scale * center_x;
	const float center_y_scaled = y_scale * center_y;

	vertices[0].x = -center_x_scaled;
	vertices[0].y = -center_y_scaled;
	vertices[0].z = +0.5f;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = -center_x_scaled + w;
	vertices[1].y = -center_y_scaled;
	vertices[1].z = +0.5f;
	vertices[1].u = 1.0f;
	vertices[1].v = 0.0f;

	vertices[2].x = -center_x_scaled;
	vertices[2].y = -center_y_scaled + h;
	vertices[2].z = +0.5f;
	vertices[2].u = 0.0f;
	vertices[2].v = 1.0f;

	vertices[3].x = -center_x_scaled + w;
	vertices[3].y = -center_y_scaled + h;
	vertices[3].z = +0.5f;
	vertices[3].u = 1.0f;
	vertices[3].v = 1.0f;

	float c = cosf(rad);
	float s = sinf(rad);
	int i;
	for (i = 0; i < 4; ++i) { // Rotate and translate
		float _x = vertices[i].x;
		float _y = vertices[i].y;
		vertices[i].x = _x*c - _y*s + x + center_x_scaled;
		vertices[i].y = _x*s + _y*c + y + center_y_scaled;
	}

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

void vita2d_draw_texture_scale_rotate_hotspot(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad, float center_x, float center_y)
{
	set_texture_program();
	set_texture_wvp_uniform();
	draw_texture_scale_rotate_hotspot_generic(texture, x, y, x_scale, y_scale,
		rad, center_x, center_y);
}

void vita2d_draw_texture_scale_rotate(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad)
{
	vita2d_draw_texture_scale_rotate_hotspot(texture, x, y, x_scale, y_scale,
		rad, vita2d_texture_get_width(texture)/2.0f,
		vita2d_texture_get_height(texture)/2.0f);
}

void vita2d_draw_texture_tint_scale_rotate_hotspot(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad, float center_x, float center_y, unsigned int color)
{
	set_texture_tint_program();
	set_texture_wvp_uniform();
	draw_texture_scale_rotate_hotspot_generic(texture, x, y, x_scale, y_scale,
		rad, center_x, center_y);
}

void vita2d_draw_texture_tint_scale_rotate(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad, unsigned int color)
{
	vita2d_draw_texture_tint_scale_rotate_hotspot(texture, x, y, x_scale, y_scale,
		rad, vita2d_texture_get_width(texture)/2.0f,
		vita2d_texture_get_height(texture)/2.0f, color);
}

static inline void draw_texture_part_scale_rotate_generic(const vita2d_texture *texture, float x, float y,
	float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, float rad)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	const float w_full = vita2d_texture_get_width(texture);
	const float h_full = vita2d_texture_get_height(texture);

	const float w_half = (tex_w * x_scale) / 2.0f;
	const float h_half = (tex_h * y_scale) / 2.0f;

	const float u0 = tex_x / w_full;
	const float v0 = tex_y / h_full;
	const float u1 = (tex_x + tex_w) / w_full;
	const float v1 = (tex_y + tex_h) / h_full;

	vertices[0].x = -w_half;
	vertices[0].y = -h_half;
	vertices[0].z = +0.5f;
	vertices[0].u = u0;
	vertices[0].v = v0;

	vertices[1].x = w_half;
	vertices[1].y = -h_half;
	vertices[1].z = +0.5f;
	vertices[1].u = u1;
	vertices[1].v = v0;

	vertices[2].x = -w_half;
	vertices[2].y = h_half;
	vertices[2].z = +0.5f;
	vertices[2].u = u0;
	vertices[2].v = v1;

	vertices[3].x = w_half;
	vertices[3].y = h_half;
	vertices[3].z = +0.5f;
	vertices[3].u = u1;
	vertices[3].v = v1;

	const float c = cosf(rad);
	const float s = sinf(rad);
	int i;
	for (i = 0; i < 4; ++i) { // Rotate and translate
		float _x = vertices[i].x;
		float _y = vertices[i].y;
		vertices[i].x = _x*c - _y*s + x;
		vertices[i].y = _x*s + _y*c + y;
	}

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), 4);
}

void vita2d_draw_texture_part_scale_rotate(const vita2d_texture *texture, float x, float y,
	float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, float rad)
{
	set_texture_program();
	set_texture_wvp_uniform();
	draw_texture_part_scale_rotate_generic(texture, x, y,
		tex_x, tex_y, tex_w, tex_h, x_scale, y_scale, rad);
}

void vita2d_draw_texture_part_tint_scale_rotate(const vita2d_texture *texture, float x, float y,
	float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, float rad, unsigned int color)
{
	set_texture_tint_program();
	set_texture_wvp_uniform();
	draw_texture_part_scale_rotate_generic(texture, x, y,
		tex_x, tex_y, tex_w, tex_h, x_scale, y_scale, rad);
}

void vita2d_draw_array_textured(const vita2d_texture *texture, SceGxmPrimitiveType mode, const vita2d_texture_vertex *vertices, size_t count, unsigned int color)
{
	set_texture_tint_program();
	set_texture_wvp_uniform();

	sceGxmSetBackPolygonMode(_vita2d_context, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(_vita2d_context, 0, vertices);
	sceGxmDraw(_vita2d_context, mode, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), count);
}

void vita2d_draw_array_textured_mat(const vita2d_texture *texture, const vita2d_texture_tint_vertex *vertices, size_t count, float *mat)
{
	set_texture_tint_program();

   void *vertex_wvp_buffer;
   sceGxmReserveVertexDefaultUniformBuffer(_vita2d_context, &vertex_wvp_buffer);
   sceGxmSetUniformDataF(vertex_wvp_buffer, _vita2d_textureWvpParam, 0, 16, mat);

   // Set the texture to the TEXUNIT0
   sceGxmSetFragmentTexture(_vita2d_context, 0, &texture->gxm_tex);

   sceGxmSetVertexStream(_vita2d_context, 0, vertices);
   sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, vita2d_get_linear_indices(), count);
}