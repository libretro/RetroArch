/* 
 * legacy.c:
 * Implementation for legacy openGL 1.0 rendering method
 */

#include "shared.h"

// Vertex list struct
typedef struct vertexList {
	vector3f v;
	void *next;
} vertexList;

// Color vertex list struct
typedef struct rgbaList {
	vector4f v;
	void *next;
} rgbaList;

// Texture coord list struct
typedef struct uvList {
	vector2f v;
	void *next;
} uvList;

static vertexList *model_vertices = NULL; // Pointer to vertex list
static vertexList *last_vert = NULL; // Pointer to last element in vertex list
static rgbaList *model_color = NULL; // Pointer to color vertex list
static rgbaList *last_clr = NULL; // Pointer to last element in color vertex list
static uvList *model_uv = NULL; // Pointer to texcoord list
static uvList *last_uv = NULL; // Pointer to last element in texcoord list
static uint64_t vertex_count = 0; // Vertex counter for vertex list
static SceGxmPrimitiveType prim; // Current in use primitive for rendering
static SceGxmPrimitiveTypeExtra prim_extra = SCE_GXM_PRIMITIVE_NONE; // Current in use non native primitive for rendering
static uint8_t np = 0xFF; // Number of expected vertices per element for current in use primitive

vector4f current_color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Current in use color

static void purge_vertex_list() {
	vertexList *old;
	rgbaList *old2;
	uvList *old3;

	// Purging color and vertex lists
	while (model_vertices != NULL) {
		old = model_vertices;
		old2 = model_color;
		model_vertices = model_vertices->next;
		model_color = model_color->next;
		free(old);
		free(old2);
	}

	// Purging texcoord list
	while (model_uv != NULL) {
		old3 = model_uv;
		model_uv = model_uv->next;
		free(old3);
	}
}

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */

void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase != MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Adding a new element to color and vertex lists
	if (model_vertices == NULL) {
		model_vertices = last_vert = (vertexList *)malloc(sizeof(vertexList));
		model_color = last_clr = (rgbaList *)malloc(sizeof(rgbaList));
	} else {
		last_vert->next = (vertexList *)malloc(sizeof(vertexList));
		last_clr->next = (rgbaList *)malloc(sizeof(rgbaList));
		last_vert = last_vert->next;
		last_clr = last_clr->next;
	}

	// Properly populating the new element
	last_vert->v.x = x;
	last_vert->v.y = y;
	last_vert->v.z = z;
	memcpy(&last_clr->v, &current_color.r, sizeof(vector4f));
	last_clr->next = last_vert->next = NULL;

	// Increasing vertex counter
	vertex_count++;
}

void glVertex3fv(const GLfloat *v) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase != MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Adding a new element to color and vertex lists
	if (model_vertices == NULL) {
		model_vertices = last_vert = (vertexList *)malloc(sizeof(vertexList));
		model_color = last_clr = (rgbaList *)malloc(sizeof(rgbaList));
	} else {
		last_vert->next = (vertexList *)malloc(sizeof(vertexList));
		last_clr->next = (rgbaList *)malloc(sizeof(rgbaList));
		last_vert = last_vert->next;
		last_clr = last_clr->next;
	}

	// Properly populating the new element
	memcpy(&last_vert->v, v, sizeof(vector3f));
	memcpy(&last_clr->v, &current_color.r, sizeof(vector4f));
	last_clr->next = last_vert->next = NULL;

	// Increasing vertex counter
	vertex_count++;
}

void glVertex2f(GLfloat x, GLfloat y) {
	glVertex3f(x, y, 0.0f);
}

void glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
	// Setting current color value
	current_color.r = red;
	current_color.g = green;
	current_color.b = blue;
	current_color.a = 1.0f;
}

void glColor3fv(const GLfloat *v) {
	// Setting current color value
	memcpy(&current_color.r, v, sizeof(vector3f));
	current_color.a = 1.0f;
}

void glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
	// Setting current color value
	current_color.r = (1.0f * red) / 255.0f;
	current_color.g = (1.0f * green) / 255.0f;
	current_color.b = (1.0f * blue) / 255.0f;
	current_color.a = 1.0f;
}

void glColor3ubv(const GLubyte *c) {
	// Setting current color value
	current_color.r = (1.0f * c[0]) / 255.0f;
	current_color.g = (1.0f * c[1]) / 255.0f;
	current_color.b = (1.0f * c[2]) / 255.0f;
	current_color.a = 1.0f;
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
	// Setting current color value
	current_color.r = red;
	current_color.g = green;
	current_color.b = blue;
	current_color.a = alpha;
}

void glColor4fv(const GLfloat *v) {
	// Setting current color value
	memcpy(&current_color.r, v, sizeof(vector4f));
}

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
	current_color.r = (1.0f * red) / 255.0f;
	current_color.g = (1.0f * green) / 255.0f;
	current_color.b = (1.0f * blue) / 255.0f;
	current_color.a = (1.0f * alpha) / 255.0f;
}

void glColor4ubv(const GLubyte *c) {
	// Setting current color value
	current_color.r = (1.0f * c[0]) / 255.0f;
	current_color.g = (1.0f * c[1]) / 255.0f;
	current_color.b = (1.0f * c[2]) / 255.0f;
	current_color.a = (1.0f * c[3]) / 255.0f;
}

void glTexCoord2fv(GLfloat *f) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase != MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Adding a new element to texcoord list
	if (model_uv == NULL) {
		model_uv = last_uv = (uvList *)malloc(sizeof(uvList));
	} else {
		last_uv->next = (uvList *)malloc(sizeof(uvList));
		last_uv = last_uv->next;
	}

	// Properly populating the new element
	last_uv->v.x = f[0];
	last_uv->v.y = f[1];
	last_uv->next = NULL;
}

void glTexCoord2f(GLfloat s, GLfloat t) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase != MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Adding a new element to texcoord list
	if (model_uv == NULL) {
		model_uv = last_uv = (uvList *)malloc(sizeof(uvList));
	} else {
		last_uv->next = (uvList *)malloc(sizeof(uvList));
		last_uv = last_uv->next;
	}

	// Properly populating the new element
	last_uv->v.x = s;
	last_uv->v.y = t;
	last_uv->next = NULL;
}

void glTexCoord2i(GLint s, GLint t) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase != MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Adding a new element to texcoord list
	if (model_uv == NULL) {
		model_uv = last_uv = (uvList *)malloc(sizeof(uvList));
	} else {
		last_uv->next = (uvList *)malloc(sizeof(uvList));
		last_uv = last_uv->next;
	}

	// Properly populating the new element
	last_uv->v.x = s;
	last_uv->v.y = t;
	last_uv->next = NULL;
}

void glArrayElement(GLint i) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (i < 0) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif

	// Aliasing client texture unit and client texture id for better code readability
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	int texture2d_idx = tex_unit->tex_id;

	// Checking if current texture unit has GL_VERTEX_ARRAY enabled
	if (tex_unit->vertex_array_state) {
		// Calculating offset of requested element
		uint8_t *ptr;
		if (tex_unit->vertex_array.stride == 0)
			ptr = ((uint8_t *)tex_unit->vertex_array.pointer) + (i * (tex_unit->vertex_array.num * tex_unit->vertex_array.size));
		else
			ptr = ((uint8_t *)tex_unit->vertex_array.pointer) + (i * tex_unit->vertex_array.stride);

		// Adding a new element to vertex and color lists
		if (model_vertices == NULL) {
			model_vertices = last_vert = (vertexList *)malloc(sizeof(vertexList));
			model_color = last_clr = (rgbaList *)malloc(sizeof(rgbaList));
		} else {
			last_vert->next = (vertexList *)malloc(sizeof(vertexList));
			last_clr->next = (rgbaList *)malloc(sizeof(rgbaList));
			last_vert = last_vert->next;
			last_clr = last_clr->next;
		}
		last_vert->next = NULL;
		last_clr->next = NULL;

		// Populating new vertex element
		memcpy(&last_vert->v, ptr, tex_unit->vertex_array.size * tex_unit->vertex_array.num);

		// Checking if current texture unit has GL_COLOR_ARRAY enabled
		if (tex_unit->color_array_state) {
			// Calculating offset of requested element
			uint8_t *ptr_clr;
			if (tex_unit->color_array.stride == 0)
				ptr_clr = ((uint8_t *)tex_unit->color_array.pointer) + (i * (tex_unit->color_array.num * tex_unit->color_array.size));
			else
				ptr_clr = ((uint8_t *)tex_unit->color_array.pointer) + (i * tex_unit->color_array.stride);

			// Populating new color element
			last_clr->v.a = 1.0f;
			memcpy(&last_clr->v, ptr_clr, tex_unit->color_array.size * tex_unit->color_array.num);

		} else {
			// Populating new color element with current color
			memcpy(&last_clr->v, &current_color.r, sizeof(vector4f));
		}

		// Checking if current texture unit has GL_TEXTURE_COORD_ARRAY enabled
		if (tex_unit->texture_array_state) {
			// Calculating offset of requested element
			uint8_t *ptr_tex;
			if (tex_unit->texture_array.stride == 0)
				ptr_tex = ((uint8_t *)tex_unit->texture_array.pointer) + (i * (tex_unit->texture_array.num * tex_unit->texture_array.size));
			else
				ptr_tex = ((uint8_t *)tex_unit->texture_array.pointer) + (i * tex_unit->texture_array.stride);

			// Adding a new element to texcoord list
			if (model_uv == NULL) {
				model_uv = last_uv = (uvList *)malloc(sizeof(uvList));
			} else {
				last_uv->next = (uvList *)malloc(sizeof(uvList));
				last_uv = last_uv->next;
			}

			// Populating new texcoord element
			memcpy(&last_uv->v, ptr_tex, tex_unit->vertex_array.size * 2);
			last_uv->next = NULL;
		}
	}
}

void glBegin(GLenum mode) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (phase == MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Changing current openGL machine state
	phase = MODEL_CREATION;

	// Translating primitive to sceGxm one
	prim_extra = SCE_GXM_PRIMITIVE_NONE;
	switch (mode) {
	case GL_POINTS:
		prim = SCE_GXM_PRIMITIVE_POINTS;
		np = 1;
		break;
	case GL_LINES:
		prim = SCE_GXM_PRIMITIVE_LINES;
		np = 2;
		break;
	case GL_TRIANGLES:
		prim = SCE_GXM_PRIMITIVE_TRIANGLES;
		np = 3;
		break;
	case GL_TRIANGLE_STRIP:
		prim = SCE_GXM_PRIMITIVE_TRIANGLE_STRIP;
		np = 1;
		break;
	case GL_TRIANGLE_FAN:
		prim = SCE_GXM_PRIMITIVE_TRIANGLE_FAN;
		np = 1;
		break;
	case GL_QUADS:
		prim = SCE_GXM_PRIMITIVE_TRIANGLES;
		prim_extra = SCE_GXM_PRIMITIVE_QUADS;
		np = 4;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}

	// Resetting vertex count
	vertex_count = 0;
}

void glEnd(void) {
#ifndef SKIP_ERROR_HANDLING
	// Integrity checks
	if (vertex_count == 0 || ((vertex_count % np) != 0))
		return;

	// Error handling
	if (phase != MODEL_CREATION) {
		_vitagl_error = GL_INVALID_OPERATION;
		return;
	}
#endif

	// Changing current openGL machine state
	phase = NONE;

	// Checking if we can totally skip drawing cause of culling mode
	if (no_polygons_mode && ((prim == SCE_GXM_PRIMITIVE_TRIANGLES) || (prim >= SCE_GXM_PRIMITIVE_TRIANGLE_STRIP))) {
		purge_vertex_list();
		vertex_count = 0;
		return;
	}

	// Aliasing server texture unit and texture id for better code readability
	texture_unit *tex_unit = &texture_units[server_texture_unit];
	int texture2d_idx = tex_unit->tex_id;

	// Calculating mvp matrix
	if (mvp_modified) {
		matrix4x4_multiply(mvp_matrix, projection_matrix, modelview_matrix);
		mvp_modified = GL_FALSE;
	}

	// Checking if we have to write a texture
	if ((server_texture_unit >= 0) && (tex_unit->enabled) && (model_uv != NULL) && (tex_unit->textures[texture2d_idx].valid)) {
		// Setting proper vertex and fragment programs
		sceGxmSetVertexProgram(gxm_context, texture2d_vertex_program_patched);
		sceGxmSetFragmentProgram(gxm_context, texture2d_fragment_program_patched);

		// Setting fragment uniforms for alpha test and texture environment
		void *alpha_buffer;
		sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &alpha_buffer);
		sceGxmSetUniformDataF(alpha_buffer, texture2d_alpha_cut, 0, 1, &alpha_ref);
		float alpha_operation = (float)alpha_op;
		sceGxmSetUniformDataF(alpha_buffer, texture2d_alpha_op, 0, 1, &alpha_operation);
		sceGxmSetUniformDataF(alpha_buffer, texture2d_tint_color, 0, 4, &current_color.r);
		float tex_env = (float)tex_unit->env_mode;
		sceGxmSetUniformDataF(alpha_buffer, texture2d_tex_env, 0, 1, &tex_env);
		float fogmode = (float)internal_fog_mode;
		sceGxmSetUniformDataF(alpha_buffer, texture2d_fog_mode, 0, 1, &fogmode);
		sceGxmSetUniformDataF(alpha_buffer, texture2d_fog_color, 0, 4, &fog_color.r);
		sceGxmSetUniformDataF(alpha_buffer, texture2d_tex_env_color, 0, 4, &texenv_color.r);

	} else {
		// Setting proper vertex and fragment programs
		sceGxmSetVertexProgram(gxm_context, rgba_vertex_program_patched);
		sceGxmSetFragmentProgram(gxm_context, rgba_fragment_program_patched);
	}

	// Reserving default uniform buffer for wvp
	int i, j;
	void *vertex_wvp_buffer;
	sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vertex_wvp_buffer);

	// Checking  if we have to write a texture
	if (model_uv != NULL) {
		// Setting wvp matrix
		sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_wvp, 0, 16, (const float *)mvp_matrix);

		// Setting fogging uniforms
		float fogmode = (float)internal_fog_mode;
		sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_mode2, 0, 1, (const float *)&fogmode);
		float clipplane0 = (float)clip_plane0;
		sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_clip_plane0, 0, 1, &clipplane0);
		sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_clip_plane0_eq, 0, 4, &clip_plane0_eq.x);
		sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_mv, 0, 16, (const float *)modelview_matrix);
		sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_near, 0, 1, (const float *)&fog_near);
		sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_far, 0, 1, (const float *)&fog_far);
		sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_density, 0, 1, (const float *)&fog_density);

		// Setting in use texture
		sceGxmSetFragmentTexture(gxm_context, 0, &tex_unit->textures[texture2d_idx].gxm_tex);

		// Properly generating vertices, uv map and indices buffers
		vector3f *vertices;
		vector2f *uv_map;
		uint16_t *indices;
		int n = 0, quad_n = 0;
		vertexList *object = model_vertices;
		uvList *object_uv = model_uv;
		uint64_t idx_count = vertex_count;
		switch (prim_extra) {
		case SCE_GXM_PRIMITIVE_NONE:
			vertices = (vector3f *)gpu_pool_memalign(vertex_count * sizeof(vector3f), sizeof(vector3f));
			uv_map = (vector2f *)gpu_pool_memalign(vertex_count * sizeof(vector2f), sizeof(vector2f));
			memset(vertices, 0, (vertex_count * sizeof(vector3f)));
			indices = (uint16_t *)gpu_pool_memalign(idx_count * sizeof(uint16_t), sizeof(uint16_t));
			for (i = 0; i < vertex_count; i++) {
				memcpy(&vertices[n], &object->v, sizeof(vector3f));
				memcpy(&uv_map[n], &object_uv->v, sizeof(vector2f));
				indices[n] = n;
				object = object->next;
				object_uv = object_uv->next;
				n++;
			}
			break;
		case SCE_GXM_PRIMITIVE_QUADS:
			quad_n = vertex_count >> 2;
			idx_count = quad_n * 6;
			vertices = (vector3f *)gpu_pool_memalign(vertex_count * sizeof(vector3f), sizeof(vector3f));
			uv_map = (vector2f *)gpu_pool_memalign(vertex_count * sizeof(vector2f), sizeof(vector2f));
			memset(vertices, 0, (vertex_count * sizeof(vector3f)));
			indices = (uint16_t *)gpu_pool_memalign(idx_count * sizeof(uint16_t), sizeof(uint16_t));
			for (i = 0; i < quad_n; i++) {
				indices[i * 6] = i * 4;
				indices[i * 6 + 1] = i * 4 + 1;
				indices[i * 6 + 2] = i * 4 + 3;
				indices[i * 6 + 3] = i * 4 + 1;
				indices[i * 6 + 4] = i * 4 + 2;
				indices[i * 6 + 5] = i * 4 + 3;
			}
			for (j = 0; j < vertex_count; j++) {
				memcpy(&vertices[j], &object->v, sizeof(vector3f));
				memcpy(&uv_map[j], &object_uv->v, sizeof(vector2f));
				object = object->next;
				object_uv = object_uv->next;
			}
			break;
		}

		// Performing the requested draw call
		sceGxmSetVertexStream(gxm_context, 0, vertices);
		sceGxmSetVertexStream(gxm_context, 1, uv_map);
		sceGxmDraw(gxm_context, prim, SCE_GXM_INDEX_FORMAT_U16, indices, idx_count);

	} else {
		// Setting wvp matrix
		sceGxmSetUniformDataF(vertex_wvp_buffer, rgba_wvp, 0, 16, (const float *)mvp_matrix);

		// Properly generating vertices, colors and indices buffers
		vector3f *vertices;
		vector4f *colors;
		uint16_t *indices;
		int n = 0, quad_n = 0;
		vertexList *object = model_vertices;
		rgbaList *object_clr = model_color;
		uint64_t idx_count = vertex_count;
		switch (prim_extra) {
		case SCE_GXM_PRIMITIVE_NONE:
			vertices = (vector3f *)gpu_pool_memalign(vertex_count * sizeof(vector3f), sizeof(vector3f));
			colors = (vector4f *)gpu_pool_memalign(vertex_count * sizeof(vector4f), sizeof(vector4f));
			memset(vertices, 0, (vertex_count * sizeof(vector3f)));
			indices = (uint16_t *)gpu_pool_memalign(idx_count * sizeof(uint16_t), sizeof(uint16_t));
			for (i = 0; i < vertex_count; i++) {
				memcpy(&vertices[n], &object->v, sizeof(vector3f));
				memcpy(&colors[n], &object_clr->v, sizeof(vector4f));
				indices[n] = n;
				object = object->next;
				object_clr = object_clr->next;
				n++;
			}
			break;
		case SCE_GXM_PRIMITIVE_QUADS:
			quad_n = vertex_count >> 2;
			idx_count = quad_n * 6;
			vertices = (vector3f *)gpu_pool_memalign(vertex_count * sizeof(vector3f), sizeof(vector3f));
			colors = (vector4f *)gpu_pool_memalign(vertex_count * sizeof(vector4f), sizeof(vector4f));
			memset(vertices, 0, (vertex_count * sizeof(vector3f)));
			indices = (uint16_t *)gpu_pool_memalign(idx_count * sizeof(uint16_t), sizeof(uint16_t));
			int i, j;
			for (i = 0; i < quad_n; i++) {
				indices[i * 6] = i * 4;
				indices[i * 6 + 1] = i * 4 + 1;
				indices[i * 6 + 2] = i * 4 + 3;
				indices[i * 6 + 3] = i * 4 + 1;
				indices[i * 6 + 4] = i * 4 + 2;
				indices[i * 6 + 5] = i * 4 + 3;
			}
			for (j = 0; j < vertex_count; j++) {
				memcpy(&vertices[j], &object->v, sizeof(vector3f));
				memcpy(&colors[j], &object_clr->v, sizeof(vector4f));
				object = object->next;
				object_clr = object_clr->next;
			}
			break;
		}

		// Performing the requested draw call
		sceGxmSetVertexStream(gxm_context, 0, vertices);
		sceGxmSetVertexStream(gxm_context, 1, colors);
		sceGxmDraw(gxm_context, prim, SCE_GXM_INDEX_FORMAT_U16, indices, idx_count);
	}

	// Purging vertex, colors and texcoord lists
	purge_vertex_list();
	vertex_count = 0;
}
