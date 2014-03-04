#ifndef _RENDER_CHAIN_CG
#define _RENDER_CHAIN_CG

static const char *stock_program =
    "void main_vertex"
    "("
    "	float4 position : POSITION,"
    "	float2 texCoord : TEXCOORD0,"
    "  float4 color : COLOR,"
    ""
    "  uniform float4x4 modelViewProj,"
    ""
    "	out float4 oPosition : POSITION,"
    "	out float2 otexCoord : TEXCOORD0,"
    "  out float4 oColor : COLOR"
    ")"
    "{"
    "	oPosition = mul(modelViewProj, position);"
    "	otexCoord = texCoord;"
    "  oColor = color;"
    "}"
    ""
    "float4 main_fragment(in float4 color : COLOR, float2 tex : TEXCOORD0, uniform sampler2D s0 : TEXUNIT0) : COLOR"
    "{"
    "   return color * tex2D(s0, tex);"
    "}";

static inline bool validate_param_name(const char *name)
{
   static const char *illegal[] = {
      "PREV.",
      "PREV1.",
      "PREV2.",
      "PREV3.",
      "PREV4.",
      "PREV5.",
      "PREV6.",
      "ORIG.",
      "IN.",
      "PASS",
   };

   for (unsigned i = 0; i < sizeof(illegal) / sizeof(illegal[0]); i++)
      if (strstr(name, illegal[i]) == name)
         return false;

   return true;
}

static inline CGparameter find_param_from_semantic(CGparameter param, const std::string &sem)
{
   while (param)
   {
      if (cgGetParameterType(param) == CG_STRUCT)
      {
         CGparameter ret = find_param_from_semantic(cgGetFirstStructParameter(param), sem);
         if (ret)
            return ret;
      }
      else
      {
         if (cgGetParameterSemantic(param) &&
               sem == cgGetParameterSemantic(param) &&
               cgGetParameterDirection(param) == CG_IN &&
               cgGetParameterVariability(param) == CG_VARYING &&
               validate_param_name(cgGetParameterName(param)))
            return param;
      }
      param = cgGetNextParameter(param);
   }

   return NULL;
}

static inline CGparameter find_param_from_semantic(CGprogram prog, const std::string &sem)
{
   return find_param_from_semantic(cgGetFirstParameter(prog, CG_PROGRAM), sem);
}

bool RenderChain::compile_shaders(CGprogram &fPrg, CGprogram &vPrg, const std::string &shader)
{
   CGprofile vertex_profile = cgD3D9GetLatestVertexProfile();
   CGprofile fragment_profile = cgD3D9GetLatestPixelProfile();
   RARCH_LOG("[D3D Cg]: Vertex profile: %s\n", cgGetProfileString(vertex_profile));
   RARCH_LOG("[D3D Cg]: Fragment profile: %s\n", cgGetProfileString(fragment_profile));
   const char **fragment_opts = cgD3D9GetOptimalOptions(fragment_profile);
   const char **vertex_opts = cgD3D9GetOptimalOptions(vertex_profile);

   if (shader.length() > 0)
   {
      RARCH_LOG("[D3D Cg]: Compiling shader: %s.\n", shader.c_str());
      fPrg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            shader.c_str(), fragment_profile, "main_fragment", fragment_opts);

      if (cgGetLastListing(cgCtx))
         RARCH_ERR("[D3D Cg]: Fragment error:\n%s\n", cgGetLastListing(cgCtx));

      vPrg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            shader.c_str(), vertex_profile, "main_vertex", vertex_opts);

      if (cgGetLastListing(cgCtx))
         RARCH_ERR("[D3D Cg]: Vertex error:\n%s\n", cgGetLastListing(cgCtx));
   }
   else
   {
      RARCH_LOG("[D3D Cg]: Compiling stock shader.\n");

      fPrg = cgCreateProgram(cgCtx, CG_SOURCE, stock_program,
            fragment_profile, "main_fragment", fragment_opts);

      if (cgGetLastListing(cgCtx))
         RARCH_ERR("[D3D Cg]: Fragment error:\n%s\n", cgGetLastListing(cgCtx));

      vPrg = cgCreateProgram(cgCtx, CG_SOURCE, stock_program,
            vertex_profile, "main_vertex", vertex_opts);

      if (cgGetLastListing(cgCtx))
         RARCH_ERR("[D3D Cg]: Vertex error:\n%s\n", cgGetLastListing(cgCtx));
   }

   if (!fPrg || !vPrg)
      return false;

   cgD3D9LoadProgram(fPrg, true, 0);
   cgD3D9LoadProgram(vPrg, true, 0);
   return true;
}

void RenderChain::set_shaders(CGprogram &fPrg, CGprogram &vPrg)
{
   cgD3D9BindProgram(fPrg);
   cgD3D9BindProgram(vPrg);
}

void RenderChain::destroy_stock_shader(void)
{
   if (fStock)
      cgDestroyProgram(fStock);
   if (vStock)
      cgDestroyProgram(vStock);
}

void RenderChain::destroy_shader(int i)
{
   if (passes[i].fPrg)
      cgDestroyProgram(passes[i].fPrg);
   if (passes[i].vPrg)
      cgDestroyProgram(passes[i].vPrg);
}

void RenderChain::set_shader_mvp(CGprogram &vPrg, D3DXMATRIX &tmp)
{
   CGparameter cgpModelViewProj = cgGetNamedParameter(vPrg, "modelViewProj");
   if (cgpModelViewProj)
      cgD3D9SetUniformMatrix(cgpModelViewProj, &tmp);
}

#define set_cg_param(prog, param, val) do { \
   CGparameter cgp = cgGetNamedParameter(prog, param); \
   if (cgp) \
      cgD3D9SetUniform(cgp, &val); \
} while(0)

void RenderChain::set_shader_params(Pass &pass,
            unsigned video_w, unsigned video_h,
            unsigned tex_w, unsigned tex_h,
            unsigned viewport_w, unsigned viewport_h)
{
   D3DXVECTOR2 video_size, texture_size, output_size;
   video_size.x = video_w;
   video_size.y = video_h;
   texture_size.x = tex_w;
   texture_size.y = tex_h;
   output_size.x = viewport_w;
   output_size.y = viewport_h;

   set_cg_param(pass.vPrg, "IN.video_size", video_size);
   set_cg_param(pass.fPrg, "IN.video_size", video_size);
   set_cg_param(pass.vPrg, "IN.texture_size", texture_size);
   set_cg_param(pass.fPrg, "IN.texture_size", texture_size);
   set_cg_param(pass.vPrg, "IN.output_size", output_size);
   set_cg_param(pass.fPrg, "IN.output_size", output_size);

   float frame_cnt = frame_count;
   if (pass.info.pass->frame_count_mod)
      frame_cnt = frame_count % pass.info.pass->frame_count_mod;
   set_cg_param(pass.fPrg, "IN.frame_count", frame_cnt);
   set_cg_param(pass.vPrg, "IN.frame_count", frame_cnt);
}

#endif
