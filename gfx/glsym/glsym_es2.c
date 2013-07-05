#include "glsym.h"
#include <stddef.h>
#define SYM(x) { "gl" #x, &(gl##x) }
const struct rglgen_sym_map rglgen_symbol_map_rarch[] = {
    SYM(EGLImageTargetTexture2DOES),
    SYM(EGLImageTargetRenderbufferStorageOES),
    SYM(GetProgramBinaryOES),
    SYM(ProgramBinaryOES),
    SYM(MapBufferOES),
    SYM(UnmapBufferOES),
    SYM(GetBufferPointervOES),
    SYM(TexImage3DOES),
    SYM(TexSubImage3DOES),
    SYM(CopyTexSubImage3DOES),
    SYM(CompressedTexImage3DOES),
    SYM(CompressedTexSubImage3DOES),
    SYM(FramebufferTexture3DOES),
    SYM(BindVertexArrayOES),
    SYM(DeleteVertexArraysOES),
    SYM(GenVertexArraysOES),
    SYM(IsVertexArrayOES),
    SYM(DebugMessageControl),
    SYM(DebugMessageInsert),
    SYM(DebugMessageCallback),
    SYM(GetDebugMessageLog),
    SYM(PushDebugGroup),
    SYM(PopDebugGroup),
    SYM(ObjectLabel),
    SYM(GetObjectLabel),
    SYM(ObjectPtrLabel),
    SYM(GetObjectPtrLabel),
    SYM(GetPointerv),

    { NULL, NULL },
};
RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC __rglgen_glEGLImageTargetTexture2DOES;
RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC __rglgen_glEGLImageTargetRenderbufferStorageOES;
RGLSYMGLGETPROGRAMBINARYOESPROC __rglgen_glGetProgramBinaryOES;
RGLSYMGLPROGRAMBINARYOESPROC __rglgen_glProgramBinaryOES;
RGLSYMGLMAPBUFFEROESPROC __rglgen_glMapBufferOES;
RGLSYMGLUNMAPBUFFEROESPROC __rglgen_glUnmapBufferOES;
RGLSYMGLGETBUFFERPOINTERVOESPROC __rglgen_glGetBufferPointervOES;
RGLSYMGLTEXIMAGE3DOESPROC __rglgen_glTexImage3DOES;
RGLSYMGLTEXSUBIMAGE3DOESPROC __rglgen_glTexSubImage3DOES;
RGLSYMGLCOPYTEXSUBIMAGE3DOESPROC __rglgen_glCopyTexSubImage3DOES;
RGLSYMGLCOMPRESSEDTEXIMAGE3DOESPROC __rglgen_glCompressedTexImage3DOES;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DOESPROC __rglgen_glCompressedTexSubImage3DOES;
RGLSYMGLFRAMEBUFFERTEXTURE3DOESPROC __rglgen_glFramebufferTexture3DOES;
RGLSYMGLBINDVERTEXARRAYOESPROC __rglgen_glBindVertexArrayOES;
RGLSYMGLDELETEVERTEXARRAYSOESPROC __rglgen_glDeleteVertexArraysOES;
RGLSYMGLGENVERTEXARRAYSOESPROC __rglgen_glGenVertexArraysOES;
RGLSYMGLISVERTEXARRAYOESPROC __rglgen_glIsVertexArrayOES;
RGLSYMGLDEBUGMESSAGECONTROLPROC __rglgen_glDebugMessageControl;
RGLSYMGLDEBUGMESSAGEINSERTPROC __rglgen_glDebugMessageInsert;
RGLSYMGLDEBUGMESSAGECALLBACKPROC __rglgen_glDebugMessageCallback;
RGLSYMGLGETDEBUGMESSAGELOGPROC __rglgen_glGetDebugMessageLog;
RGLSYMGLPUSHDEBUGGROUPPROC __rglgen_glPushDebugGroup;
RGLSYMGLPOPDEBUGGROUPPROC __rglgen_glPopDebugGroup;
RGLSYMGLOBJECTLABELPROC __rglgen_glObjectLabel;
RGLSYMGLGETOBJECTLABELPROC __rglgen_glGetObjectLabel;
RGLSYMGLOBJECTPTRLABELPROC __rglgen_glObjectPtrLabel;
RGLSYMGLGETOBJECTPTRLABELPROC __rglgen_glGetObjectPtrLabel;
RGLSYMGLGETPOINTERVPROC __rglgen_glGetPointerv;

