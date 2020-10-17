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
 * textures.c:
 * Implementation for textures related functions
 */

#include "shared.h"

texture_unit texture_units[GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS]; // Available texture units
texture textures[TEXTURES_NUM]; // Available texture slots
palette *color_table = NULL; // Current in-use color table
int8_t server_texture_unit = 0; // Current in use server side texture unit

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */

void glGenTextures(GLsizei n, GLuint *res) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (n < 0) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
#endif

	// Aliasing to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];

	// Reserving a texture and returning its id if available
	int i, j = 0;
	for (i = 0; i < TEXTURES_NUM; i++) {
		if (!(textures[i].used)) {
			res[j++] = i;
			textures[i].used = 1;
		}
		if (j >= n)
			break;
	}
}

void glBindTexture(GLenum target, GLuint texture) {
	// Aliasing to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];

	// Setting current in use texture id for the in use server texture unit
	switch (target) {
	case GL_TEXTURE_2D:
		tex_unit->tex_id = texture;
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glDeleteTextures(GLsizei n, const GLuint *gl_textures) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (n < 0) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
#endif

	// Aliasing to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];

	// Deallocating given textures and invalidating used texture ids
	int j;
	for (j = 0; j < n; j++) {
		GLuint i = gl_textures[j];
		textures[i].used = 0;
		gpu_free_texture(&textures[i]);
	}
}

void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data) {
	// Setting some aliases to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *tex = &textures[texture2d_idx];

	SceGxmTextureFormat tex_format;
	uint8_t data_bpp = 0;
	uint8_t fast_store = GL_FALSE;

	// Support for legacy GL1.0 internalFormat
	switch (internalFormat) {
	case 1:
		internalFormat = GL_RED;
		break;
	case 2:
		internalFormat = GL_RG;
		break;
	case 3:
		internalFormat = GL_RGB;
		break;
	case 4:
		internalFormat = GL_RGBA;
		break;
	}

	/*
	 * Callbacks are actually used to just perform down/up-sampling
	 * between U8 texture formats. Reads are expected to give as result
	 * a RGBA sample that will be wrote depending on texture format
	 * by the write callback
	 */
	void (*write_cb)(void *, uint32_t) = NULL;
	uint32_t (*read_cb)(void *) = NULL;

	// Detecting proper read callaback and source bpp
	switch (format) {
	case GL_RED:
	case GL_ALPHA:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			read_cb = readR;
			data_bpp = 1;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_RG:
	case GL_LUMINANCE_ALPHA:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			read_cb = readRG;
			data_bpp = 2;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_BGR:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			data_bpp = 3;
			if (internalFormat == GL_BGR)
				fast_store = GL_TRUE;
			else
				read_cb = readBGR;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_RGB:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			data_bpp = 3;
			if (internalFormat == GL_RGB)
				fast_store = GL_TRUE;
			else
				read_cb = readRGB;
			break;
		case GL_UNSIGNED_SHORT_5_6_5:
			data_bpp = 2;
			read_cb = readRGB565;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_BGRA:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			data_bpp = 4;
			if (internalFormat == GL_BGRA)
				fast_store = GL_TRUE;
			else
				read_cb = readBGRA;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_RGBA:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			data_bpp = 4;
			if (internalFormat == GL_RGBA)
				fast_store = GL_TRUE;
			else
				read_cb = readRGBA;
			break;
		case GL_UNSIGNED_SHORT_5_5_5_1:
			data_bpp = 2;
			read_cb = readRGBA5551;
			break;
		case GL_UNSIGNED_SHORT_4_4_4_4:
			data_bpp = 2;
			read_cb = readRGBA4444;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	}

	switch (target) {
	case GL_TEXTURE_2D:

		// Detecting proper write callback and texture format
		switch (internalFormat) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			tex_format = SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			tex_format = SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR;
			break;
		case GL_RGB:
			write_cb = writeRGB;
			tex_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR;
			break;
		case GL_BGR:
			write_cb = writeBGR;
			tex_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8_RGB;
			break;
		case GL_RGBA:
			write_cb = writeRGBA;
			tex_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
			break;
		case GL_BGRA:
			write_cb = writeBGRA;
			tex_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB;
			break;
		case GL_LUMINANCE:
			write_cb = writeR;
			tex_format = SCE_GXM_TEXTURE_FORMAT_L8;
			break;
		case GL_LUMINANCE_ALPHA:
			write_cb = writeRG;
			tex_format = SCE_GXM_TEXTURE_FORMAT_A8L8;
			break;
		case GL_INTENSITY:
			write_cb = writeR;
			tex_format = SCE_GXM_TEXTURE_FORMAT_U8_RRRR;
			break;
		case GL_ALPHA:
			write_cb = writeR;
			tex_format = SCE_GXM_TEXTURE_FORMAT_A8;
			break;
		case GL_COLOR_INDEX8_EXT:
			write_cb = writeR; // TODO: This is a hack
			tex_format = SCE_GXM_TEXTURE_FORMAT_P8_ABGR;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}

		// Checking if texture is too big for sceGxm
		if (width > GXM_TEX_MAX_SIZE || height > GXM_TEX_MAX_SIZE) {
			SET_GL_ERROR(GL_INVALID_VALUE)
		}

		// Allocating texture/mipmaps depending on user call
		tex->type = internalFormat;
		tex->write_cb = write_cb;
		if (level == 0)
			if (tex->write_cb)
				gpu_alloc_texture(width, height, tex_format, data, tex, data_bpp, read_cb, write_cb, fast_store);
			else
				gpu_alloc_compressed_texture(width, height, tex_format, 0, data, tex, data_bpp, read_cb);
		else
			gpu_alloc_mipmaps(level, tex);

		// Setting texture parameters
		sceGxmTextureSetUAddrMode(&tex->gxm_tex, tex_unit->u_mode);
		sceGxmTextureSetVAddrMode(&tex->gxm_tex, tex_unit->v_mode);
		sceGxmTextureSetMinFilter(&tex->gxm_tex, tex_unit->min_filter);
		sceGxmTextureSetMagFilter(&tex->gxm_tex, tex_unit->mag_filter);
		sceGxmTextureSetMipFilter(&tex->gxm_tex, tex_unit->mip_filter);
		sceGxmTextureSetLodBias(&tex->gxm_tex, tex_unit->lod_bias);

		// Setting palette if the format requests one
		if (tex->valid && tex->palette_UID)
			sceGxmTextureSetPalette(&tex->gxm_tex, color_table->data);

		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) {
	// Setting some aliases to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *target_texture = &textures[texture2d_idx];

	// Calculating implicit texture stride and start address of requested texture modification
	uint32_t orig_w = sceGxmTextureGetWidth(&target_texture->gxm_tex);
	uint32_t orig_h = sceGxmTextureGetHeight(&target_texture->gxm_tex);
	SceGxmTextureFormat tex_format = sceGxmTextureGetFormat(&target_texture->gxm_tex);
	uint8_t bpp = tex_format_to_bytespp(tex_format);
	uint32_t stride = ALIGN(orig_w, 8) * bpp;
	uint8_t *ptr = (uint8_t *)sceGxmTextureGetData(&target_texture->gxm_tex) + xoffset * bpp + yoffset * stride;
	uint8_t *ptr_line = ptr;
	uint8_t data_bpp = 0;
	int i, j;

	if (xoffset + width > orig_w) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	} else if (yoffset + height > orig_h) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}

	// Support for legacy GL1.0 format
	switch (format) {
	case 1:
		format = GL_RED;
		break;
	case 2:
		format = GL_RG;
		break;
	case 3:
		format = GL_RGB;
		break;
	case 4:
		format = GL_RGBA;
		break;
	}

	/*
	 * Callbacks are actually used to just perform down/up-sampling
	 * between U8 texture formats. Reads are expected to give as result
	 * a RGBA sample that will be wrote depending on texture format
	 * by the write callback
	 */
	void (*write_cb)(void *, uint32_t) = NULL;
	uint32_t (*read_cb)(void *) = NULL;

	// Detecting proper read callback and source bpp
	switch (format) {
	case GL_RED:
	case GL_ALPHA:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			read_cb = readR;
			data_bpp = 1;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_RG:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			read_cb = readRG;
			data_bpp = 2;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_RGB:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			data_bpp = 3;
			read_cb = readRGB;
			break;
		case GL_UNSIGNED_SHORT_5_6_5:
			data_bpp = 2;
			read_cb = readRGB565;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_BGR:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			data_bpp = 3;
			read_cb = readBGR;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_RGBA:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			data_bpp = 4;
			read_cb = readRGBA;
			break;
		case GL_UNSIGNED_SHORT_5_5_5_1:
			data_bpp = 2;
			read_cb = readRGBA5551;
			break;
		case GL_UNSIGNED_SHORT_4_4_4_4:
			data_bpp = 2;
			read_cb = readRGBA4444;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	case GL_BGRA:
		switch (type) {
		case GL_UNSIGNED_BYTE:
			data_bpp = 4;
			read_cb = readBGRA;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	}

	switch (target) {
	case GL_TEXTURE_2D:

		// Detecting proper write callback
		switch (target_texture->type) {
		case GL_RGB:
			write_cb = writeRGB;
			break;
		case GL_BGR:
			write_cb = writeBGR;
			break;
		case GL_RGBA:
			write_cb = writeRGBA;
			break;
		case GL_BGRA:
			write_cb = writeBGRA;
			break;
		case GL_LUMINANCE:
			write_cb = writeR;
			break;
		case GL_LUMINANCE_ALPHA:
			write_cb = writeRG;
			break;
		case GL_INTENSITY:
			write_cb = writeR;
			break;
		case GL_ALPHA:
			write_cb = writeR;
			break;
		}

		// Executing texture modification via callbacks
		uint8_t *data = (uint8_t *)pixels;
		for (i = 0; i < height; i++) {
			for (j = 0; j < width; j++) {
				uint32_t clr = read_cb((uint8_t *)data);
				write_cb(ptr, clr);
				data += data_bpp;
				ptr += bpp;
			}
			ptr = ptr_line + stride;
			ptr_line = ptr;
		}

		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data) {
	// Setting some aliases to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *tex = &textures[texture2d_idx];

	SceGxmTextureFormat tex_format;

#ifndef SKIP_ERROR_HANDLING
	// Checking if texture is too big for sceGxm
	if (width > GXM_TEX_MAX_SIZE || height > GXM_TEX_MAX_SIZE) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}

	// Checking if texture dimensions are not a power of two
	if (((width & (width - 1)) != 0) || ((height & (height - 1)) != 0)) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}

	// Ensure imageSize isn't zero.
	if (imageSize == 0) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
#endif

	switch (target) {
	case GL_TEXTURE_2D:
		// Detecting proper write callback and texture format
		switch (internalFormat) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			tex_format = SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			tex_format = SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR;
			break;
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
			tex_format = SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_1BGR;
			break;
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
			tex_format = SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_ABGR;
			break;
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
			tex_format = SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_1BGR;
			break;
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
			tex_format = SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_ABGR;
			break;
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG:
			tex_format = SCE_GXM_TEXTURE_FORMAT_PVRTII2BPP_ABGR;
			break;
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG:
			tex_format = SCE_GXM_TEXTURE_FORMAT_PVRTII4BPP_ABGR;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}

		// Allocating texture/mipmaps depending on user call
		tex->type = internalFormat;
		gpu_alloc_compressed_texture(width, height, tex_format, imageSize, data, tex, 0, NULL);

		// Setting texture parameters
		sceGxmTextureSetUAddrMode(&tex->gxm_tex, tex_unit->u_mode);
		sceGxmTextureSetVAddrMode(&tex->gxm_tex, tex_unit->v_mode);
		sceGxmTextureSetMinFilter(&tex->gxm_tex, tex_unit->min_filter);
		sceGxmTextureSetMagFilter(&tex->gxm_tex, tex_unit->mag_filter);
		sceGxmTextureSetMipFilter(&tex->gxm_tex, tex_unit->mip_filter);
		sceGxmTextureSetLodBias(&tex->gxm_tex, tex_unit->lod_bias);

		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glColorTable(GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *data) {
	// Checking if a color table is already enabled, if so, deallocating it
	if (color_table != NULL) {
		gpu_free_palette(color_table);
		color_table = NULL;
	}

	// Calculating color table bpp
	uint8_t bpp = 0;
	switch (target) {
	case GL_COLOR_TABLE:
		switch (format) {
		case GL_RGBA:
			bpp = 4;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}

	// Allocating and initializing color table
	color_table = gpu_alloc_palette(data, width, bpp);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
	// Setting some aliases to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *tex = &textures[texture2d_idx];

	switch (target) {
	case GL_TEXTURE_2D:
		switch (pname) {
		case GL_TEXTURE_MIN_FILTER: // Min filter
			switch (param) {
			case GL_NEAREST: // Point
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
				break;
			case GL_LINEAR: // Linear
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
				break;
			case GL_NEAREST_MIPMAP_NEAREST:
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_MIPMAP_POINT;
				break;
			case GL_LINEAR_MIPMAP_NEAREST:
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_ENABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_MIPMAP_POINT;
				break;
			case GL_NEAREST_MIPMAP_LINEAR:
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_MIPMAP_LINEAR;
				break;
			case GL_LINEAR_MIPMAP_LINEAR:
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_ENABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_MIPMAP_LINEAR;
				break;
			default:
				SET_GL_ERROR(GL_INVALID_ENUM)
				break;
			}
			sceGxmTextureSetMinFilter(&tex->gxm_tex, tex_unit->min_filter);
			break;
		case GL_TEXTURE_MAG_FILTER: // Mag Filter
			switch (param) {
			case GL_NEAREST:
				tex_unit->mag_filter = SCE_GXM_TEXTURE_FILTER_POINT;
				break;
			case GL_LINEAR:
				tex_unit->mag_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
				break;
			default:
				SET_GL_ERROR(GL_INVALID_ENUM)
				break;
			}
			sceGxmTextureSetMagFilter(&tex->gxm_tex, tex_unit->mag_filter);
			break;
		case GL_TEXTURE_WRAP_S: // U Mode
			switch (param) {
			case GL_CLAMP_TO_EDGE: // Clamp
				tex_unit->u_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
				break;
			case GL_REPEAT: // Repeat
				tex_unit->u_mode = SCE_GXM_TEXTURE_ADDR_REPEAT;
				break;
			case GL_MIRRORED_REPEAT: // Mirror
				tex_unit->u_mode = SCE_GXM_TEXTURE_ADDR_MIRROR;
				break;
			case GL_MIRROR_CLAMP_EXT: // Mirror Clamp
				tex_unit->u_mode = SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP;
				break;
			default:
				SET_GL_ERROR(GL_INVALID_ENUM)
				break;
			}
			sceGxmTextureSetUAddrMode(&tex->gxm_tex, tex_unit->u_mode);
			break;
		case GL_TEXTURE_WRAP_T: // V Mode
			switch (param) {
			case GL_CLAMP_TO_EDGE: // Clamp
				tex_unit->v_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
				break;
			case GL_REPEAT: // Repeat
				tex_unit->v_mode = SCE_GXM_TEXTURE_ADDR_REPEAT;
				break;
			case GL_MIRRORED_REPEAT: // Mirror
				tex_unit->v_mode = SCE_GXM_TEXTURE_ADDR_MIRROR;
				break;
			case GL_MIRROR_CLAMP_EXT: // Mirror Clamp
				tex_unit->v_mode = SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP;
				break;
			default:
				SET_GL_ERROR(GL_INVALID_ENUM)
				break;
			}
			sceGxmTextureSetVAddrMode(&tex->gxm_tex, tex_unit->v_mode);
			break;
		case GL_TEXTURE_LOD_BIAS: // Distant LOD bias
			tex_unit->lod_bias = (uint32_t)(param + GL_MAX_TEXTURE_LOD_BIAS);
			sceGxmTextureSetLodBias(&tex->gxm_tex, tex_unit->lod_bias);
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
	// Setting some aliases to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *tex = &textures[texture2d_idx];

	switch (target) {
	case GL_TEXTURE_2D:
		switch (pname) {
		case GL_TEXTURE_MIN_FILTER: // Min Filter
			if (param == GL_NEAREST) {
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
			}
			if (param == GL_LINEAR) {
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
			}
			if (param == GL_NEAREST_MIPMAP_NEAREST) {
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_MIPMAP_POINT;
			}
			if (param == GL_LINEAR_MIPMAP_NEAREST) {
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_ENABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_MIPMAP_POINT;
			}
			if (param == GL_NEAREST_MIPMAP_LINEAR) {
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_MIPMAP_LINEAR;
			}
			if (param == GL_LINEAR_MIPMAP_LINEAR) {
				tex_unit->mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_ENABLED;
				tex_unit->min_filter = SCE_GXM_TEXTURE_FILTER_MIPMAP_LINEAR;
			}
			sceGxmTextureSetMinFilter(&tex->gxm_tex, tex_unit->min_filter);
			sceGxmTextureSetMipFilter(&tex->gxm_tex, tex_unit->mip_filter);
			break;
		case GL_TEXTURE_MAG_FILTER: // Mag filter
			if (param == GL_NEAREST)
				tex_unit->mag_filter = SCE_GXM_TEXTURE_FILTER_POINT;
			else if (param == GL_LINEAR)
				tex_unit->mag_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
			sceGxmTextureSetMagFilter(&tex->gxm_tex, tex_unit->mag_filter);
			break;
		case GL_TEXTURE_WRAP_S: // U Mode
			if (param == GL_CLAMP_TO_EDGE)
				tex_unit->u_mode = SCE_GXM_TEXTURE_ADDR_CLAMP; // Clamp
			else if (param == GL_REPEAT)
				tex_unit->u_mode = SCE_GXM_TEXTURE_ADDR_REPEAT; // Repeat
			else if (param == GL_MIRRORED_REPEAT)
				tex_unit->u_mode = SCE_GXM_TEXTURE_ADDR_MIRROR; // Mirror
			else if (param == GL_MIRROR_CLAMP_EXT)
				tex_unit->u_mode = SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP; // Mirror Clamp
			sceGxmTextureSetUAddrMode(&tex->gxm_tex, tex_unit->u_mode);
			break;
		case GL_TEXTURE_WRAP_T: // V Mode
			if (param == GL_CLAMP_TO_EDGE)
				tex_unit->v_mode = SCE_GXM_TEXTURE_ADDR_CLAMP; // Clamp
			else if (param == GL_REPEAT)
				tex_unit->v_mode = SCE_GXM_TEXTURE_ADDR_REPEAT; // Repeat
			else if (param == GL_MIRRORED_REPEAT)
				tex_unit->v_mode = SCE_GXM_TEXTURE_ADDR_MIRROR; // Mirror
			else if (param == GL_MIRROR_CLAMP_EXT)
				tex_unit->v_mode = SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP; // Mirror Clamp
			sceGxmTextureSetVAddrMode(&tex->gxm_tex, tex_unit->v_mode);
			break;
		case GL_TEXTURE_LOD_BIAS: // Distant LOD bias
			tex_unit->lod_bias = (uint32_t)(param + GL_MAX_TEXTURE_LOD_BIAS);
			sceGxmTextureSetLodBias(&tex->gxm_tex, tex_unit->lod_bias);
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glActiveTexture(GLenum texture) {
	// Changing current in use server texture unit
#ifndef SKIP_ERROR_HANDLING
	if ((texture < GL_TEXTURE0) && (texture > GL_TEXTURE31)) {
		SET_GL_ERROR(GL_INVALID_ENUM)
	} else
#endif
		server_texture_unit = texture - GL_TEXTURE0;
}

void glGenerateMipmap(GLenum target) {
	// Setting some aliases to make code more readable
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *tex = &textures[texture2d_idx];

#ifndef SKIP_ERROR_HANDLING
	// Checking if current texture is valid
	if (!tex->valid)
		return;
#endif

	switch (target) {
	case GL_TEXTURE_2D:

		// Generating mipmaps to the max possible level
		gpu_alloc_mipmaps(-1, tex);

		// Setting texture parameters
		sceGxmTextureSetUAddrMode(&tex->gxm_tex, tex_unit->u_mode);
		sceGxmTextureSetVAddrMode(&tex->gxm_tex, tex_unit->v_mode);
		sceGxmTextureSetMinFilter(&tex->gxm_tex, tex_unit->min_filter);
		sceGxmTextureSetMagFilter(&tex->gxm_tex, tex_unit->mag_filter);
		sceGxmTextureSetMipFilter(&tex->gxm_tex, tex_unit->mip_filter);
		sceGxmTextureSetLodBias(&tex->gxm_tex, tex_unit->lod_bias);

		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
	// Aliasing texture unit for cleaner code
	texture_unit *tex_unit = &texture_units[server_texture_unit];

	// Properly changing texture environment settings as per request
	switch (target) {
	case GL_TEXTURE_ENV:
		switch (pname) {
		case GL_TEXTURE_ENV_MODE:
			if (param == GL_MODULATE)
				tex_unit->env_mode = MODULATE;
			else if (param == GL_DECAL)
				tex_unit->env_mode = DECAL;
			else if (param == GL_REPLACE)
				tex_unit->env_mode = REPLACE;
			else if (param == GL_BLEND)
				tex_unit->env_mode = BLEND;
			else if (param == GL_ADD)
				tex_unit->env_mode = ADD;
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glTexEnvfv(GLenum target, GLenum pname, GLfloat *param) {
	// Properly changing texture environment settings as per request
	switch (target) {
	case GL_TEXTURE_ENV:
		switch (pname) {
		case GL_TEXTURE_ENV_COLOR:
			memcpy_neon(&texenv_color.r, param, sizeof(GLfloat) * 4);
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glTexEnvi(GLenum target, GLenum pname, GLint param) {
	// Aliasing texture unit for cleaner code
	texture_unit *tex_unit = &texture_units[server_texture_unit];

	// Properly changing texture environment settings as per request
	switch (target) {
	case GL_TEXTURE_ENV:
		switch (pname) {
		case GL_TEXTURE_ENV_MODE:
			switch (param) {
			case GL_MODULATE:
				tex_unit->env_mode = MODULATE;
				break;
			case GL_DECAL:
				tex_unit->env_mode = DECAL;
				break;
			case GL_REPLACE:
				tex_unit->env_mode = REPLACE;
				break;
			case GL_BLEND:
				tex_unit->env_mode = BLEND;
				break;
			case GL_ADD:
				tex_unit->env_mode = ADD;
				break;
			}
			break;
		default:
			SET_GL_ERROR(GL_INVALID_ENUM)
			break;
		}
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void *vglGetTexDataPointer(GLenum target) {
	// Aliasing texture unit for cleaner code
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *tex = &textures[texture2d_idx];

	switch (target) {
	case GL_TEXTURE_2D:
		return tex->data;
		break;
	default:
		vgl_error = GL_INVALID_ENUM;
		break;
	}

	return NULL;
}

SceGxmTexture *vglGetGxmTexture(GLenum target) {
	// Aliasing texture unit for cleaner code
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	texture *tex = &textures[texture2d_idx];

	switch (target) {
	case GL_TEXTURE_2D:
		return &tex->gxm_tex;
		break;
	default:
		vgl_error = GL_INVALID_ENUM;
		break;
	}

	return NULL;
}
