/* 
 * state.c:
 * Initial config of the openGL machine state
 */

#include "shared.h"

// Blending
GLboolean blend_state = GL_FALSE; // Current state for GL_BLEND
SceGxmBlendFactor blend_sfactor_rgb = SCE_GXM_BLEND_FACTOR_ONE; // Current in use RGB source blend factor
SceGxmBlendFactor blend_dfactor_rgb = SCE_GXM_BLEND_FACTOR_ZERO; // Current in use RGB dest blend factor
SceGxmBlendFactor blend_sfactor_a = SCE_GXM_BLEND_FACTOR_ONE; // Current in use A source blend factor
SceGxmBlendFactor blend_dfactor_a = SCE_GXM_BLEND_FACTOR_ZERO; // Current in use A dest blend factor

// Polygon Mode
GLfloat pol_factor = 0.0f; // Current factor for glPolygonOffset
GLfloat pol_units = 0.0f; // Current units for glPolygonOffset

// Texture Units
int8_t client_texture_unit = 0; // Current in use client side texture unit

// Miscellaneous
glPhase phase = NONE; // Current drawing phase for legacy openGL
vector4f clear_rgba_val; // Current clear color for glClear

// Fogging
GLboolean fogging = GL_FALSE; // Current fogging processor state
GLint fog_mode = GL_EXP; // Current fogging mode (openGL)
fogType internal_fog_mode = DISABLED; // Current fogging mode (sceGxm)
GLfloat fog_density = 1.0f; // Current fogging density
GLfloat fog_near = 0.0f; // Current fogging near distance
GLfloat fog_far = 1.0f; // Current fogging far distance
vector4f fog_color = { 0.0f, 0.0f, 0.0f, 0.0f }; // Current fogging color

// Clipping Planes
GLint clip_plane0 = GL_FALSE; // Current status of clip plane 0
vector4f clip_plane0_eq = { 0.0f, 0.0f, 0.0f, 0.0f }; // Current equation of clip plane 0

// Cullling
GLboolean cull_face_state = GL_FALSE; // Current state for GL_CULL_FACE
GLenum gl_cull_mode = GL_BACK; // Current in use openGL cull mode
GLenum gl_front_face = GL_CCW; // Current in use openGL setting for front facing primitives
GLboolean no_polygons_mode = GL_FALSE; // GL_TRUE when cull mode is set to GL_FRONT_AND_BACK

// Polygon Offset
GLboolean pol_offset_fill = GL_FALSE; // Current state for GL_POLYGON_OFFSET_FILL
GLboolean pol_offset_line = GL_FALSE; // Current state for GL_POLYGON_OFFSET_LINE
GLboolean pol_offset_point = GL_FALSE; // Current state for GL_POLYGON_OFFSET_POINT
SceGxmPolygonMode polygon_mode_front = SCE_GXM_POLYGON_MODE_TRIANGLE_FILL; // Current in use polygon mode for front
SceGxmPolygonMode polygon_mode_back = SCE_GXM_POLYGON_MODE_TRIANGLE_FILL; // Current in use polygon mode for back
GLenum gl_polygon_mode_front = GL_FILL; // Current in use polygon mode for front
GLenum gl_polygon_mode_back = GL_FILL; // Current in use polygon mode for back
viewport gl_viewport; // Current viewport state
