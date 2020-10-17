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
 * custom_shaders.c:
 * Implementation for custom shaders feature
 */

#include "shared.h"

#define MAX_CUSTOM_SHADERS 128 // Maximum number of linkable custom shaders
#define MAX_SHADER_PARAMS 8 // Maximum number of parameters per custom shader

// Internal stuffs
void *frag_uniforms = NULL;
void *vert_uniforms = NULL;
uint8_t use_shark = 1; // Flag to check if vitaShaRK should be initialized at vitaGL boot
uint8_t is_shark_online = 0; // Current vitaShaRK status

#ifdef HAVE_SHARK
// Internal runtime shader compiler settings
static int32_t compiler_fastmath = 0;
static int32_t compiler_fastprecision = 0;
static int32_t compiler_fastint = 0;
static shark_opt compiler_opts = SHARK_OPT_DEFAULT;
#endif

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
	uint32_t size;
	char *log;
} shader;

// Program struct holding vertex/fragment shader info
typedef struct program {
	shader *vshader;
	shader *fshader;
	GLboolean valid;
	SceGxmVertexAttribute attr[MAX_SHADER_PARAMS];
	SceGxmVertexStream stream[MAX_SHADER_PARAMS];
	SceGxmVertexProgram *vprog;
	SceGxmFragmentProgram *fprog;
	GLuint attr_num;
	GLuint stream_num;
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
		shaders[i].log = NULL;
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
	vert_uniforms = NULL;
	frag_uniforms = NULL;
}

void _vglDrawObjects_CustomShadersIMPL(GLenum mode, GLsizei count, GLboolean implicit_wvp) {
	if (implicit_wvp) {
		program *p = &progs[cur_program - 1];
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

#if defined(HAVE_SHARK) && defined(HAVE_SHARK_LOG)
static char *shark_log = NULL;
void shark_log_cb(const char *msg, shark_log_level msg_level, int line) {
	uint8_t append = shark_log != NULL;
	uint32_t size = (append ? strlen(shark_log) : 0) + strlen(msg);
	shark_log = append ? realloc(shark_log, size) : malloc(size);
	switch (msg_level) {
	case SHARK_LOG_INFO:
		sprintf(shark_log, "%s%sI] %s on line %d", append ? shark_log : "", append ? "\n" : "", msg, line);
		break;
	case SHARK_LOG_WARNING:
		sprintf(shark_log, "%s%sW] %s on line %d", append ? shark_log : "", append ? "\n" : "", msg, line);
		break;
	case SHARK_LOG_ERROR:
		sprintf(shark_log, "%s%sE] %s on line %d", append ? shark_log : "", append ? "\n" : "", msg, line);
		break;
	}
}
#endif

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */
void vglSetupRuntimeShaderCompiler(shark_opt opt_level, int32_t use_fastmath, int32_t use_fastprecision, int32_t use_fastint) {
#ifdef HAVE_SHARK
	compiler_opts = opt_level;
	compiler_fastmath = use_fastmath;
	compiler_fastprecision = use_fastprecision;
	compiler_fastint = use_fastint;
#endif
}
 
void vglEnableRuntimeShaderCompiler(GLboolean usage) {
	use_shark = usage;
}

GLuint glCreateShader(GLenum shaderType) {
	// Looking for a free shader slot
	GLuint i, res = 0;
	for (i = 1; i < MAX_CUSTOM_SHADERS; i++) {
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
		vgl_error = GL_INVALID_ENUM;
		return 0;
		break;
	}
	shaders[res - 1].valid = GL_TRUE;

	return res;
}

void glGetShaderiv(GLuint handle, GLenum pname, GLint *params) {
	// Grabbing passed shader
	shader *s = &shaders[handle - 1];
	
	switch (pname) {
	case GL_SHADER_TYPE:
		*params = s->type;
		break;
	case GL_COMPILE_STATUS:
		*params = s->prog ? GL_TRUE : GL_FALSE;
		break;
	case GL_INFO_LOG_LENGTH:
		*params = s->log ? strlen(s->log) : 0;
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}
}

void glGetShaderInfoLog(GLuint handle, GLsizei maxLength, GLsizei *length, GLchar *infoLog) {
#ifndef SKIP_ERROR_HANDLING
	if (maxLength < 0) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
#endif

	// Grabbing passed shader
	shader *s = &shaders[handle - 1];
	
	if (s->log) {
		*length = min(strlen(s->log), maxLength);
		memcpy_neon(infoLog, s->log, *length);
	}
}

void glShaderSource(GLuint handle, GLsizei count, const GLchar * const *string, const GLint *length) {
#ifndef SKIP_ERROR_HANDLING
	if (count < 0) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
#endif
    if (!is_shark_online) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	}
	
	// Grabbing passed shader
	shader *s = &shaders[handle - 1];
	
	// Temporarily setting prog to point to the shader source
	s->prog = (SceGxmProgram *)*string;
	s->size = length ? *length : strlen(*string);
}

void glShaderBinary(GLsizei count, const GLuint *handles, GLenum binaryFormat, const void *binary, GLsizei length) {
	// Grabbing passed shader
	shader *s = &shaders[handles[0] - 1];

	// Allocating compiled shader on RAM and registering it into sceGxmShaderPatcher
	s->prog = (SceGxmProgram *)malloc(length);
	memcpy_neon((void *)s->prog, binary, length);
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, s->prog, &s->id);
	s->prog = sceGxmShaderPatcherGetProgramFromId(s->id);
}

uint8_t shader_idxs = 0;
void glCompileShader(GLuint handle) {
	// If vitaShaRK is not enabled, we just error out
	if (!is_shark_online) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	}
#ifdef HAVE_SHARK
	// Grabbing passed shader
	shader *s = &shaders[handle - 1];
	
	// Compiling shader source
	s->prog = shark_compile_shader_extended((const char*)s->prog, &s->size, s->type == GL_FRAGMENT_SHADER ? SHARK_FRAGMENT_SHADER : SHARK_VERTEX_SHADER, compiler_opts, compiler_fastmath, compiler_fastprecision, compiler_fastint);
	if (s->prog) {
		SceGxmProgram *res = (SceGxmProgram *)malloc(s->size);
		memcpy_neon((void *)res, (void *)s->prog, s->size);
		s->prog = res;
		sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, s->prog, &s->id);
		s->prog = sceGxmShaderPatcherGetProgramFromId(s->id);
	}
#ifdef HAVE_SHARK_LOG
	s->log = shark_log;
	shark_log = NULL;
#endif
	shark_clear_output();
#endif
}

void glDeleteShader(GLuint shad) {
	// Grabbing passed shader
	shader *s = &shaders[shad - 1];

	// Deallocating shader and unregistering it from sceGxmShaderPatcher
	if (s->valid) {
		sceGxmShaderPatcherForceUnregisterProgram(gxm_shader_patcher, s->id);
		free((void *)s->prog);
		if (s->log) free(s->log);
	}
	s->log = NULL;
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
	} else {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
}

GLuint glCreateProgram(void) {
	// Looking for a free program slot
	GLuint i, res = 0;
	for (i = 1; i < (MAX_CUSTOM_SHADERS / 2); i++) {
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
		p->stream, p->stream_num, &p->vprog);
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

	// Checking if parameter is a vertex or fragment related one
	res->ptr = sceGxmProgramFindParameterByName(p->vshader->prog, name);
	res->isVertex = GL_TRUE;
	if (res->ptr == NULL) {
		res->ptr = sceGxmProgramFindParameterByName(p->fshader->prog, name);
		res->isVertex = GL_FALSE;
	}
	
	if (res->ptr == NULL) {
		free(res);
		return -1;
	}
		
	if (p->last_uniform != NULL)
		p->last_uniform->chain = (void *)res;
	p->last_uniform = res;

	return (GLint)res;
}

void glUniform1i(GLint location, GLint v0) {
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		float v0_f = (float)v0;
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 1, &v0_f);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		float v0_f = (float)v0;
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 1, &v0_f);
	}
}

void glUniform2i(GLint location, GLint v0, GLint v1) {
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		float v0_f[2];
		v0_f[0] = (float)v0;
		v0_f[1] = (float)v1;
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 2, v0_f);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		float v0_f[2];
		v0_f[0] = (float)v0;
		v0_f[1] = (float)v1;
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 2, v0_f);
	}
}

void glUniform1f(GLint location, GLfloat v0) {
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
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

void glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		float v0_f[2];
		v0_f[0] = v0;
		v0_f[1] = v1;
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 2, v0_f);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		float v0_f[2];
		v0_f[0] = v0;
		v0_f[1] = v1;
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 2, v0_f);
	}
}

void glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
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

void glUniform3fv(GLint location, GLsizei count, const GLfloat *value) {
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 3 * count, value);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 3 * count, value);
	}
}

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
	// Grabbing passed uniform
	uniform *u = (uniform *)location;
	if (u->ptr == NULL)
		return;

	// Setting passed value to desired uniform
	float v[4] = {v0, v1, v2, v3};
	if (u->isVertex) {
		if (vert_uniforms == NULL)
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vert_uniforms);
		sceGxmSetUniformDataF(vert_uniforms, u->ptr, 0, 4, v);
	} else {
		if (frag_uniforms == NULL)
			sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &frag_uniforms);
		sceGxmSetUniformDataF(frag_uniforms, u->ptr, 0, 4, v);
	}
}

void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
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
	// Checking if the uniform does exist
	if (location == -1)
		return;
	
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
	if (param == NULL) return;

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
	case GL_SHORT:
		attributes->format = SCE_GXM_ATTRIBUTE_FORMAT_S16N;
		bpe = sizeof(int16_t);
		break;
	case GL_UNSIGNED_BYTE:
		attributes->format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
		bpe = sizeof(uint8_t);
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}

	// Setting various info about the stream
	attributes->componentCount = num;
	attributes->regIndex = sceGxmProgramParameterGetResourceIndex(param);
	streams->stride = bpe * num;
	streams->indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	if (index >= p->attr_num) {
		p->attr_num = index + 1;
		p->stream_num = index + 1;
	}
}

// Equivalent of glBindAttribLocation but for sceGxm architecture when packed attributes are used
void vglBindPackedAttribLocation(GLuint prog, const GLchar *name, const GLuint num, const GLenum type, GLuint offset, GLint stride) {
	// Grabbing passed program
	program *p = &progs[prog - 1];
	SceGxmVertexAttribute *attributes = &p->attr[p->attr_num];
	SceGxmVertexStream *streams = &p->stream[0];

	// Looking for desired parameter in requested program
	const SceGxmProgramParameter *param = sceGxmProgramFindParameterByName(p->vshader->prog, name);
	if (param == NULL) return;

	// Setting stream index and offset values
	attributes->streamIndex = 0;
	attributes->offset = offset;

	// Detecting attribute format and size
	int bpe;
	switch (type) {
	case GL_FLOAT:
		attributes->format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		bpe = sizeof(float);
		break;
	case GL_SHORT:
		attributes->format = SCE_GXM_ATTRIBUTE_FORMAT_S16N;
		bpe = sizeof(int16_t);
		break;
	case GL_UNSIGNED_BYTE:
		attributes->format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
		bpe = sizeof(uint8_t);
		break;
	default:
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}

	// Setting various info about the stream
	attributes->componentCount = num;
	attributes->regIndex = sceGxmProgramParameterGetResourceIndex(param);
	streams->stride = stride ? stride : bpe * num;
	streams->indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	p->stream_num = 1;
	p->attr_num++;
}

// Equivalent of glVertexAttribPointer but for sceGxm architecture
void vglVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLuint count, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	// Error handling
	if (stride < 0) {
		SET_GL_ERROR(GL_INVALID_VALUE)
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
		SET_GL_ERROR(GL_INVALID_ENUM)
		break;
	}

	// Allocating enough memory on vitaGL mempool
	void *ptr = gpu_pool_memalign(count * bpe * size, bpe * size);

	// Copying passed data to vitaGL mempool
	if (stride == 0)
		memcpy_neon(ptr, pointer, count * bpe * size); // Faster if stride == 0
	else {
		int i;
		uint8_t *dst = (uint8_t *)ptr;
		uint8_t *src = (uint8_t *)pointer;
		for (i = 0; i < count; i++) {
			memcpy_neon(dst, src, bpe * size);
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
