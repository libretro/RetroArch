/* Copyright (C) 2010-2018 The RetroArch team
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

#include <stddef.h>

#include <glsym/glsym.h>

#define SYM(x) { "gl" #x, (void*)&(gl##x) }

const struct rglgen_sym_map rglgen_symbol_map[] = {
#ifdef HAVE_LIBNX
    SYM(ClearIndex),
    SYM(ClearColor),
    SYM(Clear),
    SYM(IndexMask),
    SYM(ColorMask),
    SYM(AlphaFunc),
    SYM(BlendFunc),
    SYM(LogicOp),
    SYM(CullFace),
    SYM(FrontFace),
    SYM(PointSize),
    SYM(LineWidth),
    SYM(LineStipple),
    SYM(PolygonMode),
    SYM(PolygonOffset),
    SYM(PolygonStipple),
    SYM(GetPolygonStipple),
    SYM(EdgeFlag),
    SYM(EdgeFlagv),
    SYM(Scissor),
    SYM(ClipPlane),
    SYM(GetClipPlane),
    SYM(DrawBuffer),
    SYM(ReadBuffer),
    SYM(Enable),
    SYM(Disable),
    SYM(IsEnabled),
    SYM(EnableClientState),
    SYM(DisableClientState),
    SYM(GetBooleanv),
    SYM(GetDoublev),
    SYM(GetFloatv),
    SYM(GetIntegerv),
    SYM(PushAttrib),
    SYM(PopAttrib),
    SYM(PushClientAttrib),
    SYM(PopClientAttrib),
    SYM(RenderMode),
    SYM(GetError),
    SYM(GetString),
    SYM(Finish),
    SYM(Flush),
    SYM(Hint),
    SYM(ClearDepth),
    SYM(DepthFunc),
    SYM(DepthMask),
    SYM(DepthRange),
    SYM(ClearAccum),
    SYM(Accum),
    SYM(MatrixMode),
    SYM(Ortho),
    SYM(Frustum),
    SYM(Viewport),
    SYM(PushMatrix),
    SYM(PopMatrix),
    SYM(LoadIdentity),
    SYM(LoadMatrixd),
    SYM(LoadMatrixf),
    SYM(MultMatrixd),
    SYM(MultMatrixf),
    SYM(Rotated),
    SYM(Rotatef),
    SYM(Scaled),
    SYM(Scalef),
    SYM(Translated),
    SYM(Translatef),
    SYM(IsList),
    SYM(DeleteLists),
    SYM(GenLists),
    SYM(NewList),
    SYM(EndList),
    SYM(CallList),
    SYM(CallLists),
    SYM(ListBase),
    SYM(Begin),
    SYM(End),
    SYM(Vertex2d),
    SYM(Vertex2f),
    SYM(Vertex2i),
    SYM(Vertex2s),
    SYM(Vertex3d),
    SYM(Vertex3f),
    SYM(Vertex3i),
    SYM(Vertex3s),
    SYM(Vertex4d),
    SYM(Vertex4f),
    SYM(Vertex4i),
    SYM(Vertex4s),
    SYM(Vertex2dv),
    SYM(Vertex2fv),
    SYM(Vertex2iv),
    SYM(Vertex2sv),
    SYM(Vertex3dv),
    SYM(Vertex3fv),
    SYM(Vertex3iv),
    SYM(Vertex3sv),
    SYM(Vertex4dv),
    SYM(Vertex4fv),
    SYM(Vertex4iv),
    SYM(Vertex4sv),
    SYM(Normal3b),
    SYM(Normal3d),
    SYM(Normal3f),
    SYM(Normal3i),
    SYM(Normal3s),
    SYM(Normal3bv),
    SYM(Normal3dv),
    SYM(Normal3fv),
    SYM(Normal3iv),
    SYM(Normal3sv),
    SYM(Indexd),
    SYM(Indexf),
    SYM(Indexi),
    SYM(Indexs),
    SYM(Indexub),
    SYM(Indexdv),
    SYM(Indexfv),
    SYM(Indexiv),
    SYM(Indexsv),
    SYM(Indexubv),
    SYM(Color3b),
    SYM(Color3d),
    SYM(Color3f),
    SYM(Color3i),
    SYM(Color3s),
    SYM(Color3ub),
    SYM(Color3ui),
    SYM(Color3us),
    SYM(Color4b),
    SYM(Color4d),
    SYM(Color4f),
    SYM(Color4i),
    SYM(Color4s),
    SYM(Color4ub),
    SYM(Color4ui),
    SYM(Color4us),
    SYM(Color3bv),
    SYM(Color3dv),
    SYM(Color3fv),
    SYM(Color3iv),
    SYM(Color3sv),
    SYM(Color3ubv),
    SYM(Color3uiv),
    SYM(Color3usv),
    SYM(Color4bv),
    SYM(Color4dv),
    SYM(Color4fv),
    SYM(Color4iv),
    SYM(Color4sv),
    SYM(Color4ubv),
    SYM(Color4uiv),
    SYM(Color4usv),
    SYM(TexCoord1d),
    SYM(TexCoord1f),
    SYM(TexCoord1i),
    SYM(TexCoord1s),
    SYM(TexCoord2d),
    SYM(TexCoord2f),
    SYM(TexCoord2i),
    SYM(TexCoord2s),
    SYM(TexCoord3d),
    SYM(TexCoord3f),
    SYM(TexCoord3i),
    SYM(TexCoord3s),
    SYM(TexCoord4d),
    SYM(TexCoord4f),
    SYM(TexCoord4i),
    SYM(TexCoord4s),
    SYM(TexCoord1dv),
    SYM(TexCoord1fv),
    SYM(TexCoord1iv),
    SYM(TexCoord1sv),
    SYM(TexCoord2dv),
    SYM(TexCoord2fv),
    SYM(TexCoord2iv),
    SYM(TexCoord2sv),
    SYM(TexCoord3dv),
    SYM(TexCoord3fv),
    SYM(TexCoord3iv),
    SYM(TexCoord3sv),
    SYM(TexCoord4dv),
    SYM(TexCoord4fv),
    SYM(TexCoord4iv),
    SYM(TexCoord4sv),
    SYM(RasterPos2d),
    SYM(RasterPos2f),
    SYM(RasterPos2i),
    SYM(RasterPos2s),
    SYM(RasterPos3d),
    SYM(RasterPos3f),
    SYM(RasterPos3i),
    SYM(RasterPos3s),
    SYM(RasterPos4d),
    SYM(RasterPos4f),
    SYM(RasterPos4i),
    SYM(RasterPos4s),
    SYM(RasterPos2dv),
    SYM(RasterPos2fv),
    SYM(RasterPos2iv),
    SYM(RasterPos2sv),
    SYM(RasterPos3dv),
    SYM(RasterPos3fv),
    SYM(RasterPos3iv),
    SYM(RasterPos3sv),
    SYM(RasterPos4dv),
    SYM(RasterPos4fv),
    SYM(RasterPos4iv),
    SYM(RasterPos4sv),
    SYM(Rectd),
    SYM(Rectf),
    SYM(Recti),
    SYM(Rects),
    SYM(Rectdv),
    SYM(Rectfv),
    SYM(Rectiv),
    SYM(Rectsv),
    SYM(VertexPointer),
    SYM(NormalPointer),
    SYM(ColorPointer),
    SYM(IndexPointer),
    SYM(TexCoordPointer),
    SYM(EdgeFlagPointer),
    SYM(GetPointerv),
    SYM(ArrayElement),
    SYM(DrawArrays),
    SYM(DrawElements),
    SYM(InterleavedArrays),
    SYM(ShadeModel),
    SYM(Lightf),
    SYM(Lighti),
    SYM(Lightfv),
    SYM(Lightiv),
    SYM(GetLightfv),
    SYM(GetLightiv),
    SYM(LightModelf),
    SYM(LightModeli),
    SYM(LightModelfv),
    SYM(LightModeliv),
    SYM(Materialf),
    SYM(Materiali),
    SYM(Materialfv),
    SYM(Materialiv),
    SYM(GetMaterialfv),
    SYM(GetMaterialiv),
    SYM(ColorMaterial),
    SYM(PixelZoom),
    SYM(PixelStoref),
    SYM(PixelStorei),
    SYM(PixelTransferf),
    SYM(PixelTransferi),
    SYM(PixelMapfv),
    SYM(PixelMapuiv),
    SYM(PixelMapusv),
    SYM(GetPixelMapfv),
    SYM(GetPixelMapuiv),
    SYM(GetPixelMapusv),
    SYM(Bitmap),
    SYM(ReadPixels),
    SYM(DrawPixels),
    SYM(CopyPixels),
    SYM(StencilFunc),
    SYM(StencilMask),
    SYM(StencilOp),
    SYM(ClearStencil),
    SYM(TexGend),
    SYM(TexGenf),
    SYM(TexGeni),
    SYM(TexGendv),
    SYM(TexGenfv),
    SYM(TexGeniv),
    SYM(GetTexGendv),
    SYM(GetTexGenfv),
    SYM(GetTexGeniv),
    SYM(TexEnvf),
    SYM(TexEnvi),
    SYM(TexEnvfv),
    SYM(TexEnviv),
    SYM(GetTexEnvfv),
    SYM(GetTexEnviv),
    SYM(TexParameterf),
    SYM(TexParameteri),
    SYM(TexParameterfv),
    SYM(TexParameteriv),
    SYM(GetTexParameterfv),
    SYM(GetTexParameteriv),
    SYM(GetTexLevelParameterfv),
    SYM(GetTexLevelParameteriv),
    SYM(TexImage1D),
    SYM(TexImage2D),
    SYM(GetTexImage),
    SYM(GenTextures),
    SYM(DeleteTextures),
    SYM(BindTexture),
    SYM(PrioritizeTextures),
    SYM(AreTexturesResident),
    SYM(IsTexture),
    SYM(TexSubImage1D),
    SYM(TexSubImage2D),
    SYM(CopyTexImage1D),
    SYM(CopyTexImage2D),
    SYM(CopyTexSubImage1D),
    SYM(CopyTexSubImage2D),
    SYM(Map1d),
    SYM(Map1f),
    SYM(Map2d),
    SYM(Map2f),
    SYM(GetMapdv),
    SYM(GetMapfv),
    SYM(GetMapiv),
    SYM(EvalCoord1d),
    SYM(EvalCoord1f),
    SYM(EvalCoord1dv),
    SYM(EvalCoord1fv),
    SYM(EvalCoord2d),
    SYM(EvalCoord2f),
    SYM(EvalCoord2dv),
    SYM(EvalCoord2fv),
    SYM(MapGrid1d),
    SYM(MapGrid1f),
    SYM(MapGrid2d),
    SYM(MapGrid2f),
    SYM(EvalPoint1),
    SYM(EvalPoint2),
    SYM(EvalMesh1),
    SYM(EvalMesh2),
    SYM(Fogf),
    SYM(Fogi),
    SYM(Fogfv),
    SYM(Fogiv),
    SYM(FeedbackBuffer),
    SYM(PassThrough),
    SYM(SelectBuffer),
    SYM(InitNames),
    SYM(LoadName),
    SYM(PushName),
    SYM(PopName),
    SYM(DrawRangeElements),
    SYM(TexImage3D),
    SYM(TexSubImage3D),
    SYM(CopyTexSubImage3D),
    SYM(ColorTable),
    SYM(ColorSubTable),
    SYM(ColorTableParameteriv),
    SYM(ColorTableParameterfv),
    SYM(CopyColorSubTable),
    SYM(CopyColorTable),
    SYM(GetColorTable),
    SYM(GetColorTableParameterfv),
    SYM(GetColorTableParameteriv),
    SYM(BlendEquation),
    SYM(BlendColor),
    SYM(Histogram),
    SYM(ResetHistogram),
    SYM(GetHistogram),
    SYM(GetHistogramParameterfv),
    SYM(GetHistogramParameteriv),
    SYM(Minmax),
    SYM(ResetMinmax),
    SYM(GetMinmax),
    SYM(GetMinmaxParameterfv),
    SYM(GetMinmaxParameteriv),
    SYM(ConvolutionFilter1D),
    SYM(ConvolutionFilter2D),
    SYM(ConvolutionParameterf),
    SYM(ConvolutionParameterfv),
    SYM(ConvolutionParameteri),
    SYM(ConvolutionParameteriv),
    SYM(CopyConvolutionFilter1D),
    SYM(CopyConvolutionFilter2D),
    SYM(GetConvolutionFilter),
    SYM(GetConvolutionParameterfv),
    SYM(GetConvolutionParameteriv),
    SYM(SeparableFilter2D),
    SYM(GetSeparableFilter),
    SYM(ActiveTexture),
    SYM(ClientActiveTexture),
    SYM(CompressedTexImage1D),
    SYM(CompressedTexImage2D),
    SYM(CompressedTexImage3D),
    SYM(CompressedTexSubImage1D),
    SYM(CompressedTexSubImage2D),
    SYM(CompressedTexSubImage3D),
    SYM(GetCompressedTexImage),
    SYM(MultiTexCoord1d),
    SYM(MultiTexCoord1dv),
    SYM(MultiTexCoord1f),
    SYM(MultiTexCoord1fv),
    SYM(MultiTexCoord1i),
    SYM(MultiTexCoord1iv),
    SYM(MultiTexCoord1s),
    SYM(MultiTexCoord1sv),
    SYM(MultiTexCoord2d),
    SYM(MultiTexCoord2dv),
    SYM(MultiTexCoord2f),
    SYM(MultiTexCoord2fv),
    SYM(MultiTexCoord2i),
    SYM(MultiTexCoord2iv),
    SYM(MultiTexCoord2s),
    SYM(MultiTexCoord2sv),
    SYM(MultiTexCoord3d),
    SYM(MultiTexCoord3dv),
    SYM(MultiTexCoord3f),
    SYM(MultiTexCoord3fv),
    SYM(MultiTexCoord3i),
    SYM(MultiTexCoord3iv),
    SYM(MultiTexCoord3s),
    SYM(MultiTexCoord3sv),
    SYM(MultiTexCoord4d),
    SYM(MultiTexCoord4dv),
    SYM(MultiTexCoord4f),
    SYM(MultiTexCoord4fv),
    SYM(MultiTexCoord4i),
    SYM(MultiTexCoord4iv),
    SYM(MultiTexCoord4s),
    SYM(MultiTexCoord4sv),
    SYM(LoadTransposeMatrixd),
    SYM(LoadTransposeMatrixf),
    SYM(MultTransposeMatrixd),
    SYM(MultTransposeMatrixf),
    SYM(SampleCoverage),
    SYM(ActiveTextureARB),
    SYM(ClientActiveTextureARB),
    SYM(MultiTexCoord1dARB),
    SYM(MultiTexCoord1dvARB),
    SYM(MultiTexCoord1fARB),
    SYM(MultiTexCoord1fvARB),
    SYM(MultiTexCoord1iARB),
    SYM(MultiTexCoord1ivARB),
    SYM(MultiTexCoord1sARB),
    SYM(MultiTexCoord1svARB),
    SYM(MultiTexCoord2dARB),
    SYM(MultiTexCoord2dvARB),
    SYM(MultiTexCoord2fARB),
    SYM(MultiTexCoord2fvARB),
    SYM(MultiTexCoord2iARB),
    SYM(MultiTexCoord2ivARB),
    SYM(MultiTexCoord2sARB),
    SYM(MultiTexCoord2svARB),
    SYM(MultiTexCoord3dARB),
    SYM(MultiTexCoord3dvARB),
    SYM(MultiTexCoord3fARB),
    SYM(MultiTexCoord3fvARB),
    SYM(MultiTexCoord3iARB),
    SYM(MultiTexCoord3ivARB),
    SYM(MultiTexCoord3sARB),
    SYM(MultiTexCoord3svARB),
    SYM(MultiTexCoord4dARB),
    SYM(MultiTexCoord4dvARB),
    SYM(MultiTexCoord4fARB),
    SYM(MultiTexCoord4fvARB),
    SYM(MultiTexCoord4iARB),
    SYM(MultiTexCoord4ivARB),
    SYM(MultiTexCoord4sARB),
    SYM(MultiTexCoord4svARB),
    SYM(EGLImageTargetTexture2DOES),
    SYM(EGLImageTargetRenderbufferStorageOES),
#endif

    SYM(DrawRangeElements),
    SYM(TexImage3D),
    SYM(TexSubImage3D),
    SYM(CopyTexSubImage3D),
    SYM(ActiveTexture),
    SYM(SampleCoverage),
    SYM(CompressedTexImage3D),
    SYM(CompressedTexImage2D),
    SYM(CompressedTexImage1D),
    SYM(CompressedTexSubImage3D),
    SYM(CompressedTexSubImage2D),
    SYM(CompressedTexSubImage1D),
    SYM(GetCompressedTexImage),
    SYM(ClientActiveTexture),
    SYM(MultiTexCoord1d),
    SYM(MultiTexCoord1dv),
    SYM(MultiTexCoord1f),
    SYM(MultiTexCoord1fv),
    SYM(MultiTexCoord1i),
    SYM(MultiTexCoord1iv),
    SYM(MultiTexCoord1s),
    SYM(MultiTexCoord1sv),
    SYM(MultiTexCoord2d),
    SYM(MultiTexCoord2dv),
    SYM(MultiTexCoord2f),
    SYM(MultiTexCoord2fv),
    SYM(MultiTexCoord2i),
    SYM(MultiTexCoord2iv),
    SYM(MultiTexCoord2s),
    SYM(MultiTexCoord2sv),
    SYM(MultiTexCoord3d),
    SYM(MultiTexCoord3dv),
    SYM(MultiTexCoord3f),
    SYM(MultiTexCoord3fv),
    SYM(MultiTexCoord3i),
    SYM(MultiTexCoord3iv),
    SYM(MultiTexCoord3s),
    SYM(MultiTexCoord3sv),
    SYM(MultiTexCoord4d),
    SYM(MultiTexCoord4dv),
    SYM(MultiTexCoord4f),
    SYM(MultiTexCoord4fv),
    SYM(MultiTexCoord4i),
    SYM(MultiTexCoord4iv),
    SYM(MultiTexCoord4s),
    SYM(MultiTexCoord4sv),
    SYM(LoadTransposeMatrixf),
    SYM(LoadTransposeMatrixd),
    SYM(MultTransposeMatrixf),
    SYM(MultTransposeMatrixd),
    SYM(BlendFuncSeparate),
    SYM(MultiDrawArrays),
    SYM(MultiDrawElements),
    SYM(PointParameterf),
    SYM(PointParameterfv),
    SYM(PointParameteri),
    SYM(PointParameteriv),
    SYM(FogCoordf),
    SYM(FogCoordfv),
    SYM(FogCoordd),
    SYM(FogCoorddv),
    SYM(FogCoordPointer),
    SYM(SecondaryColor3b),
    SYM(SecondaryColor3bv),
    SYM(SecondaryColor3d),
    SYM(SecondaryColor3dv),
    SYM(SecondaryColor3f),
    SYM(SecondaryColor3fv),
    SYM(SecondaryColor3i),
    SYM(SecondaryColor3iv),
    SYM(SecondaryColor3s),
    SYM(SecondaryColor3sv),
    SYM(SecondaryColor3ub),
    SYM(SecondaryColor3ubv),
    SYM(SecondaryColor3ui),
    SYM(SecondaryColor3uiv),
    SYM(SecondaryColor3us),
    SYM(SecondaryColor3usv),
    SYM(SecondaryColorPointer),
    SYM(WindowPos2d),
    SYM(WindowPos2dv),
    SYM(WindowPos2f),
    SYM(WindowPos2fv),
    SYM(WindowPos2i),
    SYM(WindowPos2iv),
    SYM(WindowPos2s),
    SYM(WindowPos2sv),
    SYM(WindowPos3d),
    SYM(WindowPos3dv),
    SYM(WindowPos3f),
    SYM(WindowPos3fv),
    SYM(WindowPos3i),
    SYM(WindowPos3iv),
    SYM(WindowPos3s),
    SYM(WindowPos3sv),
    SYM(BlendColor),
    SYM(BlendEquation),
    SYM(GenQueries),
    SYM(DeleteQueries),
    SYM(IsQuery),
    SYM(BeginQuery),
    SYM(EndQuery),
    SYM(GetQueryiv),
    SYM(GetQueryObjectiv),
    SYM(GetQueryObjectuiv),
    SYM(BindBuffer),
    SYM(DeleteBuffers),
    SYM(GenBuffers),
    SYM(IsBuffer),
    SYM(BufferData),
    SYM(BufferSubData),
    SYM(GetBufferSubData),
    SYM(MapBuffer),
    SYM(UnmapBuffer),
    SYM(GetBufferParameteriv),
    SYM(GetBufferPointerv),
    SYM(BlendEquationSeparate),
    SYM(DrawBuffers),
    SYM(StencilOpSeparate),
    SYM(StencilFuncSeparate),
    SYM(StencilMaskSeparate),
    SYM(AttachShader),
    SYM(BindAttribLocation),
    SYM(CompileShader),
    SYM(CreateProgram),
    SYM(CreateShader),
    SYM(DeleteProgram),
    SYM(DeleteShader),
    SYM(DetachShader),
    SYM(DisableVertexAttribArray),
    SYM(EnableVertexAttribArray),
    SYM(GetActiveAttrib),
    SYM(GetActiveUniform),
    SYM(GetAttachedShaders),
    SYM(GetAttribLocation),
    SYM(GetProgramiv),
    SYM(GetProgramInfoLog),
    SYM(GetShaderiv),
    SYM(GetShaderInfoLog),
    SYM(GetShaderSource),
    SYM(GetUniformLocation),
    SYM(GetUniformfv),
    SYM(GetUniformiv),
    SYM(GetVertexAttribdv),
    SYM(GetVertexAttribfv),
    SYM(GetVertexAttribiv),
    SYM(GetVertexAttribPointerv),
    SYM(IsProgram),
    SYM(IsShader),
    SYM(LinkProgram),
    SYM(ShaderSource),
    SYM(UseProgram),
    SYM(Uniform1f),
    SYM(Uniform2f),
    SYM(Uniform3f),
    SYM(Uniform4f),
    SYM(Uniform1i),
    SYM(Uniform2i),
    SYM(Uniform3i),
    SYM(Uniform4i),
    SYM(Uniform1fv),
    SYM(Uniform2fv),
    SYM(Uniform3fv),
    SYM(Uniform4fv),
    SYM(Uniform1iv),
    SYM(Uniform2iv),
    SYM(Uniform3iv),
    SYM(Uniform4iv),
    SYM(UniformMatrix2fv),
    SYM(UniformMatrix3fv),
    SYM(UniformMatrix4fv),
    SYM(ValidateProgram),
    SYM(VertexAttrib1d),
    SYM(VertexAttrib1dv),
    SYM(VertexAttrib1f),
    SYM(VertexAttrib1fv),
    SYM(VertexAttrib1s),
    SYM(VertexAttrib1sv),
    SYM(VertexAttrib2d),
    SYM(VertexAttrib2dv),
    SYM(VertexAttrib2f),
    SYM(VertexAttrib2fv),
    SYM(VertexAttrib2s),
    SYM(VertexAttrib2sv),
    SYM(VertexAttrib3d),
    SYM(VertexAttrib3dv),
    SYM(VertexAttrib3f),
    SYM(VertexAttrib3fv),
    SYM(VertexAttrib3s),
    SYM(VertexAttrib3sv),
    SYM(VertexAttrib4Nbv),
    SYM(VertexAttrib4Niv),
    SYM(VertexAttrib4Nsv),
    SYM(VertexAttrib4Nub),
    SYM(VertexAttrib4Nubv),
    SYM(VertexAttrib4Nuiv),
    SYM(VertexAttrib4Nusv),
    SYM(VertexAttrib4bv),
    SYM(VertexAttrib4d),
    SYM(VertexAttrib4dv),
    SYM(VertexAttrib4f),
    SYM(VertexAttrib4fv),
    SYM(VertexAttrib4iv),
    SYM(VertexAttrib4s),
    SYM(VertexAttrib4sv),
    SYM(VertexAttrib4ubv),
    SYM(VertexAttrib4uiv),
    SYM(VertexAttrib4usv),
    SYM(VertexAttribPointer),
    SYM(UniformMatrix2x3fv),
    SYM(UniformMatrix3x2fv),
    SYM(UniformMatrix2x4fv),
    SYM(UniformMatrix4x2fv),
    SYM(UniformMatrix3x4fv),
    SYM(UniformMatrix4x3fv),
    SYM(ColorMaski),
    SYM(GetBooleani_v),
    SYM(GetIntegeri_v),
    SYM(Enablei),
    SYM(Disablei),
    SYM(IsEnabledi),
    SYM(BeginTransformFeedback),
    SYM(EndTransformFeedback),
    SYM(BindBufferRange),
    SYM(BindBufferBase),
    SYM(TransformFeedbackVaryings),
    SYM(GetTransformFeedbackVarying),
    SYM(ClampColor),
    SYM(BeginConditionalRender),
    SYM(EndConditionalRender),
    SYM(VertexAttribIPointer),
    SYM(GetVertexAttribIiv),
    SYM(GetVertexAttribIuiv),
    SYM(VertexAttribI1i),
    SYM(VertexAttribI2i),
    SYM(VertexAttribI3i),
    SYM(VertexAttribI4i),
    SYM(VertexAttribI1ui),
    SYM(VertexAttribI2ui),
    SYM(VertexAttribI3ui),
    SYM(VertexAttribI4ui),
    SYM(VertexAttribI1iv),
    SYM(VertexAttribI2iv),
    SYM(VertexAttribI3iv),
    SYM(VertexAttribI4iv),
    SYM(VertexAttribI1uiv),
    SYM(VertexAttribI2uiv),
    SYM(VertexAttribI3uiv),
    SYM(VertexAttribI4uiv),
    SYM(VertexAttribI4bv),
    SYM(VertexAttribI4sv),
    SYM(VertexAttribI4ubv),
    SYM(VertexAttribI4usv),
    SYM(GetUniformuiv),
    SYM(BindFragDataLocation),
    SYM(GetFragDataLocation),
    SYM(Uniform1ui),
    SYM(Uniform2ui),
    SYM(Uniform3ui),
    SYM(Uniform4ui),
    SYM(Uniform1uiv),
    SYM(Uniform2uiv),
    SYM(Uniform3uiv),
    SYM(Uniform4uiv),
    SYM(TexParameterIiv),
    SYM(TexParameterIuiv),
    SYM(GetTexParameterIiv),
    SYM(GetTexParameterIuiv),
    SYM(ClearBufferiv),
    SYM(ClearBufferuiv),
    SYM(ClearBufferfv),
    SYM(ClearBufferfi),
    SYM(GetStringi),
    SYM(IsRenderbuffer),
    SYM(BindRenderbuffer),
    SYM(DeleteRenderbuffers),
    SYM(GenRenderbuffers),
    SYM(RenderbufferStorage),
    SYM(GetRenderbufferParameteriv),
    SYM(IsFramebuffer),
    SYM(BindFramebuffer),
    SYM(DeleteFramebuffers),
    SYM(GenFramebuffers),
    SYM(CheckFramebufferStatus),
    SYM(FramebufferTexture1D),
    SYM(FramebufferTexture2D),
    SYM(FramebufferTexture3D),
    SYM(FramebufferRenderbuffer),
    SYM(GetFramebufferAttachmentParameteriv),
    SYM(GenerateMipmap),
    SYM(BlitFramebuffer),
    SYM(RenderbufferStorageMultisample),
    SYM(FramebufferTextureLayer),
    SYM(MapBufferRange),
    SYM(FlushMappedBufferRange),
    SYM(BindVertexArray),
    SYM(DeleteVertexArrays),
    SYM(GenVertexArrays),
    SYM(IsVertexArray),
    SYM(DrawArraysInstanced),
    SYM(DrawElementsInstanced),
    SYM(TexBuffer),
    SYM(PrimitiveRestartIndex),
    SYM(CopyBufferSubData),
    SYM(GetUniformIndices),
    SYM(GetActiveUniformsiv),
    SYM(GetActiveUniformName),
    SYM(GetUniformBlockIndex),
    SYM(GetActiveUniformBlockiv),
    SYM(GetActiveUniformBlockName),
    SYM(UniformBlockBinding),
    SYM(DrawElementsBaseVertex),
    SYM(DrawRangeElementsBaseVertex),
    SYM(DrawElementsInstancedBaseVertex),
    SYM(MultiDrawElementsBaseVertex),
    SYM(ProvokingVertex),
    SYM(FenceSync),
    SYM(IsSync),
    SYM(DeleteSync),
    SYM(ClientWaitSync),
    SYM(WaitSync),
    SYM(GetInteger64v),
    SYM(GetSynciv),
    SYM(GetInteger64i_v),
    SYM(GetBufferParameteri64v),
    SYM(FramebufferTexture),
    SYM(TexImage2DMultisample),
    SYM(TexImage3DMultisample),
    SYM(GetMultisamplefv),
    SYM(SampleMaski),
    SYM(BindFragDataLocationIndexed),
    SYM(GetFragDataIndex),
    SYM(GenSamplers),
    SYM(DeleteSamplers),
    SYM(IsSampler),
    SYM(BindSampler),
    SYM(SamplerParameteri),
    SYM(SamplerParameteriv),
    SYM(SamplerParameterf),
    SYM(SamplerParameterfv),
    SYM(SamplerParameterIiv),
    SYM(SamplerParameterIuiv),
    SYM(GetSamplerParameteriv),
    SYM(GetSamplerParameterIiv),
    SYM(GetSamplerParameterfv),
    SYM(GetSamplerParameterIuiv),
    SYM(QueryCounter),
    SYM(GetQueryObjecti64v),
    SYM(GetQueryObjectui64v),
    SYM(VertexAttribDivisor),
    SYM(VertexAttribP1ui),
    SYM(VertexAttribP1uiv),
    SYM(VertexAttribP2ui),
    SYM(VertexAttribP2uiv),
    SYM(VertexAttribP3ui),
    SYM(VertexAttribP3uiv),
    SYM(VertexAttribP4ui),
    SYM(VertexAttribP4uiv),
    SYM(VertexP2ui),
    SYM(VertexP2uiv),
    SYM(VertexP3ui),
    SYM(VertexP3uiv),
    SYM(VertexP4ui),
    SYM(VertexP4uiv),
    SYM(TexCoordP1ui),
    SYM(TexCoordP1uiv),
    SYM(TexCoordP2ui),
    SYM(TexCoordP2uiv),
    SYM(TexCoordP3ui),
    SYM(TexCoordP3uiv),
    SYM(TexCoordP4ui),
    SYM(TexCoordP4uiv),
    SYM(MultiTexCoordP1ui),
    SYM(MultiTexCoordP1uiv),
    SYM(MultiTexCoordP2ui),
    SYM(MultiTexCoordP2uiv),
    SYM(MultiTexCoordP3ui),
    SYM(MultiTexCoordP3uiv),
    SYM(MultiTexCoordP4ui),
    SYM(MultiTexCoordP4uiv),
    SYM(NormalP3ui),
    SYM(NormalP3uiv),
    SYM(ColorP3ui),
    SYM(ColorP3uiv),
    SYM(ColorP4ui),
    SYM(ColorP4uiv),
    SYM(SecondaryColorP3ui),
    SYM(SecondaryColorP3uiv),
    SYM(MinSampleShading),
    SYM(BlendEquationi),
    SYM(BlendEquationSeparatei),
    SYM(BlendFunci),
    SYM(BlendFuncSeparatei),
    SYM(DrawArraysIndirect),
    SYM(DrawElementsIndirect),
    SYM(Uniform1d),
    SYM(Uniform2d),
    SYM(Uniform3d),
    SYM(Uniform4d),
    SYM(Uniform1dv),
    SYM(Uniform2dv),
    SYM(Uniform3dv),
    SYM(Uniform4dv),
    SYM(UniformMatrix2dv),
    SYM(UniformMatrix3dv),
    SYM(UniformMatrix4dv),
    SYM(UniformMatrix2x3dv),
    SYM(UniformMatrix2x4dv),
    SYM(UniformMatrix3x2dv),
    SYM(UniformMatrix3x4dv),
    SYM(UniformMatrix4x2dv),
    SYM(UniformMatrix4x3dv),
    SYM(GetUniformdv),
    SYM(GetSubroutineUniformLocation),
    SYM(GetSubroutineIndex),
    SYM(GetActiveSubroutineUniformiv),
    SYM(GetActiveSubroutineUniformName),
    SYM(GetActiveSubroutineName),
    SYM(UniformSubroutinesuiv),
    SYM(GetUniformSubroutineuiv),
    SYM(GetProgramStageiv),
    SYM(PatchParameteri),
    SYM(PatchParameterfv),
    SYM(BindTransformFeedback),
    SYM(DeleteTransformFeedbacks),
    SYM(GenTransformFeedbacks),
    SYM(IsTransformFeedback),
    SYM(PauseTransformFeedback),
    SYM(ResumeTransformFeedback),
    SYM(DrawTransformFeedback),
    SYM(DrawTransformFeedbackStream),
    SYM(BeginQueryIndexed),
    SYM(EndQueryIndexed),
    SYM(GetQueryIndexediv),
    SYM(ReleaseShaderCompiler),
    SYM(ShaderBinary),
    SYM(GetShaderPrecisionFormat),
    SYM(DepthRangef),
    SYM(ClearDepthf),
    SYM(GetProgramBinary),
    SYM(ProgramBinary),
    SYM(ProgramParameteri),
    SYM(UseProgramStages),
    SYM(ActiveShaderProgram),
    SYM(CreateShaderProgramv),
    SYM(BindProgramPipeline),
    SYM(DeleteProgramPipelines),
    SYM(GenProgramPipelines),
    SYM(IsProgramPipeline),
    SYM(GetProgramPipelineiv),
    SYM(ProgramUniform1i),
    SYM(ProgramUniform1iv),
    SYM(ProgramUniform1f),
    SYM(ProgramUniform1fv),
    SYM(ProgramUniform1d),
    SYM(ProgramUniform1dv),
    SYM(ProgramUniform1ui),
    SYM(ProgramUniform1uiv),
    SYM(ProgramUniform2i),
    SYM(ProgramUniform2iv),
    SYM(ProgramUniform2f),
    SYM(ProgramUniform2fv),
    SYM(ProgramUniform2d),
    SYM(ProgramUniform2dv),
    SYM(ProgramUniform2ui),
    SYM(ProgramUniform2uiv),
    SYM(ProgramUniform3i),
    SYM(ProgramUniform3iv),
    SYM(ProgramUniform3f),
    SYM(ProgramUniform3fv),
    SYM(ProgramUniform3d),
    SYM(ProgramUniform3dv),
    SYM(ProgramUniform3ui),
    SYM(ProgramUniform3uiv),
    SYM(ProgramUniform4i),
    SYM(ProgramUniform4iv),
    SYM(ProgramUniform4f),
    SYM(ProgramUniform4fv),
    SYM(ProgramUniform4d),
    SYM(ProgramUniform4dv),
    SYM(ProgramUniform4ui),
    SYM(ProgramUniform4uiv),
    SYM(ProgramUniformMatrix2fv),
    SYM(ProgramUniformMatrix3fv),
    SYM(ProgramUniformMatrix4fv),
    SYM(ProgramUniformMatrix2dv),
    SYM(ProgramUniformMatrix3dv),
    SYM(ProgramUniformMatrix4dv),
    SYM(ProgramUniformMatrix2x3fv),
    SYM(ProgramUniformMatrix3x2fv),
    SYM(ProgramUniformMatrix2x4fv),
    SYM(ProgramUniformMatrix4x2fv),
    SYM(ProgramUniformMatrix3x4fv),
    SYM(ProgramUniformMatrix4x3fv),
    SYM(ProgramUniformMatrix2x3dv),
    SYM(ProgramUniformMatrix3x2dv),
    SYM(ProgramUniformMatrix2x4dv),
    SYM(ProgramUniformMatrix4x2dv),
    SYM(ProgramUniformMatrix3x4dv),
    SYM(ProgramUniformMatrix4x3dv),
    SYM(ValidateProgramPipeline),
    SYM(GetProgramPipelineInfoLog),
    SYM(VertexAttribL1d),
    SYM(VertexAttribL2d),
    SYM(VertexAttribL3d),
    SYM(VertexAttribL4d),
    SYM(VertexAttribL1dv),
    SYM(VertexAttribL2dv),
    SYM(VertexAttribL3dv),
    SYM(VertexAttribL4dv),
    SYM(VertexAttribLPointer),
    SYM(GetVertexAttribLdv),
    SYM(ViewportArrayv),
    SYM(ViewportIndexedf),
    SYM(ViewportIndexedfv),
    SYM(ScissorArrayv),
    SYM(ScissorIndexed),
    SYM(ScissorIndexedv),
    SYM(DepthRangeArrayv),
    SYM(DepthRangeIndexed),
    SYM(GetFloati_v),
    SYM(GetDoublei_v),
    SYM(DrawArraysInstancedBaseInstance),
    SYM(DrawElementsInstancedBaseInstance),
    SYM(DrawElementsInstancedBaseVertexBaseInstance),
    SYM(GetInternalformativ),
    SYM(GetActiveAtomicCounterBufferiv),
    SYM(BindImageTexture),
    SYM(MemoryBarrier),
    SYM(TexStorage1D),
    SYM(TexStorage2D),
    SYM(TexStorage3D),
    SYM(DrawTransformFeedbackInstanced),
    SYM(DrawTransformFeedbackStreamInstanced),
    SYM(ClearBufferData),
    SYM(ClearBufferSubData),
    SYM(DispatchCompute),
    SYM(DispatchComputeIndirect),
    SYM(CopyImageSubData),
    SYM(FramebufferParameteri),
    SYM(GetFramebufferParameteriv),
    SYM(GetInternalformati64v),
    SYM(InvalidateTexSubImage),
    SYM(InvalidateTexImage),
    SYM(InvalidateBufferSubData),
    SYM(InvalidateBufferData),
    SYM(InvalidateFramebuffer),
    SYM(InvalidateSubFramebuffer),
    SYM(MultiDrawArraysIndirect),
    SYM(MultiDrawElementsIndirect),
    SYM(GetProgramInterfaceiv),
    SYM(GetProgramResourceIndex),
    SYM(GetProgramResourceName),
    SYM(GetProgramResourceiv),
    SYM(GetProgramResourceLocation),
    SYM(GetProgramResourceLocationIndex),
    SYM(ShaderStorageBlockBinding),
    SYM(TexBufferRange),
    SYM(TexStorage2DMultisample),
    SYM(TexStorage3DMultisample),
    SYM(TextureView),
    SYM(BindVertexBuffer),
    SYM(VertexAttribFormat),
    SYM(VertexAttribIFormat),
    SYM(VertexAttribLFormat),
    SYM(VertexAttribBinding),
    SYM(VertexBindingDivisor),
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
    SYM(BufferStorage),
    SYM(ClearTexImage),
    SYM(ClearTexSubImage),
    SYM(BindBuffersBase),
    SYM(BindBuffersRange),
    SYM(BindTextures),
    SYM(BindSamplers),
    SYM(BindImageTextures),
    SYM(BindVertexBuffers),
    SYM(GetTextureHandleARB),
    SYM(GetTextureSamplerHandleARB),
    SYM(MakeTextureHandleResidentARB),
    SYM(MakeTextureHandleNonResidentARB),
    SYM(GetImageHandleARB),
    SYM(MakeImageHandleResidentARB),
    SYM(MakeImageHandleNonResidentARB),
    SYM(UniformHandleui64ARB),
    SYM(UniformHandleui64vARB),
    SYM(ProgramUniformHandleui64ARB),
    SYM(ProgramUniformHandleui64vARB),
    SYM(IsTextureHandleResidentARB),
    SYM(IsImageHandleResidentARB),
    SYM(VertexAttribL1ui64ARB),
    SYM(VertexAttribL1ui64vARB),
    SYM(GetVertexAttribLui64vARB),
    SYM(CreateSyncFromCLeventARB),
    SYM(ClampColorARB),
    SYM(DispatchComputeGroupSizeARB),
    SYM(DebugMessageControlARB),
    SYM(DebugMessageInsertARB),
    SYM(DebugMessageCallbackARB),
    SYM(GetDebugMessageLogARB),
    SYM(DrawBuffersARB),
    SYM(BlendEquationiARB),
    SYM(BlendEquationSeparateiARB),
    SYM(BlendFunciARB),
    SYM(BlendFuncSeparateiARB),
    SYM(DrawArraysInstancedARB),
    SYM(DrawElementsInstancedARB),
    SYM(ProgramStringARB),
    SYM(BindProgramARB),
    SYM(DeleteProgramsARB),
    SYM(GenProgramsARB),
    SYM(ProgramEnvParameter4dARB),
    SYM(ProgramEnvParameter4dvARB),
    SYM(ProgramEnvParameter4fARB),
    SYM(ProgramEnvParameter4fvARB),
    SYM(ProgramLocalParameter4dARB),
    SYM(ProgramLocalParameter4dvARB),
    SYM(ProgramLocalParameter4fARB),
    SYM(ProgramLocalParameter4fvARB),
    SYM(GetProgramEnvParameterdvARB),
    SYM(GetProgramEnvParameterfvARB),
    SYM(GetProgramLocalParameterdvARB),
    SYM(GetProgramLocalParameterfvARB),
    SYM(GetProgramivARB),
    SYM(GetProgramStringARB),
    SYM(IsProgramARB),
    SYM(ProgramParameteriARB),
    SYM(FramebufferTextureARB),
    SYM(FramebufferTextureLayerARB),
    SYM(FramebufferTextureFaceARB),
    SYM(ColorTable),
    SYM(ColorTableParameterfv),
    SYM(ColorTableParameteriv),
    SYM(CopyColorTable),
    SYM(GetColorTable),
    SYM(GetColorTableParameterfv),
    SYM(GetColorTableParameteriv),
    SYM(ColorSubTable),
    SYM(CopyColorSubTable),
    SYM(ConvolutionFilter1D),
    SYM(ConvolutionFilter2D),
    SYM(ConvolutionParameterf),
    SYM(ConvolutionParameterfv),
    SYM(ConvolutionParameteri),
    SYM(ConvolutionParameteriv),
    SYM(CopyConvolutionFilter1D),
    SYM(CopyConvolutionFilter2D),
    SYM(GetConvolutionFilter),
    SYM(GetConvolutionParameterfv),
    SYM(GetConvolutionParameteriv),
    SYM(GetSeparableFilter),
    SYM(SeparableFilter2D),
    SYM(GetHistogram),
    SYM(GetHistogramParameterfv),
    SYM(GetHistogramParameteriv),
    SYM(GetMinmax),
    SYM(GetMinmaxParameterfv),
    SYM(GetMinmaxParameteriv),
    SYM(Histogram),
    SYM(Minmax),
    SYM(ResetHistogram),
    SYM(ResetMinmax),
    SYM(MultiDrawArraysIndirectCountARB),
    SYM(MultiDrawElementsIndirectCountARB),
    SYM(VertexAttribDivisorARB),
    SYM(CurrentPaletteMatrixARB),
    SYM(MatrixIndexubvARB),
    SYM(MatrixIndexusvARB),
    SYM(MatrixIndexuivARB),
    SYM(MatrixIndexPointerARB),
    SYM(SampleCoverageARB),
    SYM(ActiveTextureARB),
    SYM(ClientActiveTextureARB),
    SYM(MultiTexCoord1dARB),
    SYM(MultiTexCoord1dvARB),
    SYM(MultiTexCoord1fARB),
    SYM(MultiTexCoord1fvARB),
    SYM(MultiTexCoord1iARB),
    SYM(MultiTexCoord1ivARB),
    SYM(MultiTexCoord1sARB),
    SYM(MultiTexCoord1svARB),
    SYM(MultiTexCoord2dARB),
    SYM(MultiTexCoord2dvARB),
    SYM(MultiTexCoord2fARB),
    SYM(MultiTexCoord2fvARB),
    SYM(MultiTexCoord2iARB),
    SYM(MultiTexCoord2ivARB),
    SYM(MultiTexCoord2sARB),
    SYM(MultiTexCoord2svARB),
    SYM(MultiTexCoord3dARB),
    SYM(MultiTexCoord3dvARB),
    SYM(MultiTexCoord3fARB),
    SYM(MultiTexCoord3fvARB),
    SYM(MultiTexCoord3iARB),
    SYM(MultiTexCoord3ivARB),
    SYM(MultiTexCoord3sARB),
    SYM(MultiTexCoord3svARB),
    SYM(MultiTexCoord4dARB),
    SYM(MultiTexCoord4dvARB),
    SYM(MultiTexCoord4fARB),
    SYM(MultiTexCoord4fvARB),
    SYM(MultiTexCoord4iARB),
    SYM(MultiTexCoord4ivARB),
    SYM(MultiTexCoord4sARB),
    SYM(MultiTexCoord4svARB),
    SYM(GenQueriesARB),
    SYM(DeleteQueriesARB),
    SYM(IsQueryARB),
    SYM(BeginQueryARB),
    SYM(EndQueryARB),
    SYM(GetQueryivARB),
    SYM(GetQueryObjectivARB),
    SYM(GetQueryObjectuivARB),
    SYM(PointParameterfARB),
    SYM(PointParameterfvARB),
    SYM(GetGraphicsResetStatusARB),
    SYM(GetnTexImageARB),
    SYM(ReadnPixelsARB),
    SYM(GetnCompressedTexImageARB),
    SYM(GetnUniformfvARB),
    SYM(GetnUniformivARB),
    SYM(GetnUniformuivARB),
    SYM(GetnUniformdvARB),
    SYM(GetnMapdvARB),
    SYM(GetnMapfvARB),
    SYM(GetnMapivARB),
    SYM(GetnPixelMapfvARB),
    SYM(GetnPixelMapuivARB),
    SYM(GetnPixelMapusvARB),
    SYM(GetnPolygonStippleARB),
    SYM(GetnColorTableARB),
    SYM(GetnConvolutionFilterARB),
    SYM(GetnSeparableFilterARB),
    SYM(GetnHistogramARB),
    SYM(GetnMinmaxARB),
    SYM(MinSampleShadingARB),
    SYM(DeleteObjectARB),
    SYM(GetHandleARB),
    SYM(DetachObjectARB),
    SYM(CreateShaderObjectARB),
    SYM(ShaderSourceARB),
    SYM(CompileShaderARB),
    SYM(CreateProgramObjectARB),
    SYM(AttachObjectARB),
    SYM(LinkProgramARB),
    SYM(UseProgramObjectARB),
    SYM(ValidateProgramARB),
    SYM(Uniform1fARB),
    SYM(Uniform2fARB),
    SYM(Uniform3fARB),
    SYM(Uniform4fARB),
    SYM(Uniform1iARB),
    SYM(Uniform2iARB),
    SYM(Uniform3iARB),
    SYM(Uniform4iARB),
    SYM(Uniform1fvARB),
    SYM(Uniform2fvARB),
    SYM(Uniform3fvARB),
    SYM(Uniform4fvARB),
    SYM(Uniform1ivARB),
    SYM(Uniform2ivARB),
    SYM(Uniform3ivARB),
    SYM(Uniform4ivARB),
    SYM(UniformMatrix2fvARB),
    SYM(UniformMatrix3fvARB),
    SYM(UniformMatrix4fvARB),
    SYM(GetObjectParameterfvARB),
    SYM(GetObjectParameterivARB),
    SYM(GetInfoLogARB),
    SYM(GetAttachedObjectsARB),
    SYM(GetUniformLocationARB),
    SYM(GetActiveUniformARB),
    SYM(GetUniformfvARB),
    SYM(GetUniformivARB),
    SYM(GetShaderSourceARB),
    SYM(NamedStringARB),
    SYM(DeleteNamedStringARB),
    SYM(CompileShaderIncludeARB),
    SYM(IsNamedStringARB),
    SYM(GetNamedStringARB),
    SYM(GetNamedStringivARB),
    SYM(TexPageCommitmentARB),
    SYM(TexBufferARB),
    SYM(CompressedTexImage3DARB),
    SYM(CompressedTexImage2DARB),
    SYM(CompressedTexImage1DARB),
    SYM(CompressedTexSubImage3DARB),
    SYM(CompressedTexSubImage2DARB),
    SYM(CompressedTexSubImage1DARB),
    SYM(GetCompressedTexImageARB),
    SYM(LoadTransposeMatrixfARB),
    SYM(LoadTransposeMatrixdARB),
    SYM(MultTransposeMatrixfARB),
    SYM(MultTransposeMatrixdARB),
    SYM(WeightbvARB),
    SYM(WeightsvARB),
    SYM(WeightivARB),
    SYM(WeightfvARB),
    SYM(WeightdvARB),
    SYM(WeightubvARB),
    SYM(WeightusvARB),
    SYM(WeightuivARB),
    SYM(WeightPointerARB),
    SYM(VertexBlendARB),
    SYM(BindBufferARB),
    SYM(DeleteBuffersARB),
    SYM(GenBuffersARB),
    SYM(IsBufferARB),
    SYM(BufferDataARB),
    SYM(BufferSubDataARB),
    SYM(GetBufferSubDataARB),
    SYM(MapBufferARB),
    SYM(UnmapBufferARB),
    SYM(GetBufferParameterivARB),
    SYM(GetBufferPointervARB),
    SYM(VertexAttrib1dARB),
    SYM(VertexAttrib1dvARB),
    SYM(VertexAttrib1fARB),
    SYM(VertexAttrib1fvARB),
    SYM(VertexAttrib1sARB),
    SYM(VertexAttrib1svARB),
    SYM(VertexAttrib2dARB),
    SYM(VertexAttrib2dvARB),
    SYM(VertexAttrib2fARB),
    SYM(VertexAttrib2fvARB),
    SYM(VertexAttrib2sARB),
    SYM(VertexAttrib2svARB),
    SYM(VertexAttrib3dARB),
    SYM(VertexAttrib3dvARB),
    SYM(VertexAttrib3fARB),
    SYM(VertexAttrib3fvARB),
    SYM(VertexAttrib3sARB),
    SYM(VertexAttrib3svARB),
    SYM(VertexAttrib4NbvARB),
    SYM(VertexAttrib4NivARB),
    SYM(VertexAttrib4NsvARB),
    SYM(VertexAttrib4NubARB),
    SYM(VertexAttrib4NubvARB),
    SYM(VertexAttrib4NuivARB),
    SYM(VertexAttrib4NusvARB),
    SYM(VertexAttrib4bvARB),
    SYM(VertexAttrib4dARB),
    SYM(VertexAttrib4dvARB),
    SYM(VertexAttrib4fARB),
    SYM(VertexAttrib4fvARB),
    SYM(VertexAttrib4ivARB),
    SYM(VertexAttrib4sARB),
    SYM(VertexAttrib4svARB),
    SYM(VertexAttrib4ubvARB),
    SYM(VertexAttrib4uivARB),
    SYM(VertexAttrib4usvARB),
    SYM(VertexAttribPointerARB),
    SYM(EnableVertexAttribArrayARB),
    SYM(DisableVertexAttribArrayARB),
    SYM(GetVertexAttribdvARB),
    SYM(GetVertexAttribfvARB),
    SYM(GetVertexAttribivARB),
    SYM(GetVertexAttribPointervARB),
    SYM(BindAttribLocationARB),
    SYM(GetActiveAttribARB),
    SYM(GetAttribLocationARB),
    SYM(WindowPos2dARB),
    SYM(WindowPos2dvARB),
    SYM(WindowPos2fARB),
    SYM(WindowPos2fvARB),
    SYM(WindowPos2iARB),
    SYM(WindowPos2ivARB),
    SYM(WindowPos2sARB),
    SYM(WindowPos2svARB),
    SYM(WindowPos3dARB),
    SYM(WindowPos3dvARB),
    SYM(WindowPos3fARB),
    SYM(WindowPos3fvARB),
    SYM(WindowPos3iARB),
    SYM(WindowPos3ivARB),
    SYM(WindowPos3sARB),
    SYM(WindowPos3svARB),
    SYM(MultiTexCoord1bOES),
    SYM(MultiTexCoord1bvOES),
    SYM(MultiTexCoord2bOES),
    SYM(MultiTexCoord2bvOES),
    SYM(MultiTexCoord3bOES),
    SYM(MultiTexCoord3bvOES),
    SYM(MultiTexCoord4bOES),
    SYM(MultiTexCoord4bvOES),
    SYM(TexCoord1bOES),
    SYM(TexCoord1bvOES),
    SYM(TexCoord2bOES),
    SYM(TexCoord2bvOES),
    SYM(TexCoord3bOES),
    SYM(TexCoord3bvOES),
    SYM(TexCoord4bOES),
    SYM(TexCoord4bvOES),
    SYM(Vertex2bOES),
    SYM(Vertex2bvOES),
    SYM(Vertex3bOES),
    SYM(Vertex3bvOES),
    SYM(Vertex4bOES),
    SYM(Vertex4bvOES),
    SYM(AlphaFuncxOES),
    SYM(ClearColorxOES),
    SYM(ClearDepthxOES),
    SYM(ClipPlanexOES),
    SYM(Color4xOES),
    SYM(DepthRangexOES),
    SYM(FogxOES),
    SYM(FogxvOES),
    SYM(FrustumxOES),
    SYM(GetClipPlanexOES),
    SYM(GetFixedvOES),
    SYM(GetTexEnvxvOES),
    SYM(GetTexParameterxvOES),
    SYM(LightModelxOES),
    SYM(LightModelxvOES),
    SYM(LightxOES),
    SYM(LightxvOES),
    SYM(LineWidthxOES),
    SYM(LoadMatrixxOES),
    SYM(MaterialxOES),
    SYM(MaterialxvOES),
    SYM(MultMatrixxOES),
    SYM(MultiTexCoord4xOES),
    SYM(Normal3xOES),
    SYM(OrthoxOES),
    SYM(PointParameterxvOES),
    SYM(PointSizexOES),
    SYM(PolygonOffsetxOES),
    SYM(RotatexOES),
    SYM(SampleCoverageOES),
    SYM(ScalexOES),
    SYM(TexEnvxOES),
    SYM(TexEnvxvOES),
    SYM(TexParameterxOES),
    SYM(TexParameterxvOES),
    SYM(TranslatexOES),
    SYM(AccumxOES),
    SYM(BitmapxOES),
    SYM(BlendColorxOES),
    SYM(ClearAccumxOES),
    SYM(Color3xOES),
    SYM(Color3xvOES),
    SYM(Color4xvOES),
    SYM(ConvolutionParameterxOES),
    SYM(ConvolutionParameterxvOES),
    SYM(EvalCoord1xOES),
    SYM(EvalCoord1xvOES),
    SYM(EvalCoord2xOES),
    SYM(EvalCoord2xvOES),
    SYM(FeedbackBufferxOES),
    SYM(GetConvolutionParameterxvOES),
    SYM(GetHistogramParameterxvOES),
    SYM(GetLightxOES),
    SYM(GetMapxvOES),
    SYM(GetMaterialxOES),
    SYM(GetPixelMapxv),
    SYM(GetTexGenxvOES),
    SYM(GetTexLevelParameterxvOES),
    SYM(IndexxOES),
    SYM(IndexxvOES),
    SYM(LoadTransposeMatrixxOES),
    SYM(Map1xOES),
    SYM(Map2xOES),
    SYM(MapGrid1xOES),
    SYM(MapGrid2xOES),
    SYM(MultTransposeMatrixxOES),
    SYM(MultiTexCoord1xOES),
    SYM(MultiTexCoord1xvOES),
    SYM(MultiTexCoord2xOES),
    SYM(MultiTexCoord2xvOES),
    SYM(MultiTexCoord3xOES),
    SYM(MultiTexCoord3xvOES),
    SYM(MultiTexCoord4xvOES),
    SYM(Normal3xvOES),
    SYM(PassThroughxOES),
    SYM(PixelMapx),
    SYM(PixelStorex),
    SYM(PixelTransferxOES),
    SYM(PixelZoomxOES),
    SYM(PrioritizeTexturesxOES),
    SYM(RasterPos2xOES),
    SYM(RasterPos2xvOES),
    SYM(RasterPos3xOES),
    SYM(RasterPos3xvOES),
    SYM(RasterPos4xOES),
    SYM(RasterPos4xvOES),
    SYM(RectxOES),
    SYM(RectxvOES),
    SYM(TexCoord1xOES),
    SYM(TexCoord1xvOES),
    SYM(TexCoord2xOES),
    SYM(TexCoord2xvOES),
    SYM(TexCoord3xOES),
    SYM(TexCoord3xvOES),
    SYM(TexCoord4xOES),
    SYM(TexCoord4xvOES),
    SYM(TexGenxOES),
    SYM(TexGenxvOES),
    SYM(Vertex2xOES),
    SYM(Vertex2xvOES),
    SYM(Vertex3xOES),
    SYM(Vertex3xvOES),
    SYM(Vertex4xOES),
    SYM(Vertex4xvOES),
    SYM(QueryMatrixxOES),
    SYM(ClearDepthfOES),
    SYM(ClipPlanefOES),
    SYM(DepthRangefOES),
    SYM(FrustumfOES),
    SYM(GetClipPlanefOES),
    SYM(OrthofOES),
    SYM(ImageTransformParameteriHP),
    SYM(ImageTransformParameterfHP),
    SYM(ImageTransformParameterivHP),
    SYM(ImageTransformParameterfvHP),
    SYM(GetImageTransformParameterivHP),
    SYM(GetImageTransformParameterfvHP),

    { NULL, NULL },
};
RGLSYMGLDRAWRANGEELEMENTSPROC __rglgen_glDrawRangeElements;
RGLSYMGLTEXIMAGE3DPROC __rglgen_glTexImage3D;
RGLSYMGLTEXSUBIMAGE3DPROC __rglgen_glTexSubImage3D;
RGLSYMGLCOPYTEXSUBIMAGE3DPROC __rglgen_glCopyTexSubImage3D;
RGLSYMGLACTIVETEXTUREPROC __rglgen_glActiveTexture;
RGLSYMGLSAMPLECOVERAGEPROC __rglgen_glSampleCoverage;
RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC __rglgen_glCompressedTexImage3D;
RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC __rglgen_glCompressedTexImage2D;
RGLSYMGLCOMPRESSEDTEXIMAGE1DPROC __rglgen_glCompressedTexImage1D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC __rglgen_glCompressedTexSubImage3D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC __rglgen_glCompressedTexSubImage2D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DPROC __rglgen_glCompressedTexSubImage1D;
RGLSYMGLGETCOMPRESSEDTEXIMAGEPROC __rglgen_glGetCompressedTexImage;
RGLSYMGLCLIENTACTIVETEXTUREPROC __rglgen_glClientActiveTexture;
RGLSYMGLMULTITEXCOORD1DPROC __rglgen_glMultiTexCoord1d;
RGLSYMGLMULTITEXCOORD1DVPROC __rglgen_glMultiTexCoord1dv;
RGLSYMGLMULTITEXCOORD1FPROC __rglgen_glMultiTexCoord1f;
RGLSYMGLMULTITEXCOORD1FVPROC __rglgen_glMultiTexCoord1fv;
RGLSYMGLMULTITEXCOORD1IPROC __rglgen_glMultiTexCoord1i;
RGLSYMGLMULTITEXCOORD1IVPROC __rglgen_glMultiTexCoord1iv;
RGLSYMGLMULTITEXCOORD1SPROC __rglgen_glMultiTexCoord1s;
RGLSYMGLMULTITEXCOORD1SVPROC __rglgen_glMultiTexCoord1sv;
RGLSYMGLMULTITEXCOORD2DPROC __rglgen_glMultiTexCoord2d;
RGLSYMGLMULTITEXCOORD2DVPROC __rglgen_glMultiTexCoord2dv;
RGLSYMGLMULTITEXCOORD2FPROC __rglgen_glMultiTexCoord2f;
RGLSYMGLMULTITEXCOORD2FVPROC __rglgen_glMultiTexCoord2fv;
RGLSYMGLMULTITEXCOORD2IPROC __rglgen_glMultiTexCoord2i;
RGLSYMGLMULTITEXCOORD2IVPROC __rglgen_glMultiTexCoord2iv;
RGLSYMGLMULTITEXCOORD2SPROC __rglgen_glMultiTexCoord2s;
RGLSYMGLMULTITEXCOORD2SVPROC __rglgen_glMultiTexCoord2sv;
RGLSYMGLMULTITEXCOORD3DPROC __rglgen_glMultiTexCoord3d;
RGLSYMGLMULTITEXCOORD3DVPROC __rglgen_glMultiTexCoord3dv;
RGLSYMGLMULTITEXCOORD3FPROC __rglgen_glMultiTexCoord3f;
RGLSYMGLMULTITEXCOORD3FVPROC __rglgen_glMultiTexCoord3fv;
RGLSYMGLMULTITEXCOORD3IPROC __rglgen_glMultiTexCoord3i;
RGLSYMGLMULTITEXCOORD3IVPROC __rglgen_glMultiTexCoord3iv;
RGLSYMGLMULTITEXCOORD3SPROC __rglgen_glMultiTexCoord3s;
RGLSYMGLMULTITEXCOORD3SVPROC __rglgen_glMultiTexCoord3sv;
RGLSYMGLMULTITEXCOORD4DPROC __rglgen_glMultiTexCoord4d;
RGLSYMGLMULTITEXCOORD4DVPROC __rglgen_glMultiTexCoord4dv;
RGLSYMGLMULTITEXCOORD4FPROC __rglgen_glMultiTexCoord4f;
RGLSYMGLMULTITEXCOORD4FVPROC __rglgen_glMultiTexCoord4fv;
RGLSYMGLMULTITEXCOORD4IPROC __rglgen_glMultiTexCoord4i;
RGLSYMGLMULTITEXCOORD4IVPROC __rglgen_glMultiTexCoord4iv;
RGLSYMGLMULTITEXCOORD4SPROC __rglgen_glMultiTexCoord4s;
RGLSYMGLMULTITEXCOORD4SVPROC __rglgen_glMultiTexCoord4sv;
RGLSYMGLLOADTRANSPOSEMATRIXFPROC __rglgen_glLoadTransposeMatrixf;
RGLSYMGLLOADTRANSPOSEMATRIXDPROC __rglgen_glLoadTransposeMatrixd;
RGLSYMGLMULTTRANSPOSEMATRIXFPROC __rglgen_glMultTransposeMatrixf;
RGLSYMGLMULTTRANSPOSEMATRIXDPROC __rglgen_glMultTransposeMatrixd;
RGLSYMGLBLENDFUNCSEPARATEPROC __rglgen_glBlendFuncSeparate;
RGLSYMGLMULTIDRAWARRAYSPROC __rglgen_glMultiDrawArrays;
RGLSYMGLMULTIDRAWELEMENTSPROC __rglgen_glMultiDrawElements;
RGLSYMGLPOINTPARAMETERFPROC __rglgen_glPointParameterf;
RGLSYMGLPOINTPARAMETERFVPROC __rglgen_glPointParameterfv;
RGLSYMGLPOINTPARAMETERIPROC __rglgen_glPointParameteri;
RGLSYMGLPOINTPARAMETERIVPROC __rglgen_glPointParameteriv;
RGLSYMGLFOGCOORDFPROC __rglgen_glFogCoordf;
RGLSYMGLFOGCOORDFVPROC __rglgen_glFogCoordfv;
RGLSYMGLFOGCOORDDPROC __rglgen_glFogCoordd;
RGLSYMGLFOGCOORDDVPROC __rglgen_glFogCoorddv;
RGLSYMGLFOGCOORDPOINTERPROC __rglgen_glFogCoordPointer;
RGLSYMGLSECONDARYCOLOR3BPROC __rglgen_glSecondaryColor3b;
RGLSYMGLSECONDARYCOLOR3BVPROC __rglgen_glSecondaryColor3bv;
RGLSYMGLSECONDARYCOLOR3DPROC __rglgen_glSecondaryColor3d;
RGLSYMGLSECONDARYCOLOR3DVPROC __rglgen_glSecondaryColor3dv;
RGLSYMGLSECONDARYCOLOR3FPROC __rglgen_glSecondaryColor3f;
RGLSYMGLSECONDARYCOLOR3FVPROC __rglgen_glSecondaryColor3fv;
RGLSYMGLSECONDARYCOLOR3IPROC __rglgen_glSecondaryColor3i;
RGLSYMGLSECONDARYCOLOR3IVPROC __rglgen_glSecondaryColor3iv;
RGLSYMGLSECONDARYCOLOR3SPROC __rglgen_glSecondaryColor3s;
RGLSYMGLSECONDARYCOLOR3SVPROC __rglgen_glSecondaryColor3sv;
RGLSYMGLSECONDARYCOLOR3UBPROC __rglgen_glSecondaryColor3ub;
RGLSYMGLSECONDARYCOLOR3UBVPROC __rglgen_glSecondaryColor3ubv;
RGLSYMGLSECONDARYCOLOR3UIPROC __rglgen_glSecondaryColor3ui;
RGLSYMGLSECONDARYCOLOR3UIVPROC __rglgen_glSecondaryColor3uiv;
RGLSYMGLSECONDARYCOLOR3USPROC __rglgen_glSecondaryColor3us;
RGLSYMGLSECONDARYCOLOR3USVPROC __rglgen_glSecondaryColor3usv;
RGLSYMGLSECONDARYCOLORPOINTERPROC __rglgen_glSecondaryColorPointer;
RGLSYMGLWINDOWPOS2DPROC __rglgen_glWindowPos2d;
RGLSYMGLWINDOWPOS2DVPROC __rglgen_glWindowPos2dv;
RGLSYMGLWINDOWPOS2FPROC __rglgen_glWindowPos2f;
RGLSYMGLWINDOWPOS2FVPROC __rglgen_glWindowPos2fv;
RGLSYMGLWINDOWPOS2IPROC __rglgen_glWindowPos2i;
RGLSYMGLWINDOWPOS2IVPROC __rglgen_glWindowPos2iv;
RGLSYMGLWINDOWPOS2SPROC __rglgen_glWindowPos2s;
RGLSYMGLWINDOWPOS2SVPROC __rglgen_glWindowPos2sv;
RGLSYMGLWINDOWPOS3DPROC __rglgen_glWindowPos3d;
RGLSYMGLWINDOWPOS3DVPROC __rglgen_glWindowPos3dv;
RGLSYMGLWINDOWPOS3FPROC __rglgen_glWindowPos3f;
RGLSYMGLWINDOWPOS3FVPROC __rglgen_glWindowPos3fv;
RGLSYMGLWINDOWPOS3IPROC __rglgen_glWindowPos3i;
RGLSYMGLWINDOWPOS3IVPROC __rglgen_glWindowPos3iv;
RGLSYMGLWINDOWPOS3SPROC __rglgen_glWindowPos3s;
RGLSYMGLWINDOWPOS3SVPROC __rglgen_glWindowPos3sv;
RGLSYMGLBLENDCOLORPROC __rglgen_glBlendColor;
RGLSYMGLBLENDEQUATIONPROC __rglgen_glBlendEquation;
RGLSYMGLGENQUERIESPROC __rglgen_glGenQueries;
RGLSYMGLDELETEQUERIESPROC __rglgen_glDeleteQueries;
RGLSYMGLISQUERYPROC __rglgen_glIsQuery;
RGLSYMGLBEGINQUERYPROC __rglgen_glBeginQuery;
RGLSYMGLENDQUERYPROC __rglgen_glEndQuery;
RGLSYMGLGETQUERYIVPROC __rglgen_glGetQueryiv;
RGLSYMGLGETQUERYOBJECTIVPROC __rglgen_glGetQueryObjectiv;
RGLSYMGLGETQUERYOBJECTUIVPROC __rglgen_glGetQueryObjectuiv;
RGLSYMGLBINDBUFFERPROC __rglgen_glBindBuffer;
RGLSYMGLDELETEBUFFERSPROC __rglgen_glDeleteBuffers;
RGLSYMGLGENBUFFERSPROC __rglgen_glGenBuffers;
RGLSYMGLISBUFFERPROC __rglgen_glIsBuffer;
RGLSYMGLBUFFERDATAPROC __rglgen_glBufferData;
RGLSYMGLBUFFERSUBDATAPROC __rglgen_glBufferSubData;
RGLSYMGLGETBUFFERSUBDATAPROC __rglgen_glGetBufferSubData;
RGLSYMGLMAPBUFFERPROC __rglgen_glMapBuffer;
RGLSYMGLUNMAPBUFFERPROC __rglgen_glUnmapBuffer;
RGLSYMGLGETBUFFERPARAMETERIVPROC __rglgen_glGetBufferParameteriv;
RGLSYMGLGETBUFFERPOINTERVPROC __rglgen_glGetBufferPointerv;
RGLSYMGLBLENDEQUATIONSEPARATEPROC __rglgen_glBlendEquationSeparate;
RGLSYMGLDRAWBUFFERSPROC __rglgen_glDrawBuffers;
RGLSYMGLSTENCILOPSEPARATEPROC __rglgen_glStencilOpSeparate;
RGLSYMGLSTENCILFUNCSEPARATEPROC __rglgen_glStencilFuncSeparate;
RGLSYMGLSTENCILMASKSEPARATEPROC __rglgen_glStencilMaskSeparate;
RGLSYMGLATTACHSHADERPROC __rglgen_glAttachShader;
RGLSYMGLBINDATTRIBLOCATIONPROC __rglgen_glBindAttribLocation;
RGLSYMGLCOMPILESHADERPROC __rglgen_glCompileShader;
RGLSYMGLCREATEPROGRAMPROC __rglgen_glCreateProgram;
RGLSYMGLCREATESHADERPROC __rglgen_glCreateShader;
RGLSYMGLDELETEPROGRAMPROC __rglgen_glDeleteProgram;
RGLSYMGLDELETESHADERPROC __rglgen_glDeleteShader;
RGLSYMGLDETACHSHADERPROC __rglgen_glDetachShader;
RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC __rglgen_glDisableVertexAttribArray;
RGLSYMGLENABLEVERTEXATTRIBARRAYPROC __rglgen_glEnableVertexAttribArray;
RGLSYMGLGETACTIVEATTRIBPROC __rglgen_glGetActiveAttrib;
RGLSYMGLGETACTIVEUNIFORMPROC __rglgen_glGetActiveUniform;
RGLSYMGLGETATTACHEDSHADERSPROC __rglgen_glGetAttachedShaders;
RGLSYMGLGETATTRIBLOCATIONPROC __rglgen_glGetAttribLocation;
RGLSYMGLGETPROGRAMIVPROC __rglgen_glGetProgramiv;
RGLSYMGLGETPROGRAMINFOLOGPROC __rglgen_glGetProgramInfoLog;
RGLSYMGLGETSHADERIVPROC __rglgen_glGetShaderiv;
RGLSYMGLGETSHADERINFOLOGPROC __rglgen_glGetShaderInfoLog;
RGLSYMGLGETSHADERSOURCEPROC __rglgen_glGetShaderSource;
RGLSYMGLGETUNIFORMLOCATIONPROC __rglgen_glGetUniformLocation;
RGLSYMGLGETUNIFORMFVPROC __rglgen_glGetUniformfv;
RGLSYMGLGETUNIFORMIVPROC __rglgen_glGetUniformiv;
RGLSYMGLGETVERTEXATTRIBDVPROC __rglgen_glGetVertexAttribdv;
RGLSYMGLGETVERTEXATTRIBFVPROC __rglgen_glGetVertexAttribfv;
RGLSYMGLGETVERTEXATTRIBIVPROC __rglgen_glGetVertexAttribiv;
RGLSYMGLGETVERTEXATTRIBPOINTERVPROC __rglgen_glGetVertexAttribPointerv;
RGLSYMGLISPROGRAMPROC __rglgen_glIsProgram;
RGLSYMGLISSHADERPROC __rglgen_glIsShader;
RGLSYMGLLINKPROGRAMPROC __rglgen_glLinkProgram;
RGLSYMGLSHADERSOURCEPROC __rglgen_glShaderSource;
RGLSYMGLUSEPROGRAMPROC __rglgen_glUseProgram;
RGLSYMGLUNIFORM1FPROC __rglgen_glUniform1f;
RGLSYMGLUNIFORM2FPROC __rglgen_glUniform2f;
RGLSYMGLUNIFORM3FPROC __rglgen_glUniform3f;
RGLSYMGLUNIFORM4FPROC __rglgen_glUniform4f;
RGLSYMGLUNIFORM1IPROC __rglgen_glUniform1i;
RGLSYMGLUNIFORM2IPROC __rglgen_glUniform2i;
RGLSYMGLUNIFORM3IPROC __rglgen_glUniform3i;
RGLSYMGLUNIFORM4IPROC __rglgen_glUniform4i;
RGLSYMGLUNIFORM1FVPROC __rglgen_glUniform1fv;
RGLSYMGLUNIFORM2FVPROC __rglgen_glUniform2fv;
RGLSYMGLUNIFORM3FVPROC __rglgen_glUniform3fv;
RGLSYMGLUNIFORM4FVPROC __rglgen_glUniform4fv;
RGLSYMGLUNIFORM1IVPROC __rglgen_glUniform1iv;
RGLSYMGLUNIFORM2IVPROC __rglgen_glUniform2iv;
RGLSYMGLUNIFORM3IVPROC __rglgen_glUniform3iv;
RGLSYMGLUNIFORM4IVPROC __rglgen_glUniform4iv;
RGLSYMGLUNIFORMMATRIX2FVPROC __rglgen_glUniformMatrix2fv;
RGLSYMGLUNIFORMMATRIX3FVPROC __rglgen_glUniformMatrix3fv;
RGLSYMGLUNIFORMMATRIX4FVPROC __rglgen_glUniformMatrix4fv;
RGLSYMGLVALIDATEPROGRAMPROC __rglgen_glValidateProgram;
RGLSYMGLVERTEXATTRIB1DPROC __rglgen_glVertexAttrib1d;
RGLSYMGLVERTEXATTRIB1DVPROC __rglgen_glVertexAttrib1dv;
RGLSYMGLVERTEXATTRIB1FPROC __rglgen_glVertexAttrib1f;
RGLSYMGLVERTEXATTRIB1FVPROC __rglgen_glVertexAttrib1fv;
RGLSYMGLVERTEXATTRIB1SPROC __rglgen_glVertexAttrib1s;
RGLSYMGLVERTEXATTRIB1SVPROC __rglgen_glVertexAttrib1sv;
RGLSYMGLVERTEXATTRIB2DPROC __rglgen_glVertexAttrib2d;
RGLSYMGLVERTEXATTRIB2DVPROC __rglgen_glVertexAttrib2dv;
RGLSYMGLVERTEXATTRIB2FPROC __rglgen_glVertexAttrib2f;
RGLSYMGLVERTEXATTRIB2FVPROC __rglgen_glVertexAttrib2fv;
RGLSYMGLVERTEXATTRIB2SPROC __rglgen_glVertexAttrib2s;
RGLSYMGLVERTEXATTRIB2SVPROC __rglgen_glVertexAttrib2sv;
RGLSYMGLVERTEXATTRIB3DPROC __rglgen_glVertexAttrib3d;
RGLSYMGLVERTEXATTRIB3DVPROC __rglgen_glVertexAttrib3dv;
RGLSYMGLVERTEXATTRIB3FPROC __rglgen_glVertexAttrib3f;
RGLSYMGLVERTEXATTRIB3FVPROC __rglgen_glVertexAttrib3fv;
RGLSYMGLVERTEXATTRIB3SPROC __rglgen_glVertexAttrib3s;
RGLSYMGLVERTEXATTRIB3SVPROC __rglgen_glVertexAttrib3sv;
RGLSYMGLVERTEXATTRIB4NBVPROC __rglgen_glVertexAttrib4Nbv;
RGLSYMGLVERTEXATTRIB4NIVPROC __rglgen_glVertexAttrib4Niv;
RGLSYMGLVERTEXATTRIB4NSVPROC __rglgen_glVertexAttrib4Nsv;
RGLSYMGLVERTEXATTRIB4NUBPROC __rglgen_glVertexAttrib4Nub;
RGLSYMGLVERTEXATTRIB4NUBVPROC __rglgen_glVertexAttrib4Nubv;
RGLSYMGLVERTEXATTRIB4NUIVPROC __rglgen_glVertexAttrib4Nuiv;
RGLSYMGLVERTEXATTRIB4NUSVPROC __rglgen_glVertexAttrib4Nusv;
RGLSYMGLVERTEXATTRIB4BVPROC __rglgen_glVertexAttrib4bv;
RGLSYMGLVERTEXATTRIB4DPROC __rglgen_glVertexAttrib4d;
RGLSYMGLVERTEXATTRIB4DVPROC __rglgen_glVertexAttrib4dv;
RGLSYMGLVERTEXATTRIB4FPROC __rglgen_glVertexAttrib4f;
RGLSYMGLVERTEXATTRIB4FVPROC __rglgen_glVertexAttrib4fv;
RGLSYMGLVERTEXATTRIB4IVPROC __rglgen_glVertexAttrib4iv;
RGLSYMGLVERTEXATTRIB4SPROC __rglgen_glVertexAttrib4s;
RGLSYMGLVERTEXATTRIB4SVPROC __rglgen_glVertexAttrib4sv;
RGLSYMGLVERTEXATTRIB4UBVPROC __rglgen_glVertexAttrib4ubv;
RGLSYMGLVERTEXATTRIB4UIVPROC __rglgen_glVertexAttrib4uiv;
RGLSYMGLVERTEXATTRIB4USVPROC __rglgen_glVertexAttrib4usv;
RGLSYMGLVERTEXATTRIBPOINTERPROC __rglgen_glVertexAttribPointer;
RGLSYMGLUNIFORMMATRIX2X3FVPROC __rglgen_glUniformMatrix2x3fv;
RGLSYMGLUNIFORMMATRIX3X2FVPROC __rglgen_glUniformMatrix3x2fv;
RGLSYMGLUNIFORMMATRIX2X4FVPROC __rglgen_glUniformMatrix2x4fv;
RGLSYMGLUNIFORMMATRIX4X2FVPROC __rglgen_glUniformMatrix4x2fv;
RGLSYMGLUNIFORMMATRIX3X4FVPROC __rglgen_glUniformMatrix3x4fv;
RGLSYMGLUNIFORMMATRIX4X3FVPROC __rglgen_glUniformMatrix4x3fv;
RGLSYMGLCOLORMASKIPROC __rglgen_glColorMaski;
RGLSYMGLGETBOOLEANI_VPROC __rglgen_glGetBooleani_v;
RGLSYMGLGETINTEGERI_VPROC __rglgen_glGetIntegeri_v;
RGLSYMGLENABLEIPROC __rglgen_glEnablei;
RGLSYMGLDISABLEIPROC __rglgen_glDisablei;
RGLSYMGLISENABLEDIPROC __rglgen_glIsEnabledi;
RGLSYMGLBEGINTRANSFORMFEEDBACKPROC __rglgen_glBeginTransformFeedback;
RGLSYMGLENDTRANSFORMFEEDBACKPROC __rglgen_glEndTransformFeedback;
RGLSYMGLBINDBUFFERRANGEPROC __rglgen_glBindBufferRange;
RGLSYMGLBINDBUFFERBASEPROC __rglgen_glBindBufferBase;
RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC __rglgen_glTransformFeedbackVaryings;
RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC __rglgen_glGetTransformFeedbackVarying;
RGLSYMGLCLAMPCOLORPROC __rglgen_glClampColor;
RGLSYMGLBEGINCONDITIONALRENDERPROC __rglgen_glBeginConditionalRender;
RGLSYMGLENDCONDITIONALRENDERPROC __rglgen_glEndConditionalRender;
RGLSYMGLVERTEXATTRIBIPOINTERPROC __rglgen_glVertexAttribIPointer;
RGLSYMGLGETVERTEXATTRIBIIVPROC __rglgen_glGetVertexAttribIiv;
RGLSYMGLGETVERTEXATTRIBIUIVPROC __rglgen_glGetVertexAttribIuiv;
RGLSYMGLVERTEXATTRIBI1IPROC __rglgen_glVertexAttribI1i;
RGLSYMGLVERTEXATTRIBI2IPROC __rglgen_glVertexAttribI2i;
RGLSYMGLVERTEXATTRIBI3IPROC __rglgen_glVertexAttribI3i;
RGLSYMGLVERTEXATTRIBI4IPROC __rglgen_glVertexAttribI4i;
RGLSYMGLVERTEXATTRIBI1UIPROC __rglgen_glVertexAttribI1ui;
RGLSYMGLVERTEXATTRIBI2UIPROC __rglgen_glVertexAttribI2ui;
RGLSYMGLVERTEXATTRIBI3UIPROC __rglgen_glVertexAttribI3ui;
RGLSYMGLVERTEXATTRIBI4UIPROC __rglgen_glVertexAttribI4ui;
RGLSYMGLVERTEXATTRIBI1IVPROC __rglgen_glVertexAttribI1iv;
RGLSYMGLVERTEXATTRIBI2IVPROC __rglgen_glVertexAttribI2iv;
RGLSYMGLVERTEXATTRIBI3IVPROC __rglgen_glVertexAttribI3iv;
RGLSYMGLVERTEXATTRIBI4IVPROC __rglgen_glVertexAttribI4iv;
RGLSYMGLVERTEXATTRIBI1UIVPROC __rglgen_glVertexAttribI1uiv;
RGLSYMGLVERTEXATTRIBI2UIVPROC __rglgen_glVertexAttribI2uiv;
RGLSYMGLVERTEXATTRIBI3UIVPROC __rglgen_glVertexAttribI3uiv;
RGLSYMGLVERTEXATTRIBI4UIVPROC __rglgen_glVertexAttribI4uiv;
RGLSYMGLVERTEXATTRIBI4BVPROC __rglgen_glVertexAttribI4bv;
RGLSYMGLVERTEXATTRIBI4SVPROC __rglgen_glVertexAttribI4sv;
RGLSYMGLVERTEXATTRIBI4UBVPROC __rglgen_glVertexAttribI4ubv;
RGLSYMGLVERTEXATTRIBI4USVPROC __rglgen_glVertexAttribI4usv;
RGLSYMGLGETUNIFORMUIVPROC __rglgen_glGetUniformuiv;
RGLSYMGLBINDFRAGDATALOCATIONPROC __rglgen_glBindFragDataLocation;
RGLSYMGLGETFRAGDATALOCATIONPROC __rglgen_glGetFragDataLocation;
RGLSYMGLUNIFORM1UIPROC __rglgen_glUniform1ui;
RGLSYMGLUNIFORM2UIPROC __rglgen_glUniform2ui;
RGLSYMGLUNIFORM3UIPROC __rglgen_glUniform3ui;
RGLSYMGLUNIFORM4UIPROC __rglgen_glUniform4ui;
RGLSYMGLUNIFORM1UIVPROC __rglgen_glUniform1uiv;
RGLSYMGLUNIFORM2UIVPROC __rglgen_glUniform2uiv;
RGLSYMGLUNIFORM3UIVPROC __rglgen_glUniform3uiv;
RGLSYMGLUNIFORM4UIVPROC __rglgen_glUniform4uiv;
RGLSYMGLTEXPARAMETERIIVPROC __rglgen_glTexParameterIiv;
RGLSYMGLTEXPARAMETERIUIVPROC __rglgen_glTexParameterIuiv;
RGLSYMGLGETTEXPARAMETERIIVPROC __rglgen_glGetTexParameterIiv;
RGLSYMGLGETTEXPARAMETERIUIVPROC __rglgen_glGetTexParameterIuiv;
RGLSYMGLCLEARBUFFERIVPROC __rglgen_glClearBufferiv;
RGLSYMGLCLEARBUFFERUIVPROC __rglgen_glClearBufferuiv;
RGLSYMGLCLEARBUFFERFVPROC __rglgen_glClearBufferfv;
RGLSYMGLCLEARBUFFERFIPROC __rglgen_glClearBufferfi;
RGLSYMGLGETSTRINGIPROC __rglgen_glGetStringi;
RGLSYMGLISRENDERBUFFERPROC __rglgen_glIsRenderbuffer;
RGLSYMGLBINDRENDERBUFFERPROC __rglgen_glBindRenderbuffer;
RGLSYMGLDELETERENDERBUFFERSPROC __rglgen_glDeleteRenderbuffers;
RGLSYMGLGENRENDERBUFFERSPROC __rglgen_glGenRenderbuffers;
RGLSYMGLRENDERBUFFERSTORAGEPROC __rglgen_glRenderbufferStorage;
RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC __rglgen_glGetRenderbufferParameteriv;
RGLSYMGLISFRAMEBUFFERPROC __rglgen_glIsFramebuffer;
RGLSYMGLBINDFRAMEBUFFERPROC __rglgen_glBindFramebuffer;
RGLSYMGLDELETEFRAMEBUFFERSPROC __rglgen_glDeleteFramebuffers;
RGLSYMGLGENFRAMEBUFFERSPROC __rglgen_glGenFramebuffers;
RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC __rglgen_glCheckFramebufferStatus;
RGLSYMGLFRAMEBUFFERTEXTURE1DPROC __rglgen_glFramebufferTexture1D;
RGLSYMGLFRAMEBUFFERTEXTURE2DPROC __rglgen_glFramebufferTexture2D;
RGLSYMGLFRAMEBUFFERTEXTURE3DPROC __rglgen_glFramebufferTexture3D;
RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC __rglgen_glFramebufferRenderbuffer;
RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC __rglgen_glGetFramebufferAttachmentParameteriv;
RGLSYMGLGENERATEMIPMAPPROC __rglgen_glGenerateMipmap;
RGLSYMGLBLITFRAMEBUFFERPROC __rglgen_glBlitFramebuffer;
RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC __rglgen_glRenderbufferStorageMultisample;
RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC __rglgen_glFramebufferTextureLayer;
RGLSYMGLMAPBUFFERRANGEPROC __rglgen_glMapBufferRange;
RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC __rglgen_glFlushMappedBufferRange;
RGLSYMGLBINDVERTEXARRAYPROC __rglgen_glBindVertexArray;
RGLSYMGLDELETEVERTEXARRAYSPROC __rglgen_glDeleteVertexArrays;
RGLSYMGLGENVERTEXARRAYSPROC __rglgen_glGenVertexArrays;
RGLSYMGLISVERTEXARRAYPROC __rglgen_glIsVertexArray;
RGLSYMGLDRAWARRAYSINSTANCEDPROC __rglgen_glDrawArraysInstanced;
RGLSYMGLDRAWELEMENTSINSTANCEDPROC __rglgen_glDrawElementsInstanced;
RGLSYMGLTEXBUFFERPROC __rglgen_glTexBuffer;
RGLSYMGLPRIMITIVERESTARTINDEXPROC __rglgen_glPrimitiveRestartIndex;
RGLSYMGLCOPYBUFFERSUBDATAPROC __rglgen_glCopyBufferSubData;
RGLSYMGLGETUNIFORMINDICESPROC __rglgen_glGetUniformIndices;
RGLSYMGLGETACTIVEUNIFORMSIVPROC __rglgen_glGetActiveUniformsiv;
RGLSYMGLGETACTIVEUNIFORMNAMEPROC __rglgen_glGetActiveUniformName;
RGLSYMGLGETUNIFORMBLOCKINDEXPROC __rglgen_glGetUniformBlockIndex;
RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC __rglgen_glGetActiveUniformBlockiv;
RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC __rglgen_glGetActiveUniformBlockName;
RGLSYMGLUNIFORMBLOCKBINDINGPROC __rglgen_glUniformBlockBinding;
RGLSYMGLDRAWELEMENTSBASEVERTEXPROC __rglgen_glDrawElementsBaseVertex;
RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC __rglgen_glDrawRangeElementsBaseVertex;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC __rglgen_glDrawElementsInstancedBaseVertex;
RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXPROC __rglgen_glMultiDrawElementsBaseVertex;
RGLSYMGLPROVOKINGVERTEXPROC __rglgen_glProvokingVertex;
RGLSYMGLFENCESYNCPROC __rglgen_glFenceSync;
RGLSYMGLISSYNCPROC __rglgen_glIsSync;
RGLSYMGLDELETESYNCPROC __rglgen_glDeleteSync;
RGLSYMGLCLIENTWAITSYNCPROC __rglgen_glClientWaitSync;
RGLSYMGLWAITSYNCPROC __rglgen_glWaitSync;
RGLSYMGLGETINTEGER64VPROC __rglgen_glGetInteger64v;
RGLSYMGLGETSYNCIVPROC __rglgen_glGetSynciv;
RGLSYMGLGETINTEGER64I_VPROC __rglgen_glGetInteger64i_v;
RGLSYMGLGETBUFFERPARAMETERI64VPROC __rglgen_glGetBufferParameteri64v;
RGLSYMGLFRAMEBUFFERTEXTUREPROC __rglgen_glFramebufferTexture;
RGLSYMGLTEXIMAGE2DMULTISAMPLEPROC __rglgen_glTexImage2DMultisample;
RGLSYMGLTEXIMAGE3DMULTISAMPLEPROC __rglgen_glTexImage3DMultisample;
RGLSYMGLGETMULTISAMPLEFVPROC __rglgen_glGetMultisamplefv;
RGLSYMGLSAMPLEMASKIPROC __rglgen_glSampleMaski;
RGLSYMGLBINDFRAGDATALOCATIONINDEXEDPROC __rglgen_glBindFragDataLocationIndexed;
RGLSYMGLGETFRAGDATAINDEXPROC __rglgen_glGetFragDataIndex;
RGLSYMGLGENSAMPLERSPROC __rglgen_glGenSamplers;
RGLSYMGLDELETESAMPLERSPROC __rglgen_glDeleteSamplers;
RGLSYMGLISSAMPLERPROC __rglgen_glIsSampler;
RGLSYMGLBINDSAMPLERPROC __rglgen_glBindSampler;
RGLSYMGLSAMPLERPARAMETERIPROC __rglgen_glSamplerParameteri;
RGLSYMGLSAMPLERPARAMETERIVPROC __rglgen_glSamplerParameteriv;
RGLSYMGLSAMPLERPARAMETERFPROC __rglgen_glSamplerParameterf;
RGLSYMGLSAMPLERPARAMETERFVPROC __rglgen_glSamplerParameterfv;
RGLSYMGLSAMPLERPARAMETERIIVPROC __rglgen_glSamplerParameterIiv;
RGLSYMGLSAMPLERPARAMETERIUIVPROC __rglgen_glSamplerParameterIuiv;
RGLSYMGLGETSAMPLERPARAMETERIVPROC __rglgen_glGetSamplerParameteriv;
RGLSYMGLGETSAMPLERPARAMETERIIVPROC __rglgen_glGetSamplerParameterIiv;
RGLSYMGLGETSAMPLERPARAMETERFVPROC __rglgen_glGetSamplerParameterfv;
RGLSYMGLGETSAMPLERPARAMETERIUIVPROC __rglgen_glGetSamplerParameterIuiv;
RGLSYMGLQUERYCOUNTERPROC __rglgen_glQueryCounter;
RGLSYMGLGETQUERYOBJECTI64VPROC __rglgen_glGetQueryObjecti64v;
RGLSYMGLGETQUERYOBJECTUI64VPROC __rglgen_glGetQueryObjectui64v;
RGLSYMGLVERTEXATTRIBDIVISORPROC __rglgen_glVertexAttribDivisor;
RGLSYMGLVERTEXATTRIBP1UIPROC __rglgen_glVertexAttribP1ui;
RGLSYMGLVERTEXATTRIBP1UIVPROC __rglgen_glVertexAttribP1uiv;
RGLSYMGLVERTEXATTRIBP2UIPROC __rglgen_glVertexAttribP2ui;
RGLSYMGLVERTEXATTRIBP2UIVPROC __rglgen_glVertexAttribP2uiv;
RGLSYMGLVERTEXATTRIBP3UIPROC __rglgen_glVertexAttribP3ui;
RGLSYMGLVERTEXATTRIBP3UIVPROC __rglgen_glVertexAttribP3uiv;
RGLSYMGLVERTEXATTRIBP4UIPROC __rglgen_glVertexAttribP4ui;
RGLSYMGLVERTEXATTRIBP4UIVPROC __rglgen_glVertexAttribP4uiv;
RGLSYMGLVERTEXP2UIPROC __rglgen_glVertexP2ui;
RGLSYMGLVERTEXP2UIVPROC __rglgen_glVertexP2uiv;
RGLSYMGLVERTEXP3UIPROC __rglgen_glVertexP3ui;
RGLSYMGLVERTEXP3UIVPROC __rglgen_glVertexP3uiv;
RGLSYMGLVERTEXP4UIPROC __rglgen_glVertexP4ui;
RGLSYMGLVERTEXP4UIVPROC __rglgen_glVertexP4uiv;
RGLSYMGLTEXCOORDP1UIPROC __rglgen_glTexCoordP1ui;
RGLSYMGLTEXCOORDP1UIVPROC __rglgen_glTexCoordP1uiv;
RGLSYMGLTEXCOORDP2UIPROC __rglgen_glTexCoordP2ui;
RGLSYMGLTEXCOORDP2UIVPROC __rglgen_glTexCoordP2uiv;
RGLSYMGLTEXCOORDP3UIPROC __rglgen_glTexCoordP3ui;
RGLSYMGLTEXCOORDP3UIVPROC __rglgen_glTexCoordP3uiv;
RGLSYMGLTEXCOORDP4UIPROC __rglgen_glTexCoordP4ui;
RGLSYMGLTEXCOORDP4UIVPROC __rglgen_glTexCoordP4uiv;
RGLSYMGLMULTITEXCOORDP1UIPROC __rglgen_glMultiTexCoordP1ui;
RGLSYMGLMULTITEXCOORDP1UIVPROC __rglgen_glMultiTexCoordP1uiv;
RGLSYMGLMULTITEXCOORDP2UIPROC __rglgen_glMultiTexCoordP2ui;
RGLSYMGLMULTITEXCOORDP2UIVPROC __rglgen_glMultiTexCoordP2uiv;
RGLSYMGLMULTITEXCOORDP3UIPROC __rglgen_glMultiTexCoordP3ui;
RGLSYMGLMULTITEXCOORDP3UIVPROC __rglgen_glMultiTexCoordP3uiv;
RGLSYMGLMULTITEXCOORDP4UIPROC __rglgen_glMultiTexCoordP4ui;
RGLSYMGLMULTITEXCOORDP4UIVPROC __rglgen_glMultiTexCoordP4uiv;
RGLSYMGLNORMALP3UIPROC __rglgen_glNormalP3ui;
RGLSYMGLNORMALP3UIVPROC __rglgen_glNormalP3uiv;
RGLSYMGLCOLORP3UIPROC __rglgen_glColorP3ui;
RGLSYMGLCOLORP3UIVPROC __rglgen_glColorP3uiv;
RGLSYMGLCOLORP4UIPROC __rglgen_glColorP4ui;
RGLSYMGLCOLORP4UIVPROC __rglgen_glColorP4uiv;
RGLSYMGLSECONDARYCOLORP3UIPROC __rglgen_glSecondaryColorP3ui;
RGLSYMGLSECONDARYCOLORP3UIVPROC __rglgen_glSecondaryColorP3uiv;
RGLSYMGLMINSAMPLESHADINGPROC __rglgen_glMinSampleShading;
RGLSYMGLBLENDEQUATIONIPROC __rglgen_glBlendEquationi;
RGLSYMGLBLENDEQUATIONSEPARATEIPROC __rglgen_glBlendEquationSeparatei;
RGLSYMGLBLENDFUNCIPROC __rglgen_glBlendFunci;
RGLSYMGLBLENDFUNCSEPARATEIPROC __rglgen_glBlendFuncSeparatei;
RGLSYMGLDRAWARRAYSINDIRECTPROC __rglgen_glDrawArraysIndirect;
RGLSYMGLDRAWELEMENTSINDIRECTPROC __rglgen_glDrawElementsIndirect;
RGLSYMGLUNIFORM1DPROC __rglgen_glUniform1d;
RGLSYMGLUNIFORM2DPROC __rglgen_glUniform2d;
RGLSYMGLUNIFORM3DPROC __rglgen_glUniform3d;
RGLSYMGLUNIFORM4DPROC __rglgen_glUniform4d;
RGLSYMGLUNIFORM1DVPROC __rglgen_glUniform1dv;
RGLSYMGLUNIFORM2DVPROC __rglgen_glUniform2dv;
RGLSYMGLUNIFORM3DVPROC __rglgen_glUniform3dv;
RGLSYMGLUNIFORM4DVPROC __rglgen_glUniform4dv;
RGLSYMGLUNIFORMMATRIX2DVPROC __rglgen_glUniformMatrix2dv;
RGLSYMGLUNIFORMMATRIX3DVPROC __rglgen_glUniformMatrix3dv;
RGLSYMGLUNIFORMMATRIX4DVPROC __rglgen_glUniformMatrix4dv;
RGLSYMGLUNIFORMMATRIX2X3DVPROC __rglgen_glUniformMatrix2x3dv;
RGLSYMGLUNIFORMMATRIX2X4DVPROC __rglgen_glUniformMatrix2x4dv;
RGLSYMGLUNIFORMMATRIX3X2DVPROC __rglgen_glUniformMatrix3x2dv;
RGLSYMGLUNIFORMMATRIX3X4DVPROC __rglgen_glUniformMatrix3x4dv;
RGLSYMGLUNIFORMMATRIX4X2DVPROC __rglgen_glUniformMatrix4x2dv;
RGLSYMGLUNIFORMMATRIX4X3DVPROC __rglgen_glUniformMatrix4x3dv;
RGLSYMGLGETUNIFORMDVPROC __rglgen_glGetUniformdv;
RGLSYMGLGETSUBROUTINEUNIFORMLOCATIONPROC __rglgen_glGetSubroutineUniformLocation;
RGLSYMGLGETSUBROUTINEINDEXPROC __rglgen_glGetSubroutineIndex;
RGLSYMGLGETACTIVESUBROUTINEUNIFORMIVPROC __rglgen_glGetActiveSubroutineUniformiv;
RGLSYMGLGETACTIVESUBROUTINEUNIFORMNAMEPROC __rglgen_glGetActiveSubroutineUniformName;
RGLSYMGLGETACTIVESUBROUTINENAMEPROC __rglgen_glGetActiveSubroutineName;
RGLSYMGLUNIFORMSUBROUTINESUIVPROC __rglgen_glUniformSubroutinesuiv;
RGLSYMGLGETUNIFORMSUBROUTINEUIVPROC __rglgen_glGetUniformSubroutineuiv;
RGLSYMGLGETPROGRAMSTAGEIVPROC __rglgen_glGetProgramStageiv;
RGLSYMGLPATCHPARAMETERIPROC __rglgen_glPatchParameteri;
RGLSYMGLPATCHPARAMETERFVPROC __rglgen_glPatchParameterfv;
RGLSYMGLBINDTRANSFORMFEEDBACKPROC __rglgen_glBindTransformFeedback;
RGLSYMGLDELETETRANSFORMFEEDBACKSPROC __rglgen_glDeleteTransformFeedbacks;
RGLSYMGLGENTRANSFORMFEEDBACKSPROC __rglgen_glGenTransformFeedbacks;
RGLSYMGLISTRANSFORMFEEDBACKPROC __rglgen_glIsTransformFeedback;
RGLSYMGLPAUSETRANSFORMFEEDBACKPROC __rglgen_glPauseTransformFeedback;
RGLSYMGLRESUMETRANSFORMFEEDBACKPROC __rglgen_glResumeTransformFeedback;
RGLSYMGLDRAWTRANSFORMFEEDBACKPROC __rglgen_glDrawTransformFeedback;
RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMPROC __rglgen_glDrawTransformFeedbackStream;
RGLSYMGLBEGINQUERYINDEXEDPROC __rglgen_glBeginQueryIndexed;
RGLSYMGLENDQUERYINDEXEDPROC __rglgen_glEndQueryIndexed;
RGLSYMGLGETQUERYINDEXEDIVPROC __rglgen_glGetQueryIndexediv;
RGLSYMGLRELEASESHADERCOMPILERPROC __rglgen_glReleaseShaderCompiler;
RGLSYMGLSHADERBINARYPROC __rglgen_glShaderBinary;
RGLSYMGLGETSHADERPRECISIONFORMATPROC __rglgen_glGetShaderPrecisionFormat;
RGLSYMGLDEPTHRANGEFPROC __rglgen_glDepthRangef;
RGLSYMGLCLEARDEPTHFPROC __rglgen_glClearDepthf;
RGLSYMGLGETPROGRAMBINARYPROC __rglgen_glGetProgramBinary;
RGLSYMGLPROGRAMBINARYPROC __rglgen_glProgramBinary;
RGLSYMGLPROGRAMPARAMETERIPROC __rglgen_glProgramParameteri;
RGLSYMGLUSEPROGRAMSTAGESPROC __rglgen_glUseProgramStages;
RGLSYMGLACTIVESHADERPROGRAMPROC __rglgen_glActiveShaderProgram;
RGLSYMGLCREATESHADERPROGRAMVPROC __rglgen_glCreateShaderProgramv;
RGLSYMGLBINDPROGRAMPIPELINEPROC __rglgen_glBindProgramPipeline;
RGLSYMGLDELETEPROGRAMPIPELINESPROC __rglgen_glDeleteProgramPipelines;
RGLSYMGLGENPROGRAMPIPELINESPROC __rglgen_glGenProgramPipelines;
RGLSYMGLISPROGRAMPIPELINEPROC __rglgen_glIsProgramPipeline;
RGLSYMGLGETPROGRAMPIPELINEIVPROC __rglgen_glGetProgramPipelineiv;
RGLSYMGLPROGRAMUNIFORM1IPROC __rglgen_glProgramUniform1i;
RGLSYMGLPROGRAMUNIFORM1IVPROC __rglgen_glProgramUniform1iv;
RGLSYMGLPROGRAMUNIFORM1FPROC __rglgen_glProgramUniform1f;
RGLSYMGLPROGRAMUNIFORM1FVPROC __rglgen_glProgramUniform1fv;
RGLSYMGLPROGRAMUNIFORM1DPROC __rglgen_glProgramUniform1d;
RGLSYMGLPROGRAMUNIFORM1DVPROC __rglgen_glProgramUniform1dv;
RGLSYMGLPROGRAMUNIFORM1UIPROC __rglgen_glProgramUniform1ui;
RGLSYMGLPROGRAMUNIFORM1UIVPROC __rglgen_glProgramUniform1uiv;
RGLSYMGLPROGRAMUNIFORM2IPROC __rglgen_glProgramUniform2i;
RGLSYMGLPROGRAMUNIFORM2IVPROC __rglgen_glProgramUniform2iv;
RGLSYMGLPROGRAMUNIFORM2FPROC __rglgen_glProgramUniform2f;
RGLSYMGLPROGRAMUNIFORM2FVPROC __rglgen_glProgramUniform2fv;
RGLSYMGLPROGRAMUNIFORM2DPROC __rglgen_glProgramUniform2d;
RGLSYMGLPROGRAMUNIFORM2DVPROC __rglgen_glProgramUniform2dv;
RGLSYMGLPROGRAMUNIFORM2UIPROC __rglgen_glProgramUniform2ui;
RGLSYMGLPROGRAMUNIFORM2UIVPROC __rglgen_glProgramUniform2uiv;
RGLSYMGLPROGRAMUNIFORM3IPROC __rglgen_glProgramUniform3i;
RGLSYMGLPROGRAMUNIFORM3IVPROC __rglgen_glProgramUniform3iv;
RGLSYMGLPROGRAMUNIFORM3FPROC __rglgen_glProgramUniform3f;
RGLSYMGLPROGRAMUNIFORM3FVPROC __rglgen_glProgramUniform3fv;
RGLSYMGLPROGRAMUNIFORM3DPROC __rglgen_glProgramUniform3d;
RGLSYMGLPROGRAMUNIFORM3DVPROC __rglgen_glProgramUniform3dv;
RGLSYMGLPROGRAMUNIFORM3UIPROC __rglgen_glProgramUniform3ui;
RGLSYMGLPROGRAMUNIFORM3UIVPROC __rglgen_glProgramUniform3uiv;
RGLSYMGLPROGRAMUNIFORM4IPROC __rglgen_glProgramUniform4i;
RGLSYMGLPROGRAMUNIFORM4IVPROC __rglgen_glProgramUniform4iv;
RGLSYMGLPROGRAMUNIFORM4FPROC __rglgen_glProgramUniform4f;
RGLSYMGLPROGRAMUNIFORM4FVPROC __rglgen_glProgramUniform4fv;
RGLSYMGLPROGRAMUNIFORM4DPROC __rglgen_glProgramUniform4d;
RGLSYMGLPROGRAMUNIFORM4DVPROC __rglgen_glProgramUniform4dv;
RGLSYMGLPROGRAMUNIFORM4UIPROC __rglgen_glProgramUniform4ui;
RGLSYMGLPROGRAMUNIFORM4UIVPROC __rglgen_glProgramUniform4uiv;
RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC __rglgen_glProgramUniformMatrix2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC __rglgen_glProgramUniformMatrix3fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC __rglgen_glProgramUniformMatrix4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2DVPROC __rglgen_glProgramUniformMatrix2dv;
RGLSYMGLPROGRAMUNIFORMMATRIX3DVPROC __rglgen_glProgramUniformMatrix3dv;
RGLSYMGLPROGRAMUNIFORMMATRIX4DVPROC __rglgen_glProgramUniformMatrix4dv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC __rglgen_glProgramUniformMatrix2x3fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC __rglgen_glProgramUniformMatrix3x2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC __rglgen_glProgramUniformMatrix2x4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC __rglgen_glProgramUniformMatrix4x2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC __rglgen_glProgramUniformMatrix3x4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC __rglgen_glProgramUniformMatrix4x3fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVPROC __rglgen_glProgramUniformMatrix2x3dv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVPROC __rglgen_glProgramUniformMatrix3x2dv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVPROC __rglgen_glProgramUniformMatrix2x4dv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVPROC __rglgen_glProgramUniformMatrix4x2dv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVPROC __rglgen_glProgramUniformMatrix3x4dv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVPROC __rglgen_glProgramUniformMatrix4x3dv;
RGLSYMGLVALIDATEPROGRAMPIPELINEPROC __rglgen_glValidateProgramPipeline;
RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC __rglgen_glGetProgramPipelineInfoLog;
RGLSYMGLVERTEXATTRIBL1DPROC __rglgen_glVertexAttribL1d;
RGLSYMGLVERTEXATTRIBL2DPROC __rglgen_glVertexAttribL2d;
RGLSYMGLVERTEXATTRIBL3DPROC __rglgen_glVertexAttribL3d;
RGLSYMGLVERTEXATTRIBL4DPROC __rglgen_glVertexAttribL4d;
RGLSYMGLVERTEXATTRIBL1DVPROC __rglgen_glVertexAttribL1dv;
RGLSYMGLVERTEXATTRIBL2DVPROC __rglgen_glVertexAttribL2dv;
RGLSYMGLVERTEXATTRIBL3DVPROC __rglgen_glVertexAttribL3dv;
RGLSYMGLVERTEXATTRIBL4DVPROC __rglgen_glVertexAttribL4dv;
RGLSYMGLVERTEXATTRIBLPOINTERPROC __rglgen_glVertexAttribLPointer;
RGLSYMGLGETVERTEXATTRIBLDVPROC __rglgen_glGetVertexAttribLdv;
RGLSYMGLVIEWPORTARRAYVPROC __rglgen_glViewportArrayv;
RGLSYMGLVIEWPORTINDEXEDFPROC __rglgen_glViewportIndexedf;
RGLSYMGLVIEWPORTINDEXEDFVPROC __rglgen_glViewportIndexedfv;
RGLSYMGLSCISSORARRAYVPROC __rglgen_glScissorArrayv;
RGLSYMGLSCISSORINDEXEDPROC __rglgen_glScissorIndexed;
RGLSYMGLSCISSORINDEXEDVPROC __rglgen_glScissorIndexedv;
RGLSYMGLDEPTHRANGEARRAYVPROC __rglgen_glDepthRangeArrayv;
RGLSYMGLDEPTHRANGEINDEXEDPROC __rglgen_glDepthRangeIndexed;
RGLSYMGLGETFLOATI_VPROC __rglgen_glGetFloati_v;
RGLSYMGLGETDOUBLEI_VPROC __rglgen_glGetDoublei_v;
RGLSYMGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC __rglgen_glDrawArraysInstancedBaseInstance;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC __rglgen_glDrawElementsInstancedBaseInstance;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC __rglgen_glDrawElementsInstancedBaseVertexBaseInstance;
RGLSYMGLGETINTERNALFORMATIVPROC __rglgen_glGetInternalformativ;
RGLSYMGLGETACTIVEATOMICCOUNTERBUFFERIVPROC __rglgen_glGetActiveAtomicCounterBufferiv;
RGLSYMGLBINDIMAGETEXTUREPROC __rglgen_glBindImageTexture;
RGLSYMGLMEMORYBARRIERPROC __rglgen_glMemoryBarrier;
RGLSYMGLTEXSTORAGE1DPROC __rglgen_glTexStorage1D;
RGLSYMGLTEXSTORAGE2DPROC __rglgen_glTexStorage2D;
RGLSYMGLTEXSTORAGE3DPROC __rglgen_glTexStorage3D;
RGLSYMGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC __rglgen_glDrawTransformFeedbackInstanced;
RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC __rglgen_glDrawTransformFeedbackStreamInstanced;
RGLSYMGLCLEARBUFFERDATAPROC __rglgen_glClearBufferData;
RGLSYMGLCLEARBUFFERSUBDATAPROC __rglgen_glClearBufferSubData;
RGLSYMGLDISPATCHCOMPUTEPROC __rglgen_glDispatchCompute;
RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC __rglgen_glDispatchComputeIndirect;
RGLSYMGLCOPYIMAGESUBDATAPROC __rglgen_glCopyImageSubData;
RGLSYMGLFRAMEBUFFERPARAMETERIPROC __rglgen_glFramebufferParameteri;
RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC __rglgen_glGetFramebufferParameteriv;
RGLSYMGLGETINTERNALFORMATI64VPROC __rglgen_glGetInternalformati64v;
RGLSYMGLINVALIDATETEXSUBIMAGEPROC __rglgen_glInvalidateTexSubImage;
RGLSYMGLINVALIDATETEXIMAGEPROC __rglgen_glInvalidateTexImage;
RGLSYMGLINVALIDATEBUFFERSUBDATAPROC __rglgen_glInvalidateBufferSubData;
RGLSYMGLINVALIDATEBUFFERDATAPROC __rglgen_glInvalidateBufferData;
RGLSYMGLINVALIDATEFRAMEBUFFERPROC __rglgen_glInvalidateFramebuffer;
RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC __rglgen_glInvalidateSubFramebuffer;
RGLSYMGLMULTIDRAWARRAYSINDIRECTPROC __rglgen_glMultiDrawArraysIndirect;
RGLSYMGLMULTIDRAWELEMENTSINDIRECTPROC __rglgen_glMultiDrawElementsIndirect;
RGLSYMGLGETPROGRAMINTERFACEIVPROC __rglgen_glGetProgramInterfaceiv;
RGLSYMGLGETPROGRAMRESOURCEINDEXPROC __rglgen_glGetProgramResourceIndex;
RGLSYMGLGETPROGRAMRESOURCENAMEPROC __rglgen_glGetProgramResourceName;
RGLSYMGLGETPROGRAMRESOURCEIVPROC __rglgen_glGetProgramResourceiv;
RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC __rglgen_glGetProgramResourceLocation;
RGLSYMGLGETPROGRAMRESOURCELOCATIONINDEXPROC __rglgen_glGetProgramResourceLocationIndex;
RGLSYMGLSHADERSTORAGEBLOCKBINDINGPROC __rglgen_glShaderStorageBlockBinding;
RGLSYMGLTEXBUFFERRANGEPROC __rglgen_glTexBufferRange;
RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC __rglgen_glTexStorage2DMultisample;
RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC __rglgen_glTexStorage3DMultisample;
RGLSYMGLTEXTUREVIEWPROC __rglgen_glTextureView;
RGLSYMGLBINDVERTEXBUFFERPROC __rglgen_glBindVertexBuffer;
RGLSYMGLVERTEXATTRIBFORMATPROC __rglgen_glVertexAttribFormat;
RGLSYMGLVERTEXATTRIBIFORMATPROC __rglgen_glVertexAttribIFormat;
RGLSYMGLVERTEXATTRIBLFORMATPROC __rglgen_glVertexAttribLFormat;
RGLSYMGLVERTEXATTRIBBINDINGPROC __rglgen_glVertexAttribBinding;
RGLSYMGLVERTEXBINDINGDIVISORPROC __rglgen_glVertexBindingDivisor;
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
RGLSYMGLBUFFERSTORAGEPROC __rglgen_glBufferStorage;
RGLSYMGLCLEARTEXIMAGEPROC __rglgen_glClearTexImage;
RGLSYMGLCLEARTEXSUBIMAGEPROC __rglgen_glClearTexSubImage;
RGLSYMGLBINDBUFFERSBASEPROC __rglgen_glBindBuffersBase;
RGLSYMGLBINDBUFFERSRANGEPROC __rglgen_glBindBuffersRange;
RGLSYMGLBINDTEXTURESPROC __rglgen_glBindTextures;
RGLSYMGLBINDSAMPLERSPROC __rglgen_glBindSamplers;
RGLSYMGLBINDIMAGETEXTURESPROC __rglgen_glBindImageTextures;
RGLSYMGLBINDVERTEXBUFFERSPROC __rglgen_glBindVertexBuffers;
RGLSYMGLGETTEXTUREHANDLEARBPROC __rglgen_glGetTextureHandleARB;
RGLSYMGLGETTEXTURESAMPLERHANDLEARBPROC __rglgen_glGetTextureSamplerHandleARB;
RGLSYMGLMAKETEXTUREHANDLERESIDENTARBPROC __rglgen_glMakeTextureHandleResidentARB;
RGLSYMGLMAKETEXTUREHANDLENONRESIDENTARBPROC __rglgen_glMakeTextureHandleNonResidentARB;
RGLSYMGLGETIMAGEHANDLEARBPROC __rglgen_glGetImageHandleARB;
RGLSYMGLMAKEIMAGEHANDLERESIDENTARBPROC __rglgen_glMakeImageHandleResidentARB;
RGLSYMGLMAKEIMAGEHANDLENONRESIDENTARBPROC __rglgen_glMakeImageHandleNonResidentARB;
RGLSYMGLUNIFORMHANDLEUI64ARBPROC __rglgen_glUniformHandleui64ARB;
RGLSYMGLUNIFORMHANDLEUI64VARBPROC __rglgen_glUniformHandleui64vARB;
RGLSYMGLPROGRAMUNIFORMHANDLEUI64ARBPROC __rglgen_glProgramUniformHandleui64ARB;
RGLSYMGLPROGRAMUNIFORMHANDLEUI64VARBPROC __rglgen_glProgramUniformHandleui64vARB;
RGLSYMGLISTEXTUREHANDLERESIDENTARBPROC __rglgen_glIsTextureHandleResidentARB;
RGLSYMGLISIMAGEHANDLERESIDENTARBPROC __rglgen_glIsImageHandleResidentARB;
RGLSYMGLVERTEXATTRIBL1UI64ARBPROC __rglgen_glVertexAttribL1ui64ARB;
RGLSYMGLVERTEXATTRIBL1UI64VARBPROC __rglgen_glVertexAttribL1ui64vARB;
RGLSYMGLGETVERTEXATTRIBLUI64VARBPROC __rglgen_glGetVertexAttribLui64vARB;
RGLSYMGLCREATESYNCFROMCLEVENTARBPROC __rglgen_glCreateSyncFromCLeventARB;
RGLSYMGLCLAMPCOLORARBPROC __rglgen_glClampColorARB;
RGLSYMGLDISPATCHCOMPUTEGROUPSIZEARBPROC __rglgen_glDispatchComputeGroupSizeARB;
RGLSYMGLDEBUGMESSAGECONTROLARBPROC __rglgen_glDebugMessageControlARB;
RGLSYMGLDEBUGMESSAGEINSERTARBPROC __rglgen_glDebugMessageInsertARB;
RGLSYMGLDEBUGMESSAGECALLBACKARBPROC __rglgen_glDebugMessageCallbackARB;
RGLSYMGLGETDEBUGMESSAGELOGARBPROC __rglgen_glGetDebugMessageLogARB;
RGLSYMGLDRAWBUFFERSARBPROC __rglgen_glDrawBuffersARB;
RGLSYMGLBLENDEQUATIONIARBPROC __rglgen_glBlendEquationiARB;
RGLSYMGLBLENDEQUATIONSEPARATEIARBPROC __rglgen_glBlendEquationSeparateiARB;
RGLSYMGLBLENDFUNCIARBPROC __rglgen_glBlendFunciARB;
RGLSYMGLBLENDFUNCSEPARATEIARBPROC __rglgen_glBlendFuncSeparateiARB;
RGLSYMGLDRAWARRAYSINSTANCEDARBPROC __rglgen_glDrawArraysInstancedARB;
RGLSYMGLDRAWELEMENTSINSTANCEDARBPROC __rglgen_glDrawElementsInstancedARB;
RGLSYMGLPROGRAMSTRINGARBPROC __rglgen_glProgramStringARB;
RGLSYMGLBINDPROGRAMARBPROC __rglgen_glBindProgramARB;
RGLSYMGLDELETEPROGRAMSARBPROC __rglgen_glDeleteProgramsARB;
RGLSYMGLGENPROGRAMSARBPROC __rglgen_glGenProgramsARB;
RGLSYMGLPROGRAMENVPARAMETER4DARBPROC __rglgen_glProgramEnvParameter4dARB;
RGLSYMGLPROGRAMENVPARAMETER4DVARBPROC __rglgen_glProgramEnvParameter4dvARB;
RGLSYMGLPROGRAMENVPARAMETER4FARBPROC __rglgen_glProgramEnvParameter4fARB;
RGLSYMGLPROGRAMENVPARAMETER4FVARBPROC __rglgen_glProgramEnvParameter4fvARB;
RGLSYMGLPROGRAMLOCALPARAMETER4DARBPROC __rglgen_glProgramLocalParameter4dARB;
RGLSYMGLPROGRAMLOCALPARAMETER4DVARBPROC __rglgen_glProgramLocalParameter4dvARB;
RGLSYMGLPROGRAMLOCALPARAMETER4FARBPROC __rglgen_glProgramLocalParameter4fARB;
RGLSYMGLPROGRAMLOCALPARAMETER4FVARBPROC __rglgen_glProgramLocalParameter4fvARB;
RGLSYMGLGETPROGRAMENVPARAMETERDVARBPROC __rglgen_glGetProgramEnvParameterdvARB;
RGLSYMGLGETPROGRAMENVPARAMETERFVARBPROC __rglgen_glGetProgramEnvParameterfvARB;
RGLSYMGLGETPROGRAMLOCALPARAMETERDVARBPROC __rglgen_glGetProgramLocalParameterdvARB;
RGLSYMGLGETPROGRAMLOCALPARAMETERFVARBPROC __rglgen_glGetProgramLocalParameterfvARB;
RGLSYMGLGETPROGRAMIVARBPROC __rglgen_glGetProgramivARB;
RGLSYMGLGETPROGRAMSTRINGARBPROC __rglgen_glGetProgramStringARB;
RGLSYMGLISPROGRAMARBPROC __rglgen_glIsProgramARB;
RGLSYMGLPROGRAMPARAMETERIARBPROC __rglgen_glProgramParameteriARB;
RGLSYMGLFRAMEBUFFERTEXTUREARBPROC __rglgen_glFramebufferTextureARB;
RGLSYMGLFRAMEBUFFERTEXTURELAYERARBPROC __rglgen_glFramebufferTextureLayerARB;
RGLSYMGLFRAMEBUFFERTEXTUREFACEARBPROC __rglgen_glFramebufferTextureFaceARB;
RGLSYMGLCOLORTABLEPROC __rglgen_glColorTable;
RGLSYMGLCOLORTABLEPARAMETERFVPROC __rglgen_glColorTableParameterfv;
RGLSYMGLCOLORTABLEPARAMETERIVPROC __rglgen_glColorTableParameteriv;
RGLSYMGLCOPYCOLORTABLEPROC __rglgen_glCopyColorTable;
RGLSYMGLGETCOLORTABLEPROC __rglgen_glGetColorTable;
RGLSYMGLGETCOLORTABLEPARAMETERFVPROC __rglgen_glGetColorTableParameterfv;
RGLSYMGLGETCOLORTABLEPARAMETERIVPROC __rglgen_glGetColorTableParameteriv;
RGLSYMGLCOLORSUBTABLEPROC __rglgen_glColorSubTable;
RGLSYMGLCOPYCOLORSUBTABLEPROC __rglgen_glCopyColorSubTable;
RGLSYMGLCONVOLUTIONFILTER1DPROC __rglgen_glConvolutionFilter1D;
RGLSYMGLCONVOLUTIONFILTER2DPROC __rglgen_glConvolutionFilter2D;
RGLSYMGLCONVOLUTIONPARAMETERFPROC __rglgen_glConvolutionParameterf;
RGLSYMGLCONVOLUTIONPARAMETERFVPROC __rglgen_glConvolutionParameterfv;
RGLSYMGLCONVOLUTIONPARAMETERIPROC __rglgen_glConvolutionParameteri;
RGLSYMGLCONVOLUTIONPARAMETERIVPROC __rglgen_glConvolutionParameteriv;
RGLSYMGLCOPYCONVOLUTIONFILTER1DPROC __rglgen_glCopyConvolutionFilter1D;
RGLSYMGLCOPYCONVOLUTIONFILTER2DPROC __rglgen_glCopyConvolutionFilter2D;
RGLSYMGLGETCONVOLUTIONFILTERPROC __rglgen_glGetConvolutionFilter;
RGLSYMGLGETCONVOLUTIONPARAMETERFVPROC __rglgen_glGetConvolutionParameterfv;
RGLSYMGLGETCONVOLUTIONPARAMETERIVPROC __rglgen_glGetConvolutionParameteriv;
RGLSYMGLGETSEPARABLEFILTERPROC __rglgen_glGetSeparableFilter;
RGLSYMGLSEPARABLEFILTER2DPROC __rglgen_glSeparableFilter2D;
RGLSYMGLGETHISTOGRAMPROC __rglgen_glGetHistogram;
RGLSYMGLGETHISTOGRAMPARAMETERFVPROC __rglgen_glGetHistogramParameterfv;
RGLSYMGLGETHISTOGRAMPARAMETERIVPROC __rglgen_glGetHistogramParameteriv;
RGLSYMGLGETMINMAXPROC __rglgen_glGetMinmax;
RGLSYMGLGETMINMAXPARAMETERFVPROC __rglgen_glGetMinmaxParameterfv;
RGLSYMGLGETMINMAXPARAMETERIVPROC __rglgen_glGetMinmaxParameteriv;
RGLSYMGLHISTOGRAMPROC __rglgen_glHistogram;
RGLSYMGLMINMAXPROC __rglgen_glMinmax;
RGLSYMGLRESETHISTOGRAMPROC __rglgen_glResetHistogram;
RGLSYMGLRESETMINMAXPROC __rglgen_glResetMinmax;
RGLSYMGLMULTIDRAWARRAYSINDIRECTCOUNTARBPROC __rglgen_glMultiDrawArraysIndirectCountARB;
RGLSYMGLMULTIDRAWELEMENTSINDIRECTCOUNTARBPROC __rglgen_glMultiDrawElementsIndirectCountARB;
RGLSYMGLVERTEXATTRIBDIVISORARBPROC __rglgen_glVertexAttribDivisorARB;
RGLSYMGLCURRENTPALETTEMATRIXARBPROC __rglgen_glCurrentPaletteMatrixARB;
RGLSYMGLMATRIXINDEXUBVARBPROC __rglgen_glMatrixIndexubvARB;
RGLSYMGLMATRIXINDEXUSVARBPROC __rglgen_glMatrixIndexusvARB;
RGLSYMGLMATRIXINDEXUIVARBPROC __rglgen_glMatrixIndexuivARB;
RGLSYMGLMATRIXINDEXPOINTERARBPROC __rglgen_glMatrixIndexPointerARB;
RGLSYMGLSAMPLECOVERAGEARBPROC __rglgen_glSampleCoverageARB;
RGLSYMGLACTIVETEXTUREARBPROC __rglgen_glActiveTextureARB;
RGLSYMGLCLIENTACTIVETEXTUREARBPROC __rglgen_glClientActiveTextureARB;
RGLSYMGLMULTITEXCOORD1DARBPROC __rglgen_glMultiTexCoord1dARB;
RGLSYMGLMULTITEXCOORD1DVARBPROC __rglgen_glMultiTexCoord1dvARB;
RGLSYMGLMULTITEXCOORD1FARBPROC __rglgen_glMultiTexCoord1fARB;
RGLSYMGLMULTITEXCOORD1FVARBPROC __rglgen_glMultiTexCoord1fvARB;
RGLSYMGLMULTITEXCOORD1IARBPROC __rglgen_glMultiTexCoord1iARB;
RGLSYMGLMULTITEXCOORD1IVARBPROC __rglgen_glMultiTexCoord1ivARB;
RGLSYMGLMULTITEXCOORD1SARBPROC __rglgen_glMultiTexCoord1sARB;
RGLSYMGLMULTITEXCOORD1SVARBPROC __rglgen_glMultiTexCoord1svARB;
RGLSYMGLMULTITEXCOORD2DARBPROC __rglgen_glMultiTexCoord2dARB;
RGLSYMGLMULTITEXCOORD2DVARBPROC __rglgen_glMultiTexCoord2dvARB;
RGLSYMGLMULTITEXCOORD2FARBPROC __rglgen_glMultiTexCoord2fARB;
RGLSYMGLMULTITEXCOORD2FVARBPROC __rglgen_glMultiTexCoord2fvARB;
RGLSYMGLMULTITEXCOORD2IARBPROC __rglgen_glMultiTexCoord2iARB;
RGLSYMGLMULTITEXCOORD2IVARBPROC __rglgen_glMultiTexCoord2ivARB;
RGLSYMGLMULTITEXCOORD2SARBPROC __rglgen_glMultiTexCoord2sARB;
RGLSYMGLMULTITEXCOORD2SVARBPROC __rglgen_glMultiTexCoord2svARB;
RGLSYMGLMULTITEXCOORD3DARBPROC __rglgen_glMultiTexCoord3dARB;
RGLSYMGLMULTITEXCOORD3DVARBPROC __rglgen_glMultiTexCoord3dvARB;
RGLSYMGLMULTITEXCOORD3FARBPROC __rglgen_glMultiTexCoord3fARB;
RGLSYMGLMULTITEXCOORD3FVARBPROC __rglgen_glMultiTexCoord3fvARB;
RGLSYMGLMULTITEXCOORD3IARBPROC __rglgen_glMultiTexCoord3iARB;
RGLSYMGLMULTITEXCOORD3IVARBPROC __rglgen_glMultiTexCoord3ivARB;
RGLSYMGLMULTITEXCOORD3SARBPROC __rglgen_glMultiTexCoord3sARB;
RGLSYMGLMULTITEXCOORD3SVARBPROC __rglgen_glMultiTexCoord3svARB;
RGLSYMGLMULTITEXCOORD4DARBPROC __rglgen_glMultiTexCoord4dARB;
RGLSYMGLMULTITEXCOORD4DVARBPROC __rglgen_glMultiTexCoord4dvARB;
RGLSYMGLMULTITEXCOORD4FARBPROC __rglgen_glMultiTexCoord4fARB;
RGLSYMGLMULTITEXCOORD4FVARBPROC __rglgen_glMultiTexCoord4fvARB;
RGLSYMGLMULTITEXCOORD4IARBPROC __rglgen_glMultiTexCoord4iARB;
RGLSYMGLMULTITEXCOORD4IVARBPROC __rglgen_glMultiTexCoord4ivARB;
RGLSYMGLMULTITEXCOORD4SARBPROC __rglgen_glMultiTexCoord4sARB;
RGLSYMGLMULTITEXCOORD4SVARBPROC __rglgen_glMultiTexCoord4svARB;
RGLSYMGLGENQUERIESARBPROC __rglgen_glGenQueriesARB;
RGLSYMGLDELETEQUERIESARBPROC __rglgen_glDeleteQueriesARB;
RGLSYMGLISQUERYARBPROC __rglgen_glIsQueryARB;
RGLSYMGLBEGINQUERYARBPROC __rglgen_glBeginQueryARB;
RGLSYMGLENDQUERYARBPROC __rglgen_glEndQueryARB;
RGLSYMGLGETQUERYIVARBPROC __rglgen_glGetQueryivARB;
RGLSYMGLGETQUERYOBJECTIVARBPROC __rglgen_glGetQueryObjectivARB;
RGLSYMGLGETQUERYOBJECTUIVARBPROC __rglgen_glGetQueryObjectuivARB;
RGLSYMGLPOINTPARAMETERFARBPROC __rglgen_glPointParameterfARB;
RGLSYMGLPOINTPARAMETERFVARBPROC __rglgen_glPointParameterfvARB;
RGLSYMGLGETGRAPHICSRESETSTATUSARBPROC __rglgen_glGetGraphicsResetStatusARB;
RGLSYMGLGETNTEXIMAGEARBPROC __rglgen_glGetnTexImageARB;
RGLSYMGLREADNPIXELSARBPROC __rglgen_glReadnPixelsARB;
RGLSYMGLGETNCOMPRESSEDTEXIMAGEARBPROC __rglgen_glGetnCompressedTexImageARB;
RGLSYMGLGETNUNIFORMFVARBPROC __rglgen_glGetnUniformfvARB;
RGLSYMGLGETNUNIFORMIVARBPROC __rglgen_glGetnUniformivARB;
RGLSYMGLGETNUNIFORMUIVARBPROC __rglgen_glGetnUniformuivARB;
RGLSYMGLGETNUNIFORMDVARBPROC __rglgen_glGetnUniformdvARB;
RGLSYMGLGETNMAPDVARBPROC __rglgen_glGetnMapdvARB;
RGLSYMGLGETNMAPFVARBPROC __rglgen_glGetnMapfvARB;
RGLSYMGLGETNMAPIVARBPROC __rglgen_glGetnMapivARB;
RGLSYMGLGETNPIXELMAPFVARBPROC __rglgen_glGetnPixelMapfvARB;
RGLSYMGLGETNPIXELMAPUIVARBPROC __rglgen_glGetnPixelMapuivARB;
RGLSYMGLGETNPIXELMAPUSVARBPROC __rglgen_glGetnPixelMapusvARB;
RGLSYMGLGETNPOLYGONSTIPPLEARBPROC __rglgen_glGetnPolygonStippleARB;
RGLSYMGLGETNCOLORTABLEARBPROC __rglgen_glGetnColorTableARB;
RGLSYMGLGETNCONVOLUTIONFILTERARBPROC __rglgen_glGetnConvolutionFilterARB;
RGLSYMGLGETNSEPARABLEFILTERARBPROC __rglgen_glGetnSeparableFilterARB;
RGLSYMGLGETNHISTOGRAMARBPROC __rglgen_glGetnHistogramARB;
RGLSYMGLGETNMINMAXARBPROC __rglgen_glGetnMinmaxARB;
RGLSYMGLMINSAMPLESHADINGARBPROC __rglgen_glMinSampleShadingARB;
RGLSYMGLDELETEOBJECTARBPROC __rglgen_glDeleteObjectARB;
RGLSYMGLGETHANDLEARBPROC __rglgen_glGetHandleARB;
RGLSYMGLDETACHOBJECTARBPROC __rglgen_glDetachObjectARB;
RGLSYMGLCREATESHADEROBJECTARBPROC __rglgen_glCreateShaderObjectARB;
RGLSYMGLSHADERSOURCEARBPROC __rglgen_glShaderSourceARB;
RGLSYMGLCOMPILESHADERARBPROC __rglgen_glCompileShaderARB;
RGLSYMGLCREATEPROGRAMOBJECTARBPROC __rglgen_glCreateProgramObjectARB;
RGLSYMGLATTACHOBJECTARBPROC __rglgen_glAttachObjectARB;
RGLSYMGLLINKPROGRAMARBPROC __rglgen_glLinkProgramARB;
RGLSYMGLUSEPROGRAMOBJECTARBPROC __rglgen_glUseProgramObjectARB;
RGLSYMGLVALIDATEPROGRAMARBPROC __rglgen_glValidateProgramARB;
RGLSYMGLUNIFORM1FARBPROC __rglgen_glUniform1fARB;
RGLSYMGLUNIFORM2FARBPROC __rglgen_glUniform2fARB;
RGLSYMGLUNIFORM3FARBPROC __rglgen_glUniform3fARB;
RGLSYMGLUNIFORM4FARBPROC __rglgen_glUniform4fARB;
RGLSYMGLUNIFORM1IARBPROC __rglgen_glUniform1iARB;
RGLSYMGLUNIFORM2IARBPROC __rglgen_glUniform2iARB;
RGLSYMGLUNIFORM3IARBPROC __rglgen_glUniform3iARB;
RGLSYMGLUNIFORM4IARBPROC __rglgen_glUniform4iARB;
RGLSYMGLUNIFORM1FVARBPROC __rglgen_glUniform1fvARB;
RGLSYMGLUNIFORM2FVARBPROC __rglgen_glUniform2fvARB;
RGLSYMGLUNIFORM3FVARBPROC __rglgen_glUniform3fvARB;
RGLSYMGLUNIFORM4FVARBPROC __rglgen_glUniform4fvARB;
RGLSYMGLUNIFORM1IVARBPROC __rglgen_glUniform1ivARB;
RGLSYMGLUNIFORM2IVARBPROC __rglgen_glUniform2ivARB;
RGLSYMGLUNIFORM3IVARBPROC __rglgen_glUniform3ivARB;
RGLSYMGLUNIFORM4IVARBPROC __rglgen_glUniform4ivARB;
RGLSYMGLUNIFORMMATRIX2FVARBPROC __rglgen_glUniformMatrix2fvARB;
RGLSYMGLUNIFORMMATRIX3FVARBPROC __rglgen_glUniformMatrix3fvARB;
RGLSYMGLUNIFORMMATRIX4FVARBPROC __rglgen_glUniformMatrix4fvARB;
RGLSYMGLGETOBJECTPARAMETERFVARBPROC __rglgen_glGetObjectParameterfvARB;
RGLSYMGLGETOBJECTPARAMETERIVARBPROC __rglgen_glGetObjectParameterivARB;
RGLSYMGLGETINFOLOGARBPROC __rglgen_glGetInfoLogARB;
RGLSYMGLGETATTACHEDOBJECTSARBPROC __rglgen_glGetAttachedObjectsARB;
RGLSYMGLGETUNIFORMLOCATIONARBPROC __rglgen_glGetUniformLocationARB;
RGLSYMGLGETACTIVEUNIFORMARBPROC __rglgen_glGetActiveUniformARB;
RGLSYMGLGETUNIFORMFVARBPROC __rglgen_glGetUniformfvARB;
RGLSYMGLGETUNIFORMIVARBPROC __rglgen_glGetUniformivARB;
RGLSYMGLGETSHADERSOURCEARBPROC __rglgen_glGetShaderSourceARB;
RGLSYMGLNAMEDSTRINGARBPROC __rglgen_glNamedStringARB;
RGLSYMGLDELETENAMEDSTRINGARBPROC __rglgen_glDeleteNamedStringARB;
RGLSYMGLCOMPILESHADERINCLUDEARBPROC __rglgen_glCompileShaderIncludeARB;
RGLSYMGLISNAMEDSTRINGARBPROC __rglgen_glIsNamedStringARB;
RGLSYMGLGETNAMEDSTRINGARBPROC __rglgen_glGetNamedStringARB;
RGLSYMGLGETNAMEDSTRINGIVARBPROC __rglgen_glGetNamedStringivARB;
RGLSYMGLTEXPAGECOMMITMENTARBPROC __rglgen_glTexPageCommitmentARB;
RGLSYMGLTEXBUFFERARBPROC __rglgen_glTexBufferARB;
RGLSYMGLCOMPRESSEDTEXIMAGE3DARBPROC __rglgen_glCompressedTexImage3DARB;
RGLSYMGLCOMPRESSEDTEXIMAGE2DARBPROC __rglgen_glCompressedTexImage2DARB;
RGLSYMGLCOMPRESSEDTEXIMAGE1DARBPROC __rglgen_glCompressedTexImage1DARB;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DARBPROC __rglgen_glCompressedTexSubImage3DARB;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DARBPROC __rglgen_glCompressedTexSubImage2DARB;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DARBPROC __rglgen_glCompressedTexSubImage1DARB;
RGLSYMGLGETCOMPRESSEDTEXIMAGEARBPROC __rglgen_glGetCompressedTexImageARB;
RGLSYMGLLOADTRANSPOSEMATRIXFARBPROC __rglgen_glLoadTransposeMatrixfARB;
RGLSYMGLLOADTRANSPOSEMATRIXDARBPROC __rglgen_glLoadTransposeMatrixdARB;
RGLSYMGLMULTTRANSPOSEMATRIXFARBPROC __rglgen_glMultTransposeMatrixfARB;
RGLSYMGLMULTTRANSPOSEMATRIXDARBPROC __rglgen_glMultTransposeMatrixdARB;
RGLSYMGLWEIGHTBVARBPROC __rglgen_glWeightbvARB;
RGLSYMGLWEIGHTSVARBPROC __rglgen_glWeightsvARB;
RGLSYMGLWEIGHTIVARBPROC __rglgen_glWeightivARB;
RGLSYMGLWEIGHTFVARBPROC __rglgen_glWeightfvARB;
RGLSYMGLWEIGHTDVARBPROC __rglgen_glWeightdvARB;
RGLSYMGLWEIGHTUBVARBPROC __rglgen_glWeightubvARB;
RGLSYMGLWEIGHTUSVARBPROC __rglgen_glWeightusvARB;
RGLSYMGLWEIGHTUIVARBPROC __rglgen_glWeightuivARB;
RGLSYMGLWEIGHTPOINTERARBPROC __rglgen_glWeightPointerARB;
RGLSYMGLVERTEXBLENDARBPROC __rglgen_glVertexBlendARB;
RGLSYMGLBINDBUFFERARBPROC __rglgen_glBindBufferARB;
RGLSYMGLDELETEBUFFERSARBPROC __rglgen_glDeleteBuffersARB;
RGLSYMGLGENBUFFERSARBPROC __rglgen_glGenBuffersARB;
RGLSYMGLISBUFFERARBPROC __rglgen_glIsBufferARB;
RGLSYMGLBUFFERDATAARBPROC __rglgen_glBufferDataARB;
RGLSYMGLBUFFERSUBDATAARBPROC __rglgen_glBufferSubDataARB;
RGLSYMGLGETBUFFERSUBDATAARBPROC __rglgen_glGetBufferSubDataARB;
RGLSYMGLMAPBUFFERARBPROC __rglgen_glMapBufferARB;
RGLSYMGLUNMAPBUFFERARBPROC __rglgen_glUnmapBufferARB;
RGLSYMGLGETBUFFERPARAMETERIVARBPROC __rglgen_glGetBufferParameterivARB;
RGLSYMGLGETBUFFERPOINTERVARBPROC __rglgen_glGetBufferPointervARB;
RGLSYMGLVERTEXATTRIB1DARBPROC __rglgen_glVertexAttrib1dARB;
RGLSYMGLVERTEXATTRIB1DVARBPROC __rglgen_glVertexAttrib1dvARB;
RGLSYMGLVERTEXATTRIB1FARBPROC __rglgen_glVertexAttrib1fARB;
RGLSYMGLVERTEXATTRIB1FVARBPROC __rglgen_glVertexAttrib1fvARB;
RGLSYMGLVERTEXATTRIB1SARBPROC __rglgen_glVertexAttrib1sARB;
RGLSYMGLVERTEXATTRIB1SVARBPROC __rglgen_glVertexAttrib1svARB;
RGLSYMGLVERTEXATTRIB2DARBPROC __rglgen_glVertexAttrib2dARB;
RGLSYMGLVERTEXATTRIB2DVARBPROC __rglgen_glVertexAttrib2dvARB;
RGLSYMGLVERTEXATTRIB2FARBPROC __rglgen_glVertexAttrib2fARB;
RGLSYMGLVERTEXATTRIB2FVARBPROC __rglgen_glVertexAttrib2fvARB;
RGLSYMGLVERTEXATTRIB2SARBPROC __rglgen_glVertexAttrib2sARB;
RGLSYMGLVERTEXATTRIB2SVARBPROC __rglgen_glVertexAttrib2svARB;
RGLSYMGLVERTEXATTRIB3DARBPROC __rglgen_glVertexAttrib3dARB;
RGLSYMGLVERTEXATTRIB3DVARBPROC __rglgen_glVertexAttrib3dvARB;
RGLSYMGLVERTEXATTRIB3FARBPROC __rglgen_glVertexAttrib3fARB;
RGLSYMGLVERTEXATTRIB3FVARBPROC __rglgen_glVertexAttrib3fvARB;
RGLSYMGLVERTEXATTRIB3SARBPROC __rglgen_glVertexAttrib3sARB;
RGLSYMGLVERTEXATTRIB3SVARBPROC __rglgen_glVertexAttrib3svARB;
RGLSYMGLVERTEXATTRIB4NBVARBPROC __rglgen_glVertexAttrib4NbvARB;
RGLSYMGLVERTEXATTRIB4NIVARBPROC __rglgen_glVertexAttrib4NivARB;
RGLSYMGLVERTEXATTRIB4NSVARBPROC __rglgen_glVertexAttrib4NsvARB;
RGLSYMGLVERTEXATTRIB4NUBARBPROC __rglgen_glVertexAttrib4NubARB;
RGLSYMGLVERTEXATTRIB4NUBVARBPROC __rglgen_glVertexAttrib4NubvARB;
RGLSYMGLVERTEXATTRIB4NUIVARBPROC __rglgen_glVertexAttrib4NuivARB;
RGLSYMGLVERTEXATTRIB4NUSVARBPROC __rglgen_glVertexAttrib4NusvARB;
RGLSYMGLVERTEXATTRIB4BVARBPROC __rglgen_glVertexAttrib4bvARB;
RGLSYMGLVERTEXATTRIB4DARBPROC __rglgen_glVertexAttrib4dARB;
RGLSYMGLVERTEXATTRIB4DVARBPROC __rglgen_glVertexAttrib4dvARB;
RGLSYMGLVERTEXATTRIB4FARBPROC __rglgen_glVertexAttrib4fARB;
RGLSYMGLVERTEXATTRIB4FVARBPROC __rglgen_glVertexAttrib4fvARB;
RGLSYMGLVERTEXATTRIB4IVARBPROC __rglgen_glVertexAttrib4ivARB;
RGLSYMGLVERTEXATTRIB4SARBPROC __rglgen_glVertexAttrib4sARB;
RGLSYMGLVERTEXATTRIB4SVARBPROC __rglgen_glVertexAttrib4svARB;
RGLSYMGLVERTEXATTRIB4UBVARBPROC __rglgen_glVertexAttrib4ubvARB;
RGLSYMGLVERTEXATTRIB4UIVARBPROC __rglgen_glVertexAttrib4uivARB;
RGLSYMGLVERTEXATTRIB4USVARBPROC __rglgen_glVertexAttrib4usvARB;
RGLSYMGLVERTEXATTRIBPOINTERARBPROC __rglgen_glVertexAttribPointerARB;
RGLSYMGLENABLEVERTEXATTRIBARRAYARBPROC __rglgen_glEnableVertexAttribArrayARB;
RGLSYMGLDISABLEVERTEXATTRIBARRAYARBPROC __rglgen_glDisableVertexAttribArrayARB;
RGLSYMGLGETVERTEXATTRIBDVARBPROC __rglgen_glGetVertexAttribdvARB;
RGLSYMGLGETVERTEXATTRIBFVARBPROC __rglgen_glGetVertexAttribfvARB;
RGLSYMGLGETVERTEXATTRIBIVARBPROC __rglgen_glGetVertexAttribivARB;
RGLSYMGLGETVERTEXATTRIBPOINTERVARBPROC __rglgen_glGetVertexAttribPointervARB;
RGLSYMGLBINDATTRIBLOCATIONARBPROC __rglgen_glBindAttribLocationARB;
RGLSYMGLGETACTIVEATTRIBARBPROC __rglgen_glGetActiveAttribARB;
RGLSYMGLGETATTRIBLOCATIONARBPROC __rglgen_glGetAttribLocationARB;
RGLSYMGLWINDOWPOS2DARBPROC __rglgen_glWindowPos2dARB;
RGLSYMGLWINDOWPOS2DVARBPROC __rglgen_glWindowPos2dvARB;
RGLSYMGLWINDOWPOS2FARBPROC __rglgen_glWindowPos2fARB;
RGLSYMGLWINDOWPOS2FVARBPROC __rglgen_glWindowPos2fvARB;
RGLSYMGLWINDOWPOS2IARBPROC __rglgen_glWindowPos2iARB;
RGLSYMGLWINDOWPOS2IVARBPROC __rglgen_glWindowPos2ivARB;
RGLSYMGLWINDOWPOS2SARBPROC __rglgen_glWindowPos2sARB;
RGLSYMGLWINDOWPOS2SVARBPROC __rglgen_glWindowPos2svARB;
RGLSYMGLWINDOWPOS3DARBPROC __rglgen_glWindowPos3dARB;
RGLSYMGLWINDOWPOS3DVARBPROC __rglgen_glWindowPos3dvARB;
RGLSYMGLWINDOWPOS3FARBPROC __rglgen_glWindowPos3fARB;
RGLSYMGLWINDOWPOS3FVARBPROC __rglgen_glWindowPos3fvARB;
RGLSYMGLWINDOWPOS3IARBPROC __rglgen_glWindowPos3iARB;
RGLSYMGLWINDOWPOS3IVARBPROC __rglgen_glWindowPos3ivARB;
RGLSYMGLWINDOWPOS3SARBPROC __rglgen_glWindowPos3sARB;
RGLSYMGLWINDOWPOS3SVARBPROC __rglgen_glWindowPos3svARB;
RGLSYMGLMULTITEXCOORD1BOESPROC __rglgen_glMultiTexCoord1bOES;
RGLSYMGLMULTITEXCOORD1BVOESPROC __rglgen_glMultiTexCoord1bvOES;
RGLSYMGLMULTITEXCOORD2BOESPROC __rglgen_glMultiTexCoord2bOES;
RGLSYMGLMULTITEXCOORD2BVOESPROC __rglgen_glMultiTexCoord2bvOES;
RGLSYMGLMULTITEXCOORD3BOESPROC __rglgen_glMultiTexCoord3bOES;
RGLSYMGLMULTITEXCOORD3BVOESPROC __rglgen_glMultiTexCoord3bvOES;
RGLSYMGLMULTITEXCOORD4BOESPROC __rglgen_glMultiTexCoord4bOES;
RGLSYMGLMULTITEXCOORD4BVOESPROC __rglgen_glMultiTexCoord4bvOES;
RGLSYMGLTEXCOORD1BOESPROC __rglgen_glTexCoord1bOES;
RGLSYMGLTEXCOORD1BVOESPROC __rglgen_glTexCoord1bvOES;
RGLSYMGLTEXCOORD2BOESPROC __rglgen_glTexCoord2bOES;
RGLSYMGLTEXCOORD2BVOESPROC __rglgen_glTexCoord2bvOES;
RGLSYMGLTEXCOORD3BOESPROC __rglgen_glTexCoord3bOES;
RGLSYMGLTEXCOORD3BVOESPROC __rglgen_glTexCoord3bvOES;
RGLSYMGLTEXCOORD4BOESPROC __rglgen_glTexCoord4bOES;
RGLSYMGLTEXCOORD4BVOESPROC __rglgen_glTexCoord4bvOES;
RGLSYMGLVERTEX2BOESPROC __rglgen_glVertex2bOES;
RGLSYMGLVERTEX2BVOESPROC __rglgen_glVertex2bvOES;
RGLSYMGLVERTEX3BOESPROC __rglgen_glVertex3bOES;
RGLSYMGLVERTEX3BVOESPROC __rglgen_glVertex3bvOES;
RGLSYMGLVERTEX4BOESPROC __rglgen_glVertex4bOES;
RGLSYMGLVERTEX4BVOESPROC __rglgen_glVertex4bvOES;
RGLSYMGLALPHAFUNCXOESPROC __rglgen_glAlphaFuncxOES;
RGLSYMGLCLEARCOLORXOESPROC __rglgen_glClearColorxOES;
RGLSYMGLCLEARDEPTHXOESPROC __rglgen_glClearDepthxOES;
RGLSYMGLCLIPPLANEXOESPROC __rglgen_glClipPlanexOES;
RGLSYMGLCOLOR4XOESPROC __rglgen_glColor4xOES;
RGLSYMGLDEPTHRANGEXOESPROC __rglgen_glDepthRangexOES;
RGLSYMGLFOGXOESPROC __rglgen_glFogxOES;
RGLSYMGLFOGXVOESPROC __rglgen_glFogxvOES;
RGLSYMGLFRUSTUMXOESPROC __rglgen_glFrustumxOES;
RGLSYMGLGETCLIPPLANEXOESPROC __rglgen_glGetClipPlanexOES;
RGLSYMGLGETFIXEDVOESPROC __rglgen_glGetFixedvOES;
RGLSYMGLGETTEXENVXVOESPROC __rglgen_glGetTexEnvxvOES;
RGLSYMGLGETTEXPARAMETERXVOESPROC __rglgen_glGetTexParameterxvOES;
RGLSYMGLLIGHTMODELXOESPROC __rglgen_glLightModelxOES;
RGLSYMGLLIGHTMODELXVOESPROC __rglgen_glLightModelxvOES;
RGLSYMGLLIGHTXOESPROC __rglgen_glLightxOES;
RGLSYMGLLIGHTXVOESPROC __rglgen_glLightxvOES;
RGLSYMGLLINEWIDTHXOESPROC __rglgen_glLineWidthxOES;
RGLSYMGLLOADMATRIXXOESPROC __rglgen_glLoadMatrixxOES;
RGLSYMGLMATERIALXOESPROC __rglgen_glMaterialxOES;
RGLSYMGLMATERIALXVOESPROC __rglgen_glMaterialxvOES;
RGLSYMGLMULTMATRIXXOESPROC __rglgen_glMultMatrixxOES;
RGLSYMGLMULTITEXCOORD4XOESPROC __rglgen_glMultiTexCoord4xOES;
RGLSYMGLNORMAL3XOESPROC __rglgen_glNormal3xOES;
RGLSYMGLORTHOXOESPROC __rglgen_glOrthoxOES;
RGLSYMGLPOINTPARAMETERXVOESPROC __rglgen_glPointParameterxvOES;
RGLSYMGLPOINTSIZEXOESPROC __rglgen_glPointSizexOES;
RGLSYMGLPOLYGONOFFSETXOESPROC __rglgen_glPolygonOffsetxOES;
RGLSYMGLROTATEXOESPROC __rglgen_glRotatexOES;
RGLSYMGLSAMPLECOVERAGEOESPROC __rglgen_glSampleCoverageOES;
RGLSYMGLSCALEXOESPROC __rglgen_glScalexOES;
RGLSYMGLTEXENVXOESPROC __rglgen_glTexEnvxOES;
RGLSYMGLTEXENVXVOESPROC __rglgen_glTexEnvxvOES;
RGLSYMGLTEXPARAMETERXOESPROC __rglgen_glTexParameterxOES;
RGLSYMGLTEXPARAMETERXVOESPROC __rglgen_glTexParameterxvOES;
RGLSYMGLTRANSLATEXOESPROC __rglgen_glTranslatexOES;
RGLSYMGLACCUMXOESPROC __rglgen_glAccumxOES;
RGLSYMGLBITMAPXOESPROC __rglgen_glBitmapxOES;
RGLSYMGLBLENDCOLORXOESPROC __rglgen_glBlendColorxOES;
RGLSYMGLCLEARACCUMXOESPROC __rglgen_glClearAccumxOES;
RGLSYMGLCOLOR3XOESPROC __rglgen_glColor3xOES;
RGLSYMGLCOLOR3XVOESPROC __rglgen_glColor3xvOES;
RGLSYMGLCOLOR4XVOESPROC __rglgen_glColor4xvOES;
RGLSYMGLCONVOLUTIONPARAMETERXOESPROC __rglgen_glConvolutionParameterxOES;
RGLSYMGLCONVOLUTIONPARAMETERXVOESPROC __rglgen_glConvolutionParameterxvOES;
RGLSYMGLEVALCOORD1XOESPROC __rglgen_glEvalCoord1xOES;
RGLSYMGLEVALCOORD1XVOESPROC __rglgen_glEvalCoord1xvOES;
RGLSYMGLEVALCOORD2XOESPROC __rglgen_glEvalCoord2xOES;
RGLSYMGLEVALCOORD2XVOESPROC __rglgen_glEvalCoord2xvOES;
RGLSYMGLFEEDBACKBUFFERXOESPROC __rglgen_glFeedbackBufferxOES;
RGLSYMGLGETCONVOLUTIONPARAMETERXVOESPROC __rglgen_glGetConvolutionParameterxvOES;
RGLSYMGLGETHISTOGRAMPARAMETERXVOESPROC __rglgen_glGetHistogramParameterxvOES;
RGLSYMGLGETLIGHTXOESPROC __rglgen_glGetLightxOES;
RGLSYMGLGETMAPXVOESPROC __rglgen_glGetMapxvOES;
RGLSYMGLGETMATERIALXOESPROC __rglgen_glGetMaterialxOES;
RGLSYMGLGETPIXELMAPXVPROC __rglgen_glGetPixelMapxv;
RGLSYMGLGETTEXGENXVOESPROC __rglgen_glGetTexGenxvOES;
RGLSYMGLGETTEXLEVELPARAMETERXVOESPROC __rglgen_glGetTexLevelParameterxvOES;
RGLSYMGLINDEXXOESPROC __rglgen_glIndexxOES;
RGLSYMGLINDEXXVOESPROC __rglgen_glIndexxvOES;
RGLSYMGLLOADTRANSPOSEMATRIXXOESPROC __rglgen_glLoadTransposeMatrixxOES;
RGLSYMGLMAP1XOESPROC __rglgen_glMap1xOES;
RGLSYMGLMAP2XOESPROC __rglgen_glMap2xOES;
RGLSYMGLMAPGRID1XOESPROC __rglgen_glMapGrid1xOES;
RGLSYMGLMAPGRID2XOESPROC __rglgen_glMapGrid2xOES;
RGLSYMGLMULTTRANSPOSEMATRIXXOESPROC __rglgen_glMultTransposeMatrixxOES;
RGLSYMGLMULTITEXCOORD1XOESPROC __rglgen_glMultiTexCoord1xOES;
RGLSYMGLMULTITEXCOORD1XVOESPROC __rglgen_glMultiTexCoord1xvOES;
RGLSYMGLMULTITEXCOORD2XOESPROC __rglgen_glMultiTexCoord2xOES;
RGLSYMGLMULTITEXCOORD2XVOESPROC __rglgen_glMultiTexCoord2xvOES;
RGLSYMGLMULTITEXCOORD3XOESPROC __rglgen_glMultiTexCoord3xOES;
RGLSYMGLMULTITEXCOORD3XVOESPROC __rglgen_glMultiTexCoord3xvOES;
RGLSYMGLMULTITEXCOORD4XVOESPROC __rglgen_glMultiTexCoord4xvOES;
RGLSYMGLNORMAL3XVOESPROC __rglgen_glNormal3xvOES;
RGLSYMGLPASSTHROUGHXOESPROC __rglgen_glPassThroughxOES;
RGLSYMGLPIXELMAPXPROC __rglgen_glPixelMapx;
RGLSYMGLPIXELSTOREXPROC __rglgen_glPixelStorex;
RGLSYMGLPIXELTRANSFERXOESPROC __rglgen_glPixelTransferxOES;
RGLSYMGLPIXELZOOMXOESPROC __rglgen_glPixelZoomxOES;
RGLSYMGLPRIORITIZETEXTURESXOESPROC __rglgen_glPrioritizeTexturesxOES;
RGLSYMGLRASTERPOS2XOESPROC __rglgen_glRasterPos2xOES;
RGLSYMGLRASTERPOS2XVOESPROC __rglgen_glRasterPos2xvOES;
RGLSYMGLRASTERPOS3XOESPROC __rglgen_glRasterPos3xOES;
RGLSYMGLRASTERPOS3XVOESPROC __rglgen_glRasterPos3xvOES;
RGLSYMGLRASTERPOS4XOESPROC __rglgen_glRasterPos4xOES;
RGLSYMGLRASTERPOS4XVOESPROC __rglgen_glRasterPos4xvOES;
RGLSYMGLRECTXOESPROC __rglgen_glRectxOES;
RGLSYMGLRECTXVOESPROC __rglgen_glRectxvOES;
RGLSYMGLTEXCOORD1XOESPROC __rglgen_glTexCoord1xOES;
RGLSYMGLTEXCOORD1XVOESPROC __rglgen_glTexCoord1xvOES;
RGLSYMGLTEXCOORD2XOESPROC __rglgen_glTexCoord2xOES;
RGLSYMGLTEXCOORD2XVOESPROC __rglgen_glTexCoord2xvOES;
RGLSYMGLTEXCOORD3XOESPROC __rglgen_glTexCoord3xOES;
RGLSYMGLTEXCOORD3XVOESPROC __rglgen_glTexCoord3xvOES;
RGLSYMGLTEXCOORD4XOESPROC __rglgen_glTexCoord4xOES;
RGLSYMGLTEXCOORD4XVOESPROC __rglgen_glTexCoord4xvOES;
RGLSYMGLTEXGENXOESPROC __rglgen_glTexGenxOES;
RGLSYMGLTEXGENXVOESPROC __rglgen_glTexGenxvOES;
RGLSYMGLVERTEX2XOESPROC __rglgen_glVertex2xOES;
RGLSYMGLVERTEX2XVOESPROC __rglgen_glVertex2xvOES;
RGLSYMGLVERTEX3XOESPROC __rglgen_glVertex3xOES;
RGLSYMGLVERTEX3XVOESPROC __rglgen_glVertex3xvOES;
RGLSYMGLVERTEX4XOESPROC __rglgen_glVertex4xOES;
RGLSYMGLVERTEX4XVOESPROC __rglgen_glVertex4xvOES;
RGLSYMGLQUERYMATRIXXOESPROC __rglgen_glQueryMatrixxOES;
RGLSYMGLCLEARDEPTHFOESPROC __rglgen_glClearDepthfOES;
RGLSYMGLCLIPPLANEFOESPROC __rglgen_glClipPlanefOES;
RGLSYMGLDEPTHRANGEFOESPROC __rglgen_glDepthRangefOES;
RGLSYMGLFRUSTUMFOESPROC __rglgen_glFrustumfOES;
RGLSYMGLGETCLIPPLANEFOESPROC __rglgen_glGetClipPlanefOES;
RGLSYMGLORTHOFOESPROC __rglgen_glOrthofOES;
RGLSYMGLIMAGETRANSFORMPARAMETERIHPPROC __rglgen_glImageTransformParameteriHP;
RGLSYMGLIMAGETRANSFORMPARAMETERFHPPROC __rglgen_glImageTransformParameterfHP;
RGLSYMGLIMAGETRANSFORMPARAMETERIVHPPROC __rglgen_glImageTransformParameterivHP;
RGLSYMGLIMAGETRANSFORMPARAMETERFVHPPROC __rglgen_glImageTransformParameterfvHP;
RGLSYMGLGETIMAGETRANSFORMPARAMETERIVHPPROC __rglgen_glGetImageTransformParameterivHP;
RGLSYMGLGETIMAGETRANSFORMPARAMETERFVHPPROC __rglgen_glGetImageTransformParameterfvHP;
