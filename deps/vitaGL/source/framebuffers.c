/* 
 * framebuffers.c:
 * Implementation for framebuffers related functions
 */

#include "shared.h"

static framebuffer framebuffers[BUFFERS_NUM]; // Framebuffers array

framebuffer *active_read_fb = NULL; // Current readback framebuffer in use
framebuffer *active_write_fb = NULL; // Current write framebuffer in use

uint32_t get_color_from_texture(uint32_t type) {
	uint32_t res = 0;
	switch (type) {
	case GL_RGB:
		res = SCE_GXM_COLOR_FORMAT_U8U8U8_BGR;
		break;
	case GL_RGBA:
		res = SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR;
		break;
	case GL_LUMINANCE:
		res = SCE_GXM_COLOR_FORMAT_U8_R;
		break;
	case GL_LUMINANCE_ALPHA:
		res = SCE_GXM_COLOR_FORMAT_U8U8_GR;
		break;
	case GL_INTENSITY:
		res = SCE_GXM_COLOR_FORMAT_U8_R;
		break;
	case GL_ALPHA:
		res = SCE_GXM_COLOR_FORMAT_U8_A;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	return res;
}

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */

void glGenFramebuffers(GLsizei n, GLuint *ids) {
	int i = 0, j = 0;
#ifndef SKIP_ERROR_HANDLING
	if (n < 0) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	for (i = 0; i < BUFFERS_NUM; i++) {
		if (!framebuffers[i].active) {
			ids[j++] = (GLuint)&framebuffers[i];
			framebuffers[i].active = 1;
			framebuffers[i].depth_buffer_addr = NULL;
			framebuffers[i].stencil_buffer_addr = NULL;
		}
		if (j >= n)
			break;
	}
}

void glDeleteFramebuffers(GLsizei n, GLuint *framebuffers) {
#ifndef SKIP_ERROR_HANDLING
	if (n < 0) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	while (n > 0) {
		framebuffer *fb = (framebuffer *)framebuffers[n--];
		fb->active = 0;
		if (fb->target) {
			sceGxmDestroyRenderTarget(fb->target);
			fb->target = NULL;
		}
		if (fb->depth_buffer_addr) {
			vitagl_mempool_free(fb->depth_buffer_addr, fb->depth_buffer_mem_type);
			vitagl_mempool_free(fb->stencil_buffer_addr, fb->stencil_buffer_mem_type);
			fb->depth_buffer_addr = NULL;
			fb->stencil_buffer_addr = NULL;
		}
	}
}

void glBindFramebuffer(GLenum target, GLuint fb) {
	switch (target) {
	case GL_DRAW_FRAMEBUFFER:
		active_write_fb = (framebuffer *)fb;
		break;
	case GL_READ_FRAMEBUFFER:
		active_read_fb = (framebuffer *)fb;
		break;
	case GL_FRAMEBUFFER:
		active_write_fb = active_read_fb = (framebuffer *)fb;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
}

void glFramebufferTexture(GLenum target, GLenum attachment, GLuint tex_id, GLint level) {
	// Detecting requested framebuffer
	framebuffer *fb = NULL;
	switch (target) {
	case GL_DRAW_FRAMEBUFFER:
	case GL_FRAMEBUFFER:
		fb = active_write_fb;
		break;
	case GL_READ_FRAMEBUFFER:
		fb = active_read_fb;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}

	// Aliasing to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	texture *tex = &tex_unit->textures[tex_id];

	// Extracting texture sizes
	int tex_w = sceGxmTextureGetWidth(&tex->gxm_tex);
	int tex_h = sceGxmTextureGetHeight(&tex->gxm_tex);

	// Detecting requested attachment
	switch (attachment) {
	case GL_COLOR_ATTACHMENT0:

		// Allocating colorbuffer
		sceGxmColorSurfaceInit(
			&fb->colorbuffer,
			get_color_from_texture(tex->type),
			SCE_GXM_COLOR_SURFACE_LINEAR,
			msaa_mode == SCE_GXM_MULTISAMPLE_NONE ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			tex_w,
			tex_h,
			tex_w,
			sceGxmTextureGetData(&tex->gxm_tex));

		// Allocating depth and stencil buffer (FIXME: This probably shouldn't be here)
		initDepthStencilBuffer(tex_w, tex_h, &fb->depthbuffer, &fb->depth_buffer_addr, &fb->stencil_buffer_addr, &fb->depth_buffer_mem_type, &fb->stencil_buffer_mem_type);

		// Creating rendertarget
		SceGxmRenderTargetParams renderTargetParams;
		memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
		renderTargetParams.flags = 0;
		renderTargetParams.width = sceGxmTextureGetWidth(&tex->gxm_tex);
		renderTargetParams.height = sceGxmTextureGetHeight(&tex->gxm_tex);
		renderTargetParams.scenesPerFrame = 1;
		renderTargetParams.multisampleMode = msaa_mode;
		renderTargetParams.multisampleLocations = 0;
		renderTargetParams.driverMemBlock = -1;
		sceGxmCreateRenderTarget(&renderTargetParams, &fb->target);
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
}
