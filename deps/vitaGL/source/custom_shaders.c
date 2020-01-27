/* 
 * custom_shaders.c:
 * Implementation for custom shaders feature
 */

#include "shared.h"

#define MAX_CUSTOM_SHADERS 32 // Maximum number of linkable custom shaders
#define MAX_SHADER_PARAMS 16 // Maximum number of parameters per custom shader

// Internal stuffs
void *frag_uniforms = NULL;
void *vert_uniforms = NULL;

GLuint cur_program = 0; // Current in use custom program (0 = No custom program)

// Uniform struct
typedef struct uniform {
	GLboolean isVertex;
	const SceGxmProgramParameter *ptr;
	void *chain;
} uniform;

// Generic shader struct
typedef struct shader {
	GLenum type;
	GLboolean valid;
	SceGxmShaderPatcherId id;
	const SceGxmProgram *prog;
} shader;

// Program struct holding vertex/fragment shader info
typedef struct program {
	shader *vshader;
	shader *fshader;
	GLboolean valid;
	SceGxmVertexAttribute attr[16];
	SceGxmVertexStream stream[16];
	SceGxmVertexProgram *vprog;
	SceGxmFragmentProgram *fprog;
	GLuint attr_num;
	const SceGxmProgramParameter *wvp;
	uniform *uniforms;
	uniform *last_uniform;
} program;

// Internal shaders array
static shader shaders[MAX_CUSTOM_SHADERS];

// Internal programs array
static program progs[MAX_CUSTOM_SHADERS / 2];

void resetCustomShaders(void) {
	// Init custom shaders
	int i;
	for (i = 0; i < MAX_CUSTOM_SHADERS; i++) {
		shaders[i].valid = 0;
		progs[i >> 1].valid = 0;
	}
}

void changeCustomShadersBlend(SceGxmBlendInfo *blend_info) {
	int j;
	for (j = 0; j < MAX_CUSTOM_SHADERS / 2; j++) {
		program *p = &progs[j];
		if (p->valid) {
			sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
				p->fshader->id,
				SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
				msaa_mode,
				blend_info,
				p->vshader->prog,
				&p->fprog);
		}
	}
}

void reloadCustomShader(void) {
	if (cur_program == 0)
		return;
	program *p = &progs[cur_program - 1];
	sceGxmSetVertexProgram(gxm_context, p->vprog);
	sceGxmSetFragmentProgram(gxm_context, p->fprog);
}

void _vglDrawObjects_CustomShadersIMPL(GLenum mode, GLsizei count, GLboolean implicit_wvp) {
	program *p = &progs[cur_program - 1];
	if (implicit_wvp) {
		if (mvp_modified) {
			matrix4x4_multiply(mvp_matrix, projection_matrix, modelview_matrix);
			mvp_modified = GL_FALSE;
		}
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		if (p->wvp == NULL)
			p->wvp = sceGxmProgramFindParameterByName(p->vshader->prog, "wvp");
		sceGxmSetUniformDataF(vert_uniforms, p->wvp, 0, 16, (const float *)mvp_matrix);
	}
}

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */

GLuint glCreateShader(GLenum shaderType) {
	// Looking for a free shader slot
	GLuint i, res = 0;
	for (i = 1; i <= MAX_CUSTOM_SHADERS; i++) {
		if (!(shaders[i - 1].valid)) {
			res = i;
			break;
		}
	}

	// All shader slots are busy, exiting call
	if (res == 0)
		return res;

	// Reserving and initializing shader slot
	switch (shaderType) {
	case GL_FRAGMENT_SHADER:
		shaders[res - 1].type = GL_FRAGMENT_SHADER;
		break;
	case GL_VERTEX_SHADER:
		shaders[res - 1].type = GL_VERTEX_SHADER;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	shaders[res - 1].valid = GL_TRUE;

	return res;
}

void glShaderBinary(GLsizei count, const GLuint *handles, GLenum binaryFormat, const void *binary, GLsizei length) {
	// Grabbing passed shader
	shader *s = &shaders[handles[0] - 1];

	// Allocating compiled shader on RAM and registering it into sceGxmShaderPatcher
	s->prog = (SceGxmProgram *)malloc(length);
	memcpy((void *)s->prog, binary, length);
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, s->prog, &s->id);
	s->prog = sceGxmShaderPatcherGetProgramFromId(s->id);
}

void glDeleteShader(GLuint shad) {
	// Grabbing passed shader
	shader *s = &shaders[shad - 1];

	// Deallocating shader and unregistering it from sceGxmShaderPatcher
	if (s->valid) {
		sceGxmShaderPatcherForceUnregisterProgram(gxm_shader_patcher, s->id);
		free((void *)s->prog);
	}
	s->valid = GL_FALSE;
}

void glAttachShader(GLuint prog, GLuint shad) {
	// Grabbing passed shader and program
	shader *s = &shaders[shad - 1];
	program *p = &progs[prog - 1];

	// Attaching shader to desired program
	if (p->valid && s->valid) {
		switch (s->type) {
		case GL_VERTEX_SHADER:
			p->vshader = s;
			break;
		case GL_FRAGMENT_SHADER:
			p->fshader = s;
			break;
		default:
			break;
		}
	} else
		_vitagl_error = GL_INVALID_VALUE;
}

GLuint glCreateProgram(void) {
	// Looking for a free program slot
	GLuint i, res = 0;
	for (i = 1; i <= (MAX_CUSTOM_SHADERS / 2); i++) {
		// Program slot found, reserving and initializing it
		if (!(progs[i - 1].valid)) {
			res = i;
			progs[i - 1].valid = GL_TRUE;
			progs[i - 1].attr_num = 0;
			progs[i - 1].wvp = NULL;
			progs[i - 1].uniforms = NULL;
			progs[i - 1].last_uniform = NULL;
			break;
		}
	}
	return res;
}

void glDeleteProgram(GLuint prog) {
	// Grabbing passed program
	program *p = &progs[prog - 1];

	// Releasing both vertex and fragment programs from sceGxmShaderPatcher
	if (p->valid) {
		unsigned int count, i;
		sceGxmShaderPatcherGetFragmentProgramRefCount(gxm_shader_patcher, p->fprog, &count);
		for (i = 0; i < count; i++) {
			sceGxmShaderPatcherReleaseFragmentProgram(gxm_shader_patcher, p->fprog);
			sceGxmShaderPatcherReleaseVertexProgram(gxm_shader_patcher, p->vprog);
		}
		while (p->uniforms != NULL) {
			uniform *old = p->uniforms;
			p->uniforms = (uniform *)p->uniforms->chain;
			free(old);
		}
	}
	p->valid = GL_FALSE;
}

void glLinkProgram(GLuint progr) {
	// Grabbing passed program
	program *p = &progs[progr - 1];

	// Creating fragment and vertex program via sceGxmShaderPatcher
	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		p->vshader->id, p->attr, p->attr_num,
		p->stream, p->attr_num, &p->vprog);
	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		p->fshader->id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa_mode, NULL, p->vshader->prog,
		&p->fprog);
}

void glUseProgram(GLuint prog) {
	// Setting current custom program to passed program
	cur_program = prog;

	// Setting in-use vertex and fragment program in sceGxm
	reloadCustomShader();
}

GLint glGetUniformLocation(GLuint prog, const GLchar *name) {
	// Grabbing passed program
	program *p = &progs[prog - 1];

	uniform *res = (uniform *)malloc(sizeof(uniform));
	res->chain = NULL;
	if (p->last_uniform != NULL)
		p->last_uniform->chain = (void *)res;
	p->last_uniform = res;

	// Checking if parameter is a vertex or fragment related one
	res->ptr = sceGxmProgramFindParameterByName(p->vshader->prog, name);
	res->isVertex = GL_TRUE;
	if (res->ptr == NULL) {
		res->ptr = sceGxmProgramFindParameterByName(p->fshader->prog, name);
		res->isVertex = GL_FALSE;
	}

	return (GLint)res;
}

void glUniform1f(GLint location, GLfloat v0) {
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 1, &v0);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 1, &v0);
	}
}

void glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 2 * count, value);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 2 * count, value);
	}
}

void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 4 * count, value);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 4 * count, value);
	}
}

void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 16 * count, value);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 16 * count, value);
	}
}

/*
 * ------------------------------
 * -    VGL_EXT_gxp_shaders     -
 * ------------------------------
 */

// Equivalent of glBindAttribLocation but for sceGxm architecture
void vglBindAttribLocation(GLuint prog, GLuint index, const GLchar *name, const GLuint num, const GLenum type) {
	// Grabbing passed program
	program *p = &progs[prog - 1];
	SceGxmVertexAttribute *attributes = &p->attr[index];
	SceGxmVertexStream *streams = &p->stream[index];

	// Looking for desired parameter in requested program
	const SceGxmProgramParameter *param = sceGxmProgramFindParameterByName(p->vshader->prog, name);

	// Setting stream index and offset values
	attributes->streamIndex = index;
	attributes->offset = 0;

	// Detecting attribute format and size
	int bpe;
	switch (type) {
	case GL_FLOAT:
		attributes->format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		bpe = sizeof(float);
		break;
	case GL_UNSIGNED_BYTE:
		attributes->format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
		bpe = sizeof(uint8_t);
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}

	// Setting various info about the stream
	attributes->componentCount = num;
	attributes->regIndex = sceGxmProgramParameterGetResourceIndex(param);
	streams->stride = bpe * num;
	streams->indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	if (index >= p->attr_num)
		p->attr_num = index + 1;
}

// Equivalent of glVertexAttribLocation but for sceGxm architecture
void vglVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLuint count, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (stride < 0) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif

	// Detecting type size
	int bpe;
	switch (type) {
	case GL_FLOAT:
		bpe = sizeof(GLfloat);
		break;
	case GL_SHORT:
		bpe = sizeof(GLshort);
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}

	// Allocating enough memory on vitaGL mempool
	void *ptr = gpu_pool_memalign(count * bpe * size, bpe * size);

	// Copying passed data to vitaGL mempool
	if (stride == 0)
		memcpy(ptr, pointer, count * bpe * size); // Faster if stride == 0
	else {
		int i;
		uint8_t *dst = (uint8_t *)ptr;
		uint8_t *src = (uint8_t *)pointer;
		for (i = 0; i < count; i++) {
			memcpy(dst, src, bpe * size);
			dst += (bpe * size);
			src += stride;
		}
	}

	// Setting vertex stream to passed index in sceGxm
	sceGxmSetVertexStream(gxm_context, index, ptr);
}

void vglVertexAttribPointerMapped(GLuint index, const GLvoid *pointer) {
	// Setting vertex stream to passed index in sceGxm
	sceGxmSetVertexStream(gxm_context, index, pointer);
}
