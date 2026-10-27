/* A stand-in NGX core library for the test (libnvidia-ngx.so.1 on
 * LD_LIBRARY_PATH, _nvngx.dll beside the test): the Vulkan entry points
 * gfx/common/vulkan_ngx.c resolves, with the SDK's signatures, a typed
 * parameter store, the optimal-settings callback the capability set
 * carries, and a record of every create, evaluate and release for the
 * test to read back through fake_ngx_*(). */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fake_ngx.h"

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#define CONV __cdecl
#else
#define EXPORT __attribute__((visibility("default")))
#define CONV
#endif

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

/* VkExtensionProperties; filled at the first query */
typedef struct ext_props { char name[256]; uint32_t spec; } ext_props_t;
static ext_props_t inst_exts[1];
static ext_props_t device_exts[2];

/* The library's own GPU work on the output, through the transfer stage
 * as a library may: a clear of the Vulkan output it was handed, with
 * the device's entry points from Init's vkGetDeviceProcAddr */
typedef void *(CONV *gdpa_t)(void *device, const char *name);
typedef void (CONV *clear_color_t)(void *cmd, void *image, int layout,
      const void *color, uint32_t count, const void *ranges);
static clear_color_t clear_color_image;
static void clear_output(void *cmd, const void *res)
{
   const struct
   {
      void *view; void *image;
      uint32_t aspect, base_mip, levels, base_layer, layers;
   } *r = res;
   uint32_t range[5];
   float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
   if (!clear_color_image || !res || !r->image)
      return;
   range[0] = r->aspect;
   range[1] = r->base_mip;
   range[2] = r->levels;
   range[3] = r->base_layer;
   range[4] = r->layers;
   /* (VK_IMAGE_LAYOUT_GENERAL, where the output is for an evaluate) */
   clear_color_image(cmd, r->image, 1, color, 1, range);
}

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
   NVSDK_NGX_Parameter_SetUI(p, "DLSS.Get.Dynamic.Max.Render.Width", q == 5 ? w : w);
   NVSDK_NGX_Parameter_SetUI(p, "DLSS.Get.Dynamic.Max.Render.Height", q == 5 ? h : h);
   NVSDK_NGX_Parameter_SetUI(p, "DLSS.Get.Dynamic.Min.Render.Width", rw / 2);
   NVSDK_NGX_Parameter_SetUI(p, "DLSS.Get.Dynamic.Min.Render.Height", rh / 2);
   NVSDK_NGX_Parameter_SetF(p, "Sharpness", 0.0f);
   return OK;
}

/* OptiScaler's shape: no NVSDK_NGX_VULKAN_RequiredExtensions, only the
 * feature requirement calls; the discovery info's feature id (its
 * second int) is recorded */
EXPORT int CONV NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(const void *info, uint32_t *count, void **props)
{
   rec.ext_feature_id = info ? ((const int*)info)[1] : -1;
   rec.ext_calls++;
   strcpy(inst_exts[0].name, "VK_KHR_get_physical_device_properties2");
   *count = 1; *props = inst_exts; return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(void *inst, void *pd, const void *info, uint32_t *count, void **props)
{
   (void)inst; (void)pd;
   rec.ext_feature_id = info ? ((const int*)info)[1] : -1;
   rec.ext_calls++;
   if (rec.ext_feature_id == 18 && rec.nr_missing)
   {
      *count = 0;
      return FAIL | 12;
   }
   strcpy(device_exts[0].name, "VK_KHR_maintenance1");
   strcpy(device_exts[1].name, "VK_NVX_binary_import");
   *count = 2; *props = device_exts; return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_GetFeatureRequirements(void *inst, void *pd, const void *info, void *out)
{
   (void)inst; (void)pd;
   memset(out, 0, 4 + 4 + 255);
   if (info && ((const int*)info)[1] == 18 && rec.nr_missing)
      return FAIL | 12;
   return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_Init_ProjectID_Ext(const char *project, int engine,
      const char *engine_version, const wchar_t *data_path, void *inst, void *pd,
      void *dev, void *gipa, void *gdpa, int sdk_version, const void *common_info)
{
   const wchar_t *const *const *paths = (const wchar_t *const *const *)common_info;
   (void)engine_version; (void)data_path;
   if (!project || !inst || !pd || !dev || !gipa || !gdpa || engine != 0
         || sdk_version != 0x15 || !common_info)
      return FAIL;
   rec.init_calls++;
   rec.init_device   = dev;
   clear_color_image = NULL;
   if (gdpa != (void*)1)
   {
      gdpa_t get;
      memcpy(&get, &gdpa, sizeof(get));
      clear_color_image = (clear_color_t)get(dev, "vkCmdClearColorImage");
   }
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
EXPORT int CONV NVSDK_NGX_VULKAN_Init_ProjectID(const char *project, int engine,
      const char *engine_version, const wchar_t *data_path, void *inst, void *pd,
      void *dev, int sdk_version, const void *common_info)
{
   return NVSDK_NGX_VULKAN_Init_ProjectID_Ext(project, engine, engine_version,
         data_path, inst, pd, dev, (void*)1, (void*)1, sdk_version, common_info);
}

EXPORT int CONV NVSDK_NGX_VULKAN_Shutdown1(void *dev, int *in_use)
{
   const wchar_t *const *const *paths = (const wchar_t *const *const *)init_info;
   (void)dev; *in_use = 0; inited = 0; rec.shutdown_calls++;
   rec.shutdown_path0 = (paths && paths[0] && paths[0][0]) ? paths[0][0][0] : 0;
   return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_GetCapabilityParameters(param_t **out)
{
   if (!inited) return FAIL | 7;
   *out = &caps;
   return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_AllocateParameters(param_t **out)
{
   if (params_used >= 16) return FAIL;
   memset(&params[params_used], 0, sizeof(params[0]));
   params[params_used].vtable = param_vtable;
   *out = &params[params_used++];
   return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_DestroyParameters(param_t *p) { (void)p; rec.params_destroyed++; return OK; }
EXPORT int CONV NVSDK_NGX_VULKAN_CreateFeature1(void *dev, void *cmd, int feature, param_t *p, int **out)
{
   fake_ngx_create_t *c = &rec.create[rec.creates % FAKE_NGX_MAX];
   if (!dev || !cmd || (feature != 1 && feature != 18) || !inited)
      return FAIL | 5;
   if (feature == 18 && rec.nr_missing)
      return FAIL | 12;
   if (rec.create_fails)
      return FAIL | 11;
   memset(c, 0, sizeof(*c));
   c->feature    = feature;
   if (feature == 18)
   {
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
      if (!get_ui(p, "DLSSNR.Enabled"))
         return FAIL | 5;
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
EXPORT int CONV NVSDK_NGX_VULKAN_ReleaseFeature(int *h)
{
   rec.releases++;
   rec.last_released = *h;
   *h = 0;
   return OK;
}

static void copy_res(fake_ngx_resource_t *dst, const void *src)
{
   /* ngx_vk_resource_t: view, image, range (5 u32), format, width, height, type, read_write */
   const struct
   {
      void *view; void *image;
      uint32_t aspect, base_mip, levels, base_layer, layers;
      int format; unsigned width, height;
      int type; unsigned char read_write;
   } *r = src;
   if (!src)
   {
      memset(dst, 0, sizeof(*dst));
      return;
   }
   dst->present    = 1;
   dst->view       = (uint64_t)(uintptr_t)r->view;
   dst->image      = (uint64_t)(uintptr_t)r->image;
   dst->aspect     = r->aspect;
   dst->format     = r->format;
   dst->width      = r->width;
   dst->height     = r->height;
   dst->type       = r->type;
   dst->read_write = r->read_write;
}

EXPORT int CONV NVSDK_NGX_VULKAN_EvaluateFeature(void *cmd, const int *h, param_t *p, void *cb)
{
   fake_ngx_eval_t *e = &rec.eval[rec.evals % FAKE_NGX_MAX];
   (void)cb;
   if (!cmd || !h || !*h)
      return FAIL | 4;
   if (rec.eval_fails)
      return FAIL | 2;
   memset(e, 0, sizeof(*e));
   e->feature    = *h;
   if (handle_feature[(h - handles) & 7] == 18)
   {
      e->nr = 1;
      copy_res(&e->color, get_p(p, "DLSSNR.Color"));
      copy_res(&e->output, get_p(p, "DLSSNR.Output"));
      copy_res(&e->depth, get_p(p, "DLSSNR.Depth"));
      copy_res(&e->motion_vectors, get_p(p, "DLSSNR.MVec"));
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
      return OK;
   }
   copy_res(&e->color, get_p(p, "Color"));
   copy_res(&e->output, get_p(p, "Output"));
   clear_output(cmd, get_p(p, "Output"));
   copy_res(&e->depth, get_p(p, "Depth"));
   copy_res(&e->motion_vectors, get_p(p, "MotionVectors"));
   copy_res(&e->exposure, get_p(p, "ExposureTexture"));
   copy_res(&e->transparency, get_p(p, "DLSS.Input.Bias.Current.Color.Mask"));
   e->jitter_x   = get_f(p, "Jitter.Offset.X");
   e->jitter_y   = get_f(p, "Jitter.Offset.Y");
   e->mv_scale_x = get_f(p, "MV.Scale.X");
   e->mv_scale_y = get_f(p, "MV.Scale.Y");
   e->pre_exposure = get_f(p, "DLSS.Pre.Exposure");
   e->exposure_scale = get_f(p, "DLSS.Exposure.Scale");
   e->frame_time = get_f(p, "FrameTimeDeltaInMsec");
   e->reset      = get_i(p, "Reset");
   e->sharpness = get_f(p, "Sharpness");
   e->subrect_w  = get_ui(p, "DLSS.Render.Subrect.Dimensions.Width");
   e->subrect_h  = get_ui(p, "DLSS.Render.Subrect.Dimensions.Height");
   rec.evals++;
   return OK;
}

EXPORT fake_ngx_record_t *fake_ngx_record(void)
{
   return &rec;
}
