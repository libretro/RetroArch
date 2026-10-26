/* A stand-in NGX core library (_nvngx.dll beside the test on Windows,
 * libnvidia-ngx.so.1 on LD_LIBRARY_PATH on Linux): exports the
 * NVSDK_NGX_* entry points gfx/nvidia/ngx_loader.c resolves, with the
 * SDK's signatures, so the test can call through every table. The
 * parameter store keeps one int per name; the Direct3D 12 half is
 * exported on Windows only, as the real library has it. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#define CONV __cdecl
#else
#define EXPORT __attribute__((visibility("default")))
#define CONV
#endif

#define OK   0x1
#define FAIL ((int)0xBAD00000)

typedef struct param { const void *const *vtable; char name[8][48]; int value[8]; unsigned n; } param_t;
static const void *const param_vtable[17];
static param_t params[4];
static unsigned params_used;
static int handles[4];
static const char *inst_exts[]   = { "VK_KHR_get_physical_device_properties2" };
static const char *device_exts[] = { "VK_NVX_binary_import", "VK_NVX_image_view_handle" };

static param_t *get_param(void)
{
   if (params_used >= 4)
      return NULL;
   memset(&params[params_used], 0, sizeof(params[0]));
   params[params_used].vtable = param_vtable;
   return &params[params_used++];
}

static int *slot(param_t *p, const char *name, int make)
{
   unsigned i;
   for (i = 0; i < p->n; i++)
      if (!strcmp(p->name[i], name))
         return &p->value[i];
   if (!make || p->n >= 8 || strlen(name) >= sizeof(p->name[0]))
      return NULL;
   strcpy(p->name[p->n], name);
   return &p->value[p->n++];
}

static void CONV NVSDK_NGX_Parameter_SetULL(param_t *p, const char *n, uint64_t v) { int *s = slot(p, n, 1); if (s) *s = (int)v; }
static void CONV NVSDK_NGX_Parameter_SetF(param_t *p, const char *n, float v) { int *s = slot(p, n, 1); if (s) *s = (int)v; }
static void CONV NVSDK_NGX_Parameter_SetD(param_t *p, const char *n, double v) { int *s = slot(p, n, 1); if (s) *s = (int)v; }
static void CONV NVSDK_NGX_Parameter_SetUI(param_t *p, const char *n, unsigned v) { int *s = slot(p, n, 1); if (s) *s = (int)v; }
static void CONV NVSDK_NGX_Parameter_SetI(param_t *p, const char *n, int v) { int *s = slot(p, n, 1); if (s) *s = v; }
static void CONV NVSDK_NGX_Parameter_SetD3d11Resource(param_t *p, const char *n, void *v) { int *s = slot(p, n, 1); if (s) *s = v ? 1 : 0; }
static void CONV NVSDK_NGX_Parameter_SetD3d12Resource(param_t *p, const char *n, void *v) { int *s = slot(p, n, 1); if (s) *s = v ? 1 : 0; }
static void CONV NVSDK_NGX_Parameter_SetVoidPointer(param_t *p, const char *n, void *v) { int *s = slot(p, n, 1); if (s) *s = v ? 1 : 0; }
static int CONV NVSDK_NGX_Parameter_GetULL(param_t *p, const char *n, uint64_t *v) { int *s = slot(p, n, 0); if (!s) return FAIL; *v = (uint64_t)*s; return OK; }
static int CONV NVSDK_NGX_Parameter_GetF(param_t *p, const char *n, float *v) { int *s = slot(p, n, 0); if (!s) return FAIL; *v = (float)*s; return OK; }
static int CONV NVSDK_NGX_Parameter_GetD(param_t *p, const char *n, double *v) { int *s = slot(p, n, 0); if (!s) return FAIL; *v = (double)*s; return OK; }
static int CONV NVSDK_NGX_Parameter_GetUI(param_t *p, const char *n, unsigned *v) { int *s = slot(p, n, 0); if (!s) return FAIL; *v = (unsigned)*s; return OK; }
static int CONV NVSDK_NGX_Parameter_GetI(param_t *p, const char *n, int *v) { int *s = slot(p, n, 0); if (!s) return FAIL; *v = *s; return OK; }
static int CONV NVSDK_NGX_Parameter_GetD3d11Resource(param_t *p, const char *n, void **v) { int *s = slot(p, n, 0); if (!s) return FAIL; *v = *s ? p : NULL; return OK; }
static int CONV NVSDK_NGX_Parameter_GetD3d12Resource(param_t *p, const char *n, void **v) { int *s = slot(p, n, 0); if (!s) return FAIL; *v = *s ? p : NULL; return OK; }
static int CONV NVSDK_NGX_Parameter_GetVoidPointer(param_t *p, const char *n, void **v) { int *s = slot(p, n, 0); if (!s) return FAIL; *v = *s ? p : NULL; return OK; }

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

/* Vulkan: init records the project id as parameter "Project" of the
 * capability set, and the first path of the list as "Paths" */
static param_t caps;
/* The test's knob: the tables carry OptiScaler's key, as its stand-in's do */
static int optiscaler;
EXPORT void fake_ngx_set_optiscaler(int on) { optiscaler = on; }
static int inited;

EXPORT int CONV NVSDK_NGX_VULKAN_RequiredExtensions(unsigned *ic, const char ***ie, unsigned *dc, const char ***de)
{
   *ic = 1; *ie = inst_exts; *dc = 2; *de = device_exts;
   return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(const void *info, uint32_t *count, void **props)
{
   (void)info; *count = 0; *props = NULL; return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(void *inst, void *pd, const void *info, uint32_t *count, void **props)
{
   (void)inst; (void)pd; (void)info; *count = 0; *props = NULL; return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_GetFeatureRequirements(void *inst, void *pd, const void *info, void *out)
{
   (void)inst; (void)pd; (void)info; memset(out, 0, 4 + 4 + 255); return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_Init_ProjectID_Ext(const char *project, int engine,
      const char *engine_version, const wchar_t *data_path, void *inst, void *pd,
      void *dev, void *gipa, void *gdpa, int sdk_version, const void *common_info)
{
   /* ngx_feature_common_info_t: path list first */
   const wchar_t *const *const *paths = (const wchar_t *const *const *)common_info;
   (void)engine_version; (void)data_path; (void)inst; (void)pd; (void)dev; (void)gipa; (void)gdpa;
   if (!project || !dev || engine != 0 || sdk_version != 0x15)
      return FAIL;
   memset(&caps, 0, sizeof(caps));
   caps.vtable = param_vtable;
   *slot(&caps, "SuperSampling.Available", 1) = 1;
   *slot(&caps, "Project", 1) = (int)strlen(project);
   *slot(&caps, "Paths", 1) = (common_info && paths[0] && paths[0][0])
      ? (int)paths[0][0][0] : -1;
   if (optiscaler)
      *slot(&caps, "OptiScaler", 1) = 1;
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

EXPORT int CONV NVSDK_NGX_VULKAN_Shutdown1(void *dev, int *in_use) { (void)dev; *in_use = 0; inited = 0; return OK; }
EXPORT int CONV NVSDK_NGX_VULKAN_GetCapabilityParameters(param_t **out) { if (!inited) return FAIL | 7; *out = &caps; return OK; }
EXPORT int CONV NVSDK_NGX_VULKAN_AllocateParameters(param_t **out) { *out = get_param(); return *out ? OK : FAIL; }
EXPORT int CONV NVSDK_NGX_VULKAN_DestroyParameters(param_t *p) { (void)p; return OK; }
EXPORT int CONV NVSDK_NGX_VULKAN_CreateFeature1(void *dev, void *cmd, int feature, param_t *p, int **out)
{
   int *w = slot(p, "Width", 0);
   (void)dev; (void)cmd;
   if (feature != 1 || !w)
      return FAIL | 5;
   handles[0] = *w;
   *out = &handles[0];
   return OK;
}
EXPORT int CONV NVSDK_NGX_VULKAN_ReleaseFeature(int *h) { *h = 0; return OK; }
EXPORT int CONV NVSDK_NGX_VULKAN_EvaluateFeature(void *cmd, const int *h, param_t *p, void *cb)
{
   int *s = slot(p, "Evaluated", 1);
   (void)cmd; (void)cb;
   if (!h || !*h)
      return FAIL | 4;
   if (s)
      *s = *h;
   return OK;
}

#if defined(_WIN32)
EXPORT int CONV NVSDK_NGX_D3D12_Init_ProjectID(const char *project, int engine,
      const char *engine_version, const wchar_t *data_path, void *dev,
      int sdk_version, const void *common_info)
{
   (void)engine_version; (void)data_path; (void)common_info;
   if (!project || !dev || engine != 0 || sdk_version != 0x15)
      return FAIL;
   memset(&caps, 0, sizeof(caps));
   caps.vtable = param_vtable;
   *slot(&caps, "SuperSampling.Available", 1) = 1;
   inited = 1;
   return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_Shutdown1(void *dev, int *in_use) { (void)dev; *in_use = 0; inited = 0; return OK; }
EXPORT int CONV NVSDK_NGX_D3D12_GetCapabilityParameters(param_t **out) { if (!inited) return FAIL | 7; *out = &caps; return OK; }
EXPORT int CONV NVSDK_NGX_D3D12_AllocateParameters(param_t **out) { *out = get_param(); return *out ? OK : FAIL; }
EXPORT int CONV NVSDK_NGX_D3D12_DestroyParameters(param_t *p) { (void)p; return OK; }
EXPORT int CONV NVSDK_NGX_D3D12_GetFeatureRequirements(void *adapter, const void *info, void *out)
{
   (void)adapter; (void)info; memset(out, 0, 4 + 4 + 255); return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_CreateFeature(void *cmd, int feature, param_t *p, int **out)
{
   int *w = slot(p, "Width", 0);
   (void)cmd;
   if (feature != 1 || !w)
      return FAIL | 5;
   handles[1] = *w;
   *out = &handles[1];
   return OK;
}
EXPORT int CONV NVSDK_NGX_D3D12_ReleaseFeature(int *h) { *h = 0; return OK; }
EXPORT int CONV NVSDK_NGX_D3D12_EvaluateFeature(void *cmd, const int *h, param_t *p, void *cb)
{
   return NVSDK_NGX_VULKAN_EvaluateFeature(cmd, h, p, cb);
}
#endif
