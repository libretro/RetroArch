#ifndef _D3D_CG
#define _D3D_CG

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

bool renderchain_compile_shaders(void *data, CGprogram &fPrg, CGprogram &vPrg, const std::string &shader)
{
   renderchain_t *chain = (renderchain_t*)data;
   CGprofile vertex_profile = cgD3D9GetLatestVertexProfile();
   CGprofile fragment_profile = cgD3D9GetLatestPixelProfile();
   RARCH_LOG("[D3D Cg]: Vertex profile: %s\n", cgGetProfileString(vertex_profile));
   RARCH_LOG("[D3D Cg]: Fragment profile: %s\n", cgGetProfileString(fragment_profile));
   const char **fragment_opts = cgD3D9GetOptimalOptions(fragment_profile);
   const char **vertex_opts = cgD3D9GetOptimalOptions(vertex_profile);

   if (shader.length() > 0)
   {
      RARCH_LOG("[D3D Cg]: Compiling shader: %s.\n", shader.c_str());
      fPrg = cgCreateProgramFromFile(chain->cgCtx, CG_SOURCE,
            shader.c_str(), fragment_profile, "main_fragment", fragment_opts);

      if (cgGetLastListing(chain->cgCtx))
         RARCH_ERR("[D3D Cg]: Fragment error:\n%s\n", cgGetLastListing(chain->cgCtx));

      vPrg = cgCreateProgramFromFile(chain->cgCtx, CG_SOURCE,
            shader.c_str(), vertex_profile, "main_vertex", vertex_opts);

      if (cgGetLastListing(chain->cgCtx))
         RARCH_ERR("[D3D Cg]: Vertex error:\n%s\n", cgGetLastListing(chain->cgCtx));
   }
   else
   {
      RARCH_LOG("[D3D Cg]: Compiling stock shader.\n");

      fPrg = cgCreateProgram(chain->cgCtx, CG_SOURCE, stock_program,
            fragment_profile, "main_fragment", fragment_opts);

      if (cgGetLastListing(chain->cgCtx))
         RARCH_ERR("[D3D Cg]: Fragment error:\n%s\n", cgGetLastListing(chain->cgCtx));

      vPrg = cgCreateProgram(chain->cgCtx, CG_SOURCE, stock_program,
            vertex_profile, "main_vertex", vertex_opts);

      if (cgGetLastListing(chain->cgCtx))
         RARCH_ERR("[D3D Cg]: Vertex error:\n%s\n", cgGetLastListing(chain->cgCtx));
   }

   if (!fPrg || !vPrg)
      return false;

   cgD3D9LoadProgram(fPrg, true, 0);
   cgD3D9LoadProgram(vPrg, true, 0);
   return true;
}

void renderchain_set_shaders(void *data, CGprogram &fPrg, CGprogram &vPrg)
{
   cgD3D9BindProgram(fPrg);
   cgD3D9BindProgram(vPrg);
}

void renderchain_destroy_stock_shader(void *data)
{
   renderchain_t *chain = (renderchain_t*)data;
#ifdef HAVE_CG
   if (chain->fStock)
      cgDestroyProgram(chain->fStock);
   if (chain->vStock)
      cgDestroyProgram(chain->vStock);
#endif
}

void renderchain_destroy_shader(void *data, int i)
{
   renderchain_t *chain = (renderchain_t*)data;
#ifdef HAVE_CG
   if (chain->passes[i].fPrg)
      cgDestroyProgram(chain->passes[i].fPrg);
   if (chain->passes[i].vPrg)
      cgDestroyProgram(chain->passes[i].vPrg);
#endif
}

void renderchain_set_shader_mvp(void *data, CGprogram &vPrg, D3DXMATRIX &tmp)
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

void renderchain_set_shader_params(void *data, Pass &pass,
            unsigned video_w, unsigned video_h,
            unsigned tex_w, unsigned tex_h,
            unsigned viewport_w, unsigned viewport_h)
{
   renderchain_t *chain = (renderchain_t*)data;
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

   float frame_cnt = chain->frame_count;
   if (pass.info.pass->frame_count_mod)
      frame_cnt = chain->frame_count % pass.info.pass->frame_count_mod;
   set_cg_param(pass.fPrg, "IN.frame_count", frame_cnt);
   set_cg_param(pass.vPrg, "IN.frame_count", frame_cnt);
}


void renderchain_bind_tracker(void *data, Pass &pass, unsigned pass_index)
{
   renderchain_t *chain = (renderchain_t*)data;
   if (!chain->tracker)
      return;

   if (pass_index == 1)
      chain->uniform_cnt = state_get_uniform(chain->tracker, chain->uniform_info, MAX_VARIABLES, chain->frame_count);

   for (unsigned i = 0; i < chain->uniform_cnt; i++)
   {
      set_cg_param(pass.fPrg, chain->uniform_info[i].id, chain->uniform_info[i].value);
      set_cg_param(pass.vPrg, chain->uniform_info[i].id, chain->uniform_info[i].value);
   }
}

#define DECL_FVF_POSITION(stream) \
   { (WORD)(stream), 0 * sizeof(float), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_POSITION, 0 }
#define DECL_FVF_TEXCOORD(stream, offset, index) \
   { (WORD)(stream), (WORD)(offset * sizeof(float)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_TEXCOORD, (BYTE)(index) }
#define DECL_FVF_COLOR(stream, offset, index) \
   { (WORD)(stream), (WORD)(offset * sizeof(float)), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_COLOR, (BYTE)(index) } \


bool renderchain_init_shader_fvf(void *data, Pass &pass)
{
   renderchain_t *chain = (renderchain_t*)data;
   static const D3DVERTEXELEMENT decl_end = D3DDECL_END();
   static const D3DVERTEXELEMENT position_decl = DECL_FVF_POSITION(0);
   static const D3DVERTEXELEMENT tex_coord0 = DECL_FVF_TEXCOORD(1, 3, 0);
   static const D3DVERTEXELEMENT tex_coord1 = DECL_FVF_TEXCOORD(2, 5, 1);
   static const D3DVERTEXELEMENT color = DECL_FVF_COLOR(3, 7, 0);

   D3DVERTEXELEMENT decl[MAXD3DDECLLENGTH] = {{0}};
   if (cgD3D9GetVertexDeclaration(pass.vPrg, decl) == CG_FALSE)
      return false;

   unsigned count;
   for (count = 0; count < MAXD3DDECLLENGTH; count++)
   {
      if (memcmp(&decl_end, &decl[count], sizeof(decl_end)) == 0)
         break;
   }

   // This is completely insane.
   // We do not have a good and easy way of setting up our
   // attribute streams, so we have to do it ourselves, yay!
   // Stream 0 => POSITION
   // Stream 1 => TEXCOORD0
   // Stream 2 => TEXCOORD1
   // Stream 3 => COLOR // Not really used for anything.
   // Stream {4..N} => Texture coord streams for varying resources which have no semantics.

   std::vector<bool> indices(count);
   bool texcoord0_taken = false;
   bool texcoord1_taken = false;
   bool stream_taken[4] = {false};

   CGparameter param = find_param_from_semantic(pass.vPrg, "POSITION");
   if (!param)
      param = find_param_from_semantic(pass.vPrg, "POSITION0");
   if (param)
   {
      stream_taken[0] = true;
      RARCH_LOG("[FVF]: POSITION semantic found.\n");
      unsigned index = cgGetParameterResourceIndex(param);
      decl[index] = position_decl;
      indices[index] = true;
   }

   param = find_param_from_semantic(pass.vPrg, "TEXCOORD");
   if (!param)
      param = find_param_from_semantic(pass.vPrg, "TEXCOORD0");
   if (param)
   {
      stream_taken[1] = true;
      texcoord0_taken = true;
      RARCH_LOG("[FVF]: TEXCOORD0 semantic found.\n");
      unsigned index = cgGetParameterResourceIndex(param);
      decl[index] = tex_coord0;
      indices[index] = true;
   }

   param = find_param_from_semantic(pass.vPrg, "TEXCOORD1");
   if (param)
   {
      stream_taken[2] = true;
      texcoord1_taken = true;
      RARCH_LOG("[FVF]: TEXCOORD1 semantic found.\n");
      unsigned index = cgGetParameterResourceIndex(param);
      decl[index] = tex_coord1;
      indices[index] = true;
   }

   param = find_param_from_semantic(pass.vPrg, "COLOR");
   if (!param)
      param = find_param_from_semantic(pass.vPrg, "COLOR0");
   if (param)
   {
      stream_taken[3] = true;
      RARCH_LOG("[FVF]: COLOR0 semantic found.\n");
      unsigned index = cgGetParameterResourceIndex(param);
      decl[index] = color;
      indices[index] = true;
   }

   // Stream {0, 1, 2, 3} might be already taken. Find first vacant stream.
   unsigned index;
   for (index = 0; index < 4 && stream_taken[index]; index++);

   // Find first vacant texcoord declaration.
   unsigned tex_index = 0;
   if (texcoord0_taken && texcoord1_taken)
      tex_index = 2;
   else if (texcoord1_taken && !texcoord0_taken)
      tex_index = 0;
   else if (texcoord0_taken && !texcoord1_taken)
      tex_index = 1;

   for (unsigned i = 0; i < count; i++)
   {
      if (indices[i])
         pass.attrib_map.push_back(0);
      else
      {
         pass.attrib_map.push_back(index);
         D3DVERTEXELEMENT elem = DECL_FVF_TEXCOORD(index, 3, tex_index);
         decl[i] = elem;

         // Find next vacant stream.
         index++;
         while (index < 4 && stream_taken[index]) index++;

         // Find next vacant texcoord declaration.
         tex_index++;
         if (tex_index == 1 && texcoord1_taken)
            tex_index++;
      }
   }

   if (FAILED(chain->dev->CreateVertexDeclaration(decl, &pass.vertex_decl)))
      return false;

   return true;
}

void renderchain_bind_orig(void *data, Pass &pass)
{
   renderchain_t *chain = (renderchain_t*)data;
   D3DXVECTOR2 video_size, texture_size;
   video_size.x   = chain->passes[0].last_width;
   video_size.y   = chain->passes[0].last_height;
   texture_size.x = chain->passes[0].info.tex_w;
   texture_size.y = chain->passes[0].info.tex_h;

   set_cg_param(pass.vPrg, "ORIG.video_size", video_size);
   set_cg_param(pass.fPrg, "ORIG.video_size", video_size);
   set_cg_param(pass.vPrg, "ORIG.texture_size", texture_size);
   set_cg_param(pass.fPrg, "ORIG.texture_size", texture_size);

   CGparameter param = cgGetNamedParameter(pass.fPrg, "ORIG.texture");
   if (param)
   {
      unsigned index = cgGetParameterResourceIndex(param);
      chain->dev->SetTexture(index, chain->passes[0].tex);
      chain->dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
            translate_filter(chain->passes[0].info.pass->filter));
      chain->dev->SetSamplerState(index, D3DSAMP_MINFILTER,
            translate_filter(chain->passes[0].info.pass->filter));
      chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      chain->bound_tex.push_back(index);
   }

   param = cgGetNamedParameter(pass.vPrg, "ORIG.tex_coord");
   if (param)
   {
      unsigned index = pass.attrib_map[cgGetParameterResourceIndex(param)];
      chain->dev->SetStreamSource(index, chain->passes[0].vertex_buf, 0, sizeof(Vertex));
      chain->bound_vert.push_back(index);
   }
}

void renderchain_bind_prev(void *data, Pass &pass)
{
   renderchain_t *chain = (renderchain_t*)data;
   static const char *prev_names[] = {
      "PREV",
      "PREV1",
      "PREV2",
      "PREV3",
      "PREV4",
      "PREV5",
      "PREV6",
   };

   char attr_texture[64], attr_input_size[64], attr_tex_size[64], attr_coord[64];
   D3DXVECTOR2 texture_size;

   texture_size.x = chain->passes[0].info.tex_w;
   texture_size.y = chain->passes[0].info.tex_h;

   for (unsigned i = 0; i < TEXTURES - 1; i++)
   {
      snprintf(attr_texture,    sizeof(attr_texture),    "%s.texture",      prev_names[i]);
      snprintf(attr_input_size, sizeof(attr_input_size), "%s.video_size",   prev_names[i]);
      snprintf(attr_tex_size,   sizeof(attr_tex_size),   "%s.texture_size", prev_names[i]);
      snprintf(attr_coord,      sizeof(attr_coord),      "%s.tex_coord",    prev_names[i]);

      D3DXVECTOR2 video_size;
      video_size.x = chain->prev.last_width[(chain->prev.ptr - (i + 1)) & TEXTURESMASK];
      video_size.y = chain->prev.last_height[(chain->prev.ptr - (i + 1)) & TEXTURESMASK];

      set_cg_param(pass.vPrg, attr_input_size, video_size);
      set_cg_param(pass.fPrg, attr_input_size, video_size);
      set_cg_param(pass.vPrg, attr_tex_size, texture_size);
      set_cg_param(pass.fPrg, attr_tex_size, texture_size);

      CGparameter param = cgGetNamedParameter(pass.fPrg, attr_texture);
      if (param)
      {
         unsigned index = cgGetParameterResourceIndex(param);

         LPDIRECT3DTEXTURE tex = chain->prev.tex[(chain->prev.ptr - (i + 1)) & TEXTURESMASK];
         chain->dev->SetTexture(index, tex);
         chain->bound_tex.push_back(index);

         chain->dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
               translate_filter(chain->passes[0].info.pass->filter));
         chain->dev->SetSamplerState(index, D3DSAMP_MINFILTER,
               translate_filter(chain->passes[0].info.pass->filter));
         chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      }

      param = cgGetNamedParameter(pass.vPrg, attr_coord);
      if (param)
      {
         unsigned index = pass.attrib_map[cgGetParameterResourceIndex(param)];
         LPDIRECT3DVERTEXBUFFER vert_buf = chain->prev.vertex_buf[(chain->prev.ptr - (i + 1)) & TEXTURESMASK];
         chain->bound_vert.push_back(index);

         chain->dev->SetStreamSource(index, vert_buf, 0, sizeof(Vertex));
      }
   }
}

void renderchain_bind_luts(void *data, Pass &pass)
{
   renderchain_t *chain = (renderchain_t*)data;
   for (unsigned i = 0; i < chain->luts.size(); i++)
   {
      CGparameter fparam = cgGetNamedParameter(pass.fPrg, chain->luts[i].id.c_str());
      int bound_index = -1;
      if (fparam)
      {
         unsigned index = cgGetParameterResourceIndex(fparam);
         bound_index = index;
         chain->dev->SetTexture(index, chain->luts[i].tex);
         chain->dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
               translate_filter(chain->luts[i].smooth));
         chain->dev->SetSamplerState(index, D3DSAMP_MINFILTER,
               translate_filter(chain->luts[i].smooth));
         chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
         chain->bound_tex.push_back(index);
      }

      CGparameter vparam = cgGetNamedParameter(pass.vPrg, chain->luts[i].id.c_str());
      if (vparam)
      {
         unsigned index = cgGetParameterResourceIndex(vparam);
         if (index != (unsigned)bound_index)
         {
            chain->dev->SetTexture(index, chain->luts[i].tex);
            chain->dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
                  translate_filter(chain->luts[i].smooth));
            chain->dev->SetSamplerState(index, D3DSAMP_MINFILTER,
                  translate_filter(chain->luts[i].smooth));
            chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
            chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
            chain->bound_tex.push_back(index);
         }
      }
   }
}

void renderchain_bind_pass(void *data, Pass &pass, unsigned pass_index)
{
   renderchain_t *chain = (renderchain_t*)data;
   // We only bother binding passes which are two indices behind.
   if (pass_index < 3)
      return;

   for (unsigned i = 1; i < pass_index - 1; i++)
   {
      char pass_base[64];
      snprintf(pass_base, sizeof(pass_base), "PASS%u.", i);

      std::string attr_texture = pass_base;
      attr_texture += "texture";
      std::string attr_video_size = pass_base;
      attr_video_size += "video_size";
      std::string attr_texture_size = pass_base;
      attr_texture_size += "texture_size";
      std::string attr_tex_coord = pass_base;
      attr_tex_coord += "tex_coord";

      D3DXVECTOR2 video_size, texture_size;
      video_size.x   = chain->passes[i].last_width;
      video_size.y   = chain->passes[i].last_height;
      texture_size.x = chain->passes[i].info.tex_w;
      texture_size.y = chain->passes[i].info.tex_h;

      set_cg_param(pass.vPrg, attr_video_size.c_str(), video_size);
      set_cg_param(pass.fPrg, attr_video_size.c_str(), video_size);
      set_cg_param(pass.vPrg, attr_texture_size.c_str(), texture_size);
      set_cg_param(pass.fPrg, attr_texture_size.c_str(), texture_size);

      CGparameter param = cgGetNamedParameter(pass.fPrg, attr_texture.c_str());
      if (param)
      {
         unsigned index = cgGetParameterResourceIndex(param);
         chain->bound_tex.push_back(index);

         chain->dev->SetTexture(index, chain->passes[i].tex);
         chain->dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
               translate_filter(chain->passes[i].info.pass->filter));
         chain->dev->SetSamplerState(index, D3DSAMP_MINFILTER,
               translate_filter(chain->passes[i].info.pass->filter));
         chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         chain->dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      }

      param = cgGetNamedParameter(pass.vPrg, attr_tex_coord.c_str());
      if (param)
      {
         unsigned index = pass.attrib_map[cgGetParameterResourceIndex(param)];
         chain->dev->SetStreamSource(index, chain->passes[i].vertex_buf, 0, sizeof(Vertex));
         chain->bound_vert.push_back(index);
      }
   }
}

bool d3d_init_shader(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->cgCtx = cgCreateContext();
   if (!d3d->cgCtx)
      return false;

   RARCH_LOG("[D3D]: Created shader context.\n");

   HRESULT ret = cgD3D9SetDevice(d3d->dev);
   if (FAILED(ret))
      return false;

   return true;
}

void d3d_deinit_shader(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (!d3d->cgCtx)
      return;

   cgD3D9UnloadAllPrograms();
   cgD3D9SetDevice(NULL);
   cgDestroyContext(d3d->cgCtx);
   d3d->cgCtx = NULL;
}

#endif
