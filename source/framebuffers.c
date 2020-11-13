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
		vgl_error = GL_INVALID_ENUM;
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
		SET_GL_ERROR(GL_INVALID_VALUE)
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
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
#endif
	while (n > 0) {
		framebuffer *fb = (framebuffer *)framebuffers[n--];
		if (fb) {
			fb->active = 0;
			if (fb->target) {
				sceGxmDestroyRenderTarget(fb->target);
				fb->target = NULL;
			}
			if (fb->depth_buffer_addr) {
				mempool_free(fb->depth_buffer_addr, fb->depth_buffer_mem_type);
				mempool_free(fb->stencil_buffer_addr, fb->stencil_buffer_mem_type);
				fb->depth_buffer_addr = NULL;
				fb->stencil_buffer_addr = NULL;
			}
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
		SET_GL_ERROR(GL_INVALID_ENUM)
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
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}

	// Aliasing to make code more readable
	texture *tex = &textures[tex_id];

	// Extracting texture sizes
	fb->width = sceGxmTextureGetWidth(&tex->gxm_tex);
	fb->height = sceGxmTextureGetHeight(&tex->gxm_tex);

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
			fb->width,
			fb->height,
			fb->width,
			sceGxmTextureGetData(&tex->gxm_tex));

		// Allocating depth and stencil buffer (FIXME: This probably shouldn't be here)
		initDepthStencilBuffer(fb->width, fb->height, &fb->depthbuffer, &fb->depth_buffer_addr, &fb->stencil_buffer_addr, &fb->depth_buffer_mem_type, &fb->stencil_buffer_mem_type);

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
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

/* vgl* */

void vglTexImageDepthBuffer(GLenum target) {
	// Setting some aliases to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *tex = &textures[texture2d_idx];

	switch (target) {
	case GL_TEXTURE_2D:
		{
			if (active_read_fb)
				sceGxmTextureInitLinear(&tex->gxm_tex, active_read_fb->depth_buffer_addr, SCE_GXM_TEXTURE_FORMAT_DF32M, active_read_fb->width, active_read_fb->height, 0);
			else
				sceGxmTextureInitLinear(&tex->gxm_tex, gxm_depth_surface_addr, SCE_GXM_TEXTURE_FORMAT_DF32M, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);
			tex->valid = 1;
		}
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}
