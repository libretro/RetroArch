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
 * get_info.c:
 * Implementation for functions returning info to end user
 */

#include "shared.h"

// Constants returned by glGetString
static const GLubyte *vendor = "Rinnegatamante";
static const GLubyte *renderer = "SGX543MP4+";
static const GLubyte *version = "VitaGL 1.0";
static const GLubyte *extensions = "VGL_EXT_gpu_objects_array VGL_EXT_gxp_shaders";

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */

const GLubyte *glGetString(GLenum name) {
	switch (name) {
	case GL_VENDOR: // Vendor
		return vendor;
		break;
	case GL_RENDERER: // Renderer
		return renderer;
		break;
	case GL_VERSION: // openGL Version
		return version;
		break;
	case GL_EXTENSIONS: // Supported extensions
		return extensions;
		break;
	default:
		vgl_error = GL_INVALID_ENUM;
		return NULL;
		break;
	}
}

void glGetBooleanv(GLenum pname, GLboolean *params) {
	switch (pname) {
	case GL_BLEND: // Blending feature state
		*params = blend_state;
		break;
	case GL_BLEND_DST_ALPHA: // Blend Alpha Factor for Destination
		*params = (blend_dfactor_a == SCE_GXM_BLEND_FACTOR_ZERO) ? GL_FALSE : GL_TRUE;
		break;
	case GL_BLEND_DST_RGB: // Blend RGB Factor for Destination
		*params = (blend_dfactor_rgb == SCE_GXM_BLEND_FACTOR_ZERO) ? GL_FALSE : GL_TRUE;
		break;
	case GL_BLEND_SRC_ALPHA: // Blend Alpha Factor for Source
		*params = (blend_sfactor_a == SCE_GXM_BLEND_FACTOR_ZERO) ? GL_FALSE : GL_TRUE;
		break;
	case GL_BLEND_SRC_RGB: // Blend RGB Factor for Source
		*params = (blend_sfactor_rgb == SCE_GXM_BLEND_FACTOR_ZERO) ? GL_FALSE : GL_TRUE;
		break;
	case GL_DEPTH_TEST: // Depth test state
		*params = depth_test_state;
		break;
	case GL_ACTIVE_TEXTURE: // Active texture
		*params = GL_FALSE;
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glGetFloatv(GLenum pname, GLfloat *data) {
	int i, j;
	switch (pname) {
	case GL_POLYGON_OFFSET_FACTOR: // Polygon offset factor
		*data = pol_factor;
		break;
	case GL_POLYGON_OFFSET_UNITS: // Polygon offset units
		*data = pol_units;
		break;
	case GL_MODELVIEW_MATRIX: // Modelview matrix
		// Since we use column-major matrices internally, wee need to transpose it before returning it to the application
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 4; j++) {
				data[i * 4 + j] = modelview_matrix[j][i];
			}
		}
		break;
	case GL_PROJECTION_MATRIX: // Projection matrix
		// Since we use column-major matrices internally, wee need to transpose it before returning it to the application
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 4; j++) {
				data[i * 4 + j] = projection_matrix[j][i];
			}
		}
		break;
	case GL_ACTIVE_TEXTURE: // Active texture
		*data = (1.0f * (server_texture_unit + GL_TEXTURE0));
		break;
	case GL_MAX_MODELVIEW_STACK_DEPTH: // Max modelview stack depth
		*data = MODELVIEW_STACK_DEPTH;
		break;
	case GL_MAX_PROJECTION_STACK_DEPTH: // Max projection stack depth
		*data = GENERIC_STACK_DEPTH;
		break;
	case GL_MAX_TEXTURE_STACK_DEPTH: // Max texture stack depth
		*data = GENERIC_STACK_DEPTH;
		break;
	case GL_DEPTH_BITS:
		*data = 32;
		break;
	case GL_STENCIL_BITS:
		*data = 8;
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glGetIntegerv(GLenum pname, GLint *data) {
	// Aliasing to make code more readable
	texture_unit *server_tex_unit = &texture_units[server_texture_unit];

	switch (pname) {
	case GL_POLYGON_MODE:
		data[0] = gl_polygon_mode_front;
		data[1] = gl_polygon_mode_back;
		break;
	case GL_SCISSOR_BOX:
		data[0] = region.x;
		data[1] = region.y;
		data[2] = region.w;
		data[3] = region.h;
		break;
	case GL_TEXTURE_BINDING_2D:
		*data = server_tex_unit->tex_id;
		break;
	case GL_MAX_TEXTURE_SIZE:
		*data = 1024;
		break;
	case GL_VIEWPORT:
		data[0] = gl_viewport.x;
		data[1] = gl_viewport.y;
		data[2] = gl_viewport.w;
		data[3] = gl_viewport.h;
		break;
	case GL_DEPTH_BITS:
		data[0] = 32;
		break;
	case GL_STENCIL_BITS:
		data[0] = 8;
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

GLboolean glIsEnabled(GLenum cap) {
	GLboolean ret = GL_FALSE;
	switch (cap) {
	case GL_DEPTH_TEST:
		ret = depth_test_state;
		break;
	case GL_STENCIL_TEST:
		ret = stencil_test_state;
		break;
	case GL_BLEND:
		ret = blend_state;
		break;
	case GL_SCISSOR_TEST:
		ret = scissor_test_state;
		break;
	case GL_CULL_FACE:
		ret = cull_face_state;
		break;
	case GL_POLYGON_OFFSET_FILL:
		ret = pol_offset_fill;
		break;
	case GL_POLYGON_OFFSET_LINE:
		ret = pol_offset_line;
		break;
	case GL_POLYGON_OFFSET_POINT:
		ret = pol_offset_point;
		break;
	default:
		vgl_error = GL_INVALID_ENUM;
		break;
	}
	return ret;
}

GLenum glGetError(void) {
	GLenum ret = vgl_error;
	vgl_error = GL_NO_ERROR;
	return ret;
}
