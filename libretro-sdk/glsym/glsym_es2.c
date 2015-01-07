/* Copyright (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro SDK code part (glsym).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <glsym/glsym.h>
#include <stddef.h>

#define SYM(x) { "gl" #x, &(gl##x) }
const struct rglgen_sym_map rglgen_symbol_map[] = {
    SYM(DebugMessageControlKHR),
    SYM(DebugMessageInsertKHR),
    SYM(DebugMessageCallbackKHR),
    SYM(GetDebugMessageLogKHR),
    SYM(PushDebugGroupKHR),
    SYM(PopDebugGroupKHR),
    SYM(ObjectLabelKHR),
    SYM(GetObjectLabelKHR),
    SYM(ObjectPtrLabelKHR),
    SYM(GetObjectPtrLabelKHR),
    SYM(GetPointervKHR),
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

    { NULL, NULL },
};
RGLSYMGLDEBUGMESSAGECONTROLKHRPROC __rglgen_glDebugMessageControlKHR;
RGLSYMGLDEBUGMESSAGEINSERTKHRPROC __rglgen_glDebugMessageInsertKHR;
RGLSYMGLDEBUGMESSAGECALLBACKKHRPROC __rglgen_glDebugMessageCallbackKHR;
RGLSYMGLGETDEBUGMESSAGELOGKHRPROC __rglgen_glGetDebugMessageLogKHR;
RGLSYMGLPUSHDEBUGGROUPKHRPROC __rglgen_glPushDebugGroupKHR;
RGLSYMGLPOPDEBUGGROUPKHRPROC __rglgen_glPopDebugGroupKHR;
RGLSYMGLOBJECTLABELKHRPROC __rglgen_glObjectLabelKHR;
RGLSYMGLGETOBJECTLABELKHRPROC __rglgen_glGetObjectLabelKHR;
RGLSYMGLOBJECTPTRLABELKHRPROC __rglgen_glObjectPtrLabelKHR;
RGLSYMGLGETOBJECTPTRLABELKHRPROC __rglgen_glGetObjectPtrLabelKHR;
RGLSYMGLGETPOINTERVKHRPROC __rglgen_glGetPointervKHR;
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

