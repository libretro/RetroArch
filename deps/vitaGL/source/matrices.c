/* 
 * matrices.c:
 * Implementation for matrices related functions
 */

#include "shared.h"

matrix4x4 *matrix = NULL; // Current in-use matrix mode
static matrix4x4 modelview_matrix_stack[MODELVIEW_STACK_DEPTH]; // Modelview matrices stack
static uint8_t modelview_stack_counter = 0; // Modelview matrices stack counter
static matrix4x4 projection_matrix_stack[GENERIC_STACK_DEPTH]; // Projection matrices stack
static uint8_t projection_stack_counter = 0; // Projection matrices stack counter
GLboolean mvp_modified = GL_TRUE; // Check if ModelViewProjection matrix needs to be recreated

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */

void glMatrixMode(GLenum mode) {
	// Changing current in use matrix
	switch (mode) {
	case GL_MODELVIEW: // Modelview matrix
		matrix = &modelview_matrix;
		break;
	case GL_PROJECTION: // Projection matrix
		matrix = &projection_matrix;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
}

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearVal, GLdouble farVal) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase == MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	} else if ((left == right) || (bottom == top) || (nearVal == farVal)) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif

	// Initializing ortho matrix with requested parameters
	matrix4x4_init_orthographic(*matrix, left, right, bottom, top, nearVal, farVal);
	mvp_modified = GL_TRUE;
}

void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearVal, GLdouble farVal) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase == MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	} else if ((left == right) || (bottom == top) || (nearVal < 0) || (farVal < 0)) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif

	// Initializing frustum matrix with requested parameters
	matrix4x4_init_frustum(*matrix, left, right, bottom, top, nearVal, farVal);
	mvp_modified = GL_TRUE;
}

void glLoadIdentity(void) {
	// Set current in use matrix to identity one
	matrix4x4_identity(*matrix);
	mvp_modified = GL_TRUE;
}

void glMultMatrixf(const GLfloat *m) {
	matrix4x4 res;
		
#ifdef TRANSPOSE_MATRICES
	// Properly ordering matrix to perform multiplication
	matrix4x4 tmp;
	int i, j;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			tmp[i][j] = m[j * 4 + i];

		}
	}

	// Multiplicating passed matrix with in use one
	matrix4x4_multiply(res, *matrix, tmp);
#else
	// Multiplicating passed matrix with in use one
	matrix4x4_multiply(res, *matrix, m);
#endif
	// Copying result to in use matrix
	matrix4x4_copy(*matrix, res);
	mvp_modified = GL_TRUE;
}

void glLoadMatrixf(const GLfloat *m) {
#ifdef TRANSPOSE_MATRICES
	// Properly ordering matrix
	matrix4x4 tmp;
	int i, j;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			(*matrix)[i][j] = m[j * 4 + i];
		}
	}
#else
	memcpy(*matrix, m, sizeof(matrix4x4));
#endif
	mvp_modified = GL_TRUE;
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
	// Translating in use matrix
	matrix4x4_translate(*matrix, x, y, z);
	mvp_modified = GL_TRUE;
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
	// Scaling in use matrix
	matrix4x4_scale(*matrix, x, y, z);
	mvp_modified = GL_TRUE;
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase == MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Performing rotation on in use matrix depending on user call
	float rad = DEG_TO_RAD(angle);
	if (x == 1.0f) {
		matrix4x4_rotate_x(*matrix, rad);
	}
	if (y == 1.0f) {
		matrix4x4_rotate_y(*matrix, rad);
	}
	if (z == 1.0f) {
		matrix4x4_rotate_z(*matrix, rad);
	}
	mvp_modified = GL_TRUE;
}

void glPushMatrix(void) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase == MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	if (matrix == &modelview_matrix) {
#ifndef SKIP_ERROR_HANDLING
		// Error handling
		if (modelview_stack_counter >= MODELVIEW_STACK_DEPTH) {
			_vitagl_error = GL_STACK_OVERFLOW;
		} else
#endif
			// Copying current matrix into the matrix stack and increasing stack counter
			matrix4x4_copy(modelview_matrix_stack[modelview_stack_counter++], *matrix);

	} else if (matrix == &projection_matrix) {
#ifndef SKIP_ERROR_HANDLING
		// Error handling
		if (projection_stack_counter >= GENERIC_STACK_DEPTH) {
			_vitagl_error = GL_STACK_OVERFLOW;
		} else
#endif
			// Copying current matrix into the matrix stack and increasing stack counter
			matrix4x4_copy(projection_matrix_stack[projection_stack_counter++], *matrix);
	}
}

void glPopMatrix(void) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase == MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	if (matrix == &modelview_matrix) {
#ifndef SKIP_ERROR_HANDLING
		// Error handling
		if (modelview_stack_counter == 0)
			_vitagl_error = GL_STACK_UNDERFLOW;
		else
#endif
			// Copying last matrix on stack into current matrix and decreasing stack counter
			matrix4x4_copy(*matrix, modelview_matrix_stack[--modelview_stack_counter]);

	} else if (matrix == &projection_matrix) {
#ifndef SKIP_ERROR_HANDLING
		// Error handling
		if (projection_stack_counter == 0)
			_vitagl_error = GL_STACK_UNDERFLOW;
		else
#endif
			// Copying last matrix on stack into current matrix and decreasing stack counter
			matrix4x4_copy(*matrix, projection_matrix_stack[--projection_stack_counter]);
	}
	mvp_modified = GL_TRUE;
}
