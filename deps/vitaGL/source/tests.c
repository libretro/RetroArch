/* 
 * tests.c:
 * Implementation for all drawing tests functions
 */

#include "shared.h"

// Depth Test
GLboolean depth_test_state = GL_FALSE; // Current state for GL_DEPTH_TEST
SceGxmDepthFunc gxm_depth = SCE_GXM_DEPTH_FUNC_LESS; // Current in-use depth test func
GLenum orig_depth_test; // Original depth test state (used for depth test invalidation)
GLdouble depth_value = 1.0f; // Current depth test clear value
GLboolean depth_mask_state = GL_TRUE; // Current state for glDepthMask

// Scissor Test
scissor_region region; // Current scissor test region setup
GLboolean scissor_test_state = GL_FALSE; // Current state for GL_SCISSOR_TEST
SceGxmFragmentProgram *scissor_test_fragment_program; // Scissor test fragment program
vector2f *scissor_test_vertices = NULL; // Scissor test region vertices
SceUID scissor_test_vertices_uid; // Scissor test vertices memblock id

// Stencil Test
uint8_t stencil_mask_front = 0xFF; // Current in use mask for stencil test on front
uint8_t stencil_mask_back = 0xFF; // Current in use mask for stencil test on back
uint8_t stencil_mask_front_write = 0xFF; // Current in use mask for write stencil test on front
uint8_t stencil_mask_back_write = 0xFF; // Current in use mask for write stencil test on back
uint8_t stencil_ref_front = 0; // Current in use reference for stencil test on front
uint8_t stencil_ref_back = 0; // Current in use reference for stencil test on back
SceGxmStencilOp stencil_fail_front = SCE_GXM_STENCIL_OP_KEEP; // Current in use stencil operation when stencil test fails for front
SceGxmStencilOp depth_fail_front = SCE_GXM_STENCIL_OP_KEEP; // Current in use stencil operation when depth test fails for front
SceGxmStencilOp depth_pass_front = SCE_GXM_STENCIL_OP_KEEP; // Current in use stencil operation when depth test passes for front
SceGxmStencilOp stencil_fail_back = SCE_GXM_STENCIL_OP_KEEP; // Current in use stencil operation when stencil test fails for back
SceGxmStencilOp depth_fail_back = SCE_GXM_STENCIL_OP_KEEP; // Current in use stencil operation when depth test fails for back
SceGxmStencilOp depth_pass_back = SCE_GXM_STENCIL_OP_KEEP; // Current in use stencil operation when depth test passes for back
SceGxmStencilFunc stencil_func_front = SCE_GXM_STENCIL_FUNC_ALWAYS; // Current in use stencil function on front
SceGxmStencilFunc stencil_func_back = SCE_GXM_STENCIL_FUNC_ALWAYS; // Current in use stencil function on back
GLboolean stencil_test_state = GL_FALSE; // Current state for GL_STENCIL_TEST
GLint stencil_value = 0; // Current stencil test clear value

// Alpha Test
GLenum alpha_func = GL_ALWAYS; // Current in-use alpha test mode
GLfloat alpha_ref = 0.0f; // Current in use alpha test reference value
int alpha_op = ALWAYS; // Current in use alpha test operation
GLboolean alpha_test_state = GL_FALSE; // Current state for GL_ALPHA_TEST

void change_depth_write(SceGxmDepthWriteMode mode) {
	// Change depth write mode for both front and back primitives
	sceGxmSetFrontDepthWriteEnable(gxm_context, mode);
	sceGxmSetBackDepthWriteEnable(gxm_context, mode);
}

void change_depth_func() {
	// Setting depth function for both front and back primitives
	sceGxmSetFrontDepthFunc(gxm_context, depth_test_state ? gxm_depth : SCE_GXM_DEPTH_FUNC_ALWAYS);
	sceGxmSetBackDepthFunc(gxm_context, depth_test_state ? gxm_depth : SCE_GXM_DEPTH_FUNC_ALWAYS);

	// Calling an update for the depth write mode
	change_depth_write((depth_mask_state && depth_test_state) ? SCE_GXM_DEPTH_WRITE_ENABLED : SCE_GXM_DEPTH_WRITE_DISABLED);
}

void invalidate_depth_test() {
	// Invalidating current depth test state
	orig_depth_test = depth_test_state;
	depth_test_state = GL_FALSE;

	// Invoking a depth function update
	change_depth_func();
}

void validate_depth_test() {
	// Resetting original depth test state
	depth_test_state = orig_depth_test;

	// Invoking a depth function update
	change_depth_func();
}

void change_stencil_settings() {
	if (stencil_test_state) {
		// Setting stencil function for both front and back primitives
		sceGxmSetFrontStencilFunc(gxm_context,
			stencil_func_front,
			stencil_fail_front,
			depth_fail_front,
			depth_pass_front,
			stencil_mask_front, stencil_mask_front_write);
		sceGxmSetBackStencilFunc(gxm_context,
			stencil_func_back,
			stencil_fail_back,
			depth_fail_back,
			depth_pass_back,
			stencil_mask_back, stencil_mask_back_write);

		// Setting stencil ref for both front and back primitives
		sceGxmSetFrontStencilRef(gxm_context, stencil_ref_front);
		sceGxmSetBackStencilRef(gxm_context, stencil_ref_back);

	} else {
		sceGxmSetFrontStencilFunc(gxm_context,
			SCE_GXM_STENCIL_FUNC_ALWAYS,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			0, 0);
		sceGxmSetBackStencilFunc(gxm_context,
			SCE_GXM_STENCIL_FUNC_ALWAYS,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			0, 0);
	}
}

GLboolean change_stencil_config(SceGxmStencilOp *cfg, GLenum new) {
	// Translating openGL stencil operation value to sceGxm one
	GLboolean ret = GL_TRUE;
	switch (new) {
	case GL_KEEP:
		*cfg = SCE_GXM_STENCIL_OP_KEEP;
		break;
	case GL_ZERO:
		*cfg = SCE_GXM_STENCIL_OP_ZERO;
		break;
	case GL_REPLACE:
		*cfg = SCE_GXM_STENCIL_OP_REPLACE;
		break;
	case GL_INCR:
		*cfg = SCE_GXM_STENCIL_OP_INCR;
		break;
	case GL_INCR_WRAP:
		*cfg = SCE_GXM_STENCIL_OP_INCR_WRAP;
		break;
	case GL_DECR:
		*cfg = SCE_GXM_STENCIL_OP_DECR;
		break;
	case GL_DECR_WRAP:
		*cfg = SCE_GXM_STENCIL_OP_DECR_WRAP;
		break;
	case GL_INVERT:
		*cfg = SCE_GXM_STENCIL_OP_INVERT;
		break;
	default:
		ret = GL_FALSE;
		break;
	}
	return ret;
}

GLboolean change_stencil_func_config(SceGxmStencilFunc *cfg, GLenum new) {
	// Translating openGL stencil function to sceGxm one
	GLboolean ret = GL_TRUE;
	switch (new) {
	case GL_NEVER:
		*cfg = SCE_GXM_STENCIL_FUNC_NEVER;
		break;
	case GL_LESS:
		*cfg = SCE_GXM_STENCIL_FUNC_LESS;
		break;
	case GL_LEQUAL:
		*cfg = SCE_GXM_STENCIL_FUNC_LESS_EQUAL;
		break;
	case GL_GREATER:
		*cfg = SCE_GXM_STENCIL_FUNC_GREATER;
		break;
	case GL_GEQUAL:
		*cfg = SCE_GXM_STENCIL_FUNC_GREATER_EQUAL;
		break;
	case GL_EQUAL:
		*cfg = SCE_GXM_STENCIL_FUNC_EQUAL;
		break;
	case GL_NOTEQUAL:
		*cfg = SCE_GXM_STENCIL_FUNC_NOT_EQUAL;
		break;
	case GL_ALWAYS:
		*cfg = SCE_GXM_STENCIL_FUNC_ALWAYS;
		break;
	default:
		ret = GL_FALSE;
		break;
	}
	return ret;
}

void update_alpha_test_settings() {
	// Translating openGL alpha test operation to internal one
	if (alpha_test_state) {
		switch (alpha_func) {
		case GL_EQUAL:
			alpha_op = EQUAL;
			break;
		case GL_LEQUAL:
			alpha_op = LESS_EQUAL;
			break;
		case GL_GEQUAL:
			alpha_op = GREATER_EQUAL;
			break;
		case GL_LESS:
			alpha_op = LESS;
			break;
		case GL_GREATER:
			alpha_op = GREATER;
			break;
		case GL_NOTEQUAL:
			alpha_op = NOT_EQUAL;
			break;
		case GL_NEVER:
			alpha_op = NEVER;
			break;
		default:
			alpha_op = ALWAYS;
			break;
		}
	} else
		alpha_op = ALWAYS;
}

void update_scissor_test() {
	// Calculating scissor test region vertices
	if (scissor_test_state) {
		vector2f_convert_to_local_space(scissor_test_vertices, region.x, region.y, region.w, region.h);
	}

	// Setting current vertex program to clear screen one and fragment program to scissor test one
	sceGxmSetVertexProgram(gxm_context, clear_vertex_program_patched);
	sceGxmSetFragmentProgram(gxm_context, scissor_test_fragment_program);

	// Cleaning stencil surface mask update bit on the whole screen
	sceGxmSetFrontStencilFunc(gxm_context,
		SCE_GXM_STENCIL_FUNC_NEVER,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		0, 0);
	sceGxmSetBackStencilFunc(gxm_context,
		SCE_GXM_STENCIL_FUNC_NEVER,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		0, 0);
	sceGxmSetVertexStream(gxm_context, 0, clear_vertices);
	sceGxmDraw(gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_FAN, SCE_GXM_INDEX_FORMAT_U16, depth_clear_indices, 4);

	// Setting stencil surface mask update bit on the scissor test region
	sceGxmSetFrontStencilFunc(gxm_context,
		SCE_GXM_STENCIL_FUNC_ALWAYS,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		0, 0);
	sceGxmSetBackStencilFunc(gxm_context,
		SCE_GXM_STENCIL_FUNC_ALWAYS,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		0, 0);
	if (scissor_test_state)
		sceGxmSetVertexStream(gxm_context, 0, scissor_test_vertices);
	else
		sceGxmSetVertexStream(gxm_context, 0, clear_vertices);
	sceGxmDraw(gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_FAN, SCE_GXM_INDEX_FORMAT_U16, depth_clear_indices, 4);

	if (scissor_test_state)
		sceGxmSetRegionClip(gxm_context, SCE_GXM_REGION_CLIP_OUTSIDE, region.x, region.y, region.x + region.w, region.y + region.h);
	else
		sceGxmSetRegionClip(gxm_context, SCE_GXM_REGION_CLIP_OUTSIDE, gl_viewport.x, DISPLAY_HEIGHT - gl_viewport.y - gl_viewport.h, gl_viewport.x + gl_viewport.w, gl_viewport.y + gl_viewport.h);
}

void resetScissorTestRegion(void) {
	// Setting scissor test region to default values
	region.x = region.y = 0;
	region.w = DISPLAY_WIDTH;
	region.h = DISPLAY_HEIGHT;
}

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if ((width < 0) || (height < 0)) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif

	// Converting openGL scissor test region to sceGxm one
	region.x = x;
	region.y = DISPLAY_HEIGHT - y - height;
	region.w = width;
	region.h = height;

	// Updating in use scissor test parameters if GL_SCISSOR_TEST is enabled
	if (scissor_test_state)
		update_scissor_test();
}

void glDepthFunc(GLenum func) {
	// Properly translating openGL function to sceGxm one
	switch (func) {
	case GL_NEVER:
		gxm_depth = SCE_GXM_DEPTH_FUNC_NEVER;
		break;
	case GL_LESS:
		gxm_depth = SCE_GXM_DEPTH_FUNC_LESS;
		break;
	case GL_EQUAL:
		gxm_depth = SCE_GXM_DEPTH_FUNC_EQUAL;
		break;
	case GL_LEQUAL:
		gxm_depth = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;
		break;
	case GL_GREATER:
		gxm_depth = SCE_GXM_DEPTH_FUNC_GREATER;
		break;
	case GL_NOTEQUAL:
		gxm_depth = SCE_GXM_DEPTH_FUNC_NOT_EQUAL;
		break;
	case GL_GEQUAL:
		gxm_depth = SCE_GXM_DEPTH_FUNC_GREATER_EQUAL;
		break;
	case GL_ALWAYS:
		gxm_depth = SCE_GXM_DEPTH_FUNC_ALWAYS;
		break;
	}

	// Updating in use depth function
	change_depth_func();
}

void glClearDepth(GLdouble depth) {
	// Set current in use depth test depth value
	depth_value = depth;
}

void glDepthMask(GLboolean flag) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase == MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Set current in use depth mask and invoking a depth write mode update
	depth_mask_state = flag;
	change_depth_write((depth_mask_state && depth_test_state) ? SCE_GXM_DEPTH_WRITE_ENABLED : SCE_GXM_DEPTH_WRITE_DISABLED);
}

void glAlphaFunc(GLenum func, GLfloat ref) {
	// Updating in use alpha test parameters
	alpha_func = func;
	alpha_ref = ref;
	update_alpha_test_settings();
}

void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
	// Properly updating stencil operation settings
	switch (face) {
	case GL_FRONT:
		if (!change_stencil_config(&stencil_fail_front, sfail))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&depth_fail_front, dpfail))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&depth_pass_front, dppass))
			_vitagl_error = GL_INVALID_ENUM;
		break;
	case GL_BACK:
		if (!change_stencil_config(&stencil_fail_back, sfail))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&depth_fail_back, dpfail))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&depth_pass_front, dppass))
			_vitagl_error = GL_INVALID_ENUM;
		break;
	case GL_FRONT_AND_BACK:
		if (!change_stencil_config(&stencil_fail_front, sfail))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&stencil_fail_back, sfail))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&depth_fail_front, dpfail))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&depth_fail_back, dpfail))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&depth_pass_front, dppass))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_config(&depth_pass_back, dppass))
			_vitagl_error = GL_INVALID_ENUM;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	change_stencil_settings();
}

void glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
	glStencilOpSeparate(GL_FRONT_AND_BACK, sfail, dpfail, dppass);
}

void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
	// Properly updating stencil test function settings
	switch (face) {
	case GL_FRONT:
		if (!change_stencil_func_config(&stencil_func_front, func))
			_vitagl_error = GL_INVALID_ENUM;
		stencil_mask_front = mask;
		stencil_ref_front = ref;
		break;
	case GL_BACK:
		if (!change_stencil_func_config(&stencil_func_back, func))
			_vitagl_error = GL_INVALID_ENUM;
		stencil_mask_back = mask;
		stencil_ref_back = ref;
		break;
	case GL_FRONT_AND_BACK:
		if (!change_stencil_func_config(&stencil_func_front, func))
			_vitagl_error = GL_INVALID_ENUM;
		if (!change_stencil_func_config(&stencil_func_back, func))
			_vitagl_error = GL_INVALID_ENUM;
		stencil_mask_front = stencil_mask_back = mask;
		stencil_ref_front = stencil_ref_back = ref;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	change_stencil_settings();
}

void glStencilFunc(GLenum func, GLint ref, GLuint mask) {
	glStencilFuncSeparate(GL_FRONT_AND_BACK, func, ref, mask);
}

void glStencilMaskSeparate(GLenum face, GLuint mask) {
	// Properly updating stencil test mask settings
	switch (face) {
	case GL_FRONT:
		stencil_mask_front_write = mask;
		break;
	case GL_BACK:
		stencil_mask_back_write = mask;
		break;
	case GL_FRONT_AND_BACK:
		stencil_mask_front_write = stencil_mask_back_write = mask;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		return;
	}
	change_stencil_settings();
}

void glStencilMask(GLuint mask) {
	glStencilMaskSeparate(GL_FRONT_AND_BACK, mask);
}

void glClearStencil(GLint s) {
	stencil_value = s;
}
