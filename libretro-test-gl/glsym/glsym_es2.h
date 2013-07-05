#ifndef RGLGEN_DECL_H__
#define RGLGEN_DECL_H__
#ifdef GL_APIENTRY
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#else
typedef void (APIENTRY *RGLGENGLDEBUGPROCARB)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#endif
typedef void (GL_APIENTRYP RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
typedef void (GL_APIENTRYP RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMBINARYOESPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, GLvoid *binary);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMBINARYOESPROC) (GLuint program, GLenum binaryFormat, const GLvoid *binary, GLint length);
typedef void* (GL_APIENTRYP RGLSYMGLMAPBUFFEROESPROC) (GLenum target, GLenum access);
typedef GLboolean (GL_APIENTRYP RGLSYMGLUNMAPBUFFEROESPROC) (GLenum target);
typedef void (GL_APIENTRYP RGLSYMGLGETBUFFERPOINTERVOESPROC) (GLenum target, GLenum pname, GLvoid** params);
typedef void (GL_APIENTRYP RGLSYMGLTEXIMAGE3DOESPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
typedef void (GL_APIENTRYP RGLSYMGLTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid* pixels);
typedef void (GL_APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE3DOESPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid* data);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid* data);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE3DOESPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (GL_APIENTRYP RGLSYMGLBINDVERTEXARRAYOESPROC) (GLuint array);
typedef void (GL_APIENTRYP RGLSYMGLDELETEVERTEXARRAYSOESPROC) (GLsizei n, const GLuint *arrays);
typedef void (GL_APIENTRYP RGLSYMGLGENVERTEXARRAYSOESPROC) (GLsizei n, GLuint *arrays);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISVERTEXARRAYOESPROC) (GLuint array);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGECONTROLPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGEINSERTPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGECALLBACKPROC) (RGLGENGLDEBUGPROC callback, const void *userParam);
typedef GLuint (GL_APIENTRYP RGLSYMGLGETDEBUGMESSAGELOGPROC) (GLuint count, GLsizei bufsize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (GL_APIENTRYP RGLSYMGLPUSHDEBUGGROUPPROC) (GLenum source, GLuint id, GLsizei length, const GLchar *message);
typedef void (GL_APIENTRYP RGLSYMGLPOPDEBUGGROUPPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLOBJECTPTRLABELPROC) (const void *ptr, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETOBJECTPTRLABELPROC) (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETPOINTERVPROC) (GLenum pname, void **params);

#define glEGLImageTargetTexture2DOES __rglgen_glEGLImageTargetTexture2DOES
#define glEGLImageTargetRenderbufferStorageOES __rglgen_glEGLImageTargetRenderbufferStorageOES
#define glGetProgramBinaryOES __rglgen_glGetProgramBinaryOES
#define glProgramBinaryOES __rglgen_glProgramBinaryOES
#define glMapBufferOES __rglgen_glMapBufferOES
#define glUnmapBufferOES __rglgen_glUnmapBufferOES
#define glGetBufferPointervOES __rglgen_glGetBufferPointervOES
#define glTexImage3DOES __rglgen_glTexImage3DOES
#define glTexSubImage3DOES __rglgen_glTexSubImage3DOES
#define glCopyTexSubImage3DOES __rglgen_glCopyTexSubImage3DOES
#define glCompressedTexImage3DOES __rglgen_glCompressedTexImage3DOES
#define glCompressedTexSubImage3DOES __rglgen_glCompressedTexSubImage3DOES
#define glFramebufferTexture3DOES __rglgen_glFramebufferTexture3DOES
#define glBindVertexArrayOES __rglgen_glBindVertexArrayOES
#define glDeleteVertexArraysOES __rglgen_glDeleteVertexArraysOES
#define glGenVertexArraysOES __rglgen_glGenVertexArraysOES
#define glIsVertexArrayOES __rglgen_glIsVertexArrayOES
#define glDebugMessageControl __rglgen_glDebugMessageControl
#define glDebugMessageInsert __rglgen_glDebugMessageInsert
#define glDebugMessageCallback __rglgen_glDebugMessageCallback
#define glGetDebugMessageLog __rglgen_glGetDebugMessageLog
#define glPushDebugGroup __rglgen_glPushDebugGroup
#define glPopDebugGroup __rglgen_glPopDebugGroup
#define glObjectLabel __rglgen_glObjectLabel
#define glGetObjectLabel __rglgen_glGetObjectLabel
#define glObjectPtrLabel __rglgen_glObjectPtrLabel
#define glGetObjectPtrLabel __rglgen_glGetObjectPtrLabel
#define glGetPointerv __rglgen_glGetPointerv

extern RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC __rglgen_glEGLImageTargetTexture2DOES;
extern RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC __rglgen_glEGLImageTargetRenderbufferStorageOES;
extern RGLSYMGLGETPROGRAMBINARYOESPROC __rglgen_glGetProgramBinaryOES;
extern RGLSYMGLPROGRAMBINARYOESPROC __rglgen_glProgramBinaryOES;
extern RGLSYMGLMAPBUFFEROESPROC __rglgen_glMapBufferOES;
extern RGLSYMGLUNMAPBUFFEROESPROC __rglgen_glUnmapBufferOES;
extern RGLSYMGLGETBUFFERPOINTERVOESPROC __rglgen_glGetBufferPointervOES;
extern RGLSYMGLTEXIMAGE3DOESPROC __rglgen_glTexImage3DOES;
extern RGLSYMGLTEXSUBIMAGE3DOESPROC __rglgen_glTexSubImage3DOES;
extern RGLSYMGLCOPYTEXSUBIMAGE3DOESPROC __rglgen_glCopyTexSubImage3DOES;
extern RGLSYMGLCOMPRESSEDTEXIMAGE3DOESPROC __rglgen_glCompressedTexImage3DOES;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DOESPROC __rglgen_glCompressedTexSubImage3DOES;
extern RGLSYMGLFRAMEBUFFERTEXTURE3DOESPROC __rglgen_glFramebufferTexture3DOES;
extern RGLSYMGLBINDVERTEXARRAYOESPROC __rglgen_glBindVertexArrayOES;
extern RGLSYMGLDELETEVERTEXARRAYSOESPROC __rglgen_glDeleteVertexArraysOES;
extern RGLSYMGLGENVERTEXARRAYSOESPROC __rglgen_glGenVertexArraysOES;
extern RGLSYMGLISVERTEXARRAYOESPROC __rglgen_glIsVertexArrayOES;
extern RGLSYMGLDEBUGMESSAGECONTROLPROC __rglgen_glDebugMessageControl;
extern RGLSYMGLDEBUGMESSAGEINSERTPROC __rglgen_glDebugMessageInsert;
extern RGLSYMGLDEBUGMESSAGECALLBACKPROC __rglgen_glDebugMessageCallback;
extern RGLSYMGLGETDEBUGMESSAGELOGPROC __rglgen_glGetDebugMessageLog;
extern RGLSYMGLPUSHDEBUGGROUPPROC __rglgen_glPushDebugGroup;
extern RGLSYMGLPOPDEBUGGROUPPROC __rglgen_glPopDebugGroup;
extern RGLSYMGLOBJECTLABELPROC __rglgen_glObjectLabel;
extern RGLSYMGLGETOBJECTLABELPROC __rglgen_glGetObjectLabel;
extern RGLSYMGLOBJECTPTRLABELPROC __rglgen_glObjectPtrLabel;
extern RGLSYMGLGETOBJECTPTRLABELPROC __rglgen_glGetObjectPtrLabel;
extern RGLSYMGLGETPOINTERVPROC __rglgen_glGetPointerv;

struct rglgen_sym_map { const char *sym; void *ptr; };
extern const struct rglgen_sym_map rglgen_symbol_map[];
#endif
