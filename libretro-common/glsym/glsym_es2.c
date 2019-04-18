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
    SYM(ViewportArrayvOES),
    SYM(ViewportIndexedfOES),
    SYM(ViewportIndexedfvOES),
    SYM(ScissorArrayvOES),
    SYM(ScissorIndexedOES),
    SYM(ScissorIndexedvOES),
    SYM(DepthRangeArrayfvOES),
    SYM(DepthRangeIndexedfOES),
    SYM(GetFloati_vOES),
    SYM(DrawArraysInstancedBaseInstanceEXT),
    SYM(DrawElementsInstancedBaseInstanceEXT),
    SYM(DrawElementsInstancedBaseVertexBaseInstanceEXT),
    SYM(BindFragDataLocationIndexedEXT),
    SYM(BindFragDataLocationEXT),
    SYM(GetProgramResourceLocationIndexEXT),
    SYM(GetFragDataIndexEXT),
    SYM(BufferStorageEXT),
    SYM(ClearTexImageEXT),
    SYM(ClearTexSubImageEXT),
    SYM(CopyImageSubDataEXT),
    SYM(LabelObjectEXT),
    SYM(GetObjectLabelEXT),
    SYM(InsertEventMarkerEXT),
    SYM(PushGroupMarkerEXT),
    SYM(PopGroupMarkerEXT),
    SYM(DiscardFramebufferEXT),
    SYM(GenQueriesEXT),
    SYM(DeleteQueriesEXT),
    SYM(IsQueryEXT),
    SYM(BeginQueryEXT),
    SYM(EndQueryEXT),
    SYM(QueryCounterEXT),
    SYM(GetQueryivEXT),
    SYM(GetQueryObjectivEXT),
    SYM(GetQueryObjectuivEXT),
    SYM(DrawBuffersEXT),
    SYM(EnableiEXT),
    SYM(DisableiEXT),
    SYM(BlendEquationiEXT),
    SYM(BlendEquationSeparateiEXT),
    SYM(BlendFunciEXT),
    SYM(BlendFuncSeparateiEXT),
    SYM(ColorMaskiEXT),
    SYM(IsEnablediEXT),
    SYM(DrawElementsBaseVertexEXT),
    SYM(DrawRangeElementsBaseVertexEXT),
    SYM(DrawElementsInstancedBaseVertexEXT),
    SYM(MultiDrawElementsBaseVertexEXT),
    SYM(DrawArraysInstancedEXT),
    SYM(DrawElementsInstancedEXT),
    SYM(FramebufferTextureEXT),
    SYM(VertexAttribDivisorEXT),
    SYM(MapBufferRangeEXT),
    SYM(FlushMappedBufferRangeEXT),
    SYM(MultiDrawArraysEXT),
    SYM(MultiDrawElementsEXT),
    SYM(MultiDrawArraysIndirectEXT),
    SYM(MultiDrawElementsIndirectEXT),
    SYM(RenderbufferStorageMultisampleEXT),
    SYM(FramebufferTexture2DMultisampleEXT),
    SYM(ReadBufferIndexedEXT),
    SYM(DrawBuffersIndexedEXT),
    SYM(GetIntegeri_vEXT),
    SYM(PolygonOffsetClampEXT),
    SYM(PrimitiveBoundingBoxEXT),
    SYM(RasterSamplesEXT),
    SYM(GetGraphicsResetStatusEXT),
    SYM(ReadnPixelsEXT),
    SYM(GetnUniformfvEXT),
    SYM(GetnUniformivEXT),
    SYM(ActiveShaderProgramEXT),
    SYM(BindProgramPipelineEXT),
    SYM(CreateShaderProgramvEXT),
    SYM(DeleteProgramPipelinesEXT),
    SYM(GenProgramPipelinesEXT),
    SYM(GetProgramPipelineInfoLogEXT),
    SYM(GetProgramPipelineivEXT),
    SYM(IsProgramPipelineEXT),
    SYM(ProgramParameteriEXT),
    SYM(ProgramUniform1fEXT),
    SYM(ProgramUniform1fvEXT),
    SYM(ProgramUniform1iEXT),
    SYM(ProgramUniform1ivEXT),
    SYM(ProgramUniform2fEXT),
    SYM(ProgramUniform2fvEXT),
    SYM(ProgramUniform2iEXT),
    SYM(ProgramUniform2ivEXT),
    SYM(ProgramUniform3fEXT),
    SYM(ProgramUniform3fvEXT),
    SYM(ProgramUniform3iEXT),
    SYM(ProgramUniform3ivEXT),
    SYM(ProgramUniform4fEXT),
    SYM(ProgramUniform4fvEXT),
    SYM(ProgramUniform4iEXT),
    SYM(ProgramUniform4ivEXT),
    SYM(ProgramUniformMatrix2fvEXT),
    SYM(ProgramUniformMatrix3fvEXT),
    SYM(ProgramUniformMatrix4fvEXT),
    SYM(UseProgramStagesEXT),
    SYM(ValidateProgramPipelineEXT),
    SYM(ProgramUniform1uiEXT),
    SYM(ProgramUniform2uiEXT),
    SYM(ProgramUniform3uiEXT),
    SYM(ProgramUniform4uiEXT),
    SYM(ProgramUniform1uivEXT),
    SYM(ProgramUniform2uivEXT),
    SYM(ProgramUniform3uivEXT),
    SYM(ProgramUniform4uivEXT),
    SYM(ProgramUniformMatrix2x3fvEXT),
    SYM(ProgramUniformMatrix3x2fvEXT),
    SYM(ProgramUniformMatrix2x4fvEXT),
    SYM(ProgramUniformMatrix4x2fvEXT),
    SYM(ProgramUniformMatrix3x4fvEXT),
    SYM(ProgramUniformMatrix4x3fvEXT),
    SYM(FramebufferPixelLocalStorageSizeEXT),
    SYM(GetFramebufferPixelLocalStorageSizeEXT),
    SYM(ClearPixelLocalStorageuiEXT),
    SYM(TexPageCommitmentEXT),
    SYM(PatchParameteriEXT),
    SYM(TexParameterIivEXT),
    SYM(TexParameterIuivEXT),
    SYM(GetTexParameterIivEXT),
    SYM(GetTexParameterIuivEXT),
    SYM(SamplerParameterIivEXT),
    SYM(SamplerParameterIuivEXT),
    SYM(GetSamplerParameterIivEXT),
    SYM(GetSamplerParameterIuivEXT),
    SYM(TexBufferEXT),
    SYM(TexBufferRangeEXT),
    SYM(TexStorage1DEXT),
    SYM(TexStorage2DEXT),
    SYM(TexStorage3DEXT),
    SYM(TextureStorage1DEXT),
    SYM(TextureStorage2DEXT),
    SYM(TextureStorage3DEXT),
    SYM(TextureViewEXT),
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
RGLSYMGLVIEWPORTARRAYVOESPROC __rglgen_glViewportArrayvOES;
RGLSYMGLVIEWPORTINDEXEDFOESPROC __rglgen_glViewportIndexedfOES;
RGLSYMGLVIEWPORTINDEXEDFVOESPROC __rglgen_glViewportIndexedfvOES;
RGLSYMGLSCISSORARRAYVOESPROC __rglgen_glScissorArrayvOES;
RGLSYMGLSCISSORINDEXEDOESPROC __rglgen_glScissorIndexedOES;
RGLSYMGLSCISSORINDEXEDVOESPROC __rglgen_glScissorIndexedvOES;
RGLSYMGLDEPTHRANGEARRAYFVOESPROC __rglgen_glDepthRangeArrayfvOES;
RGLSYMGLDEPTHRANGEINDEXEDFOESPROC __rglgen_glDepthRangeIndexedfOES;
RGLSYMGLGETFLOATI_VOESPROC __rglgen_glGetFloati_vOES;
RGLSYMGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC __rglgen_glDrawArraysInstancedBaseInstanceEXT;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC __rglgen_glDrawElementsInstancedBaseInstanceEXT;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC __rglgen_glDrawElementsInstancedBaseVertexBaseInstanceEXT;
RGLSYMGLBINDFRAGDATALOCATIONINDEXEDEXTPROC __rglgen_glBindFragDataLocationIndexedEXT;
RGLSYMGLBINDFRAGDATALOCATIONEXTPROC __rglgen_glBindFragDataLocationEXT;
RGLSYMGLGETPROGRAMRESOURCELOCATIONINDEXEXTPROC __rglgen_glGetProgramResourceLocationIndexEXT;
RGLSYMGLGETFRAGDATAINDEXEXTPROC __rglgen_glGetFragDataIndexEXT;
RGLSYMGLBUFFERSTORAGEEXTPROC __rglgen_glBufferStorageEXT;
RGLSYMGLCLEARTEXIMAGEEXTPROC __rglgen_glClearTexImageEXT;
RGLSYMGLCLEARTEXSUBIMAGEEXTPROC __rglgen_glClearTexSubImageEXT;
RGLSYMGLCOPYIMAGESUBDATAEXTPROC __rglgen_glCopyImageSubDataEXT;
RGLSYMGLLABELOBJECTEXTPROC __rglgen_glLabelObjectEXT;
RGLSYMGLGETOBJECTLABELEXTPROC __rglgen_glGetObjectLabelEXT;
RGLSYMGLINSERTEVENTMARKEREXTPROC __rglgen_glInsertEventMarkerEXT;
RGLSYMGLPUSHGROUPMARKEREXTPROC __rglgen_glPushGroupMarkerEXT;
RGLSYMGLPOPGROUPMARKEREXTPROC __rglgen_glPopGroupMarkerEXT;
RGLSYMGLDISCARDFRAMEBUFFEREXTPROC __rglgen_glDiscardFramebufferEXT;
RGLSYMGLGENQUERIESEXTPROC __rglgen_glGenQueriesEXT;
RGLSYMGLDELETEQUERIESEXTPROC __rglgen_glDeleteQueriesEXT;
RGLSYMGLISQUERYEXTPROC __rglgen_glIsQueryEXT;
RGLSYMGLBEGINQUERYEXTPROC __rglgen_glBeginQueryEXT;
RGLSYMGLENDQUERYEXTPROC __rglgen_glEndQueryEXT;
RGLSYMGLQUERYCOUNTEREXTPROC __rglgen_glQueryCounterEXT;
RGLSYMGLGETQUERYIVEXTPROC __rglgen_glGetQueryivEXT;
RGLSYMGLGETQUERYOBJECTIVEXTPROC __rglgen_glGetQueryObjectivEXT;
RGLSYMGLGETQUERYOBJECTUIVEXTPROC __rglgen_glGetQueryObjectuivEXT;
RGLSYMGLDRAWBUFFERSEXTPROC __rglgen_glDrawBuffersEXT;
RGLSYMGLENABLEIEXTPROC __rglgen_glEnableiEXT;
RGLSYMGLDISABLEIEXTPROC __rglgen_glDisableiEXT;
RGLSYMGLBLENDEQUATIONIEXTPROC __rglgen_glBlendEquationiEXT;
RGLSYMGLBLENDEQUATIONSEPARATEIEXTPROC __rglgen_glBlendEquationSeparateiEXT;
RGLSYMGLBLENDFUNCIEXTPROC __rglgen_glBlendFunciEXT;
RGLSYMGLBLENDFUNCSEPARATEIEXTPROC __rglgen_glBlendFuncSeparateiEXT;
RGLSYMGLCOLORMASKIEXTPROC __rglgen_glColorMaskiEXT;
RGLSYMGLISENABLEDIEXTPROC __rglgen_glIsEnablediEXT;
RGLSYMGLDRAWELEMENTSBASEVERTEXEXTPROC __rglgen_glDrawElementsBaseVertexEXT;
RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXEXTPROC __rglgen_glDrawRangeElementsBaseVertexEXT;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXEXTPROC __rglgen_glDrawElementsInstancedBaseVertexEXT;
RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC __rglgen_glMultiDrawElementsBaseVertexEXT;
RGLSYMGLDRAWARRAYSINSTANCEDEXTPROC __rglgen_glDrawArraysInstancedEXT;
RGLSYMGLDRAWELEMENTSINSTANCEDEXTPROC __rglgen_glDrawElementsInstancedEXT;
RGLSYMGLFRAMEBUFFERTEXTUREEXTPROC __rglgen_glFramebufferTextureEXT;
RGLSYMGLVERTEXATTRIBDIVISOREXTPROC __rglgen_glVertexAttribDivisorEXT;
RGLSYMGLMAPBUFFERRANGEEXTPROC __rglgen_glMapBufferRangeEXT;
RGLSYMGLFLUSHMAPPEDBUFFERRANGEEXTPROC __rglgen_glFlushMappedBufferRangeEXT;
RGLSYMGLMULTIDRAWARRAYSEXTPROC __rglgen_glMultiDrawArraysEXT;
RGLSYMGLMULTIDRAWELEMENTSEXTPROC __rglgen_glMultiDrawElementsEXT;
RGLSYMGLMULTIDRAWARRAYSINDIRECTEXTPROC __rglgen_glMultiDrawArraysIndirectEXT;
RGLSYMGLMULTIDRAWELEMENTSINDIRECTEXTPROC __rglgen_glMultiDrawElementsIndirectEXT;
RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC __rglgen_glRenderbufferStorageMultisampleEXT;
RGLSYMGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC __rglgen_glFramebufferTexture2DMultisampleEXT;
RGLSYMGLREADBUFFERINDEXEDEXTPROC __rglgen_glReadBufferIndexedEXT;
RGLSYMGLDRAWBUFFERSINDEXEDEXTPROC __rglgen_glDrawBuffersIndexedEXT;
RGLSYMGLGETINTEGERI_VEXTPROC __rglgen_glGetIntegeri_vEXT;
RGLSYMGLPOLYGONOFFSETCLAMPEXTPROC __rglgen_glPolygonOffsetClampEXT;
RGLSYMGLPRIMITIVEBOUNDINGBOXEXTPROC __rglgen_glPrimitiveBoundingBoxEXT;
RGLSYMGLRASTERSAMPLESEXTPROC __rglgen_glRasterSamplesEXT;
RGLSYMGLGETGRAPHICSRESETSTATUSEXTPROC __rglgen_glGetGraphicsResetStatusEXT;
RGLSYMGLREADNPIXELSEXTPROC __rglgen_glReadnPixelsEXT;
RGLSYMGLGETNUNIFORMFVEXTPROC __rglgen_glGetnUniformfvEXT;
RGLSYMGLGETNUNIFORMIVEXTPROC __rglgen_glGetnUniformivEXT;
RGLSYMGLACTIVESHADERPROGRAMEXTPROC __rglgen_glActiveShaderProgramEXT;
RGLSYMGLBINDPROGRAMPIPELINEEXTPROC __rglgen_glBindProgramPipelineEXT;
RGLSYMGLCREATESHADERPROGRAMVEXTPROC __rglgen_glCreateShaderProgramvEXT;
RGLSYMGLDELETEPROGRAMPIPELINESEXTPROC __rglgen_glDeleteProgramPipelinesEXT;
RGLSYMGLGENPROGRAMPIPELINESEXTPROC __rglgen_glGenProgramPipelinesEXT;
RGLSYMGLGETPROGRAMPIPELINEINFOLOGEXTPROC __rglgen_glGetProgramPipelineInfoLogEXT;
RGLSYMGLGETPROGRAMPIPELINEIVEXTPROC __rglgen_glGetProgramPipelineivEXT;
RGLSYMGLISPROGRAMPIPELINEEXTPROC __rglgen_glIsProgramPipelineEXT;
RGLSYMGLPROGRAMPARAMETERIEXTPROC __rglgen_glProgramParameteriEXT;
RGLSYMGLPROGRAMUNIFORM1FEXTPROC __rglgen_glProgramUniform1fEXT;
RGLSYMGLPROGRAMUNIFORM1FVEXTPROC __rglgen_glProgramUniform1fvEXT;
RGLSYMGLPROGRAMUNIFORM1IEXTPROC __rglgen_glProgramUniform1iEXT;
RGLSYMGLPROGRAMUNIFORM1IVEXTPROC __rglgen_glProgramUniform1ivEXT;
RGLSYMGLPROGRAMUNIFORM2FEXTPROC __rglgen_glProgramUniform2fEXT;
RGLSYMGLPROGRAMUNIFORM2FVEXTPROC __rglgen_glProgramUniform2fvEXT;
RGLSYMGLPROGRAMUNIFORM2IEXTPROC __rglgen_glProgramUniform2iEXT;
RGLSYMGLPROGRAMUNIFORM2IVEXTPROC __rglgen_glProgramUniform2ivEXT;
RGLSYMGLPROGRAMUNIFORM3FEXTPROC __rglgen_glProgramUniform3fEXT;
RGLSYMGLPROGRAMUNIFORM3FVEXTPROC __rglgen_glProgramUniform3fvEXT;
RGLSYMGLPROGRAMUNIFORM3IEXTPROC __rglgen_glProgramUniform3iEXT;
RGLSYMGLPROGRAMUNIFORM3IVEXTPROC __rglgen_glProgramUniform3ivEXT;
RGLSYMGLPROGRAMUNIFORM4FEXTPROC __rglgen_glProgramUniform4fEXT;
RGLSYMGLPROGRAMUNIFORM4FVEXTPROC __rglgen_glProgramUniform4fvEXT;
RGLSYMGLPROGRAMUNIFORM4IEXTPROC __rglgen_glProgramUniform4iEXT;
RGLSYMGLPROGRAMUNIFORM4IVEXTPROC __rglgen_glProgramUniform4ivEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2FVEXTPROC __rglgen_glProgramUniformMatrix2fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3FVEXTPROC __rglgen_glProgramUniformMatrix3fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4FVEXTPROC __rglgen_glProgramUniformMatrix4fvEXT;
RGLSYMGLUSEPROGRAMSTAGESEXTPROC __rglgen_glUseProgramStagesEXT;
RGLSYMGLVALIDATEPROGRAMPIPELINEEXTPROC __rglgen_glValidateProgramPipelineEXT;
RGLSYMGLPROGRAMUNIFORM1UIEXTPROC __rglgen_glProgramUniform1uiEXT;
RGLSYMGLPROGRAMUNIFORM2UIEXTPROC __rglgen_glProgramUniform2uiEXT;
RGLSYMGLPROGRAMUNIFORM3UIEXTPROC __rglgen_glProgramUniform3uiEXT;
RGLSYMGLPROGRAMUNIFORM4UIEXTPROC __rglgen_glProgramUniform4uiEXT;
RGLSYMGLPROGRAMUNIFORM1UIVEXTPROC __rglgen_glProgramUniform1uivEXT;
RGLSYMGLPROGRAMUNIFORM2UIVEXTPROC __rglgen_glProgramUniform2uivEXT;
RGLSYMGLPROGRAMUNIFORM3UIVEXTPROC __rglgen_glProgramUniform3uivEXT;
RGLSYMGLPROGRAMUNIFORM4UIVEXTPROC __rglgen_glProgramUniform4uivEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC __rglgen_glProgramUniformMatrix2x3fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC __rglgen_glProgramUniformMatrix3x2fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC __rglgen_glProgramUniformMatrix2x4fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC __rglgen_glProgramUniformMatrix4x2fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC __rglgen_glProgramUniformMatrix3x4fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC __rglgen_glProgramUniformMatrix4x3fvEXT;
RGLSYMGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC __rglgen_glFramebufferPixelLocalStorageSizeEXT;
RGLSYMGLGETFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC __rglgen_glGetFramebufferPixelLocalStorageSizeEXT;
RGLSYMGLCLEARPIXELLOCALSTORAGEUIEXTPROC __rglgen_glClearPixelLocalStorageuiEXT;
RGLSYMGLTEXPAGECOMMITMENTEXTPROC __rglgen_glTexPageCommitmentEXT;
RGLSYMGLPATCHPARAMETERIEXTPROC __rglgen_glPatchParameteriEXT;
RGLSYMGLTEXPARAMETERIIVEXTPROC __rglgen_glTexParameterIivEXT;
RGLSYMGLTEXPARAMETERIUIVEXTPROC __rglgen_glTexParameterIuivEXT;
RGLSYMGLGETTEXPARAMETERIIVEXTPROC __rglgen_glGetTexParameterIivEXT;
RGLSYMGLGETTEXPARAMETERIUIVEXTPROC __rglgen_glGetTexParameterIuivEXT;
RGLSYMGLSAMPLERPARAMETERIIVEXTPROC __rglgen_glSamplerParameterIivEXT;
RGLSYMGLSAMPLERPARAMETERIUIVEXTPROC __rglgen_glSamplerParameterIuivEXT;
RGLSYMGLGETSAMPLERPARAMETERIIVEXTPROC __rglgen_glGetSamplerParameterIivEXT;
RGLSYMGLGETSAMPLERPARAMETERIUIVEXTPROC __rglgen_glGetSamplerParameterIuivEXT;
RGLSYMGLTEXBUFFEREXTPROC __rglgen_glTexBufferEXT;
RGLSYMGLTEXBUFFERRANGEEXTPROC __rglgen_glTexBufferRangeEXT;
RGLSYMGLTEXSTORAGE1DEXTPROC __rglgen_glTexStorage1DEXT;
RGLSYMGLTEXSTORAGE2DEXTPROC __rglgen_glTexStorage2DEXT;
RGLSYMGLTEXSTORAGE3DEXTPROC __rglgen_glTexStorage3DEXT;
RGLSYMGLTEXTURESTORAGE1DEXTPROC __rglgen_glTextureStorage1DEXT;
RGLSYMGLTEXTURESTORAGE2DEXTPROC __rglgen_glTextureStorage2DEXT;
RGLSYMGLTEXTURESTORAGE3DEXTPROC __rglgen_glTextureStorage3DEXT;
RGLSYMGLTEXTUREVIEWEXTPROC __rglgen_glTextureViewEXT;
RGLSYMGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC __rglgen_glFramebufferTextureMultiviewOVR;
RGLSYMGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC __rglgen_glFramebufferTextureMultisampleMultiviewOVR;
