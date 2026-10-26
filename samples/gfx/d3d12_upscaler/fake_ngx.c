/* A stand-in NGX core library for the test (_nvngx.dll beside it): the
 * Direct3D 12 entry points gfx/common/d3d12_ngx.c resolves, with the
 * SDK's signatures, a typed parameter store, the optimal-settings
 * callback the capability set carries, and a record of every create,
 * evaluate and release for the test to read back through fake_ngx_*().
 * Evaluate copies the top-left of the colour texture into the output
 * on the command list it is given, so the test can see the list is the
 * frame's and the output readable after it. */
#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fake_ngx.h"

#define EXPORT __declspec(dllexport)
#define CONV __cdecl

#define OK   0x1
#define FAIL ((int)0xBAD00000)

typedef struct param_slot
{
   char name[64];
   union { uint64_t ull; double d; float f; unsigned ui; int i; void *p; } v;
   int kind;  /* 1 ull, 2 f, 3 d, 4 ui, 5 i, 6 pointer */
} param_slot_t;

typedef struct param { const void *const *vtable; param_slot_t slot[64]; unsigned n; } param_t;
static const void *init_info;
static const void *const param_vtable[17];
static param_t params[16];
static unsigned params_used;
static param_t caps;
static int inited;
static unsigned features_made;
static int handles[8];
static int handle_feature[8];

static fake_ngx_record_t rec;

static param_slot_t *slot(param_t *p, const char *name, int make)
{
   unsigned i;
   for (i = 0; i < p->n; i++)
      if (!strcmp(p->slot[i].name, name))
         return &p->slot[i];
   if (!make || p->n >= 64 || strlen(name) >= sizeof(p->slot[0].name))
      return NULL;
   strcpy(p->slot[p->n].name, name);
   return &p->slot[p->n++];
}

#define SETTER(suffix, type, field, k) \
   static void CONV NVSDK_NGX_Parameter_Set##suffix(param_t *p, const char *n, type v) \
   { param_slot_t *s = slot(p, n, 1); if (s) { s->v.field = v; s->kind = k; } }
#define GETTER(suffix, type, field, k) \
   static int CONV NVSDK_NGX_Parameter_Get##suffix(param_t *p, const char *n, type *v) \
   { param_slot_t *s = slot(p, n, 0); if (!s) return FAIL; *v = s->v.field; return OK; }

SETTER(ULL, uint64_t, ull, 1)
SETTER(F, float, f, 2)
SETTER(D, double, d, 3)
SETTER(UI, unsigned, ui, 4)
SETTER(I, int, i, 5)
SETTER(D3d11Resource, void *, p, 6)
SETTER(D3d12Resource, void *, p, 6)
SETTER(VoidPointer, void *, p, 6)
GETTER(ULL, uint64_t, ull, 1)
GETTER(F, float, f, 2)
GETTER(D, double, d, 3)
GETTER(UI, unsigned, ui, 4)
GETTER(I, int, i, 5)
GETTER(D3d11Resource, void *, p, 6)
GETTER(D3d12Resource, void *, p, 6)
GETTER(VoidPointer, void *, p, 6)

/* The object's vtable, as the real library's: NVSDK_NGX_Parameter's
 * virtual Set and Get overloads, in MSVC's order (each overload group
 * reversed) on Windows and the Itanium ABI's elsewhere, then Reset. */
static void CONV NVSDK_NGX_Parameter_Reset(param_t *p) { (void)p; }
#define M(f) ((const void*)(size_t)(f))
static const void *const param_vtable[17] = {
#if defined(_WIN32)
   M(NVSDK_NGX_Parameter_SetVoidPointer), M(NVSDK_NGX_Parameter_SetD3d12Resource),
   M(NVSDK_NGX_Parameter_SetD3d11Resource), M(NVSDK_NGX_Parameter_SetI),
   M(NVSDK_NGX_Parameter_SetUI), M(NVSDK_NGX_Parameter_SetD),
   M(NVSDK_NGX_Parameter_SetF), M(NVSDK_NGX_Parameter_SetULL),
   M(NVSDK_NGX_Parameter_GetVoidPointer), M(NVSDK_NGX_Parameter_GetD3d12Resource),
   M(NVSDK_NGX_Parameter_GetD3d11Resource), M(NVSDK_NGX_Parameter_GetI),
   M(NVSDK_NGX_Parameter_GetUI), M(NVSDK_NGX_Parameter_GetD),
   M(NVSDK_NGX_Parameter_GetF), M(NVSDK_NGX_Parameter_GetULL),
#else
   M(NVSDK_NGX_Parameter_SetULL), M(NVSDK_NGX_Parameter_SetF),
   M(NVSDK_NGX_Parameter_SetD), M(NVSDK_NGX_Parameter_SetUI),
   M(NVSDK_NGX_Parameter_SetI), M(NVSDK_NGX_Parameter_SetD3d11Resource),
   M(NVSDK_NGX_Parameter_SetD3d12Resource), M(NVSDK_NGX_Parameter_SetVoidPointer),
   M(NVSDK_NGX_Parameter_GetULL), M(NVSDK_NGX_Parameter_GetF),
   M(NVSDK_NGX_Parameter_GetD), M(NVSDK_NGX_Parameter_GetUI),
   M(NVSDK_NGX_Parameter_GetI), M(NVSDK_NGX_Parameter_GetD3d11Resource),
   M(NVSDK_NGX_Parameter_GetD3d12Resource), M(NVSDK_NGX_Parameter_GetVoidPointer),
#endif
   M(NVSDK_NGX_Parameter_Reset)
};
#undef M

static unsigned get_ui(param_t *p, const char *n)
{
   param_slot_t *s = slot(p, n, 0);
   return s ? s->v.ui : 0;
}
static int get_i(param_t *p, const char *n)
{
   param_slot_t *s = slot(p, n, 0);
   return s ? s->v.i : 0;
}
static float get_f(param_t *p, const char *n)
{
   param_slot_t *s = slot(p, n, 0);
   return s ? s->v.f : 0.0f;
}
static void *get_p(param_t *p, const char *n)
{
   param_slot_t *s = slot(p, n, 0);
   return (s && s->kind == 6) ? s->v.p : NULL;
}

/* Quality: 0 MaxPerf 1/2, 1 Balanced 0.58, 2 MaxQuality 2/3,
 * 3 UltraPerformance 1/3, 5 DLAA 1; dynamic range from 1/2 of optimal
 * to the output
 * (a quality the library has not got, 4 UltraQuality, answers 0) */
static int CONV optimal_cb(param_t *p)
{
   unsigned w = get_ui(p, "Width"), h = get_ui(p, "Height");
   int q = get_i(p, "PerfQualityValue");
   double ratio;
   unsigned rw, rh;
   rec.optimal_calls++;
   switch (q)
   {
      case 0: ratio = 0.5; break;
      case 1: ratio = 0.58; break;
      case 2: ratio = 2.0 / 3.0; break;
      case 3: ratio = 1.0 / 3.0; break;
      case 5: ratio = 1.0; break;
      default: ratio = 0.0; break;
   }
   rw = (unsigned)(w * ratio);
   rh = (unsigned)(h * ratio);
   NVSDK_NGX_Parameter_SetUI(p, "OutWidth", rw);
   NVSDK_NGX_Parameter_SetUI(p, "OutHeight", rh);
   NVSDK_NGX_Parameter_SetUI(p, "DLSS.Get.Dynamic.Max.Render.Width", w);
   NVSDK_NGX_Parameter_SetUI(p, "DLSS.Get.Dynamic.Max.Render.Height", h);
   NVSDK_NGX_Parameter_SetUI(p, "DLSS.Get.Dynamic.Min.Render.Width", rw / 2);
   NVSDK_NGX_Parameter_SetUI(p, "DLSS.Get.Dynamic.Min.Render.Height", rh / 2);
   NVSDK_NGX_Parameter_SetF(p, "Sharpness", 0.0f);
   return OK;
}

EXPORT int CONV NVSDK_NGX_D3D12_Init_ProjectID(const char *project, int engine,
      const char *engine_version, const wchar_t *data_path, void *dev,
      int sdk_version, const void *common_info)
{
   const wchar_t *const *const *paths = (const wchar_t *const *const *)common_info;
   (void)engine_version; (void)data_path;
   if (!project || !dev || engine != 0 || sdk_version != 0x15 || !common_info)
      return FAIL;
   rec.init_calls++;
   rec.init_device   = dev;
   rec.init_path0    = (paths[0] && paths[0][0]) ? paths[0][0][0] : 0;
   init_info         = common_info;
   memset(&caps, 0, sizeof(caps));
   caps.vtable = param_vtable;
   NVSDK_NGX_Parameter_SetI(&caps, "SuperSampling.Available", rec.unavailable ? 0 : 1);
   NVSDK_NGX_Parameter_SetI(&caps, "SuperSampling.NeedsUpdatedDriver", 0);
   if (!rec.nr_missing)
      NVSDK_NGX_Parameter_SetI(&caps, "DLSSNR.Available", 1);
   if (rec.optiscaler)
      NVSDK_NGX_Parameter_SetI(&caps, "OptiScaler", 1);
   {
      void *cb;
      int (CONV *fn)(param_t*) = optimal_cb;
      memcpy(&cb, &fn, sizeof(cb));
      NVSDK_NGX_Parameter_SetVoidPointer(&caps, "DLSSOptimalSettingsCallback", cb);
   }
   inited = 1;
   return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_Shutdown1(void *dev, int *in_use)
{
   const wchar_t *const *const *paths = (const wchar_t *const *const *)init_info;
   (void)dev; *in_use = 0; inited = 0; rec.shutdown_calls++;
   rec.shutdown_path0 = (paths && paths[0] && paths[0][0]) ? paths[0][0][0] : 0;
   return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_GetCapabilityParameters(param_t **out)
{
   if (!inited) return FAIL | 7;
   *out = &caps;
   return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_AllocateParameters(param_t **out)
{
   if (params_used >= 16) return FAIL;
   memset(&params[params_used], 0, sizeof(params[0]));
   params[params_used].vtable = param_vtable;
   *out = &params[params_used++];
   return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_DestroyParameters(param_t *p) { (void)p; rec.params_destroyed++; return OK; }
EXPORT int CONV NVSDK_NGX_D3D12_GetFeatureRequirements(void *adapter, const void *info, void *out)
{
   (void)adapter; (void)info; memset(out, 0, 4 + 4 + 255); return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_CreateFeature(void *cmd, int feature, param_t *p, int **out)
{
   fake_ngx_create_t *c = &rec.create[rec.creates % FAKE_NGX_MAX];
   if (!cmd || (feature != 1 && feature != 18) || !inited)
      return FAIL | 5;
   if (feature == 18 && rec.nr_missing)
      return FAIL | 12;
   if (rec.create_fails)
      return FAIL | 11;
   memset(c, 0, sizeof(*c));
   c->cmd        = cmd;
   c->feature    = feature;
   if (feature == 18)
   {
      if (!get_ui(p, "DLSSNR.Enabled"))
         return FAIL | 5;
      c->out_width    = get_ui(p, "DLSSNR.Width");
      c->out_height   = get_ui(p, "DLSSNR.Height");
      c->node_mask    = get_ui(p, "CreationNodeMask") | (get_ui(p, "VisibilityNodeMask") << 8);
      c->nr_preset    = get_ui(p, "DLSSNR.Hint.Render.Preset");
      c->nr_style     = get_ui(p, "DLSSNR.Style");
      c->nr_auto_mask = get_ui(p, "DLSSNR.UseAutoMask");
      c->nr_intensity = get_f(p, "DLSSNR.Intensity");
      c->nr_structure = get_f(p, "DLSSNR.LocalStructureStrength");
      c->nr_tone      = get_f(p, "DLSSNR.LocalToneStrength");
      c->nr_skin      = get_f(p, "DLSSNR.SkinStructureStrength");
      rec.creates++;
      handle_feature[features_made % 8] = 18;
      handles[features_made % 8] = (int)(features_made + 1);
      *out = &handles[features_made % 8];
      features_made++;
      return OK;
   }
   c->width      = get_ui(p, "Width");
   c->height     = get_ui(p, "Height");
   c->out_width  = get_ui(p, "OutWidth");
   c->out_height = get_ui(p, "OutHeight");
   c->quality    = get_i(p, "PerfQualityValue");
   c->flags      = get_i(p, "DLSS.Feature.Create.Flags");
   c->node_mask  = get_ui(p, "CreationNodeMask") | (get_ui(p, "VisibilityNodeMask") << 8);
   c->preset     = get_ui(p, "DLSS.Hint.Render.Preset.Quality");
   rec.creates++;
   handle_feature[features_made % 8] = 1;
   handles[features_made % 8] = (int)(features_made + 1);
   *out = &handles[features_made % 8];
   features_made++;
   return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_ReleaseFeature(int *h)
{
   rec.releases++;
   rec.last_released = *h;
   *h = 0;
   return OK;
}

/* The colour's top-left into the output: a copy stands in for the
 * compute pass, moving the two through the copy states it needs and
 * back to the states the caller put them in. */
static void copy_in(ID3D12GraphicsCommandList *cmd, ID3D12Resource *color,
      ID3D12Resource *output, unsigned w, unsigned h)
{
   D3D12_RESOURCE_BARRIER b[2];
   D3D12_TEXTURE_COPY_LOCATION src, dst;
   D3D12_BOX box;
   memset(b, 0, sizeof(b));
   b[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   b[0].Transition.pResource   = color;
   b[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   b[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   b[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
   b[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   b[1].Transition.pResource   = output;
   b[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   b[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   b[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
   ID3D12GraphicsCommandList_ResourceBarrier(cmd, 2, b);
   src.pResource        = color;
   src.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   src.SubresourceIndex = 0;
   dst.pResource        = output;
   dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   dst.SubresourceIndex = 0;
   box.left   = 0;
   box.top    = 0;
   box.front  = 0;
   box.right  = w;
   box.bottom = h;
   box.back   = 1;
   ID3D12GraphicsCommandList_CopyTextureRegion(cmd, &dst, 0, 0, 0, &src, &box);
   b[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
   b[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   b[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
   b[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   ID3D12GraphicsCommandList_ResourceBarrier(cmd, 2, b);
}

EXPORT int CONV NVSDK_NGX_D3D12_EvaluateFeature(void *cmd, const int *h, param_t *p, void *cb)
{
   fake_ngx_eval_t *e = &rec.eval[rec.evals % FAKE_NGX_MAX];
   (void)cb;
   if (!cmd || !h || !*h)
      return FAIL | 4;
   if (rec.eval_fails)
      return FAIL | 2;
   memset(e, 0, sizeof(*e));
   e->cmd            = cmd;
   e->feature        = *h;
   if (handle_feature[(h - handles) & 7] == 18)
   {
      e->nr             = 1;
      e->color          = get_p(p, "DLSSNR.Color");
      e->output         = get_p(p, "DLSSNR.Output");
      e->depth          = get_p(p, "DLSSNR.Depth");
      e->motion_vectors = get_p(p, "DLSSNR.MVec");
      e->mv_scale_x     = get_f(p, "DLSSNR.MVecScaleX");
      e->mv_scale_y     = get_f(p, "DLSSNR.MVecScaleY");
      e->reset          = (int)get_ui(p, "DLSSNR.Reset");
      e->depth_inverted = (int)get_ui(p, "DLSSNR.DepthInverted");
      e->subrect_w      = get_ui(p, "DLSSNR.ColorSubrectWidth");
      e->subrect_h      = get_ui(p, "DLSSNR.ColorSubrectHeight");
      e->depth_w        = get_ui(p, "DLSSNR.DepthSubrectWidth");
      e->depth_h        = get_ui(p, "DLSSNR.DepthSubrectHeight");
      e->mv_w           = get_ui(p, "DLSSNR.MVecSubrectWidth");
      e->mv_h           = get_ui(p, "DLSSNR.MVecSubrectHeight");
      rec.evals++;
      /* the input's top-left into the output, as the pass would */
      if (e->color && e->output)
         copy_in((ID3D12GraphicsCommandList*)cmd, (ID3D12Resource*)e->color,
               (ID3D12Resource*)e->output, e->subrect_w, e->subrect_h);
      return OK;
   }
   e->color          = get_p(p, "Color");
   e->output         = get_p(p, "Output");
   e->depth          = get_p(p, "Depth");
   e->motion_vectors = get_p(p, "MotionVectors");
   e->exposure       = get_p(p, "ExposureTexture");
   e->transparency   = get_p(p, "DLSS.Input.Bias.Current.Color.Mask");
   e->jitter_x       = get_f(p, "Jitter.Offset.X");
   e->jitter_y       = get_f(p, "Jitter.Offset.Y");
   e->mv_scale_x     = get_f(p, "MV.Scale.X");
   e->mv_scale_y     = get_f(p, "MV.Scale.Y");
   e->pre_exposure   = get_f(p, "DLSS.Pre.Exposure");
   e->exposure_scale = get_f(p, "DLSS.Exposure.Scale");
   e->frame_time     = get_f(p, "FrameTimeDeltaInMsec");
   e->reset          = get_i(p, "Reset");
   e->sharpness = get_f(p, "Sharpness");
   e->subrect_w      = get_ui(p, "DLSS.Render.Subrect.Dimensions.Width");
   e->subrect_h      = get_ui(p, "DLSS.Render.Subrect.Dimensions.Height");
   rec.evals++;
   if (e->color && e->output)
      copy_in((ID3D12GraphicsCommandList*)cmd, (ID3D12Resource*)e->color,
            (ID3D12Resource*)e->output, e->subrect_w, e->subrect_h);
   return OK;
}

EXPORT fake_ngx_record_t *fake_ngx_record(void)
{
   return &rec;
}
