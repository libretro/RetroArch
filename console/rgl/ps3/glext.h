#ifndef __gl_ext_h_
#define __gl_ext_h_

#ifndef _MSC_VER
#include <stdint.h>
#else
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef intptr_t GLintptr;
typedef intptr_t GLsizeiptr;
typedef unsigned short GLhalfARB;

#define GL_QUADS			  0x0007
#define GL_QUAD_STRIP			  0x0008

#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405

#define GL_UNSIGNED_BYTE_3_3_2		0x8032
#define GL_UNSIGNED_BYTE_2_3_3_REV	0x8362
#define GL_UNSIGNED_SHORT_5_6_5_REV	0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4_REV	  0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV	  0x8366
#define GL_UNSIGNED_INT_8_8_8_8           0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#define GL_UNSIGNED_INT_10_10_10_2        0x8036
#define GL_UNSIGNED_INT_2_10_10_10_REV    0x8368
#define GL_UNSIGNED_INT_24_8_SCE          0x6008
#define GL_UNSIGNED_INT_8_24_REV_SCE      0x6009

#define GL_HALF_FLOAT_ARB                0x140B

#define GL_ALPHA4                         0x803B
#define GL_ALPHA8                         0x803C
#define GL_ALPHA12                        0x803D
#define GL_ALPHA16                        0x803E
#define GL_LUMINANCE4                     0x803F
#define GL_LUMINANCE8                     0x8040
#define GL_LUMINANCE12                    0x8041
#define GL_LUMINANCE16                    0x8042
#define GL_LUMINANCE4_ALPHA4		  0x8043
#define GL_LUMINANCE6_ALPHA2		  0x8044
#define GL_LUMINANCE8_ALPHA8              0x8045
#define GL_LUMINANCE12_ALPHA4             0x8046
#define GL_LUMINANCE12_ALPHA12            0x8047
#define GL_LUMINANCE16_ALPHA16            0x8048
#define GL_INTENSITY                      0x8049
#define GL_INTENSITY4                     0x804A
#define GL_INTENSITY8                     0x804B
#define GL_INTENSITY12                    0x804C
#define GL_INTENSITY16                    0x804D
#define GL_R3_G3_B2                       0x2A10
#define GL_RGB4                           0x804F
#define GL_RGB5                           0x8050
#define GL_RGB8                           0x8051
#define GL_RGB10                          0x8052
#define GL_RGB12                          0x8053
#define GL_RGB16                          0x8054
#define GL_RGBA2                          0x8055
#define GL_RGBA4                          0x8056
#define GL_RGB5_A1                        0x8057
#define GL_RGB10_A2                       0x8059
#define GL_RGBA12                         0x805A
#define GL_RGBA16                         0x805B
#define GL_BGR                            0x80E0
#define GL_BGRA	                          0x80E1
#define GL_ABGR	                          0x8000
#define GL_RED                            0x1903
#define GL_GREEN                          0x1904
#define GL_BLUE                           0x1905
#define GL_ARGB_SCE                       0x6007

#define GL_UNSIGNED_SHORT_8_8_SCE	0x600B
#define GL_UNSIGNED_SHORT_8_8_REV_SCE	0x600C
#define GL_UNSIGNED_INT_16_16_SCE	0x600D
#define GL_UNSIGNED_INT_16_16_REV_SCE	0x600E

#define GL_CONSTANT_COLOR		0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR	0x8002
#define GL_CONSTANT_ALPHA		0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA	0x8004
#define GL_BLEND_EQUATION		0x8009
#define GL_FUNC_ADD			0x8006
#define GL_MIN				0x8007
#define GL_MAX				0x8008
#define GL_FUNC_SUBTRACT		0x800A
#define GL_FUNC_REVERSE_SUBTRACT	0x800B

#define GL_TEXTURE_WRAP_R		0x8072
#define GL_MAX_3D_TEXTURE_SIZE		0x8073

#define GL_POINT		0x1B00
#define GL_LINE			0x1B01
#define GL_FILL			0x1B02

#define GL_TEXTURE_FILTER_CONTROL         0x8500
#define GL_TEXTURE_LOD_BIAS               0x8501
#define GL_TEXTURE_MIN_LOD		0x813A
#define GL_TEXTURE_MAX_LOD		0x813B
#define GL_TEXTURE_MAX_LEVEL              0x813D
#define GL_TEXTURE_BORDER_COLOR           0x1004

#define GL_NONE                           0x0
#define GL_DEPTH_TEXTURE_MODE_ARB         0x884B
#define GL_TEXTURE_COMPARE_MODE_ARB       0x884C
#define GL_TEXTURE_COMPARE_FUNC_ARB       0x884D
#define GL_COMPARE_R_TO_TEXTURE_ARB       0x884E

#define GL_CLAMP                        0x2900
#define GL_MIRRORED_REPEAT              0x8370
#define GL_MIRROR_CLAMP_EXT             0x8742
#define GL_MIRROR_CLAMP_TO_EDGE_EXT     0x8743
#define GL_MIRROR_CLAMP_TO_BORDER_EXT	0x8912
#define GL_CLAMP_TO_BORDER				0x812D

/* Fragment Control TXP */ 
#define GL_FRAGMENT_PROGRAM_CONTROL_CONTROLTXP_SCE	0x8453

/* Gets */
#define GL_MODELVIEW_MATRIX               0x0BA6
#define GL_TEXTURE_MATRIX                 0x0BA8
#define GL_PROJECTION_MATRIX              0x0BA7
#define GL_MAX_TEXTURE_COORDS_ARB         0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB    0x8872

#define GL_RGBA8                          0x8058

#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_PIXEL_PACK_BUFFER_ARB          0x88EB
#define GL_PIXEL_UNPACK_BUFFER_ARB        0x88EC
#define GL_STREAM_DRAW                    0x88E0
#define GL_STREAM_READ                    0x88E1
#define GL_STREAM_COPY                    0x88E2
#define GL_STATIC_DRAW                    0x88E4
#define GL_STATIC_READ                    0x88E5
#define GL_STATIC_COPY                    0x88E6
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_DYNAMIC_READ                   0x88E9
#define GL_DYNAMIC_COPY                   0x88EA
#define GL_SYSTEM_DRAW_SCE                0x6020

#define GL_READ_ONLY                      0x88B8
#define GL_WRITE_ONLY                     0x88B9
#define GL_READ_WRITE                     0x88BA

#define GL_VSYNC_SCE					0x6006
#define GL_SKIP_FIRST_VSYNC_SCE				0x6100

#define GL_TEXTURE_GAMMA_REMAP_R_SCE		0x6010
#define GL_TEXTURE_GAMMA_REMAP_G_SCE		0x6011
#define GL_TEXTURE_GAMMA_REMAP_B_SCE		0x6012
#define GL_TEXTURE_GAMMA_REMAP_A_SCE		0x6013

#define GL_SHADER_SRGB_REMAP_SCE		0x6014

#define GL_TEXTURE_FROM_VERTEX_PROGRAM_SCE 0x6017

#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF

#define GL_ALL_COMPLETED_NV					0x84F2
#define GL_FENCE_STATUS_NV					0x84F3
#define GL_FENCE_CONDITION_NV				0x84F4

#define GL_MAX_CLIP_PLANES				0x0D32
#define GL_CLIP_PLANE0					0x3000
#define GL_CLIP_PLANE1					0x3001
#define GL_CLIP_PLANE2					0x3002
#define GL_CLIP_PLANE3					0x3003
#define GL_CLIP_PLANE4					0x3004
#define GL_CLIP_PLANE5					0x3005

#define GL_POINT_SPRITE_OES				0x8861
#define GL_COORD_REPLACE_OES			0x8862
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB			0x8642

#define GL_INVALID_FRAMEBUFFER_OPERATION_OES 0x0506
#define GL_MAX_RENDERBUFFER_SIZE_OES      0x84E8
#define GL_FRAMEBUFFER_BINDING_OES        0x8CA6
#define GL_RENDERBUFFER_BINDING_OES       0x8CA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_OES 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_OES 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_OES 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_OES 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT 0x8CD4
#define GL_FRAMEBUFFER_COMPLETE_OES       0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_OES 0x8CD8
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES 0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES 0x8CDA
#define GL_FRAMEBUFFER_UNSUPPORTED_OES    0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS_OES      0x8CDF
#define GL_COLOR_ATTACHMENT0_EXT          0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT          0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT          0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT          0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT          0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT          0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT          0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT          0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT          0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT          0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT         0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT         0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT         0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT         0x8CED
#define GL_COLOR_ATTACHMENT14_EXT         0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT         0x8CEF
#define GL_DEPTH_ATTACHMENT_OES           0x8D00
#define GL_STENCIL_ATTACHMENT_OES         0x8D20
#define GL_FRAMEBUFFER_OES                0x8D40
#define GL_RENDERBUFFER_OES               0x8D41
#define GL_RENDERBUFFER_WIDTH_OES         0x8D42
#define GL_RENDERBUFFER_HEIGHT_OES        0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_OES 0x8D44
#define GL_STENCIL_INDEX_OES              0x8D45
#define GL_STENCIL_INDEX4_OES             0x8D47
#define GL_STENCIL_INDEX8_OES             0x8D48

#define GL_BLEND_MRT0_SCE GL_COLOR_ATTACHMENT0_EXT
#define GL_BLEND_MRT1_SCE GL_COLOR_ATTACHMENT1_EXT
#define GL_BLEND_MRT2_SCE GL_COLOR_ATTACHMENT2_EXT
#define GL_BLEND_MRT3_SCE GL_COLOR_ATTACHMENT3_EXT

#define GL_TEXTURE_ALLOCATION_HINT_SCE    0x6018
#define GL_TEXTURE_LINEAR_GPU_SCE         0x601A

#define GL_INCR_WRAP                      0x8507
#define GL_DECR_WRAP                      0x8508

#define GL_DEPTH_CLAMP_NV                 0x864F

#define GL_FIXED_11_11_10_SCE             0x6020

#define GL_REDUCE_DST_COLOR_SCE 				0x6021

#define GL_MULTISAMPLING_NONE_SCE        0x6030

#define GL_TEXTURE_REFERENCE_BUFFER_SCE 0x6040
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_PITCH_SCE 0x6041

GLAPI void APIENTRY glBlendEquation( GLenum mode );
GLAPI void APIENTRY glBlendColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
GLAPI void APIENTRY glGetFloatv( GLenum pname, GLfloat* params );
GLAPI void APIENTRY glTexParameteri( GLenum target, GLenum pname, GLint param );

GLAPI void APIENTRY glBindBuffer( GLenum target, GLuint name );
GLAPI void APIENTRY glDeleteBuffers( GLsizei n, const GLuint *buffers );
GLAPI void APIENTRY glGenBuffers( GLsizei n, GLuint *buffers );
GLAPI void APIENTRY glBufferData( GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage );
GLAPI void APIENTRY glBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data );

GLAPI void APIENTRY glBindFramebufferOES( GLenum, GLuint );
GLAPI void APIENTRY glDeleteFramebuffersOES( GLsizei, const GLuint * );
GLAPI void APIENTRY glGenFramebuffersOES( GLsizei, GLuint * );
GLAPI GLenum APIENTRY glCheckFramebufferStatusOES( GLenum );
GLAPI void APIENTRY glFramebufferTexture2DOES( GLenum, GLenum, GLenum, GLuint, GLint );

GLAPI void APIENTRY glTextureReferenceSCE( GLenum target, GLuint levels, GLuint baseWidth, GLuint baseHeight, GLuint baseDepth, GLenum internalFormat, GLuint pitch, GLintptr offset );

#ifdef __cplusplus
}
#endif

#endif
