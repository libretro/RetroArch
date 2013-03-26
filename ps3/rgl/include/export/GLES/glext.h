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

#define GL_QUADS						  0x0007
#define GL_QUAD_STRIP					  0x0008

#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405

   /* Image types */
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

#define GL_HALF_FLOAT_ARB                          0x140B

   /* Image internal formats */
#define GL_ALPHA4                         0x803B
#define GL_ALPHA12                        0x803D
#define GL_ALPHA16                        0x803E
#define GL_R3_G3_B2                       0x2A10
#define GL_RGB4                           0x804F
#define GL_RGB5                           0x8050
#define GL_RGB8                           0x8051
#define GL_RGBA2                          0x8055
#define GL_RGBA4                          0x8056
#define GL_RGB5_A1                        0x8057
#define GL_RGBA12                         0x805A
#define GL_RGBA16                         0x805B
#define GL_BGR                            0x80E0
#define GL_BGRA	                          0x80E1
#define GL_ABGR	                          0x8000
#define GL_RED                            0x1903
#define GL_GREEN                          0x1904
#define GL_BLUE                           0x1905
#define GL_ARGB_SCE                       0x6007

#define GL_RGBA32F_ARB                      0x8814
#define GL_RGB32F_ARB                       0x8815
#define GL_ALPHA32F_ARB                     0x8816
#define GL_RGBA16F_ARB                      0x881A
#define GL_RGB16F_ARB                       0x881B
#define GL_ALPHA16F_ARB                     0x881C

#define GL_UNSIGNED_SHORT_8_8_SCE			0x600B
#define GL_UNSIGNED_SHORT_8_8_REV_SCE		0x600C
#define GL_UNSIGNED_INT_16_16_SCE			0x600D
#define GL_UNSIGNED_INT_16_16_REV_SCE		0x600E

#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3


   /* TexGen */
#define GL_EYE_LINEAR			0x2400
#define GL_OBJECT_LINEAR		0x2401
#define GL_SPHERE_MAP			0x2402
#define GL_NORMAL_MAP			0x8511
#define GL_REFLECTION_MAP		0x8512
#define GL_S				0x2000
#define GL_T				0x2001
#define GL_R				0x2002
#define GL_Q				0x2003
#define GL_OBJECT_PLANE			0x2501
#define GL_EYE_PLANE			0x2502
#define GL_TEXTURE_GEN_MODE		0x2500
#define GL_TEXTURE_GEN_S		0x0C60
#define GL_TEXTURE_GEN_T		0x0C61
#define GL_TEXTURE_GEN_R		0x0C62
#define GL_TEXTURE_GEN_Q		0x0C63

   /* Blending */
#define GL_CONSTANT_COLOR				0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR		0x8002
#define GL_CONSTANT_ALPHA				0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA		0x8004
#define GL_BLEND_EQUATION				0x8009
#define GL_FUNC_ADD						0x8006
#define GL_MIN							0x8007
#define GL_MAX							0x8008
#define GL_FUNC_SUBTRACT				0x800A
#define GL_FUNC_REVERSE_SUBTRACT		0x800B

   /* Texture3D */
#define GL_TEXTURE_3D                     0x806F
#define GL_TEXTURE_WRAP_R		0x8072
#define GL_MAX_3D_TEXTURE_SIZE			0x8073

   /* CubeMap */
#define GL_TEXTURE_CUBE_MAP               0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X      0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X      0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y      0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y      0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z      0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z      0x851A

   /* PolygonMode */
#define GL_POINT		0x1B00
#define GL_LINE			0x1B01
#define GL_FILL			0x1B02

   /* PolygonOffset for GL_LINE */
#define GL_POLYGON_OFFSET_LINE            0x2A02

   /* Filter Control */
#define GL_TEXTURE_FILTER_CONTROL         0x8500
#define GL_TEXTURE_LOD_BIAS               0x8501
#define GL_TEXTURE_MIN_LOD		0x813A
#define GL_TEXTURE_MAX_LOD		0x813B
#define GL_TEXTURE_MAX_LEVEL              0x813D
#define GL_TEXTURE_BORDER_COLOR           0x1004

   /* depth/shadow */
#define GL_NONE                           0x0
#define GL_DEPTH_TEXTURE_MODE_ARB         0x884B
#define GL_TEXTURE_COMPARE_MODE_ARB       0x884C
#define GL_TEXTURE_COMPARE_FUNC_ARB       0x884D
#define GL_COMPARE_R_TO_TEXTURE_ARB       0x884E

   /* Wrap modes */
#define GL_CLAMP                        0x2900
#define GL_MIRRORED_REPEAT              0x8370
#define GL_MIRROR_CLAMP_EXT             0x8742
#define GL_MIRROR_CLAMP_TO_EDGE_EXT     0x8743
#define GL_MIRROR_CLAMP_TO_BORDER_EXT	0x8912
#define GL_CLAMP_TO_BORDER				0x812D

   /* Fog Coordinate Source */
#define GL_FOG_COORDINATE_SOURCE          0x8450
#define GL_FOG_COORDINATE                 0x8451
#define GL_FRAGMENT_DEPTH                 0x8452

   /* Fragment Control TXP */ 
#define GL_FRAGMENT_PROGRAM_CONTROL_CONTROLTXP_SCE	0x8453

   /* Gets */
#define GL_MODELVIEW_MATRIX               0x0BA6
#define GL_TEXTURE_MATRIX                 0x0BA8
#define GL_PROJECTION_MATRIX              0x0BA7
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB    0x8872


   /* Surface targets */
#define GL_MAX_DRAW_BUFFERS_ATI                    0x8824
#define GL_DRAW_BUFFER0_ATI                        0x8825
#define GL_DRAW_BUFFER1_ATI                        0x8826
#define GL_DRAW_BUFFER2_ATI                        0x8827
#define GL_DRAW_BUFFER3_ATI                        0x8828
#define GL_DRAW_BUFFER4_ATI                        0x8829
#define GL_DRAW_BUFFER5_ATI                        0x882A
#define GL_DRAW_BUFFER6_ATI                        0x882B
#define GL_DRAW_BUFFER7_ATI                        0x882C
#define GL_DRAW_BUFFER8_ATI                        0x882D
#define GL_DRAW_BUFFER9_ATI                        0x882E
#define GL_DRAW_BUFFER10_ATI                       0x882F
#define GL_DRAW_BUFFER11_ATI                       0x8830
#define GL_DRAW_BUFFER12_ATI                       0x8831
#define GL_DRAW_BUFFER13_ATI                       0x8832
#define GL_DRAW_BUFFER14_ATI                       0x8833
#define GL_DRAW_BUFFER15_ATI                       0x8834
#define GL_DRAW_DEPTH_SCE                              0x6004
#define GL_DRAW_STENCIL_SCE                            0x6005


#define GL_RGBA8                          0x8058
#define GL_FLOAT_DEPTH_COMPONENT16_SCE                 0x6000
#define GL_FLOAT_DEPTH_COMPONENT32_SCE                 0x6001
#define GL_DEPTH24_STENCIL8_SCE                        0x6002
#define GL_STENCIL8_SCE                                0x6003
#define GL_DEPTH_COMPONENT                         0x1902
#define GL_DEPTH_COMPONENT16                       0x81A5
#define GL_DEPTH_COMPONENT24                       0x81A6
#define GL_DEPTH_COMPONENT32                       0x81A7
#define GL_STENCIL_INDEX                           0x1901

#define GL_DRAWABLE_BIT_SCE                                0x0001
#define GL_ALLOW_SCAN_OUT_BIT_SCE                          0x0002
#define GL_TEXTURE_READ_BIT_SCE                            0x0004
#define GL_ANTIALIASED_BIT_SCE                            0x0008

   /* VBO & PBO */
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
#define GL_SYSTEM_DRAW                	0x6020

   /* Map/Unmap */
#define GL_READ_ONLY                      0x88B8
#define GL_WRITE_ONLY                     0x88B9
#define GL_READ_WRITE                     0x88BA

   /* VSYNC */
#define GL_VSYNC_SCE					0x6006

#define GL_TEXTURE_GAMMA_REMAP_R_SCE		0x6010
#define GL_TEXTURE_GAMMA_REMAP_G_SCE		0x6011
#define GL_TEXTURE_GAMMA_REMAP_B_SCE		0x6012
#define GL_TEXTURE_GAMMA_REMAP_A_SCE		0x6013

#define GL_SHADER_SRGB_REMAP_SCE		0x6014

#define GL_DIVIDE_SCE	0x6015
#define GL_MODULO_SCE	0x6016

#define GL_TEXTURE_FROM_VERTEX_PROGRAM_SCE 0x6017

   /* Primitive restart */
#define GL_PRIMITIVE_RESTART_NV                           0x8558

   /* Anisotropic filtering */
#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF

   /* Sync */
#define GL_ALL_COMPLETED_NV					0x84F2
#define GL_FENCE_STATUS_NV					0x84F3
#define GL_FENCE_CONDITION_NV				0x84F4

   /* User clip planes */
#define GL_MAX_CLIP_PLANES				0x0D32
#define GL_CLIP_PLANE0					0x3000
#define GL_CLIP_PLANE1					0x3001
#define GL_CLIP_PLANE2					0x3002
#define GL_CLIP_PLANE3					0x3003
#define GL_CLIP_PLANE4					0x3004
#define GL_CLIP_PLANE5					0x3005

   /* Point Sprites */
#define GL_POINT_SPRITE_OES				0x8861
#define GL_COORD_REPLACE_OES			0x8862
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB			0x8642

   /* Framebuffer object */
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

   /* multiple render target blend enable enums */
#define GL_BLEND_MRT0_SCE GL_COLOR_ATTACHMENT0_EXT
#define GL_BLEND_MRT1_SCE GL_COLOR_ATTACHMENT1_EXT
#define GL_BLEND_MRT2_SCE GL_COLOR_ATTACHMENT2_EXT
#define GL_BLEND_MRT3_SCE GL_COLOR_ATTACHMENT3_EXT

   /* Texture usage hint */
#define GL_TEXTURE_ALLOCATION_HINT_SCE    0x6018
#define GL_TEXTURE_TILED_GPU_SCE          0x6019
#define GL_TEXTURE_LINEAR_GPU_SCE         0x601A
#define GL_TEXTURE_SWIZZLED_GPU_SCE       0x601B
#define GL_TEXTURE_LINEAR_SYSTEM_SCE      0x601C
#define GL_TEXTURE_SWIZZLED_SYSTEM_SCE    0x601D

   /* Occlusion query & Conditional rendering */
#define GL_SAMPLES_PASSED_ARB             0x8914
#define GL_QUERY_COUNTER_BITS_ARB         0x8864
#define GL_CURRENT_QUERY_ARB              0x8865
#define GL_QUERY_RESULT_ARB               0x8866
#define GL_QUERY_RESULT_AVAILABLE_ARB     0x8867

   /* Two-sided stencil, Stencil wrap */
#define GL_STENCIL_TEST_TWO_SIDE_EXT      0x8910
#define GL_INCR_WRAP                      0x8507
#define GL_DECR_WRAP                      0x8508

   /* depth clamp */
#define GL_DEPTH_CLAMP_NV                 0x864F

   /* 32-bit 3-component attributes (11/11/10) */
#define GL_FIXED_11_11_10_SCE             0x6020

   /* Anti-aliasing */
#define GL_REDUCE_DST_COLOR_SCE 				0x6021
#define GL_TEXTURE_MULTISAMPLING_HINT_SCE      0x6022
#define GL_FRAMEBUFFER_MULTISAMPLING_MODE_SCE   0x6023

#define GL_MULTISAMPLING_NONE_SCE        0x6030
#define GL_MULTISAMPLING_2X_DIAGONAL_CENTERED_SCE 0x6031
#define GL_MULTISAMPLING_4X_SQUARE_CENTERED_SCE 0x6032
#define GL_MULTISAMPLING_4X_SQUARE_ROTATED_SCE  0x6033

   /* Texture reference buffer */
#define GL_TEXTURE_REFERENCE_BUFFER_SCE 0x6040
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_PITCH_SCE 0x6041

   /******************************************************************************/
   GLAPI void APIENTRY glBlendEquation( GLenum mode );
   GLAPI void APIENTRY glBlendEquationSeparate( GLenum modeRGB, GLenum modeAlpha );
   GLAPI void APIENTRY glBlendFuncSeparate( GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha );
   GLAPI void APIENTRY glBlendColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
   GLAPI void APIENTRY glNormal3fv( const GLfloat* v );
   GLAPI void APIENTRY glGetBooleanv( GLenum pname, GLboolean* params );
   GLAPI void APIENTRY glGetFloatv( GLenum pname, GLfloat* params );
   GLAPI void APIENTRY glTexParameterfv( GLenum target, GLenum pname, const GLfloat* params );
   GLAPI void APIENTRY glTexParameteri( GLenum target, GLenum pname, GLint param );
   GLAPI void APIENTRY glTexParameteriv( GLenum target, GLenum pname, const GLint* params );
   GLAPI void APIENTRY glTexSubImage3D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid* pixels );

   /* VBO & PBO */
   GLAPI void APIENTRY glBindBuffer( GLenum target, GLuint name );
   GLAPI void APIENTRY glDeleteBuffers( GLsizei n, const GLuint *buffers );
   GLAPI void APIENTRY glGenBuffers( GLsizei n, GLuint *buffers );
   GLAPI void APIENTRY glBufferData( GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage );
   GLAPI void APIENTRY glBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data );
   GLAPI void APIENTRY glGetBufferParameteriv( GLenum target, GLenum pname, GLint *params );
   /* VBO & PBO map/unmap */
   GLAPI GLvoid* APIENTRY glMapBuffer( GLenum target, GLenum access );
   GLAPI GLboolean APIENTRY glUnmapBuffer( GLenum target );

   /* Sync */
   GLAPI void APIENTRY glDeleteFencesNV( GLsizei n, const GLuint *fences );
   GLAPI void APIENTRY glGenFencesNV( GLsizei n, GLuint *fences );
   GLAPI GLboolean APIENTRY glIsFenceNV( GLuint fence );
   GLAPI GLboolean APIENTRY glTestFenceNV( GLuint fence );
   GLAPI void APIENTRY glGetFenceivNV( GLuint fence, GLenum pname, GLint *params );
   GLAPI void APIENTRY glFinishFenceNV( GLuint fence );
   GLAPI void APIENTRY glSetFenceNV( GLuint fence, GLenum condition );

   /* Framebuffer object */
   GLAPI GLboolean APIENTRY glIsRenderbufferOES( GLuint );
   GLAPI void APIENTRY glBindRenderbufferOES( GLenum, GLuint );
   GLAPI void APIENTRY glDeleteRenderbuffersOES( GLsizei, const GLuint * );
   GLAPI void APIENTRY glGenRenderbuffersOES( GLsizei, GLuint * );
   GLAPI void APIENTRY glRenderbufferStorageOES( GLenum, GLenum, GLsizei, GLsizei );
   GLAPI void APIENTRY glGetRenderbufferParameterivOES( GLenum, GLenum, GLint * );
   GLAPI GLboolean APIENTRY glIsFramebufferOES( GLuint );
   GLAPI void APIENTRY glBindFramebufferOES( GLenum, GLuint );
   GLAPI void APIENTRY glDeleteFramebuffersOES( GLsizei, const GLuint * );
   GLAPI void APIENTRY glGenFramebuffersOES( GLsizei, GLuint * );
   GLAPI GLenum APIENTRY glCheckFramebufferStatusOES( GLenum );
   GLAPI void APIENTRY glFramebufferTexture2DOES( GLenum, GLenum, GLenum, GLuint, GLint );
   GLAPI void APIENTRY glFramebufferRenderbufferOES( GLenum, GLenum, GLenum, GLuint );
   GLAPI void APIENTRY glGetFramebufferAttachmentParameterivOES( GLenum, GLenum, GLenum, GLint * );
   GLAPI void APIENTRY glGenerateMipmapOES( GLenum );

   /* Texture Reference */
   GLAPI void APIENTRY glTextureReferenceSCE( GLenum target, GLuint levels, GLuint baseWidth, GLuint baseHeight, GLuint baseDepth, GLenum internalFormat, GLuint pitch, GLintptr offset );

#ifdef __cplusplus
}
#endif

#endif
