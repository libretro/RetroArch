/* 
 * math_utils.h:
 * Header file for the math utilities exposed by math_utils.c
 */

#ifndef _MATH_UTILS_H_
#define _MATH_UTILS_H_

#include <math.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD(x) ((x)*M_PI / 180.0)
#endif

// clang-format off
// vector of 2 floats struct
typedef struct {
	float x, y;
} vector2f;

// vector of 3 floats struct
typedef struct {
	union { float x; float r; };
	union { float y; float g; };
	union { float z; float b; };
} vector3f;

// vector of 4 floats struct
typedef struct {
	union { float x; float r; };
	union { float y; float g; };
	union { float z; float b; };
	union { float w; float a; };
} vector4f;
// clang-format on

// 4x4 matrix
typedef float matrix4x4[4][4];

// Creates an identity matrix
void matrix4x4_identity(matrix4x4 m);

// Copy a matrix to another one
void matrix4x4_copy(matrix4x4 dst, const matrix4x4 src);

// Perform a matrix per matrix moltiplication
void matrix4x4_multiply(matrix4x4 dst, const matrix4x4 src1, const matrix4x4 src2);

// Rotate a matrix on x,y,z axis
void matrix4x4_rotate_x(matrix4x4 m, float rad);
void matrix4x4_rotate_y(matrix4x4 m, float rad);
void matrix4x4_rotate_z(matrix4x4 m, float rad);

// Translate a matrix
void matrix4x4_translate(matrix4x4 m, float x, float y, float z);

// Scale a matrix
void matrix4x4_scale(matrix4x4 m, float scale_x, float scale_y, float scale_z);

// Transpose a matrix
void matrix4x4_transpose(matrix4x4 out, const matrix4x4 m);

// Init a matrix with different settings (ortho, frustum, perspective)
void matrix4x4_init_orthographic(matrix4x4 m, float left, float right, float bottom, float top, float near, float far);
void matrix4x4_init_frustum(matrix4x4 m, float left, float right, float bottom, float top, float near, float far);
void matrix4x4_init_perspective(matrix4x4 m, float fov, float aspect, float near, float far);

// Invert a matrix
int matrix4x4_invert(matrix4x4 out, const matrix4x4 m);

// Perform a matrix per vector moltiplication
void vector4f_matrix4x4_mult(vector4f *u, const matrix4x4 m, const vector4f *v);

#endif
