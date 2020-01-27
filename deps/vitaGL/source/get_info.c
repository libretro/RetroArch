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
		_vitagl_error = GL_INVALID_ENUM;
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
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
}

void glGetFloatv(GLenum pname, GLfloat *data) {
	switch (pname) {
	case GL_POLYGON_OFFSET_FACTOR: // Polygon offset factor
		*data = pol_factor;
		break;
	case GL_POLYGON_OFFSET_UNITS: // Polygon offset units
		*data = pol_units;
		break;
	case GL_MODELVIEW_MATRIX: // Modelview matrix
		memcpy(data, &modelview_matrix, sizeof(matrix4x4));
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
	default:
		_vitagl_error = GL_INVALID_ENUM;
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
	default:
		_vitagl_error = GL_INVALID_ENUM;
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
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	return ret;
}

GLenum glGetError(void) {
	GLenum ret = _vitagl_error;
	_vitagl_error = GL_NO_ERROR;
	return ret;
}
