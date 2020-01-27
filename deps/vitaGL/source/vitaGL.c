/*
 *
 */
#include "vitaGL.h"
#include "shared.h"
#include "texture_callbacks.h"

// Shaders
#include "shaders/clear_f.h"
#include "shaders/clear_v.h"
#include "shaders/disable_color_buffer_f.h"
#include "shaders/rgb_v.h"
#include "shaders/rgba_f.h"
#include "shaders/rgba_v.h"
#include "shaders/texture2d_f.h"
#include "shaders/texture2d_rgba_f.h"
#include "shaders/texture2d_rgba_v.h"
#include "shaders/texture2d_v.h"

typedef struct gpubuffer {
	void *ptr;
} gpubuffer;

// sceGxm viewport setup (NOTE: origin is on center screen)
float x_port = 480.0f;
float y_port = -272.0f;
float z_port = 0.5f;
float x_scale = 480.0f;
float y_scale = 272.0f;
float z_scale = 0.5f;

uint8_t viewport_mode = 0; // Current setting for viewport mode
GLboolean vblank = GL_TRUE; // Current setting for VSync

extern int _newlib_heap_memblock; // Newlib Heap memblock
extern unsigned _newlib_heap_size; // Newlib Heap size

static const SceGxmProgram *const gxm_program_disable_color_buffer_f = (SceGxmProgram *)&disable_color_buffer_f;
static const SceGxmProgram *const gxm_program_clear_v = (SceGxmProgram *)&clear_v;
static const SceGxmProgram *const gxm_program_clear_f = (SceGxmProgram *)&clear_f;
static const SceGxmProgram *const gxm_program_rgba_v = (SceGxmProgram *)&rgba_v;
static const SceGxmProgram *const gxm_program_rgba_f = (SceGxmProgram *)&rgba_f;
static const SceGxmProgram *const gxm_program_rgb_v = (SceGxmProgram *)&rgb_v;
static const SceGxmProgram *const gxm_program_texture2d_v = (SceGxmProgram *)&texture2d_v;
static const SceGxmProgram *const gxm_program_texture2d_f = (SceGxmProgram *)&texture2d_f;
static const SceGxmProgram *const gxm_program_texture2d_rgba_v = (SceGxmProgram *)&texture2d_rgba_v;
static const SceGxmProgram *const gxm_program_texture2d_rgba_f = (SceGxmProgram *)&texture2d_rgba_f;

// Disable color buffer shader
uint16_t *depth_clear_indices = NULL; // Memblock starting address for clear screen indices

// Clear shaders
SceGxmVertexProgram *clear_vertex_program_patched; // Patched vertex program for clearing screen
vector2f *clear_vertices = NULL; // Memblock starting address for clear screen vertices
vector3f *depth_vertices = NULL; // Memblock starting address for depth clear screen vertices

// Internal stuffs
SceGxmMultisampleMode msaa_mode = SCE_GXM_MULTISAMPLE_NONE;

static SceGxmBlendInfo *cur_blend_info_ptr = NULL;
extern uint8_t use_vram;

static GLuint buffers[BUFFERS_NUM]; // Buffers array
static gpubuffer gpu_buffers[BUFFERS_NUM]; // Buffers array
static SceGxmColorMask blend_color_mask = SCE_GXM_COLOR_MASK_ALL; // Current in-use color mask (glColorMask)
static SceGxmBlendFunc blend_func_rgb = SCE_GXM_BLEND_FUNC_ADD; // Current in-use RGB blend func
static SceGxmBlendFunc blend_func_a = SCE_GXM_BLEND_FUNC_ADD; // Current in-use A blend func
static int vertex_array_unit = -1; // Current in-use vertex array unit
static int index_array_unit = -1; // Current in-use index array unit

vector4f texenv_color = { 0.0f, 0.0f, 0.0f, 0.0f }; // Current in use texture environment color

// Internal functions

#ifdef ENABLE_LOG
void LOG(const char *format, ...) {
	__gnuc_va_list arg;
	int done;
	va_start(arg, format);
	char msg[512];
	done = vsprintf(msg, format, arg);
	va_end(arg);
	int i;
	sprintf(msg, "%s\n", msg);
	FILE *log = fopen("ux0:/data/vitaGL.log", "a+");
	if (log != NULL) {
		fwrite(msg, 1, strlen(msg), log);
		fclose(log);
	}
}
#endif

static void _change_blend_factor(SceGxmBlendInfo *blend_info) {
	changeCustomShadersBlend(blend_info);

	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		rgba_fragment_id,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa_mode,
		blend_info,
		NULL,
		&rgba_fragment_program_patched);

	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		texture2d_fragment_id,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa_mode,
		blend_info,
		NULL,
		&texture2d_fragment_program_patched);

	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		texture2d_rgba_fragment_id,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa_mode,
		blend_info,
		NULL,
		&texture2d_rgba_fragment_program_patched);
}

void change_blend_factor() {
	static SceGxmBlendInfo blend_info;
	blend_info.colorMask = blend_color_mask;
	blend_info.colorFunc = blend_func_rgb;
	blend_info.alphaFunc = blend_func_a;
	blend_info.colorSrc = blend_sfactor_rgb;
	blend_info.colorDst = blend_dfactor_rgb;
	blend_info.alphaSrc = blend_sfactor_a;
	blend_info.alphaDst = blend_dfactor_a;

	_change_blend_factor(&blend_info);
	cur_blend_info_ptr = &blend_info;
	if (cur_program != 0) {
		reloadCustomShader();
	}
}

void change_blend_mask() {
	static SceGxmBlendInfo blend_info;
	blend_info.colorMask = blend_color_mask;
	blend_info.colorFunc = SCE_GXM_BLEND_FUNC_ADD;
	blend_info.alphaFunc = SCE_GXM_BLEND_FUNC_ADD;
	blend_info.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
	blend_info.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_info.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
	blend_info.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;

	_change_blend_factor(&blend_info);
	cur_blend_info_ptr = &blend_info;
	if (cur_program != 0) {
		reloadCustomShader();
	}
}

void disable_blend() {
	if (blend_color_mask == SCE_GXM_COLOR_MASK_ALL) {
		_change_blend_factor(NULL);
		cur_blend_info_ptr = NULL;
		if (cur_program != 0) {
			reloadCustomShader();
		}
	} else
		change_blend_mask();
}

void vector2f_convert_to_local_space(vector2f *out, int x, int y, int width, int height) {
	out[0].x = (float)(2 * x) / (float)DISPLAY_WIDTH_FLOAT - 1.0f;
	out[1].x = (float)(2 * (x + width)) / (float)DISPLAY_WIDTH_FLOAT - 1.0f;
	out[2].x = (float)(2 * (x + width)) / (float)DISPLAY_WIDTH_FLOAT - 1.0f;
	out[3].x = (float)(2 * x) / (float)DISPLAY_WIDTH_FLOAT - 1.0f;
	out[0].y = 1.0f - (float)(2 * y) / (float)DISPLAY_HEIGHT_FLOAT;
	out[1].y = 1.0f - (float)(2 * y) / (float)DISPLAY_HEIGHT_FLOAT;
	out[2].y = 1.0f - (float)(2 * (y + height)) / (float)DISPLAY_HEIGHT_FLOAT;
	out[3].y = 1.0f - (float)(2 * (y + height)) / (float)DISPLAY_HEIGHT_FLOAT;
}

// vitaGL specific functions

void vglUseVram(GLboolean usage) {
	use_vram = usage;
}

void vglInitExtended(uint32_t gpu_pool_size, int width, int height, int ram_threshold, SceGxmMultisampleMode msaa) {
	// Setting our display size
	msaa_mode = msaa;
	DISPLAY_WIDTH = width;
	DISPLAY_HEIGHT = height;
	DISPLAY_WIDTH_FLOAT = width * 1.0f;
	DISPLAY_HEIGHT_FLOAT = height * 1.0f;
	switch (DISPLAY_WIDTH) {
	case 480:
		DISPLAY_STRIDE = 512;
		break;
	case 640:
		DISPLAY_STRIDE = 640;
		break;
	case 720:
		DISPLAY_STRIDE = 768;
		break;
	default:
		DISPLAY_STRIDE = 960;
		break;
	}

	// Initializing sceGxm
	initGxm();

	// Getting max allocatable CDRAM and RAM memory
	SceKernelFreeMemorySizeInfo info;
	info.size = sizeof(SceKernelFreeMemorySizeInfo);
	sceKernelGetFreeMemorySize(&info);

	// Initializing memory heap for CDRAM and RAM memory
	vitagl_mem_init(info.size_user - ram_threshold, info.size_cdram - 1 * 1024 * 1024, info.size_phycont - 1 * 1024 * 1024); // leave some just in case

	// Initializing sceGxm context
	initGxmContext();

	// Creating render target for the display
	createDisplayRenderTarget();

	// Creating color surfaces for the display
	initDisplayColorSurfaces();

	// Creating depth and stencil surfaces for the display
	initDepthStencilSurfaces();

	// Starting a sceGxmShaderPatcher instance
	startShaderPatcher();

	// Disable color buffer shader register
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_disable_color_buffer_f,
		&disable_color_buffer_fragment_id);

	const SceGxmProgram *disable_color_buffer_fragment_program = sceGxmShaderPatcherGetProgramFromId(disable_color_buffer_fragment_id);

	clear_depth = sceGxmProgramFindParameterByName(
		disable_color_buffer_fragment_program, "depth_clear");

	SceGxmBlendInfo disable_color_buffer_blend_info;
	memset(&disable_color_buffer_blend_info, 0, sizeof(SceGxmBlendInfo));
	disable_color_buffer_blend_info.colorMask = SCE_GXM_COLOR_MASK_NONE;
	disable_color_buffer_blend_info.colorFunc = SCE_GXM_BLEND_FUNC_NONE;
	disable_color_buffer_blend_info.alphaFunc = SCE_GXM_BLEND_FUNC_NONE;
	disable_color_buffer_blend_info.colorSrc = SCE_GXM_BLEND_FACTOR_ZERO;
	disable_color_buffer_blend_info.colorDst = SCE_GXM_BLEND_FACTOR_ZERO;
	disable_color_buffer_blend_info.alphaSrc = SCE_GXM_BLEND_FACTOR_ZERO;
	disable_color_buffer_blend_info.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;

	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		disable_color_buffer_fragment_id,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		&disable_color_buffer_blend_info, NULL,
		&disable_color_buffer_fragment_program_patched);

	vglMemType type = VGL_MEM_RAM;
	clear_vertices = gpu_alloc_mapped(4 * sizeof(vector2f), &type);
	depth_clear_indices = gpu_alloc_mapped(4 * sizeof(unsigned short), &type);

	vector2f_convert_to_local_space(clear_vertices, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

	depth_clear_indices[0] = 0;
	depth_clear_indices[1] = 1;
	depth_clear_indices[2] = 2;
	depth_clear_indices[3] = 3;

	// Clear shader register
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_clear_v,
		&clear_vertex_id);
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_clear_f,
		&clear_fragment_id);

	const SceGxmProgram *clear_vertex_program = sceGxmShaderPatcherGetProgramFromId(clear_vertex_id);
	const SceGxmProgram *clear_fragment_program = sceGxmShaderPatcherGetProgramFromId(clear_fragment_id);

	clear_position = sceGxmProgramFindParameterByName(
		clear_vertex_program, "position");

	clear_color = sceGxmProgramFindParameterByName(
		clear_fragment_program, "u_clear_color");

	SceGxmVertexAttribute clear_vertex_attribute;
	SceGxmVertexStream clear_vertex_stream;
	clear_vertex_attribute.streamIndex = 0;
	clear_vertex_attribute.offset = 0;
	clear_vertex_attribute.format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	clear_vertex_attribute.componentCount = 2;
	clear_vertex_attribute.regIndex = sceGxmProgramParameterGetResourceIndex(
		clear_position);
	clear_vertex_stream.stride = sizeof(vector2f);
	clear_vertex_stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		clear_vertex_id, &clear_vertex_attribute,
		1, &clear_vertex_stream, 1, &clear_vertex_program_patched);

	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		clear_fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa, NULL, NULL,
		&clear_fragment_program_patched);

	// Color shader register
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_rgba_v,
		&rgba_vertex_id);
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_rgb_v,
		&rgb_vertex_id);
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_rgba_f,
		&rgba_fragment_id);

	const SceGxmProgram *rgba_vertex_program = sceGxmShaderPatcherGetProgramFromId(rgba_vertex_id);
	const SceGxmProgram *rgb_vertex_program = sceGxmShaderPatcherGetProgramFromId(rgb_vertex_id);
	rgba_fragment_program = sceGxmShaderPatcherGetProgramFromId(rgba_fragment_id);

	rgba_position = sceGxmProgramFindParameterByName(
		rgba_vertex_program, "aPosition");

	rgba_color = sceGxmProgramFindParameterByName(
		rgba_vertex_program, "aColor");

	rgb_position = sceGxmProgramFindParameterByName(
		rgba_vertex_program, "aPosition");

	rgb_color = sceGxmProgramFindParameterByName(
		rgba_vertex_program, "aColor");

	SceGxmVertexAttribute rgba_vertex_attribute[2];
	SceGxmVertexStream rgba_vertex_stream[2];
	rgba_vertex_attribute[0].streamIndex = 0;
	rgba_vertex_attribute[0].offset = 0;
	rgba_vertex_attribute[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	rgba_vertex_attribute[0].componentCount = 3;
	rgba_vertex_attribute[0].regIndex = sceGxmProgramParameterGetResourceIndex(
		rgba_position);
	rgba_vertex_attribute[1].streamIndex = 1;
	rgba_vertex_attribute[1].offset = 0;
	rgba_vertex_attribute[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	rgba_vertex_attribute[1].componentCount = 4;
	rgba_vertex_attribute[1].regIndex = sceGxmProgramParameterGetResourceIndex(
		rgba_color);
	rgba_vertex_stream[0].stride = sizeof(vector3f);
	rgba_vertex_stream[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	rgba_vertex_stream[1].stride = sizeof(vector4f);
	rgba_vertex_stream[1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		rgba_vertex_id, rgba_vertex_attribute,
		2, rgba_vertex_stream, 2, &rgba_vertex_program_patched);

	rgba_vertex_attribute[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	rgba_vertex_stream[1].stride = sizeof(uint8_t) * 4;

	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		rgba_vertex_id, rgba_vertex_attribute,
		2, rgba_vertex_stream, 2, &rgba_u8n_vertex_program_patched);

	SceGxmVertexAttribute rgb_vertex_attribute[2];
	SceGxmVertexStream rgb_vertex_stream[2];
	rgb_vertex_attribute[0].streamIndex = 0;
	rgb_vertex_attribute[0].offset = 0;
	rgb_vertex_attribute[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	rgb_vertex_attribute[0].componentCount = 3;
	rgb_vertex_attribute[0].regIndex = sceGxmProgramParameterGetResourceIndex(
		rgb_position);
	rgb_vertex_attribute[1].streamIndex = 1;
	rgb_vertex_attribute[1].offset = 0;
	rgb_vertex_attribute[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	rgb_vertex_attribute[1].componentCount = 3;
	rgb_vertex_attribute[1].regIndex = sceGxmProgramParameterGetResourceIndex(
		rgb_color);
	rgb_vertex_stream[0].stride = sizeof(vector3f);
	rgb_vertex_stream[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	rgb_vertex_stream[1].stride = sizeof(vector3f);
	rgb_vertex_stream[1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		rgb_vertex_id, rgb_vertex_attribute,
		2, rgb_vertex_stream, 2, &rgb_vertex_program_patched);

	rgb_vertex_attribute[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	rgb_vertex_stream[1].stride = sizeof(uint8_t) * 3;

	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		rgb_vertex_id, rgb_vertex_attribute,
		2, rgb_vertex_stream, 2, &rgb_u8n_vertex_program_patched);

	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		rgba_fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa, NULL, NULL,
		&rgba_fragment_program_patched);

	rgba_wvp = sceGxmProgramFindParameterByName(rgba_vertex_program, "wvp");
	rgb_wvp = sceGxmProgramFindParameterByName(rgb_vertex_program, "wvp");

	// Texture2D shader register
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_texture2d_v,
		&texture2d_vertex_id);
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_texture2d_f,
		&texture2d_fragment_id);

	const SceGxmProgram *texture2d_vertex_program = sceGxmShaderPatcherGetProgramFromId(texture2d_vertex_id);
	texture2d_fragment_program = sceGxmShaderPatcherGetProgramFromId(texture2d_fragment_id);

	texture2d_position = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "position");

	texture2d_texcoord = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "texcoord");

	texture2d_alpha_cut = sceGxmProgramFindParameterByName(
		texture2d_fragment_program, "alphaCut");

	texture2d_alpha_op = sceGxmProgramFindParameterByName(
		texture2d_fragment_program, "alphaOp");

	texture2d_tint_color = sceGxmProgramFindParameterByName(
		texture2d_fragment_program, "tintColor");

	texture2d_tex_env = sceGxmProgramFindParameterByName(
		texture2d_fragment_program, "texEnv");

	texture2d_fog_mode = sceGxmProgramFindParameterByName(
		texture2d_fragment_program, "fog_mode");

	texture2d_fog_color = sceGxmProgramFindParameterByName(
		texture2d_fragment_program, "fogColor");

	texture2d_fog_mode2 = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "fog_mode");

	texture2d_clip_plane0 = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "clip_plane0");

	texture2d_clip_plane0_eq = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "clip_plane0_eq");

	texture2d_mv = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "modelview");

	texture2d_fog_near = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "fog_near");

	texture2d_fog_far = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "fog_far");

	texture2d_fog_density = sceGxmProgramFindParameterByName(
		texture2d_vertex_program, "fog_density");

	texture2d_tex_env_color = sceGxmProgramFindParameterByName(
		texture2d_fragment_program, "texEnvColor");

	SceGxmVertexAttribute texture2d_vertex_attribute[2];
	SceGxmVertexStream texture2d_vertex_stream[2];
	texture2d_vertex_attribute[0].streamIndex = 0;
	texture2d_vertex_attribute[0].offset = 0;
	texture2d_vertex_attribute[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	texture2d_vertex_attribute[0].componentCount = 3;
	texture2d_vertex_attribute[0].regIndex = sceGxmProgramParameterGetResourceIndex(
		texture2d_position);
	texture2d_vertex_attribute[1].streamIndex = 1;
	texture2d_vertex_attribute[1].offset = 0;
	texture2d_vertex_attribute[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	texture2d_vertex_attribute[1].componentCount = 2;
	texture2d_vertex_attribute[1].regIndex = sceGxmProgramParameterGetResourceIndex(
		texture2d_texcoord);
	texture2d_vertex_stream[0].stride = sizeof(vector3f);
	texture2d_vertex_stream[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	texture2d_vertex_stream[1].stride = sizeof(vector2f);
	texture2d_vertex_stream[1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		texture2d_vertex_id, texture2d_vertex_attribute,
		2, texture2d_vertex_stream, 2, &texture2d_vertex_program_patched);

	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		texture2d_fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa, NULL, NULL,
		&texture2d_fragment_program_patched);

	texture2d_wvp = sceGxmProgramFindParameterByName(texture2d_vertex_program, "wvp");

	// Texture2D+RGBA shader register
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_texture2d_rgba_v,
		&texture2d_rgba_vertex_id);
	sceGxmShaderPatcherRegisterProgram(gxm_shader_patcher, gxm_program_texture2d_rgba_f,
		&texture2d_rgba_fragment_id);

	const SceGxmProgram *texture2d_rgba_vertex_program = sceGxmShaderPatcherGetProgramFromId(texture2d_rgba_vertex_id);
	texture2d_rgba_fragment_program = sceGxmShaderPatcherGetProgramFromId(texture2d_rgba_fragment_id);

	texture2d_rgba_position = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "position");

	texture2d_rgba_texcoord = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "texcoord");

	texture2d_rgba_alpha_cut = sceGxmProgramFindParameterByName(
		texture2d_rgba_fragment_program, "alphaCut");

	texture2d_rgba_alpha_op = sceGxmProgramFindParameterByName(
		texture2d_rgba_fragment_program, "alphaOp");

	texture2d_rgba_color = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "color");

	texture2d_rgba_tex_env = sceGxmProgramFindParameterByName(
		texture2d_rgba_fragment_program, "texEnv");

	texture2d_rgba_fog_mode = sceGxmProgramFindParameterByName(
		texture2d_rgba_fragment_program, "fog_mode");

	texture2d_rgba_fog_mode2 = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "fog_mode");

	texture2d_rgba_clip_plane0 = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "clip_plane0");

	texture2d_rgba_clip_plane0_eq = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "clip_plane0_eq");

	texture2d_rgba_mv = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "modelview");

	texture2d_rgba_fog_near = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "fog_near");

	texture2d_rgba_fog_far = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "fog_far");

	texture2d_rgba_fog_density = sceGxmProgramFindParameterByName(
		texture2d_rgba_vertex_program, "fog_density");

	texture2d_rgba_fog_color = sceGxmProgramFindParameterByName(
		texture2d_rgba_fragment_program, "fogColor");

	texture2d_rgba_tex_env_color = sceGxmProgramFindParameterByName(
		texture2d_rgba_fragment_program, "texEnvColor");

	SceGxmVertexAttribute texture2d_rgba_vertex_attribute[3];
	SceGxmVertexStream texture2d_rgba_vertex_stream[3];
	texture2d_rgba_vertex_attribute[0].streamIndex = 0;
	texture2d_rgba_vertex_attribute[0].offset = 0;
	texture2d_rgba_vertex_attribute[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	texture2d_rgba_vertex_attribute[0].componentCount = 3;
	texture2d_rgba_vertex_attribute[0].regIndex = sceGxmProgramParameterGetResourceIndex(
		texture2d_rgba_position);
	texture2d_rgba_vertex_attribute[1].streamIndex = 1;
	texture2d_rgba_vertex_attribute[1].offset = 0;
	texture2d_rgba_vertex_attribute[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	texture2d_rgba_vertex_attribute[1].componentCount = 2;
	texture2d_rgba_vertex_attribute[1].regIndex = sceGxmProgramParameterGetResourceIndex(
		texture2d_rgba_texcoord);
	texture2d_rgba_vertex_attribute[2].streamIndex = 2;
	texture2d_rgba_vertex_attribute[2].offset = 0;
	texture2d_rgba_vertex_attribute[2].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	texture2d_rgba_vertex_attribute[2].componentCount = 4;
	texture2d_rgba_vertex_attribute[2].regIndex = sceGxmProgramParameterGetResourceIndex(
		texture2d_rgba_color);
	texture2d_rgba_vertex_stream[0].stride = sizeof(vector3f);
	texture2d_rgba_vertex_stream[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	texture2d_rgba_vertex_stream[1].stride = sizeof(vector2f);
	texture2d_rgba_vertex_stream[1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	texture2d_rgba_vertex_stream[2].stride = sizeof(vector4f);
	texture2d_rgba_vertex_stream[2].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		texture2d_rgba_vertex_id, texture2d_rgba_vertex_attribute,
		3, texture2d_rgba_vertex_stream, 3, &texture2d_rgba_vertex_program_patched);

	texture2d_rgba_vertex_attribute[2].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	texture2d_rgba_vertex_stream[2].stride = sizeof(uint8_t) * 4;

	sceGxmShaderPatcherCreateVertexProgram(gxm_shader_patcher,
		texture2d_rgba_vertex_id, texture2d_rgba_vertex_attribute,
		3, texture2d_rgba_vertex_stream, 3, &texture2d_rgba_u8n_vertex_program_patched);

	sceGxmShaderPatcherCreateFragmentProgram(gxm_shader_patcher,
		texture2d_rgba_fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa, NULL, NULL,
		&texture2d_rgba_fragment_program_patched);

	texture2d_rgba_wvp = sceGxmProgramFindParameterByName(texture2d_rgba_vertex_program, "wvp");

	sceGxmSetTwoSidedEnable(gxm_context, SCE_GXM_TWO_SIDED_ENABLED);

	// Scissor Test shader register
	sceGxmShaderPatcherCreateMaskUpdateFragmentProgram(gxm_shader_patcher, &scissor_test_fragment_program);

	scissor_test_vertices = gpu_alloc_mapped(4 * sizeof(vector2f), &type);

	// Allocate temp pool for non-VBO drawing
	gpu_pool_init(gpu_pool_size);

	// Init texture units
	int i, j;
	for (i = 0; i < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS; i++) {
		for (j = 0; j < TEXTURES_NUM; j++) {
			texture_units[i].textures[j].used = 0;
			texture_units[i].textures[j].valid = 0;
		}
		texture_units[i].env_mode = MODULATE;
		texture_units[i].tex_id = 0;
		texture_units[i].enabled = 0;
		texture_units[i].min_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
		texture_units[i].mag_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
		texture_units[i].u_mode = SCE_GXM_TEXTURE_ADDR_REPEAT;
		texture_units[i].v_mode = SCE_GXM_TEXTURE_ADDR_REPEAT;
	}

	// Init custom shaders
	resetCustomShaders();

	// Init buffers
	for (i = 0; i < BUFFERS_NUM; i++) {
		buffers[i] = BUFFERS_ADDR + i;
		gpu_buffers[i].ptr = NULL;
	}

	// Init scissor test state
	resetScissorTestRegion();

	// Init viewport state
	gl_viewport.x = 0;
	gl_viewport.y = 0;
	gl_viewport.w = DISPLAY_WIDTH;
	gl_viewport.h = DISPLAY_HEIGHT;

	// Getting newlib heap memblock starting address
	void *addr = NULL;
	sceKernelGetMemBlockBase(_newlib_heap_memblock, &addr);

	// Mapping newlib heap into sceGxm
	sceGxmMapMemory(addr, _newlib_heap_size, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);
}

void vglInit(uint32_t gpu_pool_size) {
	vglInitExtended(gpu_pool_size, DISPLAY_WIDTH_DEF, DISPLAY_HEIGHT_DEF, 0x1000000, SCE_GXM_MULTISAMPLE_NONE);
}

void vglEnd(void) {
	// Wait for rendering to be finished
	waitRenderingDone();

	// Deallocating default vertices buffers
	vitagl_mempool_free(clear_vertices, VGL_MEM_RAM);
	vitagl_mempool_free(depth_vertices, VGL_MEM_RAM);
	vitagl_mempool_free(depth_clear_indices, VGL_MEM_RAM);
	vitagl_mempool_free(scissor_test_vertices, VGL_MEM_RAM);

	// Releasing shader programs from sceGxmShaderPatcher
	sceGxmShaderPatcherReleaseFragmentProgram(gxm_shader_patcher, scissor_test_fragment_program);
	sceGxmShaderPatcherReleaseFragmentProgram(gxm_shader_patcher, disable_color_buffer_fragment_program_patched);
	sceGxmShaderPatcherReleaseVertexProgram(gxm_shader_patcher, clear_vertex_program_patched);
	sceGxmShaderPatcherReleaseFragmentProgram(gxm_shader_patcher, clear_fragment_program_patched);
	sceGxmShaderPatcherReleaseVertexProgram(gxm_shader_patcher, rgba_vertex_program_patched);
	sceGxmShaderPatcherReleaseVertexProgram(gxm_shader_patcher, rgb_vertex_program_patched);
	sceGxmShaderPatcherReleaseFragmentProgram(gxm_shader_patcher, rgba_fragment_program_patched);
	sceGxmShaderPatcherReleaseVertexProgram(gxm_shader_patcher, texture2d_vertex_program_patched);
	sceGxmShaderPatcherReleaseFragmentProgram(gxm_shader_patcher, texture2d_fragment_program_patched);

	// Unregistering shader programs from sceGxmShaderPatcher
	sceGxmShaderPatcherUnregisterProgram(gxm_shader_patcher, clear_vertex_id);
	sceGxmShaderPatcherUnregisterProgram(gxm_shader_patcher, clear_fragment_id);
	sceGxmShaderPatcherUnregisterProgram(gxm_shader_patcher, rgb_vertex_id);
	sceGxmShaderPatcherUnregisterProgram(gxm_shader_patcher, rgba_vertex_id);
	sceGxmShaderPatcherUnregisterProgram(gxm_shader_patcher, rgba_fragment_id);
	sceGxmShaderPatcherUnregisterProgram(gxm_shader_patcher, texture2d_vertex_id);
	sceGxmShaderPatcherUnregisterProgram(gxm_shader_patcher, texture2d_fragment_id);
	sceGxmShaderPatcherUnregisterProgram(gxm_shader_patcher, disable_color_buffer_fragment_id);

	// Terminating shader patcher
	stopShaderPatcher();

	// Deallocating depth and stencil surfaces for display
	termDepthStencilSurfaces();

	// Terminating display's color surfaces
	termDisplayColorSurfaces();

	// Destroing display's render target
	destroyDisplayRenderTarget();

	// Terminating sceGxm context
	termGxmContext();

	// Terminating sceGxm
	sceGxmTerminate();
}

void vglWaitVblankStart(GLboolean enable) {
	vblank = enable;
}

// openGL implementation

void glGenBuffers(GLsizei n, GLuint *res) {
	int i = 0, j = 0;
#ifndef SKIP_ERROR_HANDLING
	if (n < 0) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	for (i = 0; i < BUFFERS_NUM; i++) {
		if (buffers[i] != 0x0000) {
			res[j++] = buffers[i];
			buffers[i] = 0x0000;
		}
		if (j >= n)
			break;
	}
}

void glBindBuffer(GLenum target, GLuint buffer) {
#ifndef SKIP_ERROR_HANDLING
	if ((buffer != 0x0000) && ((buffer >= BUFFERS_ADDR + BUFFERS_NUM) || (buffer < BUFFERS_ADDR))) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	switch (target) {
	case GL_ARRAY_BUFFER:
		vertex_array_unit = buffer - BUFFERS_ADDR;
		break;
	case GL_ELEMENT_ARRAY_BUFFER:
		index_array_unit = buffer - BUFFERS_ADDR;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
}

void glDeleteBuffers(GLsizei n, const GLuint *gl_buffers) {
#ifndef SKIP_ERROR_HANDLING
	if (n < 0) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	int i, j;
	for (j = 0; j < n; j++) {
		if (gl_buffers[j] >= BUFFERS_ADDR && gl_buffers[j] < (BUFFERS_ADDR + BUFFERS_NUM)) {
			uint8_t idx = gl_buffers[j] - BUFFERS_ADDR;
			buffers[idx] = gl_buffers[j];
			if (gpu_buffers[idx].ptr != NULL) {
				vitagl_mempool_free(gpu_buffers[idx].ptr, VGL_MEM_VRAM);
				gpu_buffers[idx].ptr = NULL;
			}
		}
	}
}

void glBufferData(GLenum target, GLsizei size, const GLvoid *data, GLenum usage) {
#ifndef SKIP_ERROR_HANDLING
	if (size < 0) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	int idx = 0;
	switch (target) {
	case GL_ARRAY_BUFFER:
		idx = vertex_array_unit;
		break;
	case GL_ELEMENT_ARRAY_BUFFER:
		idx = index_array_unit;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	vglMemType type = VGL_MEM_VRAM;
	gpu_buffers[idx].ptr = gpu_alloc_mapped(size, &type);
	memcpy(gpu_buffers[idx].ptr, data, size);
}

void glBlendFunc(GLenum sfactor, GLenum dfactor) {
	switch (sfactor) {
	case GL_ZERO:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ZERO;
		break;
	case GL_ONE:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE;
		break;
	case GL_SRC_COLOR:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_SRC_COLOR;
		break;
	case GL_ONE_MINUS_SRC_COLOR:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		break;
	case GL_DST_COLOR:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_DST_COLOR;
		break;
	case GL_ONE_MINUS_DST_COLOR:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		break;
	case GL_SRC_ALPHA:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
		break;
	case GL_ONE_MINUS_SRC_ALPHA:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case GL_DST_ALPHA:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_DST_ALPHA;
		break;
	case GL_ONE_MINUS_DST_ALPHA:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case GL_SRC_ALPHA_SATURATE:
		blend_sfactor_rgb = blend_sfactor_a = SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	switch (dfactor) {
	case GL_ZERO:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ZERO;
		break;
	case GL_ONE:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE;
		break;
	case GL_SRC_COLOR:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_SRC_COLOR;
		break;
	case GL_ONE_MINUS_SRC_COLOR:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		break;
	case GL_DST_COLOR:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_DST_COLOR;
		break;
	case GL_ONE_MINUS_DST_COLOR:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		break;
	case GL_SRC_ALPHA:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
		break;
	case GL_ONE_MINUS_SRC_ALPHA:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case GL_DST_ALPHA:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_DST_ALPHA;
		break;
	case GL_ONE_MINUS_DST_ALPHA:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case GL_SRC_ALPHA_SATURATE:
		blend_dfactor_rgb = blend_dfactor_a = SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	if (blend_state)
		change_blend_factor();
}

void glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
	switch (srcRGB) {
	case GL_ZERO:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_ZERO;
		break;
	case GL_ONE:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE;
		break;
	case GL_SRC_COLOR:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_SRC_COLOR;
		break;
	case GL_ONE_MINUS_SRC_COLOR:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		break;
	case GL_DST_COLOR:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_DST_COLOR;
		break;
	case GL_ONE_MINUS_DST_COLOR:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		break;
	case GL_SRC_ALPHA:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
		break;
	case GL_ONE_MINUS_SRC_ALPHA:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case GL_DST_ALPHA:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_DST_ALPHA;
		break;
	case GL_ONE_MINUS_DST_ALPHA:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case GL_SRC_ALPHA_SATURATE:
		blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	switch (dstRGB) {
	case GL_ZERO:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_ZERO;
		break;
	case GL_ONE:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE;
		break;
	case GL_SRC_COLOR:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_SRC_COLOR;
		break;
	case GL_ONE_MINUS_SRC_COLOR:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		break;
	case GL_DST_COLOR:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_DST_COLOR;
		break;
	case GL_ONE_MINUS_DST_COLOR:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		break;
	case GL_SRC_ALPHA:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
		break;
	case GL_ONE_MINUS_SRC_ALPHA:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case GL_DST_ALPHA:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_DST_ALPHA;
		break;
	case GL_ONE_MINUS_DST_ALPHA:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case GL_SRC_ALPHA_SATURATE:
		blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	switch (srcAlpha) {
	case GL_ZERO:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ZERO;
		break;
	case GL_ONE:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE;
		break;
	case GL_SRC_COLOR:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_SRC_COLOR;
		break;
	case GL_ONE_MINUS_SRC_COLOR:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		break;
	case GL_DST_COLOR:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_DST_COLOR;
		break;
	case GL_ONE_MINUS_DST_COLOR:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		break;
	case GL_SRC_ALPHA:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
		break;
	case GL_ONE_MINUS_SRC_ALPHA:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case GL_DST_ALPHA:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_DST_ALPHA;
		break;
	case GL_ONE_MINUS_DST_ALPHA:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case GL_SRC_ALPHA_SATURATE:
		blend_sfactor_a = SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	switch (dstAlpha) {
	case GL_ZERO:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ZERO;
		break;
	case GL_ONE:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE;
		break;
	case GL_SRC_COLOR:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_SRC_COLOR;
		break;
	case GL_ONE_MINUS_SRC_COLOR:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		break;
	case GL_DST_COLOR:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_DST_COLOR;
		break;
	case GL_ONE_MINUS_DST_COLOR:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		break;
	case GL_SRC_ALPHA:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
		break;
	case GL_ONE_MINUS_SRC_ALPHA:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case GL_DST_ALPHA:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_DST_ALPHA;
		break;
	case GL_ONE_MINUS_DST_ALPHA:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case GL_SRC_ALPHA_SATURATE:
		blend_dfactor_a = SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	if (blend_state)
		change_blend_factor();
}

void glBlendEquation(GLenum mode) {
	switch (mode) {
	case GL_FUNC_ADD:
		blend_func_rgb = blend_func_a = SCE_GXM_BLEND_FUNC_ADD;
		break;
	case GL_FUNC_SUBTRACT:
		blend_func_rgb = blend_func_a = SCE_GXM_BLEND_FUNC_SUBTRACT;
		break;
	case GL_FUNC_REVERSE_SUBTRACT:
		blend_func_rgb = blend_func_a = SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT;
		break;
	case GL_MIN:
		blend_func_rgb = blend_func_a = SCE_GXM_BLEND_FUNC_MIN;
		break;
	case GL_MAX:
		blend_func_rgb = blend_func_a = SCE_GXM_BLEND_FUNC_MAX;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	if (blend_state)
		change_blend_factor();
}

void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
	switch (modeRGB) {
	case GL_FUNC_ADD:
		blend_func_rgb = SCE_GXM_BLEND_FUNC_ADD;
		break;
	case GL_FUNC_SUBTRACT:
		blend_func_rgb = SCE_GXM_BLEND_FUNC_SUBTRACT;
		break;
	case GL_FUNC_REVERSE_SUBTRACT:
		blend_func_rgb = SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT;
		break;
	case GL_MIN:
		blend_func_rgb = SCE_GXM_BLEND_FUNC_MIN;
		break;
	case GL_MAX:
		blend_func_rgb = SCE_GXM_BLEND_FUNC_MAX;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	switch (modeAlpha) {
	case GL_FUNC_ADD:
		blend_func_a = SCE_GXM_BLEND_FUNC_ADD;
		break;
	case GL_FUNC_SUBTRACT:
		blend_func_a = SCE_GXM_BLEND_FUNC_SUBTRACT;
		break;
	case GL_FUNC_REVERSE_SUBTRACT:
		blend_func_a = SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT;
		break;
	case GL_MIN:
		blend_func_a = SCE_GXM_BLEND_FUNC_MIN;
		break;
	case GL_MAX:
		blend_func_a = SCE_GXM_BLEND_FUNC_MAX;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	if (blend_state)
		change_blend_factor();
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
	blend_color_mask = SCE_GXM_COLOR_MASK_NONE;
	if (red)
		blend_color_mask += SCE_GXM_COLOR_MASK_R;
	if (green)
		blend_color_mask += SCE_GXM_COLOR_MASK_G;
	if (blue)
		blend_color_mask += SCE_GXM_COLOR_MASK_B;
	if (alpha)
		blend_color_mask += SCE_GXM_COLOR_MASK_A;
	if (blend_state)
		change_blend_factor();
	else
		change_blend_mask();
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	if ((stride < 0) || ((size < 2) && (size > 4))) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	switch (type) {
	case GL_FLOAT:
		tex_unit->vertex_array.size = sizeof(GLfloat);
		break;
	case GL_SHORT:
		tex_unit->vertex_array.size = sizeof(GLshort);
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}

	tex_unit->vertex_array.num = size;
	tex_unit->vertex_array.stride = stride;
	tex_unit->vertex_array.pointer = pointer;
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	if ((stride < 0) || ((size < 3) && (size > 4))) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	switch (type) {
	case GL_FLOAT:
		tex_unit->color_array.size = sizeof(GLfloat);
		break;
	case GL_SHORT:
		tex_unit->color_array.size = sizeof(GLshort);
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}

	tex_unit->color_array.num = size;
	tex_unit->color_array.stride = stride;
	tex_unit->color_array.pointer = pointer;
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	if ((stride < 0) || ((size < 2) && (size > 4))) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	switch (type) {
	case GL_FLOAT:
		tex_unit->texture_array.size = sizeof(GLfloat);
		break;
	case GL_SHORT:
		tex_unit->texture_array.size = sizeof(GLshort);
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}

	tex_unit->texture_array.num = size;
	tex_unit->texture_array.stride = stride;
	tex_unit->texture_array.pointer = pointer;
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	SceGxmPrimitiveType gxm_p;
	GLboolean skip_draw = GL_FALSE;
	if (tex_unit->vertex_array_state) {
		switch (mode) {
		case GL_POINTS:
			gxm_p = SCE_GXM_PRIMITIVE_POINTS;
			break;
		case GL_LINES:
			gxm_p = SCE_GXM_PRIMITIVE_LINES;
			if ((count % 2) != 0)
				skip_draw = GL_TRUE;
			break;
		case GL_TRIANGLES:
			gxm_p = SCE_GXM_PRIMITIVE_TRIANGLES;
			if (no_polygons_mode)
				skip_draw = GL_TRUE;
			else if ((count % 3) != 0)
				skip_draw = GL_TRUE;
			break;
		case GL_TRIANGLE_STRIP:
			gxm_p = SCE_GXM_PRIMITIVE_TRIANGLE_STRIP;
			if (no_polygons_mode)
				skip_draw = GL_TRUE;
			break;
		case GL_TRIANGLE_FAN:
			gxm_p = SCE_GXM_PRIMITIVE_TRIANGLE_FAN;
			if (no_polygons_mode)
				skip_draw = GL_TRUE;
			break;
		default:
			_vitagl_error = GL_INVALID_ENUM;
			break;
		}
		if (!skip_draw) {
			if (mvp_modified) {
				matrix4x4_multiply(mvp_matrix, projection_matrix, modelview_matrix);
				mvp_modified = GL_FALSE;
			}

			if (tex_unit->texture_array_state) {
				if (!(tex_unit->textures[texture2d_idx].valid))
					return;
				if (tex_unit->color_array_state) {
					sceGxmSetVertexProgram(gxm_context, texture2d_rgba_vertex_program_patched);
					sceGxmSetFragmentProgram(gxm_context, texture2d_rgba_fragment_program_patched);
					void *alpha_buffer;
					sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &alpha_buffer);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_alpha_cut, 0, 1, &alpha_ref);
					float alpha_operation = (float)alpha_op;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_alpha_op, 0, 1, &alpha_operation);
					float env_mode = (float)tex_unit->env_mode;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_tex_env, 0, 1, &env_mode);
					float fogmode = (float)internal_fog_mode;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_fog_mode, 0, 1, &fogmode);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_fog_color, 0, 4, &fog_color.r);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_tex_env_color, 0, 4, &texenv_color.r);
				} else {
					sceGxmSetVertexProgram(gxm_context, texture2d_vertex_program_patched);
					sceGxmSetFragmentProgram(gxm_context, texture2d_fragment_program_patched);
					void *alpha_buffer;
					sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &alpha_buffer);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_alpha_cut, 0, 1, &alpha_ref);
					float alpha_operation = (float)alpha_op;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_alpha_op, 0, 1, &alpha_operation);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_tint_color, 0, 4, &current_color.r);
					float env_mode = (float)tex_unit->env_mode;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_tex_env, 0, 1, &env_mode);
					float fogmode = (float)internal_fog_mode;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_fog_mode, 0, 1, &fogmode);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_fog_color, 0, 4, &fog_color.r);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_tex_env_color, 0, 4, &texenv_color.r);
				}
			} else if (tex_unit->color_array_state && (tex_unit->color_array.num == 3)) {
				sceGxmSetVertexProgram(gxm_context, rgb_vertex_program_patched);
				sceGxmSetFragmentProgram(gxm_context, rgba_fragment_program_patched);
			} else {
				sceGxmSetVertexProgram(gxm_context, rgba_vertex_program_patched);
				sceGxmSetFragmentProgram(gxm_context, rgba_fragment_program_patched);
			}

			void *vertex_wvp_buffer;
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vertex_wvp_buffer);

			if (tex_unit->texture_array_state) {
				float fogmode = (float)internal_fog_mode;
				if (tex_unit->color_array_state) {
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_wvp, 0, 16, (const float *)mvp_matrix);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_mode, 0, 1, (const float *)&fogmode);
					float clipplane0 = (float)clip_plane0;
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_clip_plane0, 0, 1, &clipplane0);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_clip_plane0_eq, 0, 4, &clip_plane0_eq.x);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_mv, 0, 16, (const float *)modelview_matrix);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_near, 0, 1, (const float *)&fog_near);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_far, 0, 1, (const float *)&fog_far);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_density, 0, 1, (const float *)&fog_density);
				} else {
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_wvp, 0, 16, (const float *)mvp_matrix);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_mode2, 0, 1, (const float *)&fogmode);
					float clipplane0 = (float)clip_plane0;
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_clip_plane0, 0, 1, &clipplane0);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_clip_plane0_eq, 0, 4, &clip_plane0_eq.x);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_mv, 0, 16, (const float *)modelview_matrix);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_near, 0, 1, (const float *)&fog_near);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_far, 0, 1, (const float *)&fog_far);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_density, 0, 1, (const float *)&fog_density);
				}
				sceGxmSetFragmentTexture(gxm_context, 0, &tex_unit->textures[texture2d_idx].gxm_tex);
				vector3f *vertices = NULL;
				vector2f *uv_map = NULL;
				vector4f *colors = NULL;
				uint16_t *indices;
				uint16_t n;
				if (vertex_array_unit >= 0) {
					if (tex_unit->vertex_array.stride == 0)
						vertices = (vector3f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer) + (first * (tex_unit->vertex_array.num * tex_unit->vertex_array.size)));
					else
						vertices = (vector3f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer) + (first * tex_unit->vertex_array.stride));
					if (tex_unit->texture_array.stride == 0)
						uv_map = (vector2f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->texture_array.pointer) + (first * (tex_unit->texture_array.num * tex_unit->texture_array.size)));
					else
						uv_map = (vector2f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->texture_array.pointer) + (first * tex_unit->texture_array.stride));
					if (tex_unit->color_array_state) {
						if (tex_unit->color_array.stride == 0)
							colors = (vector4f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->color_array.pointer) + (first * (tex_unit->color_array.num * tex_unit->color_array.size)));
						else
							colors = (vector4f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->color_array.pointer) + (first * tex_unit->color_array.stride));
					}
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					for (n = 0; n < count; n++) {
						indices[n] = n;
					}
				} else {
					uint8_t *ptr;
					uint8_t *ptr_tex;
					uint8_t *ptr_clr;
					vertices = (vector3f *)gpu_pool_memalign(count * sizeof(vector3f), sizeof(vector3f));
					uv_map = (vector2f *)gpu_pool_memalign(count * sizeof(vector2f), sizeof(vector2f));
					if (tex_unit->color_array_state)
						colors = (vector4f *)gpu_pool_memalign(count * sizeof(vector4f), sizeof(vector4f));
					memset(vertices, 0, (count * sizeof(vector3f)));
					uint8_t vec_set = 0, tex_set = 0, clr_set = 0;
					if (tex_unit->vertex_array.stride == 0) {
						ptr = ((uint8_t *)tex_unit->vertex_array.pointer) + (first * (tex_unit->vertex_array.num * tex_unit->vertex_array.size));
						memcpy(&vertices[n], ptr, count * sizeof(vector3f));
						vec_set = 1;
					} else
						ptr = ((uint8_t *)tex_unit->vertex_array.pointer) + (first * tex_unit->vertex_array.stride);
					if (tex_unit->texture_array.stride == 0) {
						ptr_tex = ((uint8_t *)tex_unit->texture_array.pointer) + (first * (tex_unit->texture_array.num * tex_unit->texture_array.size));
						memcpy(&uv_map[n], ptr_tex, count * sizeof(vector2f));
						tex_set = 1;
					} else
						ptr_tex = ((uint8_t *)tex_unit->texture_array.pointer) + (first * tex_unit->texture_array.stride);
					if (tex_unit->color_array_state) {
						if (tex_unit->color_array.stride == 0) {
							ptr_clr = ((uint8_t *)tex_unit->color_array.pointer) + (first * sizeof(vector4f));
							memcpy(&colors[n], ptr_clr, count * sizeof(vector4f));
							clr_set = 1;
						} else
							ptr_clr = ((uint8_t *)tex_unit->color_array.pointer) + (first * tex_unit->color_array.stride);
					}
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					for (n = 0; n < count; n++) {
						if (!vec_set) {
							memcpy(&vertices[n], ptr, tex_unit->vertex_array.size * tex_unit->vertex_array.num);
							ptr += tex_unit->vertex_array.stride;
						}
						if (!tex_set) {
							memcpy(&uv_map[n], ptr_tex, tex_unit->texture_array.size * tex_unit->texture_array.num);
							ptr_tex += tex_unit->texture_array.stride;
						}
						if (tex_unit->color_array_state && (!clr_set)) {
							memcpy(&colors[n], ptr_clr, tex_unit->color_array.size * tex_unit->color_array.num);
							ptr_clr += tex_unit->color_array.stride;
						}
						indices[n] = n;
					}
				}
				sceGxmSetVertexStream(gxm_context, 0, vertices);
				sceGxmSetVertexStream(gxm_context, 1, uv_map);
				if (tex_unit->color_array_state)
					sceGxmSetVertexStream(gxm_context, 2, colors);
				sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, indices, count);
			} else if (tex_unit->color_array_state) {
				if (tex_unit->color_array.num == 3)
					sceGxmSetUniformDataF(vertex_wvp_buffer, rgb_wvp, 0, 16, (const float *)mvp_matrix);
				else
					sceGxmSetUniformDataF(vertex_wvp_buffer, rgba_wvp, 0, 16, (const float *)mvp_matrix);
				vector3f *vertices = NULL;
				uint8_t *colors = NULL;
				uint16_t *indices;
				uint16_t n = 0;
				if (vertex_array_unit >= 0) {
					if (tex_unit->vertex_array.stride == 0)
						vertices = (vector3f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer) + (first * (tex_unit->vertex_array.num * tex_unit->vertex_array.size)));
					else
						vertices = (vector3f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer) + (first * tex_unit->vertex_array.stride));
					if (tex_unit->color_array.stride == 0)
						colors = (uint8_t *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->color_array.pointer) + (first * (tex_unit->color_array.num * tex_unit->color_array.size)));
					else
						colors = (uint8_t *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->color_array.pointer) + (first * tex_unit->color_array.stride));
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					for (n = 0; n < count; n++) {
						indices[n] = n;
					}
				} else {
					uint8_t *ptr;
					uint8_t *ptr_clr;
					vertices = (vector3f *)gpu_pool_memalign(count * sizeof(vector3f), sizeof(vector3f));
					colors = (uint8_t *)gpu_pool_memalign(count * tex_unit->color_array.num * tex_unit->color_array.size, tex_unit->color_array.num * tex_unit->color_array.size);
					memset(vertices, 0, (count * sizeof(vector3f)));
					uint8_t vec_set = 0, clr_set = 0;
					if (tex_unit->vertex_array.stride == 0) {
						ptr = ((uint8_t *)tex_unit->vertex_array.pointer) + (first * ((tex_unit->vertex_array.num * tex_unit->vertex_array.size)));
						memcpy(&vertices[n], ptr, count * sizeof(vector3f));
						vec_set = 1;
					} else
						ptr = ((uint8_t *)tex_unit->vertex_array.pointer) + (first * (tex_unit->vertex_array.stride));
					if (tex_unit->color_array.stride == 0) {
						ptr_clr = ((uint8_t *)tex_unit->color_array.pointer) + (first * ((tex_unit->color_array.num * tex_unit->color_array.size)));
						memcpy(&colors[n], ptr_clr, count * tex_unit->color_array.num * tex_unit->color_array.size);
						clr_set = 1;
					} else
						ptr_clr = ((uint8_t *)tex_unit->color_array.pointer) + (first * tex_unit->color_array.size);
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					for (n = 0; n < count; n++) {
						if (!vec_set) {
							memcpy(&vertices[n], ptr, tex_unit->vertex_array.size * tex_unit->vertex_array.num);
							ptr += tex_unit->vertex_array.stride;
						}
						if (!clr_set) {
							memcpy(&colors[n * tex_unit->color_array.num * tex_unit->color_array.size], ptr_clr, tex_unit->color_array.size * tex_unit->color_array.num);
							ptr_clr += tex_unit->color_array.stride;
						}
						indices[n] = n;
					}
				}
				sceGxmSetVertexStream(gxm_context, 0, vertices);
				sceGxmSetVertexStream(gxm_context, 1, colors);
				sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, indices, count);
			} else {
				sceGxmSetUniformDataF(vertex_wvp_buffer, rgba_wvp, 0, 16, (const float *)mvp_matrix);
				vector3f *vertices = NULL;
				vector4f *colors = NULL;
				uint16_t *indices;
				uint16_t n = 0;
				if (vertex_array_unit >= 0) {
					if (tex_unit->vertex_array.stride == 0)
						vertices = (vector3f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer) + (first * (tex_unit->vertex_array.num * tex_unit->vertex_array.size)));
					else
						vertices = (vector3f *)(((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer) + (first * tex_unit->vertex_array.stride));
					colors = (vector4f *)gpu_pool_memalign(count * sizeof(vector4f), sizeof(vector4f));
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					for (n = 0; n < count; n++) {
						memcpy(&colors[n], &current_color.r, sizeof(vector4f));
						indices[n] = n;
					}
				} else {
					uint8_t *ptr;
					vertices = (vector3f *)gpu_pool_memalign(count * sizeof(vector3f), sizeof(vector3f));
					colors = (vector4f *)gpu_pool_memalign(count * sizeof(vector4f), sizeof(vector4f));
					memset(vertices, 0, (count * sizeof(vector3f)));
					uint8_t vec_set = 0;
					if (tex_unit->vertex_array.stride == 0) {
						ptr = ((uint8_t *)tex_unit->vertex_array.pointer) + (first * ((tex_unit->vertex_array.num * tex_unit->vertex_array.size)));
						memcpy(&vertices[n], ptr, count * sizeof(vector3f));
						vec_set = 1;
					} else
						ptr = ((uint8_t *)tex_unit->vertex_array.pointer) + (first * (tex_unit->vertex_array.stride));
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					for (n = 0; n < count; n++) {
						if (!vec_set) {
							memcpy(&vertices[n], ptr, tex_unit->vertex_array.size * tex_unit->vertex_array.num);
							ptr += tex_unit->vertex_array.stride;
						}
						memcpy(&colors[n], &current_color.r, sizeof(vector4f));
						indices[n] = n;
					}
				}
				sceGxmSetVertexStream(gxm_context, 0, vertices);
				sceGxmSetVertexStream(gxm_context, 1, colors);
				sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, indices, count);
			}
		}
	}
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *gl_indices) {
	SceGxmPrimitiveType gxm_p;
	SceGxmPrimitiveTypeExtra gxm_ep = SCE_GXM_PRIMITIVE_NONE;
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
	GLboolean skip_draw = GL_FALSE;
	if (tex_unit->vertex_array_state) {
#ifndef SKIP_ERROR_HANDLING
		if (type != GL_UNSIGNED_SHORT)
			_vitagl_error = GL_INVALID_ENUM;
		else if (phase == MODEL_CREATION)
			_vitagl_error = GL_INVALID_OPERATION;
		else if (count < 0)
			_vitagl_error = GL_INVALID_VALUE;
#endif
		switch (mode) {
		case GL_POINTS:
			gxm_p = SCE_GXM_PRIMITIVE_POINTS;
			break;
		case GL_LINES:
			gxm_p = SCE_GXM_PRIMITIVE_LINES;
			break;
		case GL_TRIANGLES:
			gxm_p = SCE_GXM_PRIMITIVE_TRIANGLES;
			if (no_polygons_mode)
				skip_draw = GL_TRUE;
			break;
		case GL_TRIANGLE_STRIP:
			gxm_p = SCE_GXM_PRIMITIVE_TRIANGLE_STRIP;
			if (no_polygons_mode)
				skip_draw = GL_TRUE;
			break;
		case GL_TRIANGLE_FAN:
			gxm_p = SCE_GXM_PRIMITIVE_TRIANGLE_FAN;
			if (no_polygons_mode)
				skip_draw = GL_TRUE;
			break;
		default:
			_vitagl_error = GL_INVALID_ENUM;
			break;
		}
		if (!skip_draw) {
			if (mvp_modified) {
				matrix4x4_multiply(mvp_matrix, projection_matrix, modelview_matrix);
				mvp_modified = GL_FALSE;
			}

			if (tex_unit->texture_array_state) {
				if (!(tex_unit->textures[texture2d_idx].valid))
					return;
				if (tex_unit->color_array_state) {
					sceGxmSetVertexProgram(gxm_context, texture2d_rgba_vertex_program_patched);
					sceGxmSetFragmentProgram(gxm_context, texture2d_rgba_fragment_program_patched);
					void *alpha_buffer;
					sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &alpha_buffer);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_alpha_cut, 0, 1, &alpha_ref);
					float alpha_operation = (float)alpha_op;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_alpha_op, 0, 1, &alpha_operation);
					float env_mode = (float)tex_unit->env_mode;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_tex_env, 0, 1, &env_mode);
					float fogmode = (float)internal_fog_mode;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_fog_mode, 0, 1, &fogmode);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_fog_color, 0, 4, &fog_color.r);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_tex_env_color, 0, 4, &texenv_color.r);
				} else {
					sceGxmSetVertexProgram(gxm_context, texture2d_vertex_program_patched);
					sceGxmSetFragmentProgram(gxm_context, texture2d_fragment_program_patched);
					void *alpha_buffer;
					sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &alpha_buffer);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_alpha_cut, 0, 1, &alpha_ref);
					float alpha_operation = (float)alpha_op;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_alpha_op, 0, 1, &alpha_operation);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_tint_color, 0, 4, &current_color.r);
					float env_mode = (float)tex_unit->env_mode;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_tex_env, 0, 1, &env_mode);
					float fogmode = (float)internal_fog_mode;
					sceGxmSetUniformDataF(alpha_buffer, texture2d_fog_mode, 0, 1, &fogmode);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_fog_color, 0, 4, &fog_color.r);
					sceGxmSetUniformDataF(alpha_buffer, texture2d_tex_env_color, 0, 4, &texenv_color.r);
				}
			} else if (tex_unit->color_array_state && (tex_unit->color_array.num == 3)) {
				sceGxmSetVertexProgram(gxm_context, rgb_vertex_program_patched);
				sceGxmSetFragmentProgram(gxm_context, rgba_fragment_program_patched);
			} else {
				sceGxmSetVertexProgram(gxm_context, rgba_vertex_program_patched);
				sceGxmSetFragmentProgram(gxm_context, rgba_fragment_program_patched);
			}

			void *vertex_wvp_buffer;
			sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vertex_wvp_buffer);

			if (tex_unit->texture_array_state) {
				float fogmode = (float)internal_fog_mode;
				if (tex_unit->color_array_state) {
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_wvp, 0, 16, (const float *)mvp_matrix);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_mode, 0, 1, (const float *)&fogmode);
					float clipplane0 = (float)clip_plane0;
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_clip_plane0, 0, 1, &clipplane0);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_clip_plane0_eq, 0, 4, &clip_plane0_eq.x);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_mv, 0, 16, (const float *)modelview_matrix);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_near, 0, 1, (const float *)&fog_near);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_far, 0, 1, (const float *)&fog_far);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_density, 0, 1, (const float *)&fog_density);
				} else {
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_wvp, 0, 16, (const float *)mvp_matrix);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_mode2, 0, 1, (const float *)&fogmode);
					float clipplane0 = (float)clip_plane0;
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_clip_plane0, 0, 1, &clipplane0);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_clip_plane0_eq, 0, 4, &clip_plane0_eq.x);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_mv, 0, 16, (const float *)modelview_matrix);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_near, 0, 1, (const float *)&fog_near);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_far, 0, 1, (const float *)&fog_far);
					sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_density, 0, 1, (const float *)&fog_density);
				}
				sceGxmSetFragmentTexture(gxm_context, 0, &texture_units[client_texture_unit].textures[texture2d_idx].gxm_tex);
				vector3f *vertices = NULL;
				vector2f *uv_map = NULL;
				vector4f *colors = NULL;
				uint16_t *indices;
				if (index_array_unit >= 0)
					indices = (uint16_t *)((uint32_t)gpu_buffers[index_array_unit].ptr + (uint32_t)gl_indices);
				else {
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					memcpy(indices, gl_indices, sizeof(uint16_t) * count);
				}
				if (vertex_array_unit >= 0) {
					vertices = (vector3f *)((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer);
					uv_map = (vector2f *)((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->texture_array.pointer);
					if (tex_unit->color_array_state)
						colors = (vector4f *)((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->color_array.pointer);
				} else {
					int n = 0, j = 0;
					uint64_t vertex_count_int = 0;
					uint16_t *ptr_idx = (uint16_t *)gl_indices;
					while (j < count) {
						if (ptr_idx[j] >= vertex_count_int)
							vertex_count_int = ptr_idx[j] + 1;
						j++;
					}
					vertices = (vector3f *)gpu_pool_memalign(vertex_count_int * sizeof(vector3f), sizeof(vector3f));
					uv_map = (vector2f *)gpu_pool_memalign(vertex_count_int * sizeof(vector2f), sizeof(vector2f));
					colors = (vector4f *)gpu_pool_memalign(vertex_count_int * sizeof(vector4f), sizeof(vector4f));
					if (tex_unit->vertex_array.stride == 0)
						memcpy(vertices, tex_unit->vertex_array.pointer, vertex_count_int * (tex_unit->vertex_array.size * tex_unit->vertex_array.num));
					if (tex_unit->texture_array.stride == 0)
						memcpy(uv_map, tex_unit->texture_array.pointer, vertex_count_int * (tex_unit->texture_array.size * tex_unit->texture_array.num));
					if (tex_unit->color_array_state && (tex_unit->color_array.stride == 0))
						memcpy(colors, tex_unit->color_array.pointer, vertex_count_int * (tex_unit->color_array.size * tex_unit->color_array.num));
					if ((tex_unit->vertex_array.stride != 0) || (tex_unit->texture_array.stride != 0)) {
						if (tex_unit->vertex_array.stride != 0)
							memset(vertices, 0, (vertex_count_int * sizeof(texture2d_vertex)));
						uint8_t *ptr = ((uint8_t *)tex_unit->vertex_array.pointer);
						uint8_t *ptr_tex = ((uint8_t *)tex_unit->texture_array.pointer);
						for (n = 0; n < vertex_count_int; n++) {
							if (tex_unit->vertex_array.stride != 0)
								memcpy(&vertices[n], ptr, tex_unit->vertex_array.size * tex_unit->vertex_array.num);
							if (tex_unit->texture_array.stride != 0)
								memcpy(&uv_map[n], ptr_tex, tex_unit->texture_array.size * tex_unit->texture_array.num);
							ptr += tex_unit->vertex_array.stride;
							ptr_tex += tex_unit->texture_array.stride;
						}
					}
				}
				sceGxmSetVertexStream(gxm_context, 0, vertices);
				sceGxmSetVertexStream(gxm_context, 1, uv_map);
				if (tex_unit->color_array_state)
					sceGxmSetVertexStream(gxm_context, 2, colors);
				sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, indices, count);
			} else if (tex_unit->color_array_state) {
				if (tex_unit->color_array.num == 3)
					sceGxmSetUniformDataF(vertex_wvp_buffer, rgb_wvp, 0, 16, (const float *)mvp_matrix);
				else
					sceGxmSetUniformDataF(vertex_wvp_buffer, rgba_wvp, 0, 16, (const float *)mvp_matrix);
				vector3f *vertices = NULL;
				uint8_t *colors = NULL;
				uint16_t *indices;
				if (index_array_unit >= 0)
					indices = (uint16_t *)((uint32_t)gpu_buffers[index_array_unit].ptr + (uint32_t)gl_indices);
				else {
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					memcpy(indices, gl_indices, sizeof(uint16_t) * count);
				}
				if (vertex_array_unit >= 0) {
					colors = (uint8_t *)((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->color_array.pointer);
					vertices = (vector3f *)((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer);
				} else {
					int n = 0, j = 0;
					uint64_t vertex_count_int = 0;
					uint16_t *ptr_idx = (uint16_t *)gl_indices;
					while (j < count) {
						if (ptr_idx[j] >= vertex_count_int)
							vertex_count_int = ptr_idx[j] + 1;
						j++;
					}
					vertices = (vector3f *)gpu_pool_memalign(vertex_count_int * sizeof(vector3f), sizeof(vector3f));
					colors = (uint8_t *)gpu_pool_memalign(vertex_count_int * tex_unit->color_array.num * tex_unit->color_array.size, tex_unit->color_array.num * tex_unit->color_array.size);
					if (tex_unit->vertex_array.stride == 0)
						memcpy(vertices, tex_unit->vertex_array.pointer, vertex_count_int * (tex_unit->vertex_array.size * tex_unit->vertex_array.num));
					if (tex_unit->color_array.stride == 0)
						memcpy(colors, tex_unit->color_array.pointer, vertex_count_int * (tex_unit->color_array.size * tex_unit->color_array.num));
					if ((tex_unit->vertex_array.stride != 0) || (tex_unit->color_array.stride != 0)) {
						if (tex_unit->vertex_array.stride != 0)
							memset(vertices, 0, (vertex_count_int * sizeof(texture2d_vertex)));
						uint8_t *ptr = ((uint8_t *)tex_unit->vertex_array.pointer);
						uint8_t *ptr_clr = ((uint8_t *)tex_unit->color_array.pointer);
						for (n = 0; n < vertex_count_int; n++) {
							if (tex_unit->vertex_array.stride != 0)
								memcpy(&vertices[n], ptr, tex_unit->vertex_array.size * tex_unit->vertex_array.num);
							if (tex_unit->color_array.stride != 0)
								memcpy(&colors[n * tex_unit->color_array.num * tex_unit->color_array.size], ptr_clr, tex_unit->color_array.size * tex_unit->color_array.num);
							ptr += tex_unit->vertex_array.stride;
							ptr_clr += tex_unit->color_array.stride;
						}
					}
				}
				sceGxmSetVertexStream(gxm_context, 0, vertices);
				sceGxmSetVertexStream(gxm_context, 1, colors);
				sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, indices, count);
			} else {
				sceGxmSetUniformDataF(vertex_wvp_buffer, rgba_wvp, 0, 16, (const float *)mvp_matrix);
				vector3f *vertices = NULL;
				vector4f *colors = NULL;
				uint16_t *indices;
				if (index_array_unit >= 0)
					indices = (uint16_t *)((uint32_t)gpu_buffers[index_array_unit].ptr + (uint32_t)gl_indices);
				else {
					indices = (uint16_t *)gpu_pool_memalign(count * sizeof(uint16_t), sizeof(uint16_t));
					memcpy(indices, gl_indices, sizeof(uint16_t) * count);
				}
				int n = 0, j = 0;
				uint64_t vertex_count_int = 0;
				uint16_t *ptr_idx = (uint16_t *)gl_indices;
				while (j < count) {
					if (ptr_idx[j] >= vertex_count_int)
						vertex_count_int = ptr_idx[j] + 1;
					j++;
				}
				if (vertex_array_unit >= 0)
					vertices = (vector3f *)((uint32_t)gpu_buffers[vertex_array_unit].ptr + (uint32_t)tex_unit->vertex_array.pointer);
				else
					vertices = (vector3f *)gpu_pool_memalign(vertex_count_int * sizeof(vector3f), sizeof(vector3f));
				colors = (vector4f *)gpu_pool_memalign(vertex_count_int * tex_unit->color_array.num * tex_unit->color_array.size, tex_unit->color_array.num * tex_unit->color_array.size);
				if ((!vertex_array_unit) && tex_unit->vertex_array.stride == 0)
					memcpy(vertices, tex_unit->vertex_array.pointer, vertex_count_int * (tex_unit->vertex_array.size * tex_unit->vertex_array.num));
				if ((!vertex_array_unit) && tex_unit->vertex_array.stride != 0)
					memset(vertices, 0, (vertex_count_int * sizeof(texture2d_vertex)));
				uint8_t *ptr = ((uint8_t *)tex_unit->vertex_array.pointer);
				for (n = 0; n < vertex_count_int; n++) {
					if ((!vertex_array_unit) && tex_unit->vertex_array.stride != 0)
						memcpy(&vertices[n], ptr, tex_unit->vertex_array.size * tex_unit->vertex_array.num);
					memcpy(&colors[n], &current_color.r, sizeof(vector4f));
					if (!vertex_array_unit)
						ptr += tex_unit->vertex_array.stride;
				}
				sceGxmSetVertexStream(gxm_context, 0, vertices);
				sceGxmSetVertexStream(gxm_context, 1, colors);
				sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, indices, count);
			}
		}
	}
}

void glEnableClientState(GLenum array) {
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	switch (array) {
	case GL_VERTEX_ARRAY:
		tex_unit->vertex_array_state = GL_TRUE;
		break;
	case GL_COLOR_ARRAY:
		tex_unit->color_array_state = GL_TRUE;
		break;
	case GL_TEXTURE_COORD_ARRAY:
		tex_unit->texture_array_state = GL_TRUE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
}

void glDisableClientState(GLenum array) {
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	switch (array) {
	case GL_VERTEX_ARRAY:
		tex_unit->vertex_array_state = GL_FALSE;
		break;
	case GL_COLOR_ARRAY:
		tex_unit->color_array_state = GL_FALSE;
		break;
	case GL_TEXTURE_COORD_ARRAY:
		tex_unit->texture_array_state = GL_FALSE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
}

void glClientActiveTexture(GLenum texture) {
#ifndef SKIP_ERROR_HANDLING
	if ((texture < GL_TEXTURE0) && (texture > GL_TEXTURE31))
		_vitagl_error = GL_INVALID_ENUM;
	else
#endif
		client_texture_unit = texture - GL_TEXTURE0;
}

// VGL_EXT_gpu_objects_array extension implementation

void vglVertexPointer(GLint size, GLenum type, GLsizei stride, GLuint count, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	if ((stride < 0) || ((size < 2) && (size > 4))) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	texture_unit *tex_unit = &texture_units[client_texture_unit];
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
	tex_unit->vertex_object = gpu_pool_memalign(count * bpe * size, bpe * size);
	if (stride == 0)
		memcpy(tex_unit->vertex_object, pointer, count * bpe * size);
	else {
		int i;
		uint8_t *dst = (uint8_t *)tex_unit->vertex_object;
		uint8_t *src = (uint8_t *)pointer;
		for (i = 0; i < count; i++) {
			memcpy(dst, src, bpe * size);
			dst += (bpe * size);
			src += stride;
		}
	}
}

void vglColorPointer(GLint size, GLenum type, GLsizei stride, GLuint count, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	if ((stride < 0) || ((size < 3) && (size > 4))) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	int bpe;
	switch (type) {
	case GL_FLOAT:
		bpe = sizeof(GLfloat);
		break;
	case GL_SHORT:
		bpe = sizeof(GLshort);
		break;
	case GL_UNSIGNED_BYTE:
		bpe = sizeof(uint8_t);
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	tex_unit->color_object = gpu_pool_memalign(count * bpe * size, bpe * size);
	tex_unit->color_object_type = type;
	if (stride == 0)
		memcpy(tex_unit->color_object, pointer, count * bpe * size);
	else {
		int i;
		uint8_t *dst = (uint8_t *)tex_unit->color_object;
		uint8_t *src = (uint8_t *)pointer;
		for (i = 0; i < count; i++) {
			memcpy(dst, src, bpe * size);
			dst += (bpe * size);
			src += stride;
		}
	}
}

void vglTexCoordPointer(GLint size, GLenum type, GLsizei stride, GLuint count, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	if ((stride < 0) || ((size < 2) && (size > 4))) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	texture_unit *tex_unit = &texture_units[client_texture_unit];
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
	tex_unit->texture_object = gpu_pool_memalign(count * bpe * size, bpe * size);
	if (stride == 0)
		memcpy(tex_unit->texture_object, pointer, count * bpe * size);
	else {
		int i;
		uint8_t *dst = (uint8_t *)tex_unit->texture_object;
		uint8_t *src = (uint8_t *)pointer;
		for (i = 0; i < count; i++) {
			memcpy(dst, src, bpe * size);
			dst += (bpe * size);
			src += stride;
		}
	}
}

void vglIndexPointer(GLenum type, GLsizei stride, GLuint count, const GLvoid *pointer) {
#ifndef SKIP_ERROR_HANDLING
	if (stride < 0) {
		_vitagl_error = GL_INVALID_VALUE;
		return;
	}
#endif
	texture_unit *tex_unit = &texture_units[client_texture_unit];
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
	tex_unit->index_object = gpu_pool_memalign(count * bpe, bpe);
	if (stride == 0)
		memcpy(tex_unit->index_object, pointer, count * bpe);
	else {
		int i;
		uint8_t *dst = (uint8_t *)tex_unit->index_object;
		uint8_t *src = (uint8_t *)pointer;
		for (i = 0; i < count; i++) {
			memcpy(dst, src, bpe);
			dst += bpe;
			src += stride;
		}
	}
}

void vglVertexPointerMapped(const GLvoid *pointer) {
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	tex_unit->vertex_object = (GLvoid *)pointer;
}

void vglColorPointerMapped(GLenum type, const GLvoid *pointer) {
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	tex_unit->color_object = (GLvoid *)pointer;
	tex_unit->color_object_type = type;
}

void vglTexCoordPointerMapped(const GLvoid *pointer) {
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	tex_unit->texture_object = (GLvoid *)pointer;
}

void vglIndexPointerMapped(const GLvoid *pointer) {
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	tex_unit->index_object = (GLvoid *)pointer;
}

void vglDrawObjects(GLenum mode, GLsizei count, GLboolean implicit_wvp) {
	SceGxmPrimitiveType gxm_p;
	texture_unit *tex_unit = &texture_units[client_texture_unit];
	int texture2d_idx = tex_unit->tex_id;
#ifndef SKIP_ERROR_HANDLING
	if (phase == MODEL_CREATION)
		_vitagl_error = GL_INVALID_OPERATION;
	else if (count < 0)
		_vitagl_error = GL_INVALID_VALUE;
#endif
	GLboolean skip_draw = GL_FALSE;
	switch (mode) {
	case GL_POINTS:
		gxm_p = SCE_GXM_PRIMITIVE_POINTS;
		break;
	case GL_LINES:
		gxm_p = SCE_GXM_PRIMITIVE_LINES;
		break;
	case GL_TRIANGLES:
		gxm_p = SCE_GXM_PRIMITIVE_TRIANGLES;
		if (no_polygons_mode)
			skip_draw = GL_TRUE;
		break;
	case GL_TRIANGLE_STRIP:
		gxm_p = SCE_GXM_PRIMITIVE_TRIANGLE_STRIP;
		if (no_polygons_mode)
			skip_draw = GL_TRUE;
		break;
	case GL_TRIANGLE_FAN:
		gxm_p = SCE_GXM_PRIMITIVE_TRIANGLE_FAN;
		if (no_polygons_mode)
			skip_draw = GL_TRUE;
		break;
	default:
		_vitagl_error = GL_INVALID_ENUM;
		break;
	}
	if (!skip_draw) {
		if (cur_program != 0) {
			_vglDrawObjects_CustomShadersIMPL(mode, count, implicit_wvp);
			sceGxmSetFragmentTexture(gxm_context, 0, &tex_unit->textures[texture2d_idx].gxm_tex);
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, tex_unit->index_object, count);
			vert_uniforms = NULL;
			frag_uniforms = NULL;
		} else {
			if (tex_unit->vertex_array_state) {
				if (mvp_modified) {
					matrix4x4_multiply(mvp_matrix, projection_matrix, modelview_matrix);
					mvp_modified = GL_FALSE;
				}
				if (tex_unit->texture_array_state) {
					if (!(tex_unit->textures[texture2d_idx].valid))
						return;
					if (tex_unit->color_array_state) {
						if (tex_unit->color_object_type == GL_FLOAT)
							sceGxmSetVertexProgram(gxm_context, texture2d_rgba_vertex_program_patched);
						else
							sceGxmSetVertexProgram(gxm_context, texture2d_rgba_u8n_vertex_program_patched);
						sceGxmSetFragmentProgram(gxm_context, texture2d_rgba_fragment_program_patched);
						void *alpha_buffer;
						sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &alpha_buffer);
						sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_alpha_cut, 0, 1, &alpha_ref);
						float alpha_operation = (float)alpha_op;
						sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_alpha_op, 0, 1, &alpha_operation);
						float env_mode = (float)tex_unit->env_mode;
						sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_tex_env, 0, 1, &env_mode);
						float fogmode = (float)internal_fog_mode;
						sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_fog_mode, 0, 1, &fogmode);
						sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_fog_color, 0, 4, &fog_color.r);
						sceGxmSetUniformDataF(alpha_buffer, texture2d_rgba_tex_env_color, 0, 4, &texenv_color.r);
					} else {
						sceGxmSetVertexProgram(gxm_context, texture2d_vertex_program_patched);
						sceGxmSetFragmentProgram(gxm_context, texture2d_fragment_program_patched);
						void *alpha_buffer;
						sceGxmReserveFragmentDefaultUniformBuffer(gxm_context, &alpha_buffer);
						sceGxmSetUniformDataF(alpha_buffer, texture2d_alpha_cut, 0, 1, &alpha_ref);
						float alpha_operation = (float)alpha_op;
						sceGxmSetUniformDataF(alpha_buffer, texture2d_alpha_op, 0, 1, &alpha_operation);
						sceGxmSetUniformDataF(alpha_buffer, texture2d_tint_color, 0, 4, &current_color.r);
						float env_mode = (float)tex_unit->env_mode;
						sceGxmSetUniformDataF(alpha_buffer, texture2d_tex_env, 0, 1, &env_mode);
						float fogmode = (float)internal_fog_mode;
						sceGxmSetUniformDataF(alpha_buffer, texture2d_fog_mode, 0, 1, &fogmode);
						sceGxmSetUniformDataF(alpha_buffer, texture2d_fog_color, 0, 4, &fog_color.r);
						sceGxmSetUniformDataF(alpha_buffer, texture2d_tint_color, 0, 4, &current_color.r);
						sceGxmSetUniformDataF(alpha_buffer, texture2d_tex_env_color, 0, 4, &texenv_color.r);
					}
				} else if (tex_unit->color_array_state && (tex_unit->color_array.num == 3)) {
					if (tex_unit->color_object_type == GL_FLOAT)
						sceGxmSetVertexProgram(gxm_context, rgb_vertex_program_patched);
					else
						sceGxmSetVertexProgram(gxm_context, rgb_u8n_vertex_program_patched);
					sceGxmSetFragmentProgram(gxm_context, rgba_fragment_program_patched);
				} else {
					if (tex_unit->color_object_type == GL_FLOAT)
						sceGxmSetVertexProgram(gxm_context, rgba_vertex_program_patched);
					else
						sceGxmSetVertexProgram(gxm_context, rgba_u8n_vertex_program_patched);
					sceGxmSetFragmentProgram(gxm_context, rgba_fragment_program_patched);
				}
				void *vertex_wvp_buffer;
				sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vertex_wvp_buffer);
				if (tex_unit->texture_array_state) {
					float fogmode = (float)internal_fog_mode;
					if (tex_unit->color_array_state) {
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_wvp, 0, 16, (const float *)mvp_matrix);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_mode2, 0, 1, (const float *)&fogmode);
						float clipplane0 = (float)clip_plane0;
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_clip_plane0, 0, 1, &clipplane0);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_clip_plane0_eq, 0, 4, &clip_plane0_eq.x);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_mv, 0, 16, (const float *)modelview_matrix);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_near, 0, 1, (const float *)&fog_near);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_far, 0, 1, (const float *)&fog_far);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_rgba_fog_density, 0, 1, (const float *)&fog_density);
					} else {
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_wvp, 0, 16, (const float *)mvp_matrix);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_mode2, 0, 1, (const float *)&fogmode);
						float clipplane0 = (float)clip_plane0;
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_clip_plane0, 0, 1, &clipplane0);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_clip_plane0_eq, 0, 4, &clip_plane0_eq.x);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_mv, 0, 16, (const float *)modelview_matrix);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_near, 0, 1, (const float *)&fog_near);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_far, 0, 1, (const float *)&fog_far);
						sceGxmSetUniformDataF(vertex_wvp_buffer, texture2d_fog_density, 0, 1, (const float *)&fog_density);
					}
					sceGxmSetFragmentTexture(gxm_context, 0, &tex_unit->textures[texture2d_idx].gxm_tex);
					sceGxmSetVertexStream(gxm_context, 0, tex_unit->vertex_object);
					sceGxmSetVertexStream(gxm_context, 1, tex_unit->texture_object);
					if (tex_unit->color_array_state)
						sceGxmSetVertexStream(gxm_context, 2, tex_unit->color_object);
					sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, tex_unit->index_object, count);
				} else if (tex_unit->color_array_state) {
					if (tex_unit->color_array.num == 3)
						sceGxmSetUniformDataF(vertex_wvp_buffer, rgb_wvp, 0, 16, (const float *)mvp_matrix);
					else
						sceGxmSetUniformDataF(vertex_wvp_buffer, rgba_wvp, 0, 16, (const float *)mvp_matrix);
					sceGxmSetVertexStream(gxm_context, 0, tex_unit->vertex_object);
					sceGxmSetVertexStream(gxm_context, 1, tex_unit->color_object);
					sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, tex_unit->index_object, count);
				} else {
					sceGxmSetUniformDataF(vertex_wvp_buffer, rgba_wvp, 0, 16, (const float *)mvp_matrix);
					vector4f *colors = (vector4f *)gpu_pool_memalign(count * sizeof(vector4f), sizeof(vector4f));
					int n;
					for (n = 0; n < count; n++) {
						memcpy(&colors[n], &current_color.r, sizeof(vector4f));
					}
					sceGxmSetVertexStream(gxm_context, 0, tex_unit->vertex_object);
					sceGxmSetVertexStream(gxm_context, 1, colors);
					sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, tex_unit->index_object, count);
				}
			}
		}
	}
}

size_t vglMemFree(vglMemType type) {
	if (type >= VGL_MEM_TYPE_COUNT)
		return 0;
	return vitagl_mempool_get_free_space(type);
}

void *vglAlloc(uint32_t size, vglMemType type) {
	if (type >= VGL_MEM_TYPE_COUNT)
		return NULL;
	return vitagl_mempool_alloc(size, type);
}

void vglFree(void *addr) {
	vitagl_mempool_free(addr, VGL_MEM_RAM); // Type is discarded so we just pass a random one
}
