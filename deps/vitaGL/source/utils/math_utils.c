/* 
 * math_utils.c:
 * Utilities for math operations
 */

#include "../shared.h"
#include <math.h>
#include <math_neon.h>

// NOTE: matrices are row-major.

void matrix4x4_identity(matrix4x4 m) {
	m[0][1] = m[0][2] = m[0][3] = 0.0f;
	m[1][0] = m[1][2] = m[1][3] = 0.0f;
	m[2][0] = m[2][1] = m[2][3] = 0.0f;
	m[3][0] = m[3][1] = m[3][2] = 0.0f;
	m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}

void matrix4x4_copy(matrix4x4 dst, const matrix4x4 src) {
	memcpy(dst, src, sizeof(matrix4x4));
}

void matrix4x4_multiply(matrix4x4 dst, const matrix4x4 src1, const matrix4x4 src2) {
	matmul4_neon((float*)src2, (float*)src1, (float*)dst);
}

void matrix4x4_init_rotation_x(matrix4x4 m, float rad) {
	float cs[2];
	sincosf_c(rad, cs);
	
	matrix4x4_identity(m);

	m[1][1] = cs[1];
	m[1][2] = -cs[0];
	m[2][1] = cs[0];
	m[2][2] = cs[1];
}

void matrix4x4_init_rotation_y(matrix4x4 m, float rad) {
	float cs[2];
	sincosf_c(rad, cs);

	matrix4x4_identity(m);

	m[0][0] = cs[1];
	m[0][2] = cs[0];
	m[2][0] = -cs[0];
	m[2][2] = cs[1];
}

void matrix4x4_init_rotation_z(matrix4x4 m, float rad) {
	float cs[2];
	sincosf_c(rad, cs);

	matrix4x4_identity(m);

	m[0][0] = cs[1];
	m[0][1] = -cs[0];
	m[1][0] = cs[0];
	m[1][1] = cs[1];
}

void matrix4x4_rotate_x(matrix4x4 m, float rad) {
	matrix4x4 m1, m2;

	matrix4x4_init_rotation_x(m1, rad);
	matrix4x4_multiply(m2, m, m1);
	matrix4x4_copy(m, m2);
}

void matrix4x4_rotate_y(matrix4x4 m, float rad) {
	matrix4x4 m1, m2;

	matrix4x4_init_rotation_y(m1, rad);
	matrix4x4_multiply(m2, m, m1);
	matrix4x4_copy(m, m2);
}

void matrix4x4_rotate_z(matrix4x4 m, float rad) {
	matrix4x4 m1, m2;

	matrix4x4_init_rotation_z(m1, rad);
	matrix4x4_multiply(m2, m, m1);
	matrix4x4_copy(m, m2);
}

void matrix4x4_init_translation(matrix4x4 m, float x, float y, float z) {
	matrix4x4_identity(m);

	m[0][3] = x;
	m[1][3] = y;
	m[2][3] = z;
}

void matrix4x4_translate(matrix4x4 m, float x, float y, float z) {
	matrix4x4 m1, m2;

	matrix4x4_init_translation(m1, x, y, z);
	matrix4x4_multiply(m2, m, m1);
	matrix4x4_copy(m, m2);
}

void matrix4x4_init_scaling(matrix4x4 m, float scale_x, float scale_y, float scale_z) {
	matrix4x4_identity(m);

	m[0][0] = scale_x;
	m[1][1] = scale_y;
	m[2][2] = scale_z;
}

void matrix4x4_scale(matrix4x4 m, float scale_x, float scale_y, float scale_z) {
	matrix4x4 m1, m2;

	matrix4x4_init_scaling(m1, scale_x, scale_y, scale_z);
	matrix4x4_multiply(m2, m, m1);
	matrix4x4_copy(m, m2);
}

void matrix4x4_transpose(matrix4x4 out, const matrix4x4 m) {
	int i, j;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			out[i][j] = m[j][i];
	}
}

void matrix4x4_init_orthographic(matrix4x4 m, float left, float right, float bottom, float top, float near, float far) {
	m[0][0] = 2.0f / (right - left);
	m[0][1] = 0.0f;
	m[0][2] = 0.0f;
	m[0][3] = -(right + left) / (right - left);

	m[1][0] = 0.0f;
	m[1][1] = 2.0f / (top - bottom);
	m[1][2] = 0.0f;
	m[1][3] = -(top + bottom) / (top - bottom);

	m[2][0] = 0.0f;
	m[2][1] = 0.0f;
	m[2][2] = -2.0f / (far - near);
	m[2][3] = -(far + near) / (far - near);

	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;
}

void matrix4x4_init_frustum(matrix4x4 m, float left, float right, float bottom, float top, float near, float far) {
	m[0][0] = (2.0f * near) / (right - left);
	m[0][1] = 0.0f;
	m[0][2] = (right + left) / (right - left);
	m[0][3] = 0.0f;

	m[1][0] = 0.0f;
	m[1][1] = (2.0f * near) / (top - bottom);
	m[1][2] = (top + bottom) / (top - bottom);
	m[1][3] = 0.0f;

	m[2][0] = 0.0f;
	m[2][1] = 0.0f;
	m[2][2] = -(far + near) / (far - near);
	m[2][3] = (-2.0f * far * near) / (far - near);

	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = -1.0f;
	m[3][3] = 0.0f;
}

void matrix4x4_init_perspective(matrix4x4 m, float fov, float aspect, float near, float far) {
	float half_height = near * tanf_neon(DEG_TO_RAD(fov) * 0.5f);
	float half_width = half_height * aspect;

	matrix4x4_init_frustum(m, -half_width, half_width, -half_height, half_height, near, far);
}

int matrix4x4_invert(matrix4x4 out, const matrix4x4 m) {
	int i, j;
	float det;
	matrix4x4 inv;

	inv[0][0] = m[1][1] * m[2][2] * m[3][3] - m[1][1] * m[3][2] * m[2][3] - m[1][2] * m[2][1] * m[3][3] + m[1][2] * m[3][1] * m[2][3] + m[1][3] * m[2][1] * m[3][2] - m[1][3] * m[3][1] * m[2][2];
	inv[0][1] = -m[0][1] * m[2][2] * m[3][3] + m[0][1] * m[3][2] * m[2][3] + m[0][2] * m[2][1] * m[3][3] - m[0][2] * m[3][1] * m[2][3] - m[0][3] * m[2][1] * m[3][2] + m[0][3] * m[3][1] * m[2][2];
	inv[0][2] = m[0][1] * m[1][2] * m[3][3] - m[0][1] * m[3][2] * m[1][3] - m[0][2] * m[1][1] * m[3][3] + m[0][2] * m[3][1] * m[1][3] + m[0][3] * m[1][1] * m[3][2] - m[0][3] * m[3][1] * m[1][2];
	inv[0][3] = -m[0][1] * m[1][2] * m[2][3] + m[0][1] * m[2][2] * m[1][3] + m[0][2] * m[1][1] * m[2][3] - m[0][2] * m[2][1] * m[1][3] - m[0][3] * m[1][1] * m[2][2] + m[0][3] * m[2][1] * m[1][2];
	inv[1][0] = -m[1][0] * m[2][2] * m[3][3] + m[1][0] * m[3][2] * m[2][3] + m[1][2] * m[2][0] * m[3][3] - m[1][2] * m[3][0] * m[2][3] - m[1][3] * m[2][0] * m[3][2] + m[1][3] * m[3][0] * m[2][2];
	inv[1][1] = m[0][0] * m[2][2] * m[3][3] - m[0][0] * m[3][2] * m[2][3] - m[0][2] * m[2][0] * m[3][3] + m[0][2] * m[3][0] * m[2][3] + m[0][3] * m[2][0] * m[3][2] - m[0][3] * m[3][0] * m[2][2];
	inv[1][2] = -m[0][0] * m[1][2] * m[3][3] + m[0][0] * m[3][2] * m[1][3] + m[0][2] * m[1][0] * m[3][3] - m[0][2] * m[3][0] * m[1][3] - m[0][3] * m[1][0] * m[3][2] + m[0][3] * m[3][0] * m[1][2];
	inv[1][3] = m[0][0] * m[1][2] * m[2][3] - m[0][0] * m[2][2] * m[1][3] - m[0][2] * m[1][0] * m[2][3] + m[0][2] * m[2][0] * m[1][3] + m[0][3] * m[1][0] * m[2][2] - m[0][3] * m[2][0] * m[1][2];
	inv[2][0] = m[1][0] * m[2][1] * m[3][3] - m[1][0] * m[3][1] * m[2][3] - m[1][1] * m[2][0] * m[3][3] + m[1][1] * m[3][0] * m[2][3] + m[1][3] * m[2][0] * m[3][1] - m[1][3] * m[3][0] * m[2][1];
	inv[2][1] = -m[0][0] * m[2][1] * m[3][3] + m[0][0] * m[3][1] * m[2][3] + m[0][1] * m[2][0] * m[3][3] - m[0][1] * m[3][0] * m[2][3] - m[0][3] * m[2][0] * m[3][1] + m[0][3] * m[3][0] * m[2][1];
	inv[2][2] = m[0][0] * m[1][1] * m[3][3] - m[0][0] * m[3][1] * m[1][3] - m[0][1] * m[1][0] * m[3][3] + m[0][1] * m[3][0] * m[1][3] + m[0][3] * m[1][0] * m[3][1] - m[0][3] * m[3][0] * m[1][1];
	inv[2][3] = -m[0][0] * m[1][1] * m[2][3] + m[0][0] * m[2][1] * m[1][3] + m[0][1] * m[1][0] * m[2][3] - m[0][1] * m[2][0] * m[1][3] - m[0][3] * m[1][0] * m[2][1] + m[0][3] * m[2][0] * m[1][1];
	inv[3][0] = -m[1][0] * m[2][1] * m[3][2] + m[1][0] * m[3][1] * m[2][2] + m[1][1] * m[2][0] * m[3][2] - m[1][1] * m[3][0] * m[2][2] - m[1][2] * m[2][0] * m[3][1] + m[1][2] * m[3][0] * m[2][1];
	inv[3][1] = m[0][0] * m[2][1] * m[3][2] - m[0][0] * m[3][1] * m[2][2] - m[0][1] * m[2][0] * m[3][2] + m[0][1] * m[3][0] * m[2][2] + m[0][2] * m[2][0] * m[3][1] - m[0][2] * m[3][0] * m[2][1];
	inv[3][2] = -m[0][0] * m[1][1] * m[3][2] + m[0][0] * m[3][1] * m[1][2] + m[0][1] * m[1][0] * m[3][2] - m[0][1] * m[3][0] * m[1][2] - m[0][2] * m[1][0] * m[3][1] + m[0][2] * m[3][0] * m[1][1];
	inv[3][3] = m[0][0] * m[1][1] * m[2][2] - m[0][0] * m[2][1] * m[1][2] - m[0][1] * m[1][0] * m[2][2] + m[0][1] * m[2][0] * m[1][2] + m[0][2] * m[1][0] * m[2][1] - m[0][2] * m[2][0] * m[1][1];

	det = m[0][0] * inv[0][0] + m[1][0] * inv[0][1] + m[2][0] * inv[0][2] + m[3][0] * inv[0][3];

	if (det == 0)
		return 0;

	det = 1.0 / det;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			out[i][j] = inv[i][j] * det;
	}

	return 1;
}

void vector4f_matrix4x4_mult(vector4f *u, const matrix4x4 m, const vector4f *v) {
	u->x = m[0][0] * v->x + m[0][1] * v->y + m[0][2] * v->z + m[0][3] * v->w;
	u->y = m[1][0] * v->x + m[1][1] * v->y + m[1][2] * v->z + m[1][3] * v->w;
	u->z = m[2][0] * v->x + m[2][1] * v->y + m[2][2] * v->z + m[2][3] * v->w;
	u->w = m[3][0] * v->x + m[3][1] * v->y + m[3][2] * v->z + m[3][3] * v->w;
}
