#include "glsym/glsym.h"
#include <stddef.h>
#define SYM(x) { "gl" #x, &(gl##x) }
const struct rglgen_sym_map rglgen_symbol_map[] = {
    SYM(BlendBarrierKHR),
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
    SYM(GetGraphicsResetStatusKHR),
    SYM(ReadnPixelsKHR),
    SYM(GetnUniformfvKHR),
    SYM(GetnUniformivKHR),
    SYM(GetnUniformuivKHR),
    SYM(EGLImageTargetTexture2DOES),
    SYM(EGLImageTargetRenderbufferStorageOES),
    SYM(CopyImageSubDataOES),
    SYM(EnableiOES),
    SYM(DisableiOES),
    SYM(BlendEquationiOES),
    SYM(BlendEquationSeparateiOES),
    SYM(BlendFunciOES),
    SYM(BlendFuncSeparateiOES),
    SYM(ColorMaskiOES),
    SYM(IsEnablediOES),
    SYM(DrawElementsBaseVertexOES),
    SYM(DrawRangeElementsBaseVertexOES),
    SYM(DrawElementsInstancedBaseVertexOES),
    SYM(MultiDrawElementsBaseVertexOES),
    SYM(FramebufferTextureOES),
    SYM(GetProgramBinaryOES),
    SYM(ProgramBinaryOES),
    SYM(MapBufferOES),
    SYM(UnmapBufferOES),
    SYM(GetBufferPointervOES),
    SYM(PrimitiveBoundingBoxOES),
    SYM(MinSampleShadingOES),
    SYM(PatchParameteriOES),
    SYM(TexImage3DOES),
    SYM(TexSubImage3DOES),
    SYM(CopyTexSubImage3DOES),
    SYM(CompressedTexImage3DOES),
    SYM(CompressedTexSubImage3DOES),
    SYM(FramebufferTexture3DOES),
    SYM(TexParameterIivOES),
    SYM(TexParameterIuivOES),
    SYM(GetTexParameterIivOES),
    SYM(GetTexParameterIuivOES),
    SYM(SamplerParameterIivOES),
    SYM(SamplerParameterIuivOES),
    SYM(GetSamplerParameterIivOES),
    SYM(GetSamplerParameterIuivOES),
    SYM(TexBufferOES),
    SYM(TexBufferRangeOES),
    SYM(TexStorage3DMultisampleOES),
    SYM(TextureViewOES),
    SYM(BindVertexArrayOES),
    SYM(DeleteVertexArraysOES),
    SYM(GenVertexArraysOES),
    SYM(IsVertexArrayOES),
    SYM(FramebufferTextureMultiviewOVR),
    SYM(FramebufferTextureMultisampleMultiviewOVR),

    { NULL, NULL },
};
RGLSYMGLBLENDBARRIERKHRPROC __rglgen_glBlendBarrierKHR;
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
RGLSYMGLGETGRAPHICSRESETSTATUSKHRPROC __rglgen_glGetGraphicsResetStatusKHR;
RGLSYMGLREADNPIXELSKHRPROC __rglgen_glReadnPixelsKHR;
RGLSYMGLGETNUNIFORMFVKHRPROC __rglgen_glGetnUniformfvKHR;
RGLSYMGLGETNUNIFORMIVKHRPROC __rglgen_glGetnUniformivKHR;
RGLSYMGLGETNUNIFORMUIVKHRPROC __rglgen_glGetnUniformuivKHR;
RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC __rglgen_glEGLImageTargetTexture2DOES;
RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC __rglgen_glEGLImageTargetRenderbufferStorageOES;
RGLSYMGLCOPYIMAGESUBDATAOESPROC __rglgen_glCopyImageSubDataOES;
RGLSYMGLENABLEIOESPROC __rglgen_glEnableiOES;
RGLSYMGLDISABLEIOESPROC __rglgen_glDisableiOES;
RGLSYMGLBLENDEQUATIONIOESPROC __rglgen_glBlendEquationiOES;
RGLSYMGLBLENDEQUATIONSEPARATEIOESPROC __rglgen_glBlendEquationSeparateiOES;
RGLSYMGLBLENDFUNCIOESPROC __rglgen_glBlendFunciOES;
RGLSYMGLBLENDFUNCSEPARATEIOESPROC __rglgen_glBlendFuncSeparateiOES;
RGLSYMGLCOLORMASKIOESPROC __rglgen_glColorMaskiOES;
RGLSYMGLISENABLEDIOESPROC __rglgen_glIsEnablediOES;
RGLSYMGLDRAWELEMENTSBASEVERTEXOESPROC __rglgen_glDrawElementsBaseVertexOES;
RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXOESPROC __rglgen_glDrawRangeElementsBaseVertexOES;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC __rglgen_glDrawElementsInstancedBaseVertexOES;
RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXOESPROC __rglgen_glMultiDrawElementsBaseVertexOES;
RGLSYMGLFRAMEBUFFERTEXTUREOESPROC __rglgen_glFramebufferTextureOES;
RGLSYMGLGETPROGRAMBINARYOESPROC __rglgen_glGetProgramBinaryOES;
RGLSYMGLPROGRAMBINARYOESPROC __rglgen_glProgramBinaryOES;
RGLSYMGLMAPBUFFEROESPROC __rglgen_glMapBufferOES;
RGLSYMGLUNMAPBUFFEROESPROC __rglgen_glUnmapBufferOES;
RGLSYMGLGETBUFFERPOINTERVOESPROC __rglgen_glGetBufferPointervOES;
RGLSYMGLPRIMITIVEBOUNDINGBOXOESPROC __rglgen_glPrimitiveBoundingBoxOES;
RGLSYMGLMINSAMPLESHADINGOESPROC __rglgen_glMinSampleShadingOES;
RGLSYMGLPATCHPARAMETERIOESPROC __rglgen_glPatchParameteriOES;
RGLSYMGLTEXIMAGE3DOESPROC __rglgen_glTexImage3DOES;
RGLSYMGLTEXSUBIMAGE3DOESPROC __rglgen_glTexSubImage3DOES;
RGLSYMGLCOPYTEXSUBIMAGE3DOESPROC __rglgen_glCopyTexSubImage3DOES;
RGLSYMGLCOMPRESSEDTEXIMAGE3DOESPROC __rglgen_glCompressedTexImage3DOES;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DOESPROC __rglgen_glCompressedTexSubImage3DOES;
RGLSYMGLFRAMEBUFFERTEXTURE3DOESPROC __rglgen_glFramebufferTexture3DOES;
RGLSYMGLTEXPARAMETERIIVOESPROC __rglgen_glTexParameterIivOES;
RGLSYMGLTEXPARAMETERIUIVOESPROC __rglgen_glTexParameterIuivOES;
RGLSYMGLGETTEXPARAMETERIIVOESPROC __rglgen_glGetTexParameterIivOES;
RGLSYMGLGETTEXPARAMETERIUIVOESPROC __rglgen_glGetTexParameterIuivOES;
RGLSYMGLSAMPLERPARAMETERIIVOESPROC __rglgen_glSamplerParameterIivOES;
RGLSYMGLSAMPLERPARAMETERIUIVOESPROC __rglgen_glSamplerParameterIuivOES;
RGLSYMGLGETSAMPLERPARAMETERIIVOESPROC __rglgen_glGetSamplerParameterIivOES;
RGLSYMGLGETSAMPLERPARAMETERIUIVOESPROC __rglgen_glGetSamplerParameterIuivOES;
RGLSYMGLTEXBUFFEROESPROC __rglgen_glTexBufferOES;
RGLSYMGLTEXBUFFERRANGEOESPROC __rglgen_glTexBufferRangeOES;
RGLSYMGLTEXSTORAGE3DMULTISAMPLEOESPROC __rglgen_glTexStorage3DMultisampleOES;
RGLSYMGLTEXTUREVIEWOESPROC __rglgen_glTextureViewOES;
RGLSYMGLBINDVERTEXARRAYOESPROC __rglgen_glBindVertexArrayOES;
RGLSYMGLDELETEVERTEXARRAYSOESPROC __rglgen_glDeleteVertexArraysOES;
RGLSYMGLGENVERTEXARRAYSOESPROC __rglgen_glGenVertexArraysOES;
RGLSYMGLISVERTEXARRAYOESPROC __rglgen_glIsVertexArrayOES;
RGLSYMGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC __rglgen_glFramebufferTextureMultiviewOVR;
RGLSYMGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC __rglgen_glFramebufferTextureMultisampleMultiviewOVR;

