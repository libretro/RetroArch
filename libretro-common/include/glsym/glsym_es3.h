#ifndef RGLGEN_DECL_H__
#define RGLGEN_DECL_H__
#ifdef __cplusplus
extern "C" {
#endif
#ifdef GL_APIENTRY
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROCKHR)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#else
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
typedef void (APIENTRY *RGLGENGLDEBUGPROCARB)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#endif
#ifndef GL_OES_EGL_image
typedef void *GLeglImageOES;
#endif
#if !defined(GL_OES_fixed_point) && !defined(HAVE_OPENGLES2)
typedef GLint GLfixed;
#endif
#if defined(OSX) && !defined(MAC_OS_X_VERSION_10_7)
typedef long long int GLint64;
typedef unsigned long long int GLuint64;
typedef unsigned long long int GLuint64EXT;
typedef struct __GLsync *GLsync;
#endif
typedef void (GL_APIENTRYP RGLSYMGLBLENDBARRIERKHRPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGECONTROLKHRPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGEINSERTKHRPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGECALLBACKKHRPROC) (RGLGENGLDEBUGPROCKHR callback, const void *userParam);
typedef GLuint (GL_APIENTRYP RGLSYMGLGETDEBUGMESSAGELOGKHRPROC) (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (GL_APIENTRYP RGLSYMGLPUSHDEBUGGROUPKHRPROC) (GLenum source, GLuint id, GLsizei length, const GLchar *message);
typedef void (GL_APIENTRYP RGLSYMGLPOPDEBUGGROUPKHRPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLOBJECTLABELKHRPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETOBJECTLABELKHRPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLOBJECTPTRLABELKHRPROC) (const void *ptr, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETOBJECTPTRLABELKHRPROC) (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETPOINTERVKHRPROC) (GLenum pname, void **params);
typedef GLenum (GL_APIENTRYP RGLSYMGLGETGRAPHICSRESETSTATUSKHRPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLREADNPIXELSKHRPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
typedef void (GL_APIENTRYP RGLSYMGLGETNUNIFORMFVKHRPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
typedef void (GL_APIENTRYP RGLSYMGLGETNUNIFORMIVKHRPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETNUNIFORMUIVKHRPROC) (GLuint program, GLint location, GLsizei bufSize, GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
typedef void (GL_APIENTRYP RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);
typedef void (GL_APIENTRYP RGLSYMGLCOPYIMAGESUBDATAOESPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
typedef void (GL_APIENTRYP RGLSYMGLENABLEIOESPROC) (GLenum target, GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLDISABLEIOESPROC) (GLenum target, GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLBLENDEQUATIONIOESPROC) (GLuint buf, GLenum mode);
typedef void (GL_APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEIOESPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (GL_APIENTRYP RGLSYMGLBLENDFUNCIOESPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (GL_APIENTRYP RGLSYMGLBLENDFUNCSEPARATEIOESPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (GL_APIENTRYP RGLSYMGLCOLORMASKIOESPROC) (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISENABLEDIOESPROC) (GLenum target, GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLDRAWELEMENTSBASEVERTEXOESPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex);
typedef void (GL_APIENTRYP RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXOESPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex);
typedef void (GL_APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex);
typedef void (GL_APIENTRYP RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXOESPROC) (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei primcount, const GLint *basevertex);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREOESPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMBINARYOESPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMBINARYOESPROC) (GLuint program, GLenum binaryFormat, const void *binary, GLint length);
typedef void *(GL_APIENTRYP RGLSYMGLMAPBUFFEROESPROC) (GLenum target, GLenum access);
typedef GLboolean (GL_APIENTRYP RGLSYMGLUNMAPBUFFEROESPROC) (GLenum target);
typedef void (GL_APIENTRYP RGLSYMGLGETBUFFERPOINTERVOESPROC) (GLenum target, GLenum pname, void **params);
typedef void (GL_APIENTRYP RGLSYMGLPRIMITIVEBOUNDINGBOXOESPROC) (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW);
typedef void (GL_APIENTRYP RGLSYMGLMINSAMPLESHADINGOESPROC) (GLfloat value);
typedef void (GL_APIENTRYP RGLSYMGLPATCHPARAMETERIOESPROC) (GLenum pname, GLint value);
typedef void (GL_APIENTRYP RGLSYMGLTEXIMAGE3DOESPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE3DOESPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE3DOESPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (GL_APIENTRYP RGLSYMGLTEXPARAMETERIIVOESPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLTEXPARAMETERIUIVOESPROC) (GLenum target, GLenum pname, const GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETTEXPARAMETERIIVOESPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETTEXPARAMETERIUIVOESPROC) (GLenum target, GLenum pname, GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLERPARAMETERIIVOESPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLERPARAMETERIUIVOESPROC) (GLuint sampler, GLenum pname, const GLuint *param);
typedef void (GL_APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIIVOESPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIUIVOESPROC) (GLuint sampler, GLenum pname, GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLTEXBUFFEROESPROC) (GLenum target, GLenum internalformat, GLuint buffer);
typedef void (GL_APIENTRYP RGLSYMGLTEXBUFFERRANGEOESPROC) (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (GL_APIENTRYP RGLSYMGLTEXSTORAGE3DMULTISAMPLEOESPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (GL_APIENTRYP RGLSYMGLTEXTUREVIEWOESPROC) (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);
typedef void (GL_APIENTRYP RGLSYMGLBINDVERTEXARRAYOESPROC) (GLuint array);
typedef void (GL_APIENTRYP RGLSYMGLDELETEVERTEXARRAYSOESPROC) (GLsizei n, const GLuint *arrays);
typedef void (GL_APIENTRYP RGLSYMGLGENVERTEXARRAYSOESPROC) (GLsizei n, GLuint *arrays);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISVERTEXARRAYOESPROC) (GLuint array);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews);

#define glBlendBarrierKHR __rglgen_glBlendBarrierKHR
#define glDebugMessageControlKHR __rglgen_glDebugMessageControlKHR
#define glDebugMessageInsertKHR __rglgen_glDebugMessageInsertKHR
#define glDebugMessageCallbackKHR __rglgen_glDebugMessageCallbackKHR
#define glGetDebugMessageLogKHR __rglgen_glGetDebugMessageLogKHR
#define glPushDebugGroupKHR __rglgen_glPushDebugGroupKHR
#define glPopDebugGroupKHR __rglgen_glPopDebugGroupKHR
#define glObjectLabelKHR __rglgen_glObjectLabelKHR
#define glGetObjectLabelKHR __rglgen_glGetObjectLabelKHR
#define glObjectPtrLabelKHR __rglgen_glObjectPtrLabelKHR
#define glGetObjectPtrLabelKHR __rglgen_glGetObjectPtrLabelKHR
#define glGetPointervKHR __rglgen_glGetPointervKHR
#define glGetGraphicsResetStatusKHR __rglgen_glGetGraphicsResetStatusKHR
#define glReadnPixelsKHR __rglgen_glReadnPixelsKHR
#define glGetnUniformfvKHR __rglgen_glGetnUniformfvKHR
#define glGetnUniformivKHR __rglgen_glGetnUniformivKHR
#define glGetnUniformuivKHR __rglgen_glGetnUniformuivKHR
#define glEGLImageTargetTexture2DOES __rglgen_glEGLImageTargetTexture2DOES
#define glEGLImageTargetRenderbufferStorageOES __rglgen_glEGLImageTargetRenderbufferStorageOES
#define glCopyImageSubDataOES __rglgen_glCopyImageSubDataOES
#define glEnableiOES __rglgen_glEnableiOES
#define glDisableiOES __rglgen_glDisableiOES
#define glBlendEquationiOES __rglgen_glBlendEquationiOES
#define glBlendEquationSeparateiOES __rglgen_glBlendEquationSeparateiOES
#define glBlendFunciOES __rglgen_glBlendFunciOES
#define glBlendFuncSeparateiOES __rglgen_glBlendFuncSeparateiOES
#define glColorMaskiOES __rglgen_glColorMaskiOES
#define glIsEnablediOES __rglgen_glIsEnablediOES
#define glDrawElementsBaseVertexOES __rglgen_glDrawElementsBaseVertexOES
#define glDrawRangeElementsBaseVertexOES __rglgen_glDrawRangeElementsBaseVertexOES
#define glDrawElementsInstancedBaseVertexOES __rglgen_glDrawElementsInstancedBaseVertexOES
#define glMultiDrawElementsBaseVertexOES __rglgen_glMultiDrawElementsBaseVertexOES
#define glFramebufferTextureOES __rglgen_glFramebufferTextureOES
#define glGetProgramBinaryOES __rglgen_glGetProgramBinaryOES
#define glProgramBinaryOES __rglgen_glProgramBinaryOES
#define glMapBufferOES __rglgen_glMapBufferOES
#define glUnmapBufferOES __rglgen_glUnmapBufferOES
#define glGetBufferPointervOES __rglgen_glGetBufferPointervOES
#define glPrimitiveBoundingBoxOES __rglgen_glPrimitiveBoundingBoxOES
#define glMinSampleShadingOES __rglgen_glMinSampleShadingOES
#define glPatchParameteriOES __rglgen_glPatchParameteriOES
#define glTexImage3DOES __rglgen_glTexImage3DOES
#define glTexSubImage3DOES __rglgen_glTexSubImage3DOES
#define glCopyTexSubImage3DOES __rglgen_glCopyTexSubImage3DOES
#define glCompressedTexImage3DOES __rglgen_glCompressedTexImage3DOES
#define glCompressedTexSubImage3DOES __rglgen_glCompressedTexSubImage3DOES
#define glFramebufferTexture3DOES __rglgen_glFramebufferTexture3DOES
#define glTexParameterIivOES __rglgen_glTexParameterIivOES
#define glTexParameterIuivOES __rglgen_glTexParameterIuivOES
#define glGetTexParameterIivOES __rglgen_glGetTexParameterIivOES
#define glGetTexParameterIuivOES __rglgen_glGetTexParameterIuivOES
#define glSamplerParameterIivOES __rglgen_glSamplerParameterIivOES
#define glSamplerParameterIuivOES __rglgen_glSamplerParameterIuivOES
#define glGetSamplerParameterIivOES __rglgen_glGetSamplerParameterIivOES
#define glGetSamplerParameterIuivOES __rglgen_glGetSamplerParameterIuivOES
#define glTexBufferOES __rglgen_glTexBufferOES
#define glTexBufferRangeOES __rglgen_glTexBufferRangeOES
#define glTexStorage3DMultisampleOES __rglgen_glTexStorage3DMultisampleOES
#define glTextureViewOES __rglgen_glTextureViewOES
#define glBindVertexArrayOES __rglgen_glBindVertexArrayOES
#define glDeleteVertexArraysOES __rglgen_glDeleteVertexArraysOES
#define glGenVertexArraysOES __rglgen_glGenVertexArraysOES
#define glIsVertexArrayOES __rglgen_glIsVertexArrayOES
#define glFramebufferTextureMultiviewOVR __rglgen_glFramebufferTextureMultiviewOVR
#define glFramebufferTextureMultisampleMultiviewOVR __rglgen_glFramebufferTextureMultisampleMultiviewOVR

extern RGLSYMGLBLENDBARRIERKHRPROC __rglgen_glBlendBarrierKHR;
extern RGLSYMGLDEBUGMESSAGECONTROLKHRPROC __rglgen_glDebugMessageControlKHR;
extern RGLSYMGLDEBUGMESSAGEINSERTKHRPROC __rglgen_glDebugMessageInsertKHR;
extern RGLSYMGLDEBUGMESSAGECALLBACKKHRPROC __rglgen_glDebugMessageCallbackKHR;
extern RGLSYMGLGETDEBUGMESSAGELOGKHRPROC __rglgen_glGetDebugMessageLogKHR;
extern RGLSYMGLPUSHDEBUGGROUPKHRPROC __rglgen_glPushDebugGroupKHR;
extern RGLSYMGLPOPDEBUGGROUPKHRPROC __rglgen_glPopDebugGroupKHR;
extern RGLSYMGLOBJECTLABELKHRPROC __rglgen_glObjectLabelKHR;
extern RGLSYMGLGETOBJECTLABELKHRPROC __rglgen_glGetObjectLabelKHR;
extern RGLSYMGLOBJECTPTRLABELKHRPROC __rglgen_glObjectPtrLabelKHR;
extern RGLSYMGLGETOBJECTPTRLABELKHRPROC __rglgen_glGetObjectPtrLabelKHR;
extern RGLSYMGLGETPOINTERVKHRPROC __rglgen_glGetPointervKHR;
extern RGLSYMGLGETGRAPHICSRESETSTATUSKHRPROC __rglgen_glGetGraphicsResetStatusKHR;
extern RGLSYMGLREADNPIXELSKHRPROC __rglgen_glReadnPixelsKHR;
extern RGLSYMGLGETNUNIFORMFVKHRPROC __rglgen_glGetnUniformfvKHR;
extern RGLSYMGLGETNUNIFORMIVKHRPROC __rglgen_glGetnUniformivKHR;
extern RGLSYMGLGETNUNIFORMUIVKHRPROC __rglgen_glGetnUniformuivKHR;
extern RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC __rglgen_glEGLImageTargetTexture2DOES;
extern RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC __rglgen_glEGLImageTargetRenderbufferStorageOES;
extern RGLSYMGLCOPYIMAGESUBDATAOESPROC __rglgen_glCopyImageSubDataOES;
extern RGLSYMGLENABLEIOESPROC __rglgen_glEnableiOES;
extern RGLSYMGLDISABLEIOESPROC __rglgen_glDisableiOES;
extern RGLSYMGLBLENDEQUATIONIOESPROC __rglgen_glBlendEquationiOES;
extern RGLSYMGLBLENDEQUATIONSEPARATEIOESPROC __rglgen_glBlendEquationSeparateiOES;
extern RGLSYMGLBLENDFUNCIOESPROC __rglgen_glBlendFunciOES;
extern RGLSYMGLBLENDFUNCSEPARATEIOESPROC __rglgen_glBlendFuncSeparateiOES;
extern RGLSYMGLCOLORMASKIOESPROC __rglgen_glColorMaskiOES;
extern RGLSYMGLISENABLEDIOESPROC __rglgen_glIsEnablediOES;
extern RGLSYMGLDRAWELEMENTSBASEVERTEXOESPROC __rglgen_glDrawElementsBaseVertexOES;
extern RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXOESPROC __rglgen_glDrawRangeElementsBaseVertexOES;
extern RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC __rglgen_glDrawElementsInstancedBaseVertexOES;
extern RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXOESPROC __rglgen_glMultiDrawElementsBaseVertexOES;
extern RGLSYMGLFRAMEBUFFERTEXTUREOESPROC __rglgen_glFramebufferTextureOES;
extern RGLSYMGLGETPROGRAMBINARYOESPROC __rglgen_glGetProgramBinaryOES;
extern RGLSYMGLPROGRAMBINARYOESPROC __rglgen_glProgramBinaryOES;
extern RGLSYMGLMAPBUFFEROESPROC __rglgen_glMapBufferOES;
extern RGLSYMGLUNMAPBUFFEROESPROC __rglgen_glUnmapBufferOES;
extern RGLSYMGLGETBUFFERPOINTERVOESPROC __rglgen_glGetBufferPointervOES;
extern RGLSYMGLPRIMITIVEBOUNDINGBOXOESPROC __rglgen_glPrimitiveBoundingBoxOES;
extern RGLSYMGLMINSAMPLESHADINGOESPROC __rglgen_glMinSampleShadingOES;
extern RGLSYMGLPATCHPARAMETERIOESPROC __rglgen_glPatchParameteriOES;
extern RGLSYMGLTEXIMAGE3DOESPROC __rglgen_glTexImage3DOES;
extern RGLSYMGLTEXSUBIMAGE3DOESPROC __rglgen_glTexSubImage3DOES;
extern RGLSYMGLCOPYTEXSUBIMAGE3DOESPROC __rglgen_glCopyTexSubImage3DOES;
extern RGLSYMGLCOMPRESSEDTEXIMAGE3DOESPROC __rglgen_glCompressedTexImage3DOES;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DOESPROC __rglgen_glCompressedTexSubImage3DOES;
extern RGLSYMGLFRAMEBUFFERTEXTURE3DOESPROC __rglgen_glFramebufferTexture3DOES;
extern RGLSYMGLTEXPARAMETERIIVOESPROC __rglgen_glTexParameterIivOES;
extern RGLSYMGLTEXPARAMETERIUIVOESPROC __rglgen_glTexParameterIuivOES;
extern RGLSYMGLGETTEXPARAMETERIIVOESPROC __rglgen_glGetTexParameterIivOES;
extern RGLSYMGLGETTEXPARAMETERIUIVOESPROC __rglgen_glGetTexParameterIuivOES;
extern RGLSYMGLSAMPLERPARAMETERIIVOESPROC __rglgen_glSamplerParameterIivOES;
extern RGLSYMGLSAMPLERPARAMETERIUIVOESPROC __rglgen_glSamplerParameterIuivOES;
extern RGLSYMGLGETSAMPLERPARAMETERIIVOESPROC __rglgen_glGetSamplerParameterIivOES;
extern RGLSYMGLGETSAMPLERPARAMETERIUIVOESPROC __rglgen_glGetSamplerParameterIuivOES;
extern RGLSYMGLTEXBUFFEROESPROC __rglgen_glTexBufferOES;
extern RGLSYMGLTEXBUFFERRANGEOESPROC __rglgen_glTexBufferRangeOES;
extern RGLSYMGLTEXSTORAGE3DMULTISAMPLEOESPROC __rglgen_glTexStorage3DMultisampleOES;
extern RGLSYMGLTEXTUREVIEWOESPROC __rglgen_glTextureViewOES;
extern RGLSYMGLBINDVERTEXARRAYOESPROC __rglgen_glBindVertexArrayOES;
extern RGLSYMGLDELETEVERTEXARRAYSOESPROC __rglgen_glDeleteVertexArraysOES;
extern RGLSYMGLGENVERTEXARRAYSOESPROC __rglgen_glGenVertexArraysOES;
extern RGLSYMGLISVERTEXARRAYOESPROC __rglgen_glIsVertexArrayOES;
extern RGLSYMGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC __rglgen_glFramebufferTextureMultiviewOVR;
extern RGLSYMGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC __rglgen_glFramebufferTextureMultisampleMultiviewOVR;

struct rglgen_sym_map { const char *sym; void *ptr; };
extern const struct rglgen_sym_map rglgen_symbol_map[];
#ifdef __cplusplus
}
#endif
#endif
