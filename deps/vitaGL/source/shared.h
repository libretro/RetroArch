/* 
 * shared.h:
 * All functions/definitions that shouldn't be exposed to
 * end users but are used in multiple source files must be here
 */

#ifndef _SHARED_H_
#define _SHARED_H_

// Internal constants
#define TEXTURES_NUM 4096 // Available textures per texture unit
#define MODELVIEW_STACK_DEPTH 32 // Depth of modelview matrix stack
#define GENERIC_STACK_DEPTH 2 // Depth of generic matrix stack
#define DISPLAY_WIDTH_DEF 960 // Default display width in pixels
#define DISPLAY_HEIGHT_DEF 544 // Default display height in pixels
#define DISPLAY_BUFFER_COUNT 2 // Display buffers to use
#define GXM_TEX_MAX_SIZE 4096 // Maximum width/height in pixels per texture
#define BUFFERS_ADDR 0xA000 // Starting address for buffers indexing
#define BUFFERS_NUM 128 // Maximum number of allocatable buffers

// Internal constants set in bootup phase
extern int DISPLAY_WIDTH; // Display width in pixels
extern int DISPLAY_HEIGHT; // Display height in pixels
extern int DISPLAY_STRIDE; // Display stride in pixels
extern float DISPLAY_WIDTH_FLOAT; // Display width in pixels (float)
extern float DISPLAY_HEIGHT_FLOAT; // Display height in pixels (float)

#include <stdio.h>
#include <stdlib.h>
#include <vitasdk.h>

#include "vitaGL.h"

#include "utils/gpu_utils.h"
#include "utils/math_utils.h"
#include "utils/mem_utils.h"

#include "state.h"
#include "texture_callbacks.h"

// Texture environment mode
typedef enum texEnvMode {
	MODULATE = 0,
	DECAL = 1,
	BLEND = 2,
	ADD = 3,
	REPLACE = 4
} texEnvMode;

// 3D vertex for position + 4D vertex for RGBA color struct
typedef struct rgba_vertex {
	vector3f position;
	vector4f color;
} rgba_vertex;

// 3D vertex for position + 3D vertex for RGB color struct
typedef struct rgb_vertex {
	vector3f position;
	vector3f color;
} rgb_vertex;

// 3D vertex for position + 2D vertex for UV map struct
typedef struct texture2d_vertex {
	vector3f position;
	vector2f texcoord;
} texture2d_vertex;

// Non native primitives implemented
typedef enum SceGxmPrimitiveTypeExtra {
	SCE_GXM_PRIMITIVE_NONE = 0,
	SCE_GXM_PRIMITIVE_QUADS = 1
} SceGxmPrimitiveTypeExtra;

#include "shaders.h"

// Internal stuffs
extern void *frag_uniforms;
extern void *vert_uniforms;
extern SceGxmMultisampleMode msaa_mode;

// Debugging tool
#ifdef ENABLE_LOG
void LOG(const char *format, ...);
#endif

// Depending on SDK, that could be or not defined
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

// sceGxm viewport setup (NOTE: origin is on center screen)
extern float x_port;
extern float y_port;
extern float z_port;
extern float x_scale;
extern float y_scale;
extern float z_scale;

extern SceGxmContext *gxm_context; // sceGxm context instance
extern GLenum _vitagl_error; // Error returned by glGetError
extern SceGxmShaderPatcher *gxm_shader_patcher; // sceGxmShaderPatcher shader patcher instance

matrix4x4 mvp_matrix; // ModelViewProjection Matrix
matrix4x4 projection_matrix; // Projection Matrix
matrix4x4 modelview_matrix; // ModelView Matrix
extern GLboolean mvp_modified; // Check if ModelViewProjection matrix needs to be recreated

extern GLuint cur_program; // Current in use custom program (0 = No custom program)
extern uint8_t viewport_mode; // Current setting for viewport mode
extern GLboolean vblank; // Current setting for VSync

extern GLenum orig_depth_test; // Original depth test state (used for depth test invalidation)

// Scissor test shaders
extern SceGxmFragmentProgram *scissor_test_fragment_program; // Scissor test fragment program
extern vector2f *scissor_test_vertices; // Scissor test region vertices
extern SceUID scissor_test_vertices_uid; // Scissor test vertices memblock id

extern uint16_t *depth_clear_indices; // Memblock starting address for clear screen indices

// Clear screen shaders
extern SceGxmVertexProgram *clear_vertex_program_patched; // Patched vertex program for clearing screen
extern vector2f *clear_vertices; // Memblock starting address for clear screen vertices

/* gxm.c */
void initGxm(void); // Inits sceGxm
void initGxmContext(void); // Inits sceGxm context
void termGxmContext(void); // Terms sceGxm context
void createDisplayRenderTarget(void); // Creates render target for the display
void destroyDisplayRenderTarget(void); // Destroys render target for the display
void initDisplayColorSurfaces(void); // Creates color surfaces for the display
void termDisplayColorSurfaces(void); // Destroys color surfaces for the display
void initDepthStencilBuffer(uint32_t w, uint32_t h, SceGxmDepthStencilSurface *surface, void **depth_buffer, void **stencil_buffer, vglMemType *depth_type, vglMemType *stencil_type); // Creates depth and stencil surfaces
void initDepthStencilSurfaces(void); // Creates depth and stencil surfaces for the display
void termDepthStencilSurfaces(void); // Destroys depth and stencil surfaces for the display
void startShaderPatcher(void); // Creates a shader patcher instance
void stopShaderPatcher(void); // Destroys a shader patcher instance
void waitRenderingDone(void); // Waits for rendering to be finished

/* tests.c */
void change_depth_write(SceGxmDepthWriteMode mode); // Changes current in use depth write mode
void change_depth_func(void); // Changes current in use depth test function
void invalidate_depth_test(void); // Invalidates depth test state
void validate_depth_test(void); // Resets original depth test state after invalidation
void change_stencil_settings(void); // Changes current in use stencil test parameters
GLboolean change_stencil_config(SceGxmStencilOp *cfg, GLenum new); // Changes current in use stencil test operation value
GLboolean change_stencil_func_config(SceGxmStencilFunc *cfg, GLenum new); // Changes current in use stencil test function value
void update_alpha_test_settings(void); // Changes current in use alpha test operation value
void update_scissor_test(void); // Changes current in use scissor test region
void resetScissorTestRegion(void); // Resets scissor test region to default values

/* blending.c */
void change_blend_factor(void); // Changes current blending settings for all used shaders
void disable_blend(void); // Disables blending for all used shaders

/* custom_shaders.c */
void resetCustomShaders(void); // Resets custom shaders
void changeCustomShadersBlend(SceGxmBlendInfo *blend_info); // Change SceGxmBlendInfo value to all custom shaders
void reloadCustomShader(void); // Reloads in use custom shader inside sceGxm
void _vglDrawObjects_CustomShadersIMPL(GLenum mode, GLsizei count, GLboolean implicit_wvp); // vglDrawObjects implementation for rendering with custom shaders

/* misc functions */
void vector2f_convert_to_local_space(vector2f *out, int x, int y, int width, int height); // Converts screen coords to local space

#endif
