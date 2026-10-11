/* The shared OpenXR units (gfx/common/openxr_*.c), as they ship,
 * against a runtime the test stands in for. Each unit's xr calls go
 * through the proc lookup it is given, so the stand-in answers them. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <openxr/openxr.h>
typedef unsigned int EGLenum; /* the header wants EGL types */
#define XR_USE_GRAPHICS_API_OPENGL_ES
/* vulkan_openxr.c's half too: the header is read once. */
#include "../../../gfx/common/vksym.h"
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>
#include <queues/message_queue.h>

#include "../../../msg_hash.h"

/* ---- logging and messages ---- */
static char t_log[4096];
static void t_append(const char *fmt, va_list ap)
{
   size_t len = strlen(t_log);
   if (len < sizeof(t_log) - 1)
      vsnprintf(t_log + len, sizeof(t_log) - len, fmt, ap);
}
void RARCH_LOG(const char *fmt, ...) { va_list ap; va_start(ap, fmt); t_append(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); t_append(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...) { va_list ap; va_start(ap, fmt); t_append(fmt, ap); va_end(ap); }
static int t_notified = -1;
const char *msg_hash_to_str(enum msg_hash_enums msg) { t_notified = (int)msg; return "msg"; }
void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category) { }

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); failures++; } } while (0)
#define LOGGED(s) (strstr(t_log, (s)) != NULL)
static void t_reset_log(void) { t_log[0] = '\0'; t_notified = -1; }

/* ---- the stand-in runtime ---- */
#define T_INSTANCE ((XrInstance)(uintptr_t)0x10)
#define T_SYSTEM   ((XrSystemId)0x20)
static const char *t_exts[8];
static unsigned    t_num_exts;
static XrResult    t_create_instance = XR_SUCCESS;
static XrResult    t_get_system      = XR_SUCCESS;
static uint32_t    t_views           = 2;
static uint32_t    t_rec_width       = 1728;
static uint32_t    t_rec_height      = 1824;
static uint32_t    t_max_w           = 8192;
static uint32_t    t_max_h           = 2048;
static uint32_t    t_max_layers;       /* 0: the minimum OpenXR allows */
static const char *t_missing;          /* a function the stand-in lacks */
static const char *t_enabled[8];
static unsigned    t_num_enabled;
static unsigned    t_destroyed_instances;
static XrResult    t_loader_res = XR_SUCCESS;
static unsigned    t_loader_calls;
static const XrLoaderInitInfoBaseHeaderKHR *t_loader_info;
static const void *t_instance_next;
static XrEnvironmentBlendMode t_modes[2] = { XR_ENVIRONMENT_BLEND_MODE_OPAQUE };
static uint32_t    t_num_modes = 1;

static XRAPI_ATTR XrResult XRAPI_CALL t_enum_exts(const char *layer,
      uint32_t cap, uint32_t *count, XrExtensionProperties *p)
{
   uint32_t i;
   *count = t_num_exts;
   if (!cap)
      return XR_SUCCESS;
   for (i = 0; i < t_num_exts && i < cap; i++)
      strlcpy(p[i].extensionName, t_exts[i], sizeof(p[i].extensionName));
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_create_instance_fn(
      const XrInstanceCreateInfo *ci, XrInstance *out)
{
   uint32_t i;
   t_num_enabled = 0;
   t_instance_next = ci->next;
   for (i = 0; i < ci->enabledExtensionCount && i < 8; i++)
      t_enabled[t_num_enabled++] = ci->enabledExtensionNames[i];
   if (t_create_instance != XR_SUCCESS)
      return t_create_instance;
   *out = T_INSTANCE;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_instance(XrInstance i)
{ t_destroyed_instances++; return XR_SUCCESS; }
static XRAPI_ATTR XrResult XRAPI_CALL t_get_system_fn(XrInstance i,
      const XrSystemGetInfo *gi, XrSystemId *out)
{
   if (t_get_system != XR_SUCCESS)
      return t_get_system;
   *out = T_SYSTEM;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_get_system_properties(XrInstance i,
      XrSystemId s, XrSystemProperties *p)
{
   strlcpy(p->systemName, "Stand-in HMD", sizeof(p->systemName));
   p->graphicsProperties.maxSwapchainImageWidth  = t_max_w;
   p->graphicsProperties.maxSwapchainImageHeight = t_max_h;
   p->graphicsProperties.maxLayerCount           = t_max_layers;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_enum_views(XrInstance i, XrSystemId s,
      XrViewConfigurationType t, uint32_t cap, uint32_t *count,
      XrViewConfigurationView *v)
{
   *count = t_views;
   if (cap >= 1 && t_views >= 1)
   {
      v[0].recommendedImageRectWidth  = t_rec_width;
      v[0].recommendedImageRectHeight = t_rec_height;
   }
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_enum_modes(XrInstance i, XrSystemId s,
      XrViewConfigurationType t, uint32_t cap, uint32_t *count,
      XrEnvironmentBlendMode *m)
{
   uint32_t k;
   *count = t_num_modes;
   for (k = 0; k < cap && k < t_num_modes; k++)
      m[k] = t_modes[k];
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_init_loader(
      const XrLoaderInitInfoBaseHeaderKHR *info)
{
   t_loader_calls++;
   t_loader_info = info;
   return t_loader_res;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_refresh_rates(XrSession s,
      uint32_t cap, uint32_t *count, float *r) { *count = 0; return XR_SUCCESS; }
static unsigned t_rate_requests;
static XRAPI_ATTR XrResult XRAPI_CALL t_request_rate(XrSession s, float hz)
{ t_rate_requests++; return XR_SUCCESS; }

#define T_SESSION ((XrSession)(uintptr_t)0x30)
#define T_OTHER   ((XrSession)(uintptr_t)0x31)
static XrEventDataBuffer t_events[8];
static unsigned t_num_events, t_next_event;
static XrResult t_poll_result = XR_SUCCESS;  /* after the queue runs dry */
static unsigned t_begun, t_ended_sessions, t_locks, t_unlocks;
static unsigned t_sessions_made, t_sessions_destroyed;
static unsigned t_spaces_made, t_spaces_destroyed, t_space_fail_at;
static unsigned t_seq, t_state_seq, t_begin_seq, t_ended_seq, t_exiting_seq;
static bool t_begin_in_lock, t_end_in_lock;

static bool t_event_room(void)
{
   CHECK(t_num_events < sizeof(t_events) / sizeof(t_events[0]),
         "event queue full");
   return t_num_events < sizeof(t_events) / sizeof(t_events[0]);
}
static void t_push_state(XrSession s, XrSessionState state)
{
   XrEventDataSessionStateChanged *e;
   if (!t_event_room())
      return;
   e = (XrEventDataSessionStateChanged*)&t_events[t_num_events++];
   memset(e, 0, sizeof(t_events[0]));
   e->type    = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
   e->session = s;
   e->state   = state;
}
static void t_push_space(XrSession s, XrReferenceSpaceType type)
{
   XrEventDataReferenceSpaceChangePending *e;
   if (!t_event_room())
      return;
   e = (XrEventDataReferenceSpaceChangePending*)&t_events[t_num_events++];
   memset(e, 0, sizeof(t_events[0]));
   e->type               = XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING;
   e->session            = s;
   e->referenceSpaceType = type;
}
static void t_push_instance_loss(void)
{
   if (!t_event_room())
      return;
   memset(&t_events[t_num_events], 0, sizeof(t_events[0]));
   t_events[t_num_events++].type = XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_poll_event(XrInstance i, XrEventDataBuffer *ev)
{
   if (t_next_event < t_num_events)
   {
      memcpy(ev, &t_events[t_next_event++], sizeof(*ev));
      return XR_SUCCESS;
   }
   return t_poll_result == XR_SUCCESS ? XR_EVENT_UNAVAILABLE : t_poll_result;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_create_session(XrInstance i,
      const XrSessionCreateInfo *ci, XrSession *s)
{ t_sessions_made++; *s = T_SESSION; return XR_SUCCESS; }
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_session(XrSession s)
{ t_sessions_destroyed++; return XR_SUCCESS; }
static XRAPI_ATTR XrResult XRAPI_CALL t_begin_session(XrSession s,
      const XrSessionBeginInfo *b)
{
   t_begun++;
   t_begin_seq     = ++t_seq;
   t_begin_in_lock = (t_locks - t_unlocks == 1);
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_end_session(XrSession s)
{
   t_ended_sessions++;
   t_end_in_lock = (t_locks - t_unlocks == 1);
   return XR_SUCCESS;
}
static XrReferenceSpaceType t_space_type;
static XrPosef t_space_pose;
static bool    t_stage_missing;
static unsigned t_stage_asked;
static XrSpace t_space_destroyed_last;
static bool    t_space_destroy_in_lock;
/* xrLocateSpace in the last STAGE space made finds t_in_stage while
 * t_in_stage_set, after t_in_stage_invalid locations it has not got. */
static XrSpace t_stage_space;
static bool    t_in_stage_set;
static XrPosef t_in_stage;
static unsigned t_in_stage_invalid;
static XRAPI_ATTR XrResult XRAPI_CALL t_create_space(XrSession s,
      const XrReferenceSpaceCreateInfo *ci, XrSpace *out)
{
   if (t_space_fail_at && t_spaces_made + 1 == t_space_fail_at)
      return XR_ERROR_RUNTIME_FAILURE;
   if (ci->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE)
      t_stage_asked++;
   if (     t_stage_missing
         && ci->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE)
      return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
   t_space_type = ci->referenceSpaceType;
   t_space_pose = ci->poseInReferenceSpace;
   *out = (XrSpace)(uintptr_t)(0x40 + (++t_spaces_made));
   if (ci->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE)
      t_stage_space = *out;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_space(XrSpace s)
{
   t_spaces_destroyed++;
   t_space_destroyed_last  = s;
   t_space_destroy_in_lock = (t_locks - t_unlocks == 1);
   return XR_SUCCESS;
}
/* xrLocateViews finds both eyes at t_view while t_views_tracked. */
static bool    t_views_tracked;
static XrView  t_view[2];
static XrSpace t_located_in;
static XrTime  t_located_at;
/* View state flags it clears, as a runtime that lost the eyes does. */
static XrViewStateFlags t_views_lost;
static XRAPI_ATTR XrResult XRAPI_CALL t_locate_views(XrSession s,
      const XrViewLocateInfo *li, XrViewState *vs, uint32_t cap, uint32_t *n, XrView *v)
{
   if (!t_views_tracked)
   {
      *n = 0;
      return XR_ERROR_RUNTIME_FAILURE;
   }
   t_located_in       = li->space;
   t_located_at       = li->displayTime;
   vs->viewStateFlags = (XR_VIEW_STATE_ORIENTATION_VALID_BIT
      | XR_VIEW_STATE_POSITION_VALID_BIT) & ~t_views_lost;
   *n = 2;
   if (cap >= 2)
   {
      v[0].pose = t_view[0].pose;
      v[0].fov  = t_view[0].fov;
      v[1].pose = t_view[1].pose;
      v[1].fov  = t_view[1].fov;
   }
   return XR_SUCCESS;
}
static bool    t_head_tracked;   /* xrLocateSpace finds the head at t_head */
/* Location flags it clears, as a runtime that lost the head does. */
static XrSpaceLocationFlags t_head_lost;
static XrPosef t_head;
static XrSpace t_head_space, t_head_base;
static unsigned t_head_locates;
static XRAPI_ATTR XrResult XRAPI_CALL t_locate_space(XrSpace a, XrSpace b,
      XrTime t, XrSpaceLocation *loc)
{
   if (t_in_stage_set && b == t_stage_space)
   {
      loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
         | XR_SPACE_LOCATION_POSITION_VALID_BIT;
      loc->pose          = t_in_stage;
      if (t_in_stage_invalid)
      {
         t_in_stage_invalid--;
         loc->locationFlags = 0;
      }
      return XR_SUCCESS;
   }
   t_head_locates++;
   if (!t_head_tracked)
      return XR_ERROR_RUNTIME_FAILURE;
   t_head_space       = a;
   t_head_base        = b;
   loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;
   loc->pose          = t_head;
   loc->locationFlags &= ~t_head_lost;
   return XR_SUCCESS;
}

/* the session's hooks, counted */
static unsigned t_h_ended, t_h_exiting, t_h_local, t_h_stage;
static XrSessionState t_h_last_state;
static void t_hook_state(void *u, XrSessionState s)
{ t_h_last_state = s; t_state_seq = ++t_seq; }
static void t_hook_ended(void *u) { t_h_ended++; t_ended_seq = ++t_seq; }
static void t_hook_exiting(void *u) { t_h_exiting++; t_exiting_seq = ++t_seq; }
static void t_hook_local(void *u) { t_h_local++; }
static void t_hook_stage(void *u) { t_h_stage++; }
static void t_hook_lock(void *u) { t_locks++; }
/* Run at each unlock, as another thread that takes the lock next. */
static void (*t_at_unlock)(void);
static void t_hook_unlock(void *u)
{
   void (*at)(void) = t_at_unlock;
   t_unlocks++;
   if (at)
   {
      t_at_unlock = NULL;
      at();
      t_at_unlock = at;
   }
}

/* the swapchain stand-in */
static uint32_t t_sc_images = 3;
static uint32_t t_sc_images_second;
static unsigned t_sc_count_queries;
static unsigned t_sc_made, t_sc_destroyed, t_sc_created_total;
static XrResult t_sc_wait = XR_SUCCESS;
static XrResult t_sc_release = XR_SUCCESS;
static XrDuration t_sc_wait_timeout;
static int64_t  t_ci_format;
static bool     t_ci_mutable;
static uint32_t t_ci_width, t_ci_height;
static unsigned t_sc_unlocked;
static unsigned t_sc_acquires, t_sc_releases;
static int64_t  t_formats[2] = { 50, 44 };
static uint32_t t_more_formats;   /* listed after those, 1000 + index */

static XRAPI_ATTR XrResult XRAPI_CALL t_enum_formats(XrSession s,
      uint32_t cap, uint32_t *n, int64_t *f)
{
   uint32_t i;
   *n = 2 + t_more_formats;
   if (cap && cap < *n)
      return XR_ERROR_SIZE_INSUFFICIENT;
   for (i = 0; i < cap && i < *n; i++)
      f[i] = (i < 2) ? t_formats[i] : 1000 + i;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_create_sc(XrSession s,
      const XrSwapchainCreateInfo *ci, XrSwapchain *out)
{
   *out = (XrSwapchain)(uintptr_t)(0x100 + (++t_sc_created_total));
   t_sc_made++;
   t_ci_format  = ci->format;
   t_ci_mutable = (ci->usageFlags & XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT) != 0;
   t_ci_width   = ci->width;
   t_ci_height  = ci->height;
   if (t_locks <= t_unlocks)
      t_sc_unlocked++;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_sc(XrSwapchain s)
{
   t_sc_destroyed++;
   if (t_locks <= t_unlocks)
      t_sc_unlocked++;
   return XR_SUCCESS;
}
static bool     t_fill_gles;    /* list GLES images, 0x500 apart a call */
static uint32_t t_gles_base;
static XRAPI_ATTR XrResult XRAPI_CALL t_enum_images(XrSwapchain s,
      uint32_t cap, uint32_t *n, XrSwapchainImageBaseHeader *imgs)
{
   uint32_t i;
   if (cap && imgs && t_fill_gles)
   {
      XrSwapchainImageOpenGLESKHR *es = (XrSwapchainImageOpenGLESKHR*)imgs;
      t_gles_base += 0x100;
      for (i = 0; i < cap && i < t_sc_images; i++)
         es[i].image = t_gles_base + i;
      *n = i;
      return XR_SUCCESS;
   }
   /* Count queries: a binding lists the images itself. */
   t_sc_count_queries++;
   *n = (t_sc_images_second && (t_sc_count_queries % 2) == 0)
      ? t_sc_images_second : t_sc_images;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_acquire(XrSwapchain s,
      const XrSwapchainImageAcquireInfo *ai, uint32_t *i)
{ t_sc_acquires++; *i = 1; return XR_SUCCESS; }
static XRAPI_ATTR XrResult XRAPI_CALL t_wait(XrSwapchain s,
      const XrSwapchainImageWaitInfo *wi)
{ t_sc_wait_timeout = wi->timeout; return t_sc_wait; }
static XRAPI_ATTR XrResult XRAPI_CALL t_release(XrSwapchain s,
      const XrSwapchainImageReleaseInfo *ri)
{ t_sc_releases++; return t_sc_release; }

static unsigned t_listed[2];
static XrSwapchain t_listed_sc[2];
static void *t_listed_user[2];
static int t_listed_got = -1;   /* images the binding gets; -1 all */
static XrResult t_images_cb(void *user, XrSwapchain sc, unsigned layer,
      uint32_t *count)
{
   t_listed[layer]      = *count;
   t_listed_sc[layer]   = sc;
   t_listed_user[layer] = user;
   if (t_listed_got >= 0)
      *count = (uint32_t)t_listed_got;
   if (t_locks <= t_unlocks)
      t_sc_unlocked++;
   return XR_SUCCESS;
}

/* the frame stand-in: a 72 Hz headset */
static unsigned t_waits, t_begins, t_ends, t_frame_unlocked;
/* A begin before the open frame's end discards it, as a runtime does. */
static unsigned t_discarded;
static bool t_frame_open;
static uint32_t t_last_layer_count;
static unsigned t_extra_cap;
static XrResult t_wait_frame_res = XR_SUCCESS;
static XrResult t_end_frame_res  = XR_SUCCESS;
static XrResult t_begin_frame_res = XR_SUCCESS;
static XRAPI_ATTR XrResult XRAPI_CALL t_wait_frame(XrSession s,
      const XrFrameWaitInfo *wi, XrFrameState *st)
{
   t_waits++;
   if (t_wait_frame_res != XR_SUCCESS)
      return t_wait_frame_res;
   st->predictedDisplayTime   = 1000000000LL + (XrTime)t_waits * 13888889LL;
   st->predictedDisplayPeriod = 13888889LL;
   st->shouldRender           = XR_TRUE;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_begin_frame(XrSession s,
      const XrFrameBeginInfo *bi)
{
   t_begins++;
   if (t_locks - t_unlocks != 1)
      t_frame_unlocked++;
   if (t_begin_frame_res == XR_SUCCESS)
   {
      if (t_frame_open)
         t_discarded++;
      t_frame_open = true;
   }
   return t_begin_frame_res;
}
static XrCompositionLayerProjection     t_proj;
static XrCompositionLayerProjectionView t_proj_views[2];
static XrStructureType                  t_layer_types[4];
static XrCompositionLayerQuad           t_quad;   /* the first quad */
static XRAPI_ATTR XrResult XRAPI_CALL t_end_frame(XrSession s,
      const XrFrameEndInfo *ei)
{
   uint32_t i;
   t_ends++;
   t_frame_open       = false;
   t_last_layer_count = ei->layerCount;
   memset(t_layer_types, 0, sizeof(t_layer_types));
   for (i = 0; i < ei->layerCount && i < 4; i++)
      t_layer_types[i] = ei->layers[i]->type;
   for (i = 0; i < ei->layerCount; i++)
      if (ei->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_QUAD)
      {
         t_quad = *(const XrCompositionLayerQuad*)ei->layers[i];
         break;
      }
   if (     ei->layerCount
         && ei->layers[0]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
   {
      t_proj = *(const XrCompositionLayerProjection*)ei->layers[0];
      memcpy(t_proj_views, t_proj.views, sizeof(t_proj_views));
   }
   if (t_locks - t_unlocks != 1)
      t_frame_unlocked++;
   return t_end_frame_res;
}
static unsigned t_extra_layers(void *user, XrTime time,
      const XrCompositionLayerBaseHeader **layers, unsigned cap)
{
   static XrCompositionLayerQuad dot;
   t_extra_cap = cap;
   layers[0]   = (const XrCompositionLayerBaseHeader*)&dot;
   return 1;
}

static XrResult t_gles_reqs_res = XR_SUCCESS;
static XRAPI_ATTR XrResult XRAPI_CALL t_gles_reqs(XrInstance i, XrSystemId s,
      XrGraphicsRequirementsOpenGLESKHR *r)
{
   r->minApiVersionSupported = XR_MAKE_VERSION(3, 0, 0);
   r->maxApiVersionSupported = XR_MAKE_VERSION(3, 2, 0);
   return t_gles_reqs_res;
}

/* XR_KHR_vulkan_enable2: only the requirements are asked for when the
 * caller makes no Vulkan object through the runtime. */
static XRAPI_ATTR XrResult XRAPI_CALL t_vk_reqs(XrInstance i, XrSystemId s,
      XrGraphicsRequirementsVulkanKHR *r)
{
   r->minApiVersionSupported = XR_MAKE_VERSION(1, 0, 0);
   r->maxApiVersionSupported = XR_MAKE_VERSION(1, 3, 0);
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_vk_instance(XrInstance i,
      const XrVulkanInstanceCreateInfoKHR *ci, VkInstance *out, VkResult *r)
{ return XR_ERROR_FUNCTION_UNSUPPORTED; }
static XRAPI_ATTR XrResult XRAPI_CALL t_vk_device(XrInstance i,
      const XrVulkanDeviceCreateInfoKHR *ci, VkDevice *out, VkResult *r)
{ return XR_ERROR_FUNCTION_UNSUPPORTED; }
static XRAPI_ATTR XrResult XRAPI_CALL t_vk_gpu(XrInstance i,
      const XrVulkanGraphicsDeviceGetInfoKHR *gi, VkPhysicalDevice *out)
{ return XR_ERROR_FUNCTION_UNSUPPORTED; }

static const struct { const char *name; PFN_xrVoidFunction fn; } t_procs[] = {
   { "xrEnumerateInstanceExtensionProperties", (PFN_xrVoidFunction)t_enum_exts },
   { "xrCreateInstance",                  (PFN_xrVoidFunction)t_create_instance_fn },
   { "xrDestroyInstance",                 (PFN_xrVoidFunction)t_destroy_instance },
   { "xrGetSystem",                       (PFN_xrVoidFunction)t_get_system_fn },
   { "xrGetSystemProperties",             (PFN_xrVoidFunction)t_get_system_properties },
   { "xrEnumerateViewConfigurationViews", (PFN_xrVoidFunction)t_enum_views },
   { "xrEnumerateEnvironmentBlendModes",  (PFN_xrVoidFunction)t_enum_modes },
   { "xrEnumerateDisplayRefreshRatesFB",  (PFN_xrVoidFunction)t_refresh_rates },
   { "xrRequestDisplayRefreshRateFB",     (PFN_xrVoidFunction)t_request_rate },
   { "xrPollEvent",            (PFN_xrVoidFunction)t_poll_event },
   { "xrCreateSession",        (PFN_xrVoidFunction)t_create_session },
   { "xrDestroySession",       (PFN_xrVoidFunction)t_destroy_session },
   { "xrBeginSession",         (PFN_xrVoidFunction)t_begin_session },
   { "xrEndSession",           (PFN_xrVoidFunction)t_end_session },
   { "xrCreateReferenceSpace", (PFN_xrVoidFunction)t_create_space },
   { "xrDestroySpace",         (PFN_xrVoidFunction)t_destroy_space },
   { "xrLocateViews",          (PFN_xrVoidFunction)t_locate_views },
   { "xrLocateSpace",          (PFN_xrVoidFunction)t_locate_space },
   { "xrEnumerateSwapchainFormats", (PFN_xrVoidFunction)t_enum_formats },
   { "xrCreateSwapchain",           (PFN_xrVoidFunction)t_create_sc },
   { "xrDestroySwapchain",          (PFN_xrVoidFunction)t_destroy_sc },
   { "xrEnumerateSwapchainImages",  (PFN_xrVoidFunction)t_enum_images },
   { "xrAcquireSwapchainImage",     (PFN_xrVoidFunction)t_acquire },
   { "xrWaitSwapchainImage",        (PFN_xrVoidFunction)t_wait },
   { "xrReleaseSwapchainImage",     (PFN_xrVoidFunction)t_release },
   { "xrWaitFrame",  (PFN_xrVoidFunction)t_wait_frame },
   { "xrBeginFrame", (PFN_xrVoidFunction)t_begin_frame },
   { "xrEndFrame",   (PFN_xrVoidFunction)t_end_frame },
   { "xrInitializeLoaderKHR",  (PFN_xrVoidFunction)t_init_loader },
   { "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction)t_gles_reqs },
   { "xrGetVulkanGraphicsRequirements2KHR",  (PFN_xrVoidFunction)t_vk_reqs },
   { "xrCreateVulkanInstanceKHR",            (PFN_xrVoidFunction)t_vk_instance },
   { "xrCreateVulkanDeviceKHR",              (PFN_xrVoidFunction)t_vk_device },
   { "xrGetVulkanGraphicsDevice2KHR",        (PFN_xrVoidFunction)t_vk_gpu },
   { NULL, NULL }
};

static XRAPI_ATTR XrResult XRAPI_CALL t_get_proc(XrInstance i,
      const char *name, PFN_xrVoidFunction *fn)
{
   unsigned k;
   *fn = NULL;
   if (t_missing && string_is_equal(name, t_missing))
      return XR_ERROR_FUNCTION_UNSUPPORTED;
   for (k = 0; t_procs[k].name; k++)
      if (string_is_equal(name, t_procs[k].name))
      {
         *fn = t_procs[k].fn;
         return XR_SUCCESS;
      }
   return XR_ERROR_FUNCTION_UNSUPPORTED;
}

/* The loader's: vulkan_openxr_new names it itself. */
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance i,
      const char *name, PFN_xrVoidFunction *fn)
{
   return t_get_proc(i, name, fn);
}

/* ---- the units, as they ship ---- */
#include "../../../gfx/common/openxr_runtime.c"
#include "../../../gfx/common/openxr_session.c"
#include "../../../gfx/common/openxr_swapchain.c"
#include "../../../gfx/common/openxr_frame.c"
#include "../../../gfx/common/openxr_tracking.c"
#include "../../../gfx/common/openxr_host.c"
#include "../../../gfx/common/openxr_binding_gles.c"
#include "../../../gfx/common/vulkan_openxr.c"

static void t_runtime_defaults(void)
{
   t_num_exts        = 3;
   t_exts[0]         = "XR_KHR_vulkan_enable2";
   t_exts[1]         = "XR_FB_display_refresh_rate";
   t_exts[2]         = "XR_VALVE_frame_controller_interaction";
   t_create_instance = XR_SUCCESS;
   t_get_system      = XR_SUCCESS;
   t_views           = 2;
   t_rec_width       = 1728;
   t_rec_height      = 1824;
   t_max_w           = 8192;
   t_max_h           = 2048;
   t_missing         = NULL;
   t_loader_res      = XR_SUCCESS;
   t_loader_calls    = 0;
   t_loader_info     = NULL;
   t_instance_next   = NULL;
   t_modes[0]        = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
   t_num_modes       = 1;
   t_reset_log();
}

static void test_runtime(void)
{
   openxr_runtime_t rt;

   /* The graphics extension and the optional ones it offers. */
   t_runtime_defaults();
   memset(&rt, 0, sizeof(rt));
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_OK, "instance");
   CHECK(t_num_enabled == 3, "enabled %u extensions", t_num_enabled);
   CHECK(rt.refresh_ext && rt.frame_controller, "optional extensions not seen");
   CHECK(rt.DestroyInstance && rt.RequestDisplayRefreshRateFB, "functions not loaded");
   CHECK(openxr_runtime_find_system(&rt) == OPENXR_RUNTIME_OK, "system");
   CHECK(rt.max_dim == 2048, "max_dim %u: the smaller side, at most 4096", rt.max_dim);
   CHECK(rt.rec_width == 1728, "rec_width %u", (unsigned)rt.rec_width);
   CHECK(rt.rec_height == 1824, "rec_height %u", (unsigned)rt.rec_height);
   CHECK(LOGGED("[OpenXR] Headset: Stand-in HMD.\n"), "log: %s", t_log);
   openxr_runtime_deinit(&rt);
   CHECK(t_destroyed_instances == 1, "instance not destroyed");

   /* No graphics extension: unavailable. */
   t_runtime_defaults();
   t_exts[0] = "XR_KHR_opengl_enable";
   memset(&rt, 0, sizeof(rt));
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_UNAVAILABLE, "no extension: not unavailable");
   CHECK(LOGGED("[OpenXR] The runtime lacks XR_KHR_vulkan_enable2.\n"), "log: %s", t_log);
   openxr_runtime_deinit(&rt);

   /* An optional extension the runtime lacks is not asked for. */
   t_runtime_defaults();
   t_num_exts = 1;
   memset(&rt, 0, sizeof(rt));
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_OK, "instance without optionals");
   CHECK(t_num_enabled == 1 && !rt.refresh_ext && !rt.frame_controller,
         "optional extensions asked for");
   openxr_runtime_deinit(&rt);

   /* xrCreateInstance failing: unavailable. */
   t_runtime_defaults();
   t_create_instance = XR_ERROR_RUNTIME_UNAVAILABLE;
   memset(&rt, 0, sizeof(rt));
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_UNAVAILABLE, "create failing");
   CHECK(LOGGED("[OpenXR] No runtime (xrCreateInstance: "), "log: %s", t_log);
   openxr_runtime_deinit(&rt);

   /* No headset. */
   t_runtime_defaults();
   t_get_system = XR_ERROR_FORM_FACTOR_UNAVAILABLE;
   memset(&rt, 0, sizeof(rt));
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   CHECK(openxr_runtime_find_system(&rt) == OPENXR_RUNTIME_UNAVAILABLE, "no headset");
   CHECK(LOGGED("[OpenXR] No headset (xrGetSystem: "), "log: %s", t_log);
   openxr_runtime_deinit(&rt);

   /* A required function missing: failed. */
   t_runtime_defaults();
   t_missing = "xrGetSystemProperties";
   memset(&rt, 0, sizeof(rt));
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   CHECK(openxr_runtime_find_system(&rt) == OPENXR_RUNTIME_FAILED, "missing function");
   CHECK(LOGGED("[OpenXR] The runtime lacks functions headset output needs.\n"),
         "log: %s", t_log);
   openxr_runtime_deinit(&rt);

   /* A refresh function missing: the rate is measured, never asked. */
   t_runtime_defaults();
   t_missing = "xrRequestDisplayRefreshRateFB";
   memset(&rt, 0, sizeof(rt));
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   CHECK(!rt.refresh_ext, "refresh ext kept without its functions");
   openxr_runtime_deinit(&rt);
}

static void t_session_fresh(openxr_runtime_t *rt, openxr_session_t *s)
{
   t_runtime_defaults();
   memset(rt, 0, sizeof(*rt));
   openxr_runtime_create_instance(rt, t_get_proc, "XR_KHR_vulkan_enable2");
   openxr_runtime_find_system(rt);
   memset(s, 0, sizeof(*s));
   s->hooks.state_changed = t_hook_state;
   s->hooks.ended         = t_hook_ended;
   s->hooks.exiting       = t_hook_exiting;
   s->hooks.local_changed = t_hook_local;
   s->hooks.stage_changed = t_hook_stage;
   s->hooks.lock          = t_hook_lock;
   s->hooks.unlock        = t_hook_unlock;
   t_num_events = t_next_event = 0;
   t_poll_result = XR_SUCCESS;
   t_begun = t_ended_sessions = t_locks = t_unlocks = 0;
   t_spaces_made = t_spaces_destroyed = t_space_fail_at = 0;
   t_h_ended = t_h_exiting = t_h_local = t_h_stage = 0;
   t_h_last_state = XR_SESSION_STATE_UNKNOWN;
   t_seq = t_state_seq = t_begin_seq = t_ended_seq = t_exiting_seq = 0;
   t_begin_in_lock = t_end_in_lock = false;
}

static void test_session(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   int dummy_binding = 0;

   /* READY begins under the lock; FOCUSED is focused; STOPPING ends. */
   t_session_fresh(&rt, &s);
   CHECK(openxr_session_load(&s, &rt), "load");
   CHECK(openxr_session_create(&s, &dummy_binding), "create");
   CHECK(t_spaces_made == 2, "LOCAL and VIEW: %u spaces", t_spaces_made);
   openxr_session_set_alive(&s, true);
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   openxr_session_poll(&s);
   CHECK(t_begun == 1 && openxr_session_running(&s), "not begun");
   CHECK(t_locks == 1 && t_unlocks == 1, "begin not under the lock");
   CHECK(t_begin_in_lock, "xrBeginSession ran outside the lock");
   CHECK(t_state_seq && t_state_seq < t_begin_seq,
         "state_changed (%u) not before begin (%u)", t_state_seq, t_begin_seq);
   t_push_state(T_SESSION, XR_SESSION_STATE_SYNCHRONIZED);
   t_push_state(T_SESSION, XR_SESSION_STATE_VISIBLE);
   t_push_state(T_SESSION, XR_SESSION_STATE_FOCUSED);
   openxr_session_poll(&s);
   CHECK(openxr_session_focused(&s) && openxr_session_visible(&s), "not focused");
   CHECK(t_h_last_state == XR_SESSION_STATE_FOCUSED, "hook missed a state");
   CHECK(LOGGED("[OpenXR] Session focused.\n"), "log: %s", t_log);
   t_push_state(T_SESSION, XR_SESSION_STATE_VISIBLE);
   openxr_session_poll(&s);
   CHECK(!openxr_session_focused(&s) && openxr_session_visible(&s), "visible");
   t_push_state(T_SESSION, XR_SESSION_STATE_STOPPING);
   openxr_session_poll(&s);
   CHECK(t_ended_sessions == 1 && !openxr_session_running(&s),
         "STOPPING did not end it");
   CHECK(t_end_in_lock, "xrEndSession ran outside the lock");
   CHECK(!openxr_session_visible(&s), "visible while stopping");
   CHECK(!openxr_session_ended(&s), "STOPPING ended the session");

   /* Another session's events are not this one's. */
   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);
   t_push_state(T_OTHER, XR_SESSION_STATE_READY);
   t_push_space(T_OTHER, XR_REFERENCE_SPACE_TYPE_LOCAL);
   t_push_space(T_OTHER, XR_REFERENCE_SPACE_TYPE_STAGE);
   openxr_session_poll(&s);
   CHECK(t_begun == 0 && t_h_local == 0 && t_h_stage == 0,
         "acted on another session's events");

   /* LOCAL and STAGE moved for this session, each told apart; VIEW is
    * neither. */
   t_push_space(T_SESSION, XR_REFERENCE_SPACE_TYPE_STAGE);
   t_push_space(T_SESSION, XR_REFERENCE_SPACE_TYPE_LOCAL);
   t_push_space(T_SESSION, XR_REFERENCE_SPACE_TYPE_VIEW);
   openxr_session_poll(&s);
   CHECK(t_h_local == 1 && t_h_stage == 1,
         "local_changed %u times, stage_changed %u", t_h_local, t_h_stage);

   /* EXITING: ended once, exiting every time. */
   openxr_session_set_alive(&s, true);
   t_push_state(T_SESSION, XR_SESSION_STATE_EXITING);
   openxr_session_poll(&s);
   CHECK(t_h_ended == 1 && t_h_exiting == 1, "exiting: ended %u exiting %u",
         t_h_ended, t_h_exiting);
   CHECK(openxr_session_ended(&s) && !openxr_session_alive(&s) && !s.lost,
         "exit state");
   CHECK(t_ended_seq && t_ended_seq < t_exiting_seq,
         "ended (%u) not before exiting (%u)", t_ended_seq, t_exiting_seq);
   t_push_state(T_SESSION, XR_SESSION_STATE_EXITING);
   openxr_session_poll(&s);
   CHECK(t_h_ended == 1 && t_h_exiting == 2, "second EXITING");

   /* LOSS_PENDING ends it, not lost; instance loss is lost. */
   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);
   t_push_state(T_SESSION, XR_SESSION_STATE_LOSS_PENDING);
   openxr_session_poll(&s);
   CHECK(openxr_session_ended(&s) && !s.lost && t_h_ended == 1,
         "loss pending");
   t_push_instance_loss();
   openxr_session_poll(&s);
   CHECK(s.lost && t_h_ended == 1, "instance loss: lost %d ended %u", s.lost, t_h_ended);

   /* xrPollEvent reporting the instance lost, from FOCUSED. */
   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);
   openxr_session_set_alive(&s, true);
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   t_push_state(T_SESSION, XR_SESSION_STATE_SYNCHRONIZED);
   t_push_state(T_SESSION, XR_SESSION_STATE_VISIBLE);
   t_push_state(T_SESSION, XR_SESSION_STATE_FOCUSED);
   openxr_session_poll(&s);
   CHECK(openxr_session_focused(&s), "not focused before the loss");
   t_poll_result = XR_ERROR_INSTANCE_LOST;
   openxr_session_poll(&s);
   CHECK(s.lost && openxr_session_ended(&s),
         "XR_ERROR_INSTANCE_LOST not lost");

   /* Reset forgets an ended session but not a lost instance (the host
    * drops the runtime on it); destroy frees the spaces. */
   openxr_session_reset(&s);
   CHECK(!s.session && !openxr_session_ended(&s) && s.lost,
         "reset keeps lost");
   CHECK(t_spaces_destroyed == 2, "spaces destroyed: %u", t_spaces_destroyed);
   CHECK(retro_atomic_load_acquire_int(&s.state) == XR_SESSION_STATE_UNKNOWN,
         "state not cleared by reset");
   openxr_session_set_alive(&s, true);
   CHECK(!openxr_session_focused(&s), "focused after reset");

   /* A failed space leaves nothing behind. */
   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   t_space_fail_at = 2;
   t_reset_log();
   CHECK(!openxr_session_create(&s, &dummy_binding), "created without VIEW");
   CHECK(t_spaces_destroyed == 1 && !s.session && !s.local_space,
         "VIEW failure left spaces: %u destroyed", t_spaces_destroyed);
   CHECK(LOGGED("[OpenXR] No VIEW space ("), "log: %s", t_log);
   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   t_space_fail_at = 1;
   t_reset_log();
   CHECK(!openxr_session_create(&s, &dummy_binding), "created without LOCAL");
   CHECK(t_spaces_destroyed == 0 && !s.session && !s.local_space,
         "LOCAL failure left spaces");
   CHECK(LOGGED("[OpenXR] No LOCAL space ("), "log: %s", t_log);
   t_space_fail_at = 0;

   /* A runtime that lacks a session function. */
   t_session_fresh(&rt, &s);
   t_missing = "xrLocateSpace";
   CHECK(!openxr_session_load(&s, &rt), "loaded without xrLocateSpace");
   openxr_runtime_deinit(&rt);
}

static void test_swapchain(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   unsigned n = 0, idx[2] = { 0, 0 };
   int dummy_binding = 0;
   int cb_user       = 0;

   t_session_fresh(&rt, &s);
   t_sc_images = 3;
   t_sc_images_second = 0;
   t_sc_count_queries = t_sc_made = t_sc_destroyed = 0;
   t_sc_acquires = t_sc_releases = 0;
   t_sc_wait = XR_SUCCESS;
   openxr_session_load(&s, &rt);
   CHECK(openxr_session_create(&s, &dummy_binding), "session create");
   memset(&sc, 0, sizeof(sc));
   t_missing = "xrCreateSwapchain";
   CHECK(!openxr_swapchains_load(&sc, &rt, &s),
         "loaded without xrCreateSwapchain");
   t_missing = NULL;
   memset(&sc, 0, sizeof(sc));
   CHECK(openxr_swapchains_load(&sc, &rt, &s), "load");
   openxr_swapchains_list_formats(&sc);
   CHECK(openxr_swapchains_supports(&sc, 44)
         && !openxr_swapchains_supports(&sc, 37), "formats");
   /* More than are kept: the runtime's first, its preferred. */
   t_more_formats = 70;
   openxr_swapchains_list_formats(&sc);
   CHECK(     sc.num_formats == OPENXR_MAX_FORMATS
         && openxr_swapchains_supports(&sc, 44) && sc.formats[2] == 1002,
         "%u of 72 formats kept", sc.num_formats);
   t_more_formats = 0;

   t_locks = t_unlocks = 0;
   t_listed[0] = t_listed[1] = 0;
   t_sc_unlocked = 0;
   CHECK(openxr_slot_create(&sc, 1, 44, true, VIDEO_SCALE_PACK(640, 480), 2,
            t_images_cb, &cb_user, &n), "create");
   CHECK(t_sc_made == 2 && n == 3 && t_listed[0] == 3 && t_listed[1] == 3,
         "made %u n %u listed %u,%u", t_sc_made, n, t_listed[0], t_listed[1]);
   CHECK(t_locks == 1 && t_unlocks == 1, "create not under one lock");
   CHECK(!t_sc_unlocked, "%u create/callback calls outside the lock",
         t_sc_unlocked);
   CHECK(t_ci_format == 44 && t_ci_mutable
         && t_ci_width == 640 && t_ci_height == 480,
         "create info: format %d mutable %d %ux%u", (int)t_ci_format,
         (int)t_ci_mutable, t_ci_width, t_ci_height);
   CHECK(t_listed_sc[0] && t_listed_sc[1] && t_listed_sc[0] != t_listed_sc[1]
         && t_listed_sc[0] == sc.slots[1].swapchains[0]
         && t_listed_sc[1] == sc.slots[1].swapchains[1],
         "callback swapchains not one per layer");
   CHECK(t_listed_user[0] == &cb_user && t_listed_user[1] == &cb_user,
         "callback user lost");
   CHECK(LOGGED("[OpenXR] Slot 1: 640x480, 2 layer(s), 3 images.\n"),
         "log: %s", t_log);

   CHECK(!retro_atomic_load_acquire_int(&sc.slots[1].content),
         "content before release");
   t_locks = t_unlocks = 0;
   t_sc_wait = XR_TIMEOUT_EXPIRED;
   t_sc_wait_timeout = -1;
   CHECK(!openxr_slot_acquire(&sc, 1, idx), "acquired while the image is busy");
   CHECK(t_sc_wait_timeout == 0, "the image wait may block: timeout %lld",
         (long long)t_sc_wait_timeout);
   t_sc_wait = XR_SUCCESS;
   CHECK(openxr_slot_acquire(&sc, 1, idx), "acquire after wait");
   CHECK(idx[0] == 1 && idx[1] == 1, "acquired index %u,%u", idx[0], idx[1]);
   CHECK(t_sc_acquires == 2, "an image acquired twice: %u acquires",
         t_sc_acquires);
   openxr_slot_release(&sc, 1);
   CHECK(t_sc_releases == 2, "released %u of 2 layers", t_sc_releases);
   CHECK(!t_locks && !t_unlocks, "acquire or release took the lock");
   CHECK(retro_atomic_load_acquire_int(&sc.slots[1].content),
         "no content after release");
   openxr_slot_forget(&sc, 1);
   CHECK(!retro_atomic_load_acquire_int(&sc.slots[1].content),
         "forget kept content");
   CHECK(openxr_slot_acquire(&sc, 1, idx), "acquire again");
   t_sc_release = XR_ERROR_RUNTIME_FAILURE;
   openxr_slot_release(&sc, 1);
   t_sc_release = XR_SUCCESS;
   CHECK(!retro_atomic_load_acquire_int(&sc.slots[1].content),
         "content after a failed release");

   /* Layers that disagree on their image count: nothing kept. */
   t_sc_made = t_sc_destroyed = 0;
   t_sc_count_queries = 0;
   t_sc_images_second = 4;
   t_listed[0] = t_listed[1] = 0;
   t_reset_log();
   CHECK(!openxr_slot_create(&sc, 2, 44, false, VIDEO_SCALE_PACK(64, 64), 2,
            t_images_cb, NULL, &n), "made with mismatched layers");
   CHECK(t_listed[0] == 3 && !t_listed[1],
         "callback ran for the mismatched layer: %u,%u",
         t_listed[0], t_listed[1]);
   CHECK(LOGGED("[OpenXR] No 64x64 swapchain with 2 layer(s) ("),
         "log: %s", t_log);
   CHECK(t_sc_made == 2 && t_sc_destroyed == t_sc_made
         && !sc.slots[2].swapchains[0],
         "made %u destroyed %u", t_sc_made, t_sc_destroyed);
   t_sc_images_second = 0;

   /* The slot has the images the binding got, fewer than asked or none,
    * and a second layer must have as many. */
   t_reset_log();
   t_listed_got = 2;
   CHECK(openxr_slot_create(&sc, 3, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
            t_images_cb, NULL, &n) && n == 2,
         "a binding that got 2 of 3 images: %u", n);
   CHECK(LOGGED("[OpenXR] Slot 3: 64x64, 1 layer(s), 2 images.\n"),
         "log: %s", t_log);
   t_listed_got = 0;
   CHECK(openxr_slot_create(&sc, 3, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
            t_images_cb, NULL, &n) && n == 0,
         "a binding that got none: %u", n);
   t_listed_got = 2;
   t_listed[0] = t_listed[1] = 0;
   CHECK(!openxr_slot_create(&sc, 4, 44, false, VIDEO_SCALE_PACK(64, 64), 2,
            t_images_cb, NULL, &n) && !sc.slots[4].swapchains[0],
         "a second layer of 3 after a first of 2");
   CHECK(t_listed[0] == 3 && !t_listed[1],
         "callback ran for the second layer: %u,%u",
         t_listed[0], t_listed[1]);
   t_listed_got = -1;
   openxr_slot_destroy(&sc, 3);

   t_sc_destroyed = 0;
   t_sc_unlocked  = 0;
   openxr_slot_destroy(&sc, 1);
   CHECK(!t_sc_unlocked, "destroy outside the lock");
   CHECK(t_sc_destroyed == 2 && !sc.slots[1].swapchains[0], "destroy");
   openxr_session_destroy(&s);
   openxr_runtime_deinit(&rt);
}

static bool t_near(float a, float b)
{
   float d = a - b;
   return d < 1e-4f && d > -1e-4f;
}

/* A level pose, turned about y by the quaternion's y and w. */
static bool t_pose_is(const video_xr_pose_t *p, float qy, float qw,
      float x, float y, float z)
{
   return t_near(p->orientation.x, 0.0f) && t_near(p->orientation.y, qy)
       && t_near(p->orientation.z, 0.0f) && t_near(p->orientation.w, qw)
       && t_near(p->position.x, x) && t_near(p->position.y, y)
       && t_near(p->position.z, z);
}

static void test_frame(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   video_xr_quad_set_t set, got;
   video_xr_pose_t anchor;
   unsigned n = 0, i;
   int recenters;
   int tick_key;
   int dummy_binding = 0;
   int cb_user       = 0;

   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);
   memset(&sc, 0, sizeof(sc));
   openxr_swapchains_load(&sc, &rt, &s);
   memset(&f, 0, sizeof(f));
   CHECK(openxr_frame_load(&f, &rt, &s, &sc), "load");
   openxr_frame_session_created(&f);

   /* What is published is what is read back, whole. */
   memset(&set, 0, sizeof(set));
   set.num_quads                = 2;
   set.quads[0].slot            = 1;
   set.quads[0].width           = 1.25f;
   set.quads[1].slot            = 2;
   set.quads[1].pose.position.z = -1.5f;
   openxr_frame_publish(&f, &set);
   openxr_frame_get_quads(&f, &got);
   CHECK(!memcmp(&set, &got, sizeof(set)), "quads not read back whole");

   /* Only quads whose slot has content become layers. */
   openxr_slot_create(&sc, 1, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   openxr_slot_create(&sc, 2, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[1].content, 1);
   {
      XrCompositionLayerQuad layers[VIDEO_XR_MAX_QUADS];
      const XrCompositionLayerBaseHeader *ptrs[VIDEO_XR_MAX_QUADS];
      CHECK(openxr_frame_layers(&f, layers, ptrs) == 1,
            "a quad without content shown");
   }

   /* A tick every interval headset frames. */
   openxr_frame_set_pacing(&f, 3);
   for (i = 0; i < 7; i++)
      openxr_frame_tick(&f);
   CHECK(retro_atomic_load_acquire_int(&f.tick_seq) == 2,
         "%d ticks in 7 frames at interval 3",
         retro_atomic_load_acquire_int(&f.tick_seq));
   /* A waiter from here on: the frames below make the next tick. */
   tick_key = retro_eventcount_prepare_wait(&f.tick);

   /* One frame: wait, begin, end, with layers only while visible. */
   openxr_session_set_alive(&s, true);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_SYNCHRONIZED);
   t_locks = t_unlocks = 0;
   openxr_frame_run(&f);
   CHECK(t_ends >= 1 && t_last_layer_count == 0, "layers while synchronized");
   CHECK(t_locks == 1 && t_unlocks == 1 && !t_frame_unlocked,
         "begin and end not under one lock");
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 1, "%u layers while focused", t_last_layer_count);
   CHECK(openxr_frame_predicted_time(&f) > 0, "predicted time not published");

   /* The host's layers follow the screens. */
   f.extra_layers = t_extra_layers;
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 2 && t_extra_cap == OPENXR_MAX_EXTRA_LAYERS,
         "%u layers with the host's, cap %u", t_last_layer_count, t_extra_cap);

   /* Each frame counts toward the tick: the ninth at interval 3 makes
    * one, and it wakes the waiter. */
   CHECK(retro_atomic_load_acquire_int(&f.tick_seq) == 3,
         "%d ticks in 10 frames at interval 3",
         retro_atomic_load_acquire_int(&f.tick_seq));
   CHECK(retro_eventcount_commit_wait_timeout(&f.tick, tick_key, 0),
         "the tick woke no waiter");

   /* The menu is premultiplied; a stereo screen's layers go one to each
    * eye. */
   openxr_slot_create(&sc, 3, 44, false, VIDEO_SCALE_PACK(64, 64), 2,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[3].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads      = 3;
   set.quads[0].kind  = VIDEO_XR_QUAD_MENU;
   set.quads[0].slot  = 1;
   set.quads[1].eye   = VIDEO_XR_EYE_LEFT;
   set.quads[1].slot  = 3;
   set.quads[2].eye   = VIDEO_XR_EYE_RIGHT;
   set.quads[2].slot  = 3;
   set.quads[2].layer = 1;
   openxr_frame_publish(&f, &set);
   {
      XrCompositionLayerQuad layers[VIDEO_XR_MAX_QUADS];
      const XrCompositionLayerBaseHeader *ptrs[VIDEO_XR_MAX_QUADS];
      CHECK(openxr_frame_layers(&f, layers, ptrs) == 3, "menu and stereo");
      CHECK(layers[0].layerFlags
               == XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT
            && layers[0].eyeVisibility == XR_EYE_VISIBILITY_BOTH,
            "menu: flags %x eyes %d", (unsigned)layers[0].layerFlags,
            (int)layers[0].eyeVisibility);
      CHECK(!layers[1].layerFlags
            && layers[1].eyeVisibility == XR_EYE_VISIBILITY_LEFT
            && layers[1].subImage.swapchain == sc.slots[3].swapchains[0]
            && !layers[2].layerFlags
            && layers[2].eyeVisibility == XR_EYE_VISIBILITY_RIGHT
            && layers[2].subImage.swapchain == sc.slots[3].swapchains[1],
            "stereo screen: eyes %d,%d", (int)layers[1].eyeVisibility,
            (int)layers[2].eyeVisibility);
   }

   /* A recenter makes the tracked head's level pose the anchor; the
    * runtime's recenter and a new session put it back and drop one
    * asked for. The head here is turned a quarter left: level already. */
   t_head_tracked        = true;
   memset(&t_head, 0, sizeof(t_head));
   t_head.orientation.y  = 0.70710678f;
   t_head.orientation.w  = 0.70710678f;
   t_head.position.x     = 0.25f;
   t_head.position.y     = 1.5f;
   t_head.position.z     = -0.5f;
   recenters = openxr_frame_recenters(&f);
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.70710678f, 0.70710678f, 0.25f, 1.5f, -0.5f)
         && !retro_atomic_load_acquire_int(&f.recenter),
         "recentered at %.2f,%.2f,%.2f yaw %.3f", anchor.position.x,
         anchor.position.y, anchor.position.z, anchor.orientation.y);
   CHECK(openxr_frame_recenters(&f) == recenters + 1,
         "a recenter was not counted");
   openxr_frame_request_recenter(&f);
   openxr_frame_local_changed(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
         "the runtime's recenter kept the anchor");
   CHECK(openxr_frame_recenters(&f) == recenters + 2,
         "the runtime's recenter was not counted");
   openxr_frame_run(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
         "a recenter asked before the runtime's ran after it");
   /* A head the runtime lost, though it answers, moves nothing; nor
    * does one found without its position. */
   t_head_lost = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
         "recentered on a head the runtime lost");
   t_head_lost = XR_SPACE_LOCATION_POSITION_VALID_BIT;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
         "recentered on a head without its position");
   t_head_lost = 0;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.70710678f, 0.70710678f, 0.25f, 1.5f, -0.5f),
         "not recentered again");
   openxr_frame_request_recenter(&f);
   openxr_frame_session_created(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
         "a new session kept the anchor");
   openxr_frame_run(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
         "a recenter asked before a new session ran in it");
   /* A head the runtime cannot find moves nothing, and counts nothing. */
   t_head_tracked = false;
   recenters      = openxr_frame_recenters(&f);
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   CHECK(openxr_frame_recenters(&f) == recenters,
         "a recenter without a head was counted");

   openxr_frame_deinit(&f);
   openxr_slot_destroy(&sc, 1);
   openxr_slot_destroy(&sc, 2);
   openxr_slot_destroy(&sc, 3);
   openxr_session_destroy(&s);
   openxr_runtime_deinit(&rt);
}

static void t_frame_fresh(openxr_runtime_t *rt, openxr_session_t *s,
      openxr_swapchains_t *sc, openxr_frame_t *f)
{
   static int binding;
   t_session_fresh(rt, s);
   openxr_session_load(s, rt);
   openxr_session_create(s, &binding);
   memset(sc, 0, sizeof(*sc));
   openxr_swapchains_load(sc, rt, s);
   memset(f, 0, sizeof(*f));
   openxr_frame_load(f, rt, s, sc);
   openxr_frame_session_created(f);
}

static void t_frame_free(openxr_runtime_t *rt, openxr_session_t *s,
      openxr_frame_t *f)
{
   openxr_frame_deinit(f);
   openxr_session_destroy(s);
   openxr_runtime_deinit(rt);
}

/* The XR thread for one wait: a tick once the waiter parks, nothing if
 * it returns first. */
static retro_atomic_int_t t_ticker_done, t_ticker_ticked;
static void t_ticker(void *data)
{
   openxr_frame_t *f = (openxr_frame_t*)data;
   while (!retro_atomic_load_acquire_int(&t_ticker_done))
   {
      if (retro_atomic_load_acquire_int(&f->tick.waiters))
      {
         openxr_frame_tick(f);
         retro_atomic_store_release_int(&t_ticker_ticked, 1);
         return;
      }
      retro_sleep(1);
   }
}

static bool t_wait_with_ticker(openxr_frame_t *f, bool *ticked)
{
   bool woke;
   sthread_t *thread;
   retro_atomic_store_release_int(&t_ticker_done, 0);
   retro_atomic_store_release_int(&t_ticker_ticked, 0);
   openxr_frame_pace_skip(f);
   thread = sthread_create(t_ticker, f);
   CHECK(thread, "no ticker thread");
   woke   = openxr_frame_wait_tick(f, 10000000000LL);
   retro_atomic_store_release_int(&t_ticker_done, 1);
   if (thread)
      sthread_join(thread);
   *ticked = retro_atomic_load_acquire_int(&t_ticker_ticked) != 0;
   return woke;
}

static void test_frame_pacing(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   bool ticked = false;

   t_frame_fresh(&rt, &s, &sc, &f);
   openxr_session_set_alive(&s, true);
   retro_atomic_store_release_int(&f.period_ns, 13888889);
   openxr_frame_set_pacing(&f, 1);

   /* The core runs on the clock while the headset does not show the
    * session, on its ticks while it does. */
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_SYNCHRONIZED);
   openxr_frame_pace_wait(&f);
   CHECK(f.pace_mode == 2, "hidden: pace mode %u", f.pace_mode);
   CHECK(LOGGED("[OpenXR] Pacing on the clock while the headset does not show the session.\n"),
         "log: %s", t_log);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   openxr_frame_tick(&f);
   openxr_frame_pace_wait(&f);
   CHECK(f.pace_mode == 1 && !f.tick_late, "shown: pace mode %u late %d",
         f.pace_mode, (int)f.tick_late);
   CHECK(LOGGED("[OpenXR] Pacing on the headset's frames.\n"),
         "log: %s", t_log);

   /* A shown session's waiter sleeps until a tick wakes it; a hidden
    * one's returns without one. */
   CHECK(t_wait_with_ticker(&f, &ticked) && ticked,
         "a tick did not end the wait");
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_SYNCHRONIZED);
   CHECK(!t_wait_with_ticker(&f, &ticked) && !ticked,
         "waited for a tick the hidden headset would not send");

   /* The session's frames reach the headset from the XR thread's start
    * to its stop, which joins it. The session never began, so the
    * thread only polls. */
   openxr_session_set_alive(&s, false);
   CHECK(openxr_frame_start(&f), "start");
   CHECK(f.thread && openxr_session_alive(&s), "not alive after start");
   openxr_frame_stop(&f);
   CHECK(!f.thread && !openxr_session_alive(&s),
         "after stop: thread %p alive %d", (void*)f.thread,
         (int)openxr_session_alive(&s));
   CHECK(!openxr_session_ended(&s), "a stopped XR thread ended the session");

   t_frame_free(&rt, &s, &f);
}

static unsigned t_logged_times(const char *line)
{
   unsigned n    = 0;
   const char *p = t_log;
   while ((p = strstr(p, line)))
   {
      n++;
      p += strlen(line);
   }
   return n;
}

/* A runtime may report the session or the instance lost from a frame
 * call, without an event: the session ends once, logged once. */
static void test_frame_lost(void)
{
   static const struct
   {
      XrResult wait, end;
      const char *line;
      bool lost;
   } cases[] = {
      { XR_ERROR_SESSION_LOST,  XR_SUCCESS,
         "[OpenXR] xrWaitFrame failed (-17).\n", false },
      { XR_SUCCESS, XR_ERROR_SESSION_LOST,
         "[OpenXR] xrEndFrame failed (-17).\n", false },
      { XR_ERROR_INSTANCE_LOST, XR_SUCCESS,
         "[OpenXR] xrWaitFrame failed (-13).\n", true },
      { XR_SUCCESS, XR_ERROR_INSTANCE_LOST,
         "[OpenXR] xrEndFrame failed (-13).\n", true }
   };
   unsigned i;

   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      openxr_runtime_t rt;
      openxr_session_t s;
      openxr_swapchains_t sc;
      openxr_frame_t f;

      t_frame_fresh(&rt, &s, &sc, &f);
      t_push_state(T_SESSION, XR_SESSION_STATE_READY);
      openxr_session_poll(&s);
      openxr_session_set_alive(&s, true);
      t_reset_log();
      t_wait_frame_res = cases[i].wait;
      t_end_frame_res  = cases[i].end;
      openxr_frame_run(&f);
      openxr_frame_run(&f);
      t_wait_frame_res = XR_SUCCESS;
      t_end_frame_res  = XR_SUCCESS;
      CHECK(t_h_ended == 1, "%u: ended %u times", i, t_h_ended);
      CHECK(     !openxr_session_alive(&s)
            && !openxr_session_running(&s) && s.lost == cases[i].lost,
            "%u: alive %d running %d lost %d", i,
            (int)openxr_session_alive(&s),
            (int)openxr_session_running(&s), (int)s.lost);
      CHECK(t_logged_times(cases[i].line) == 1, "%u: logged %u times: %s",
            i, t_logged_times(cases[i].line), t_log);
      t_frame_free(&rt, &s, &f);
   }
}

static void test_track_space(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   XrPosef origin;
   XrSpace first;
   enum retro_vr_reference_space space;
   int dummy_binding = 0;

   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);
   memset(&origin, 0, sizeof(origin));
   origin.orientation.w = 1.0f;
   origin.position.z    = -0.5f;
   t_stage_missing      = false;

   /* LOCAL at the origin given, swapped in under the lock. */
   t_locks = t_unlocks = 0;
   space   = RETRO_VR_REFERENCE_SPACE_LOCAL;
   CHECK(openxr_session_swap_track_space(&s, &space, &origin,
            NULL, NULL), "LOCAL");
   CHECK(s.track_space && t_space_type == XR_REFERENCE_SPACE_TYPE_LOCAL
         && t_near(t_space_pose.position.z, -0.5f)
         && space == RETRO_VR_REFERENCE_SPACE_LOCAL,
         "LOCAL track space: type %d z %.2f", (int)t_space_type,
         t_space_pose.position.z);
   CHECK(retro_atomic_load_acquire_int(&s.track_seq) == 1,
         "seq %d after the first", retro_atomic_load_acquire_int(&s.track_seq));
   CHECK(t_locks == 1 && t_unlocks == 1, "not swapped under the lock");
   first = s.track_space;

   /* STAGE replaces it; the old one goes under the lock. */
   t_spaces_destroyed = 0;
   space              = RETRO_VR_REFERENCE_SPACE_STAGE;
   CHECK(openxr_session_swap_track_space(&s, &space, &origin,
            NULL, NULL), "STAGE");
   CHECK(t_space_type == XR_REFERENCE_SPACE_TYPE_STAGE
         && space == RETRO_VR_REFERENCE_SPACE_STAGE && s.track_space != first,
         "STAGE not granted");
   CHECK(     t_near(t_space_pose.position.z, 0.0f)
         && t_near(t_space_pose.orientation.w, 1.0f),
         "STAGE made away from the room's origin: z %.2f",
         t_space_pose.position.z);
   CHECK(t_spaces_destroyed == 1 && t_space_destroyed_last == first
         && t_space_destroy_in_lock,
         "the old space: %u destroyed, in the lock %d", t_spaces_destroyed,
         (int)t_space_destroy_in_lock);
   CHECK(retro_atomic_load_acquire_int(&s.track_seq) == 2, "seq after STAGE");

   /* No STAGE: LOCAL, and what was granted is written back. */
   t_stage_missing = true;
   t_reset_log();
   space           = RETRO_VR_REFERENCE_SPACE_STAGE;
   CHECK(openxr_session_swap_track_space(&s, &space, &origin,
            NULL, NULL), "STAGE missing");
   CHECK(space == RETRO_VR_REFERENCE_SPACE_LOCAL
         && t_space_type == XR_REFERENCE_SPACE_TYPE_LOCAL, "no LOCAL fallback");
   CHECK(t_near(t_space_pose.position.z, -0.5f),
         "the LOCAL fallback not at the origin given: z %.2f",
         t_space_pose.position.z);
   CHECK(LOGGED("[OpenXR] No STAGE space: tracked content is seated.\n"),
         "log: %s", t_log);
   t_stage_missing = false;

   /* No STAGE and no LOCAL: nothing changes, nothing is logged. */
   first              = s.track_space;
   t_space_fail_at    = t_spaces_made + 1;
   t_spaces_destroyed = 0;
   t_locks = t_unlocks = 0;
   t_reset_log();
   space              = RETRO_VR_REFERENCE_SPACE_STAGE;
   CHECK(!openxr_session_swap_track_space(&s, &space, &origin,
            NULL, NULL),
         "made from nothing");
   CHECK(s.track_space == first
         && retro_atomic_load_acquire_int(&s.track_seq) == 3,
         "a failure replaced the space");
   CHECK(space == RETRO_VR_REFERENCE_SPACE_STAGE,
         "a failure wrote the space back");
   CHECK(!t_spaces_destroyed, "a failure destroyed a space");
   CHECK(!t_locks, "a failure took the lock");
   CHECK(!LOGGED("[OpenXR] No STAGE space"), "a failure logged: %s", t_log);
   t_space_fail_at = 0;

   /* Destroy takes it with LOCAL and VIEW. */
   t_spaces_destroyed = 0;
   openxr_session_destroy(&s);
   CHECK(t_spaces_destroyed == 3 && !s.track_space,
         "destroyed %u spaces", t_spaces_destroyed);
   openxr_runtime_deinit(&rt);
}

static void test_tracking(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_tracking_t t;
   struct retro_vr_eye_state eye;
   XrSpace base      = (XrSpace)(uintptr_t)0x77;
   int dummy_binding = 0;

   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);

   memset(t_view, 0, sizeof(t_view));
   t_view[0].pose.orientation.w = 1.0f;
   t_view[0].pose.position.x    = -0.032f;
   t_view[0].fov.angleLeft      = -0.78539816f;
   t_view[0].fov.angleRight     =  0.5f;
   t_view[0].fov.angleUp        =  0.6f;
   t_view[0].fov.angleDown      = -0.7f;
   t_view[1]                    = t_view[0];
   t_view[1].pose.position.x    =  0.032f;
   memset(&t_head, 0, sizeof(t_head));
   t_head.orientation.w         = 1.0f;
   t_head.position.y            = 1.25f;

   /* Both eyes and the head, in the base asked for, at the time asked. */
   t_views_tracked = true;
   t_head_tracked  = true;
   CHECK(openxr_tracking_sample(&t, &s, base, 5000), "sample");
   CHECK(t.valid && t_located_in == base && t_located_at == 5000
         && t_head_base == base && t_head_space == s.view_space,
         "not located in the base asked for, at the time asked");
   CHECK(t_near(t.views[1].pose.position.x, 0.032f), "right eye");
   CHECK(     (t.head.flags & RETRO_VR_HEAD_POSE_ORIENTATION_VALID)
         && (t.head.flags & RETRO_VR_HEAD_POSE_POSITION_VALID)
         && t_near(t.head.position[1], 1.25f),
         "head: flags %x y %.2f", (unsigned)t.head.flags, t.head.position[1]);

   /* An eye as the core gets it: the angles as tangents, all four
    * positive (libretro.h). */
   openxr_tracking_eye(&t.views[0], &eye);
   CHECK(t_near(eye.fov_tan[0], 1.0f) && t_near(eye.fov_tan[1], tanf(0.5f))
         && t_near(eye.fov_tan[2], tanf(0.6f))
         && t_near(eye.fov_tan[3], tanf(0.7f))
         && t_near(eye.position[0], -0.032f)
         && t_near(eye.orientation[3], 1.0f),
         "eye: tangents %.3f %.3f %.3f %.3f", eye.fov_tan[0], eye.fov_tan[1],
         eye.fov_tan[2], eye.fov_tan[3]);

   /* Untracked eyes or head: nothing valid. */
   t_views_tracked = false;
   CHECK(!openxr_tracking_sample(&t, &s, base, 5000) && !t.valid,
         "untracked eyes");
   t_views_tracked = true;
   t_head_tracked  = false;
   CHECK(!openxr_tracking_sample(&t, &s, base, 5000) && !t.valid,
         "untracked head");
   t_head_tracked  = true;

   /* A runtime that lost them still answers, with the flags off; a head
    * without its position has its orientation alone. */
   t_views_lost = XR_VIEW_STATE_ORIENTATION_VALID_BIT
      | XR_VIEW_STATE_POSITION_VALID_BIT;
   CHECK(!openxr_tracking_sample(&t, &s, base, 5000) && !t.valid,
         "eyes the runtime lost");
   t_views_lost = 0;
   t_head_lost  = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;
   CHECK(!openxr_tracking_sample(&t, &s, base, 5000) && !t.valid,
         "a head the runtime lost");
   t_head_lost  = XR_SPACE_LOCATION_POSITION_VALID_BIT;
   CHECK(     openxr_tracking_sample(&t, &s, base, 5000)
         && t.head.flags == RETRO_VR_HEAD_POSE_ORIENTATION_VALID,
         "a head without its position: flags %x", (unsigned)t.head.flags);
   t_head_lost  = 0;

   /* No base or no time: the runtime is not asked. */
   t_located_at = 0;
   CHECK(!openxr_tracking_sample(&t, &s, XR_NULL_HANDLE, 5000)
         && !openxr_tracking_sample(&t, &s, base, 0) && !t_located_at,
         "sampled without a base or a time");

   t_views_tracked = false;
   t_head_tracked  = false;
   openxr_session_destroy(&s);
   openxr_runtime_deinit(&rt);
}

static void test_projection(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_projection_t p;
   video_xr_quad_set_t set;
   XrPosef origin;
   openxr_slot_t *slot;
   enum retro_vr_reference_space space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   unsigned n  = 0;
   int cb_user = 0;

   t_frame_fresh(&rt, &s, &sc, &f);
   memset(&origin, 0, sizeof(origin));
   origin.orientation.w = 1.0f;
   openxr_session_swap_track_space(&s, &space, &origin, NULL, NULL);
   CHECK(openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
            VIDEO_SCALE_PACK(1728, 1824), 2, t_images_cb, &cb_user, &n),
         "projection slot");
   slot = &sc.slots[OPENXR_PROJECTION_SLOT];
   openxr_session_set_alive(&s, true);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);

   memset(&p, 0, sizeof(p));
   p.active                = true;
   p.width                 = 1600;
   p.height                = 4000;   /* taller than the image */
   p.pose[0].orientation.w = 1.0f;
   p.pose[0].position.x    = -0.032f;
   p.pose[1].orientation.w = 1.0f;
   p.pose[1].position.x    =  0.032f;
   p.fov[0].angleLeft      = -0.9f;
   p.fov[1].angleRight     =  0.8f;
   p.track_seq             = retro_atomic_load_acquire_int(&s.track_seq);
   openxr_frame_publish_projection(&f, &p);

   /* No image released yet: no layer. */
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 0, "a projection without content: %u layers",
         t_last_layer_count);

   /* Released: each eye from its own swapchain, in the track space, as
    * drawn, its rectangle inside the image. */
   retro_atomic_store_release_int(&slot->content, 1);
   memset(&t_proj, 0, sizeof(t_proj));
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 1
         && t_proj.type == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_proj.viewCount == 2 && t_proj.space == s.track_space,
         "projection: %u layers, type %d", t_last_layer_count, (int)t_proj.type);
   CHECK(t_proj_views[0].subImage.swapchain == slot->swapchains[0]
         && t_proj_views[1].subImage.swapchain == slot->swapchains[1],
         "an eye from the other's swapchain");
   CHECK(t_proj_views[0].subImage.imageRect.extent.width == 1600
         && t_proj_views[0].subImage.imageRect.extent.height == 1824
         && t_proj_views[1].subImage.imageRect.extent.height == 1824,
         "rectangle %dx%d", (int)t_proj_views[0].subImage.imageRect.extent.width,
         (int)t_proj_views[0].subImage.imageRect.extent.height);
   CHECK(t_near(t_proj_views[0].pose.position.x, -0.032f)
         && t_near(t_proj_views[1].pose.position.x, 0.032f)
         && t_near(t_proj_views[0].fov.angleLeft, -0.9f)
         && t_near(t_proj_views[1].fov.angleRight, 0.8f),
         "poses or fields of view not as drawn");

   /* Behind the screens and the host's layers. */
   CHECK(openxr_slot_create(&sc, 1, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
            t_images_cb, &cb_user, &n), "quad slot");
   retro_atomic_store_release_int(&sc.slots[1].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads     = 1;
   set.quads[0].kind = VIDEO_XR_QUAD_MENU;
   set.quads[0].slot = 1;
   openxr_frame_publish(&f, &set);
   f.extra_layers = t_extra_layers;
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 3
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_layer_types[1] == XR_TYPE_COMPOSITION_LAYER_QUAD,
         "order: %u layers, the first %d", t_last_layer_count,
         (int)t_layer_types[0]);
   f.extra_layers = NULL;

   /* A new track space leaves out what was drawn in the last, until
    * the eyes are drawn in it. */
   openxr_session_swap_track_space(&s, &space, &origin, NULL, NULL);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 1
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_QUAD,
         "a projection from the replaced space was shown");
   p.track_seq = retro_atomic_load_acquire_int(&s.track_seq);
   openxr_frame_publish_projection(&f, &p);
   memset(&t_proj, 0, sizeof(t_proj));
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 2
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_proj.space == s.track_space,
         "the redrawn projection was left out: %u layers, the first %d",
         t_last_layer_count, (int)t_layer_types[0]);

   /* Cleared: gone. */
   openxr_frame_clear_projection(&f);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 1, "a cleared projection was shown");

   /* Each guard alone: only the screen's layer is left. */
   p.active = false;
   openxr_frame_publish_projection(&f, &p);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 1,
         "a projection that is not active was shown");
   p.active = true;
   p.width  = 0;
   openxr_frame_publish_projection(&f, &p);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 1, "a projection without width was shown");
   p.width  = 1600;
   openxr_frame_publish_projection(&f, &p);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 2, "the valid projection was left out");

   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);
   CHECK(openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
            VIDEO_SCALE_PACK(1728, 1824), 1, t_images_cb, &cb_user, &n),
         "one-layer projection slot");
   retro_atomic_store_release_int(
         &sc.slots[OPENXR_PROJECTION_SLOT].content, 1);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 1,
         "a projection with one eye's layer was shown");

   openxr_slot_destroy(&sc, 1);
   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);
   t_frame_free(&rt, &s, &f);

   /* A session with no track space shows no projection. */
   t_frame_fresh(&rt, &s, &sc, &f);
   CHECK(openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
            VIDEO_SCALE_PACK(1728, 1824), 2, t_images_cb, &cb_user, &n),
         "projection slot without a track space");
   retro_atomic_store_release_int(
         &sc.slots[OPENXR_PROJECTION_SLOT].content, 1);
   openxr_session_set_alive(&s, true);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   p.track_seq = retro_atomic_load_acquire_int(&s.track_seq);
   openxr_frame_publish_projection(&f, &p);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 0,
         "a projection without a track space was shown");
   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);
   t_frame_free(&rt, &s, &f);
}

static void test_host(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   openxr_projection_t p;
   struct retro_vr_eye_state eyes[2];
   struct retro_vr_head_pose head;
   video_xr_pose_t anchor;
   enum retro_vr_reference_space space;
   unsigned w     = 0;
   unsigned ht    = 0;
   unsigned made  = 0;
   uint32_t flags = 0;

   t_frame_fresh(&rt, &s, &sc, &f);
   openxr_host_init(&h, &rt, &s, &f);
   openxr_host_session_created(&h);
   openxr_session_set_alive(&s, true);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   memset(t_view, 0, sizeof(t_view));
   t_view[0].pose.orientation.w = 1.0f;
   t_view[0].pose.position.x    = -0.032f;
   t_view[0].fov.angleLeft      = -0.9f;
   t_view[0].fov.angleRight     =  0.8f;
   t_view[0].fov.angleUp        =  0.7f;
   t_view[0].fov.angleDown      = -0.6f;
   t_view[1]                    = t_view[0];
   t_view[1].pose.position.x    =  0.032f;
   memset(&t_head, 0, sizeof(t_head));
   t_head.orientation.w         = 1.0f;
   t_head.position.y            = 1.25f;
   t_views_tracked              = true;
   t_head_tracked               = true;

   CHECK(openxr_host_eye_size(&h, &w, &ht) && w == 1728 && ht == 1824,
         "eye size %ux%u, want the runtime's 1728x1824", w, ht);

   /* No space asked for yet: a seated one is made for the first sample,
    * which is for the frame's display time plus the measured period. */
   openxr_frame_run(&f);
   retro_atomic_store_release_int(&f.period_ns, 13888889);
   CHECK(openxr_host_sample(&h, &flags) && !flags, "first sample: flags %u",
         (unsigned)flags);
   CHECK(     s.track_space && t_space_type == XR_REFERENCE_SPACE_TYPE_LOCAL
         && t_located_in == s.track_space
         && t_located_at == openxr_frame_predicted_time(&f) + 13888889,
         "sampled at %lld, want the predicted time plus a period",
         (long long)t_located_at);
   CHECK(openxr_host_head_pose(&h, &head) && t_near(head.position[1], 1.25f),
         "head not as sampled");

   /* The views the core was given are the projection's. */
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(!p.active, "a projection before the core was given its eyes");
   CHECK(     openxr_host_eye_state(&h, eyes)
         && t_near(eyes[1].position[0], 0.032f), "eyes not as sampled");
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(     p.active && p.width == 400 && p.height == 240
         && p.track_seq == retro_atomic_load_acquire_int(&s.track_seq)
         && t_near(p.pose[0].position.x, -0.032f)
         && t_near(p.fov[1].angleRight, 0.8f),
         "the projection is not of the views the core was given");

   /* Without a measured period, the predicted time alone. */
   retro_atomic_store_release_int(&f.period_ns, 0);
   openxr_host_sample(&h, &flags);
   CHECK(t_located_at == openxr_frame_predicted_time(&f),
         "sampled at %lld without a period", (long long)t_located_at);

   /* A recenter: the seated space is made again at the new anchor and
    * the core is told, once; the views it was given, and the projection
    * shown from them, go on there with their poses. */
   openxr_host_projection(&h, 400, 240, &p);
   openxr_frame_publish_projection(&f, &p);
   t_head.orientation.y = 0.70710678f;
   t_head.orientation.w = 0.70710678f;
   t_head.position.x    = 0.25f;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_frame_get_anchor(&f, &anchor);
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED), "a recenter was not told");
   CHECK(     t_near(t_space_pose.position.x, anchor.position.x)
         && t_near(t_space_pose.orientation.y, anchor.orientation.y),
         "the seated space was not made at the anchor");
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(     openxr_host_has_given(&h)
         && p.track_seq == retro_atomic_load_acquire_int(&s.track_seq)
         && t_near(p.pose[0].position.x, -0.032f),
         "the views the core was given did not go on in the new space");
   openxr_seq_read(&f.proj_seq, f.proj_words, &p, sizeof(p));
   CHECK(     p.active
         && p.track_seq == retro_atomic_load_acquire_int(&s.track_seq)
         && t_near(p.pose[0].position.x, -0.032f),
         "the projection shown did not go on in the new space");
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");
   /* A sample that fails still tells of the recenter it took, for the
    * caller to keep until poses answer. */
   t_head.position.x = 0.5f;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   t_views_tracked   = false;
   flags             = 0;
   CHECK(     !openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED),
         "a failed sample dropped its recenter: flags %u", (unsigned)flags);
   t_views_tracked   = true;

   /* Standing: the room's origin; a recenter is told but moves nothing;
    * a STAGE move is told, once. */
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   CHECK(     openxr_host_set_space(&h, &space, &w, &ht)
         && space == RETRO_VR_REFERENCE_SPACE_STAGE && w == 1728 && ht == 1824
         && t_space_type == XR_REFERENCE_SPACE_TYPE_STAGE
         && t_near(t_space_pose.orientation.w, 1.0f)
         && t_near(t_space_pose.position.x, 0.0f), "STAGE not as asked");
   made  = t_spaces_made;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made,
         "a recenter of standing content: flags %u, %u spaces made",
         (unsigned)flags, t_spaces_made - made);
   openxr_host_stage_changed(&h);
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED), "a STAGE move was not told");
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a STAGE move told twice");

   /* Seated content hears nothing of STAGE. */
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_stage_changed(&h);
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED),
         "seated content was told of a STAGE move");

   /* A new session starts with nothing sampled or given. */
   openxr_host_session_created(&h);
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(!p.active && !openxr_host_head_pose(&h, &head),
         "a new session kept the last one's poses");

   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

/* A host over a stand-in runtime that tracks both eyes and the head. */
static void t_host_up(openxr_runtime_t *rt, openxr_session_t *s,
      openxr_swapchains_t *sc, openxr_frame_t *f, openxr_host_t *h)
{
   t_frame_fresh(rt, s, sc, f);
   openxr_host_init(h, rt, s, f);
   openxr_host_session_created(h);
   openxr_session_set_alive(s, true);
   retro_atomic_store_release_int(&s->state, XR_SESSION_STATE_FOCUSED);
   memset(t_view, 0, sizeof(t_view));
   t_view[0].pose.orientation.w = 1.0f;
   t_view[1].pose.orientation.w = 1.0f;
   memset(&t_head, 0, sizeof(t_head));
   t_head.orientation.w         = 1.0f;
   t_views_tracked              = true;
   t_head_tracked               = true;
}

static void t_host_recenter_at(openxr_frame_t *f, float x)
{
   t_head.orientation.y = 0.70710678f;
   t_head.orientation.w = 0.70710678f;
   t_head.position.x    = x;
   openxr_frame_request_recenter(f);
   openxr_frame_run(f);
}

static void test_host_remake(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   video_xr_pose_t anchor;
   enum retro_vr_reference_space space;
   int binding;
   unsigned w     = 0;
   unsigned ht    = 0;
   unsigned made  = 0;
   uint32_t flags = 0;

   /* STAGE asked, none granted: the content is seated, so it is made at
    * the anchor and remade on every recenter, and STAGE is not asked again. */
   t_host_up(&rt, &s, &sc, &f, &h);
   t_stage_missing = true;
   t_stage_asked   = 0;
   t_located_in    = XR_NULL_HANDLE;
   retro_atomic_store_release_int(&f.period_ns, 13888889);
   CHECK(!openxr_host_sample(&h, &flags) && !t_located_in,
         "a zero predicted time was located");
   openxr_frame_run(&f);
   t_host_recenter_at(&f, 0.25f);
   openxr_frame_get_anchor(&f, &anchor);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   CHECK(     openxr_host_set_space(&h, &space, &w, &ht)
         && space == RETRO_VR_REFERENCE_SPACE_LOCAL
         && t_space_type == XR_REFERENCE_SPACE_TYPE_LOCAL
         && t_near(t_space_pose.position.x, anchor.position.x)
         && t_near(t_space_pose.orientation.y, anchor.orientation.y),
         "STAGE-asked LOCAL content not made at the anchor: x %.2f",
         t_space_pose.position.x);
   flags = 0;
   CHECK(openxr_host_sample(&h, &flags) && !flags,
         "a recenter told for the anchor the space was made at");
   t_host_recenter_at(&f, 0.5f);
   openxr_frame_get_anchor(&f, &anchor);
   made  = t_spaces_made;
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made + 1
         && t_near(t_space_pose.position.x, anchor.position.x),
         "STAGE-asked LOCAL content not remade: flags %u, %u made, x %.2f",
         (unsigned)flags, t_spaces_made - made, t_space_pose.position.x);
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");
   CHECK(t_stage_asked == 1, "STAGE asked %u times, want once", t_stage_asked);
   t_frame_free(&rt, &s, &f);

   /* A remake that fails is tried again, and its recenter is told once,
    * when it succeeds. */
   t_stage_missing = false;
   t_host_up(&rt, &s, &sc, &f, &h);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_host_recenter_at(&f, 0.25f);
   openxr_frame_get_anchor(&f, &anchor);
   t_space_fail_at = t_spaces_made + 1;
   flags           = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED),
         "a recenter told for a remake that failed");
   t_space_fail_at = 0;
   flags           = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED)
         && t_near(t_space_pose.position.x, anchor.position.x),
         "a failed remake was not retried: flags %u", (unsigned)flags);
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");

   /* The runtime's own recenter moves the LOCAL origin: the space is
    * remade at identity. */
   openxr_frame_local_changed(&f);
   made  = t_spaces_made;
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made + 1
         && t_near(t_space_pose.position.x, 0.0f)
         && t_near(t_space_pose.orientation.w, 1.0f),
         "the runtime's recenter: flags %u, %u made", (unsigned)flags,
         t_spaces_made - made);
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");

   /* A session after the first has a new origin: the core is told once,
    * and its space is made again as it asked. */
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_session_reset(&s);
   openxr_session_create(&s, &binding);
   openxr_host_session_created(&h);
   made  = t_spaces_made;
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made + 1
         && t_space_type == XR_REFERENCE_SPACE_TYPE_STAGE,
         "a new session: flags %u, %u made, type %d", (unsigned)flags,
         t_spaces_made - made, (int)t_space_type);
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");

   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

/* A paused core takes no sample: a frame it did not draw remakes its
 * seated space at a recenter, where the views it was given and the
 * projection shown from them go on, and its next sample tells it once.
 * A standing space stays. */
static void test_host_recenter_paused(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   openxr_projection_t p;
   openxr_projection_t shown;
   struct retro_vr_eye_state eyes[2];
   video_xr_pose_t anchor;
   enum retro_vr_reference_space space;
   unsigned w     = 0;
   unsigned ht    = 0;
   unsigned n     = 0;
   unsigned made  = 0;
   uint32_t flags = 0;
   int cb_user    = 0;
   int seq;

   t_host_up(&rt, &s, &sc, &f, &h);
   CHECK(openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
            VIDEO_SCALE_PACK(1728, 1824), 2, t_images_cb, &cb_user, &n),
         "projection slot");
   retro_atomic_store_release_int(
         &sc.slots[OPENXR_PROJECTION_SLOT].content, 1);
   openxr_frame_run(&f);
   made = t_spaces_made;
   t_host_recenter_at(&f, 0.25f);
   openxr_host_place_kept(&h);
   CHECK(!s.track_space && t_spaces_made == made,
         "a recenter made a space no content asked for");
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_view[0].pose.position.x = -0.25f;
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   openxr_host_projection(&h, 400, 240, &p);
   openxr_frame_publish_projection(&f, &p);
   made = t_spaces_made;
   seq  = retro_atomic_load_acquire_int(&s.track_seq);
   openxr_host_place_kept(&h);
   CHECK(     t_spaces_made == made
         && retro_atomic_load_acquire_int(&s.track_seq) == seq,
         "a frame the core did not draw remade a space nothing moved");

   t_host_recenter_at(&f, 0.5f);
   openxr_frame_get_anchor(&f, &anchor);
   t_view[0].pose.position.x = 0.75f;
   openxr_host_place_kept(&h);
   seq = retro_atomic_load_acquire_int(&s.track_seq);
   CHECK(     t_spaces_made == made + 1
         && t_near(t_space_pose.position.x, anchor.position.x)
         && t_near(t_space_pose.orientation.y, anchor.orientation.y),
         "the seated space did not follow a recenter while paused");
   openxr_host_projection(&h, 400, 240, &p);
   openxr_seq_read(&f.proj_seq, f.proj_words, &shown, sizeof(shown));
   CHECK(     openxr_host_has_given(&h) && p.track_seq == seq
         && t_near(p.pose[0].position.x, -0.25f)
         && shown.track_seq == seq && t_near(shown.pose[0].position.x, -0.25f),
         "the paused frame's views did not go on in the new space");
   memset(&t_proj, 0, sizeof(t_proj));
   openxr_frame_run(&f);
   CHECK(     t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_proj.space == s.track_space
         && t_near(t_proj_views[0].pose.position.x, -0.25f),
         "the paused frame is not shown in the new space");
   made = t_spaces_made;
   openxr_host_place_kept(&h);
   CHECK(t_spaces_made == made, "a recenter followed twice");
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made,
         "the next sample: flags %u, %u made", (unsigned)flags,
         t_spaces_made - made);
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");

   /* A remake that fails moves nothing and tells nothing; the sample
    * tries again. */
   openxr_host_eye_state(&h, eyes);
   openxr_host_projection(&h, 400, 240, &p);
   openxr_frame_publish_projection(&f, &p);
   seq = retro_atomic_load_acquire_int(&s.track_seq);
   t_host_recenter_at(&f, 0.25f);
   t_space_fail_at = t_spaces_made + 1;
   openxr_host_place_kept(&h);
   openxr_seq_read(&f.proj_seq, f.proj_words, &shown, sizeof(shown));
   CHECK(     retro_atomic_load_acquire_int(&s.track_seq) == seq
         && openxr_host_has_given(&h) && shown.track_seq == seq,
         "a failed remake moved the paused frame");
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED),
         "a recenter told for a remake that failed");
   t_space_fail_at = 0;
   flags           = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED)
         && retro_atomic_load_acquire_int(&s.track_seq) != seq,
         "a failed remake was not retried: flags %u", (unsigned)flags);

   /* Views and a projection from a space replaced since stay there. */
   openxr_host_eye_state(&h, eyes);
   openxr_host_projection(&h, 400, 240, &p);
   openxr_frame_publish_projection(&f, &p);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_host_recenter_at(&f, 0.5f);
   openxr_host_place_kept(&h);
   openxr_seq_read(&f.proj_seq, f.proj_words, &shown, sizeof(shown));
   CHECK(     !openxr_host_has_given(&h)
         && shown.track_seq == p.track_seq
         && shown.track_seq != retro_atomic_load_acquire_int(&s.track_seq),
         "views from a replaced space went on in a recentered one");

   /* Standing: the room stays, and the sample tells the recenter. */
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   made = t_spaces_made;
   seq  = retro_atomic_load_acquire_int(&s.track_seq);
   t_host_recenter_at(&f, 0.25f);
   openxr_host_place_kept(&h);
   CHECK(     t_spaces_made == made
         && retro_atomic_load_acquire_int(&s.track_seq) == seq
         && openxr_host_has_given(&h),
         "a recenter moved a standing space while paused");
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made,
         "a standing core's recenter: flags %u, %u made", (unsigned)flags,
         t_spaces_made - made);

   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);
   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

/* The XR thread's frame as it takes the lock the moment it is let go:
 * how many ran, and how many left the projection out. */
static openxr_frame_t *t_xr;
static unsigned t_xr_frames, t_xr_no_proj;
static void t_xr_frame(void)
{
   unsigned ends = t_ends;
   openxr_frame_run(t_xr);
   t_xr_frames++;
   if (     t_ends == ends
         || t_layer_types[0] != XR_TYPE_COMPOSITION_LAYER_PROJECTION)
      t_xr_no_proj++;
}

/* A headset frame between a recenter's swap and its carry would show
 * the new space without the projection: none gets in, running or
 * paused. */
static void test_host_recenter_locked(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   openxr_projection_t p;
   struct retro_vr_eye_state eyes[2];
   enum retro_vr_reference_space space;
   unsigned w     = 0;
   unsigned ht    = 0;
   unsigned n     = 0;
   unsigned pass;
   uint32_t flags = 0;
   int cb_user    = 0;

   t_host_up(&rt, &s, &sc, &f, &h);
   CHECK(openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
            VIDEO_SCALE_PACK(1728, 1824), 2, t_images_cb, &cb_user, &n),
         "projection slot");
   retro_atomic_store_release_int(
         &sc.slots[OPENXR_PROJECTION_SLOT].content, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   for (pass = 0; pass < 2; pass++)
   {
      int seq;
      openxr_host_sample(&h, &flags);
      openxr_host_eye_state(&h, eyes);
      openxr_host_projection(&h, 400, 240, &p);
      openxr_frame_publish_projection(&f, &p);
      t_host_recenter_at(&f, pass ? 0.5f : 0.25f);
      seq          = retro_atomic_load_acquire_int(&s.track_seq);
      t_xr         = &f;
      t_xr_frames  = 0;
      t_xr_no_proj = 0;
      t_at_unlock  = t_xr_frame;
      if (pass)
         openxr_host_place_kept(&h);
      else
         openxr_host_sample(&h, &flags);
      t_at_unlock  = NULL;
      CHECK(retro_atomic_load_acquire_int(&s.track_seq) != seq,
            "%s: no remake", pass ? "paused" : "running");
      CHECK(t_xr_frames && !t_xr_no_proj,
            "%s: %u of %u headset frames at the remake left the projection out",
            pass ? "paused" : "running", t_xr_no_proj, t_xr_frames);
   }

   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);
   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

static void t_pose(XrPosef *p, float x, float z, float yaw_y)
{
   memset(p, 0, sizeof(*p));
   p->position.x    = x;
   p->position.z    = z;
   p->orientation.y = yaw_y;
   p->orientation.w = (float)sqrt(1.0 - yaw_y * yaw_y);
}

/* A host's stop and the next one's resume through openxr_host's
 * record, saved and given back, so one test can hold several. */
static void t_keep(const openxr_host_t *h, openxr_host_kept_t *out)
{
   openxr_host_keep(h);
   *out = openxr_host_record;
}

static void t_resume(openxr_host_t *h, const openxr_host_kept_t *kept)
{
   openxr_host_record = *kept;
   openxr_host_resume(h);
}

/* Across a video driver reinit, a core's views go on where they were in
 * the room, through STAGE, and a core whose origin moved hears
 * RECENTERED. */
static void test_host_kept(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   openxr_host_kept_t kept;
   openxr_host_kept_t seated;
   openxr_host_kept_t none;
   openxr_projection_t p;
   struct retro_vr_eye_state eyes[2];
   enum retro_vr_reference_space space;
   unsigned w     = 0;
   unsigned ht    = 0;
   unsigned gone  = 0;
   uint32_t flags = 0;

   /* STAGE content: the views are in STAGE already. */
   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_view[0].pose.position.x = -0.25f;
   openxr_host_sample(&h, &flags);
   t_keep(&h, &kept);
   CHECK(!kept.valid && kept.session,
         "views kept before the core was given any");
   openxr_host_eye_state(&h, eyes);
   t_keep(&h, &kept);
   CHECK(     kept.valid && openxr_host_has_given(&h)
         && t_near(kept.views[0].pose.position.x, -0.25f),
         "the views a STAGE core was given were not kept");
   /* Views from before the session stopped showing tracked content drew
    * none of the frames since. */
   retro_atomic_store_release_int(&s.running, 0);
   t_keep(&h, &none);
   CHECK(!none.valid && none.session == kept.session,
         "views kept from a session that shows no tracked content");
   retro_atomic_store_release_int(&s.running, 1);
   /* Given in a space since replaced: not where they were drawn. */
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_keep(&h, &none);
   CHECK(!none.valid && !openxr_host_has_given(&h),
         "views from a replaced space were kept");
   t_frame_free(&rt, &s, &f);

   /* The next host's STAGE takes them as they are, not the head sampled
    * since, and the core hears no recenter. */
   t_host_up(&rt, &s, &sc, &f, &h);
   t_view[0].pose.position.x = 0.5f;
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, &flags);
   t_resume(&h, &kept);
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(     openxr_host_has_given(&h) && p.active
         && p.track_seq == retro_atomic_load_acquire_int(&s.track_seq)
         && t_near(p.pose[0].position.x, -0.25f),
         "the kept views are not the next STAGE space's projection");
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED),
         "a STAGE core was told of a recenter after a reinit");
   /* A new core frame was not drawn from them, but from eyes it asked
    * for since. */
   openxr_host_new_frame(&h);
   CHECK(!openxr_host_has_given(&h), "kept views outlived a new core frame");
   t_resume(&h, &kept);
   openxr_host_eye_state(&h, eyes);
   openxr_host_new_frame(&h);
   CHECK(openxr_host_has_given(&h),
         "a new frame dropped the eyes the core was given");
   t_frame_free(&rt, &s, &f);

   /* Seated: kept in STAGE through where the track space is there, and
    * the located STAGE space is destroyed. */
   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_view[0].pose.position.x = -0.25f;
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   t_pose(&t_in_stage, 1.0f, 0.0f, 0.70710678f);
   t_in_stage_set = true;
   gone           = t_spaces_destroyed;
   t_keep(&h, &seated);
   CHECK(     seated.valid && seated.track_type == space
         && t_near(seated.views[0].pose.position.x, 1.0f)
         && t_near(seated.views[0].pose.position.z, 0.25f)
         && t_near(seated.views[0].pose.orientation.y, 0.70710678f)
         && t_spaces_destroyed == gone + 1,
         "a seated core's views were not kept in STAGE: %.3f, %.3f",
         seated.views[0].pose.position.x, seated.views[0].pose.position.z);
   /* The same session again, as a kept device's: nothing moved. */
   openxr_host_set_space(&h, &space, &w, &ht);
   t_resume(&h, &seated);
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED),
         "a recenter told for a session kept across a reinit");
   t_frame_free(&rt, &s, &f);

   /* The next host places them once a frame time locates its seated
    * space in STAGE, and the core hears its origin moved, once. */
   t_host_up(&rt, &s, &sc, &f, &h);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_pose(&t_in_stage, 0.0f, 2.0f, 0.0f);
   t_resume(&h, &seated);
   CHECK(!openxr_host_has_given(&h), "kept views placed without a time");
   openxr_frame_run(&f);
   /* A new session's first location may not be had yet: tried again. */
   t_in_stage_invalid = 1;
   openxr_host_place_kept(&h);
   CHECK(!openxr_host_has_given(&h), "kept views placed without a location");
   openxr_host_place_kept(&h);
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(     openxr_host_has_given(&h) && p.active
         && t_near(p.pose[0].position.x, 1.0f)
         && t_near(p.pose[0].position.z, -1.75f)
         && t_near(p.pose[0].orientation.y, 0.70710678f),
         "the kept views are not where they were in the room: %.3f, %.3f",
         p.pose[0].position.x, p.pose[0].position.z);
   flags = 0;
   CHECK(     openxr_host_sample(&h, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED),
         "a seated core was not told its origin moved");
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");
   t_frame_free(&rt, &s, &f);

   /* Eyes the core is given, or a space set, before the time: the kept
    * views wait for neither. */
   t_host_up(&rt, &s, &sc, &f, &h);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_resume(&h, &seated);
   openxr_frame_run(&f);
   t_view[0].pose.position.x = 0.75f;
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   openxr_host_place_kept(&h);
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(t_near(p.pose[0].position.x, 0.75f),
         "kept views replaced the eyes the core was given");
   t_frame_free(&rt, &s, &f);
   t_host_up(&rt, &s, &sc, &f, &h);
   openxr_host_set_space(&h, &space, &w, &ht);
   t_resume(&h, &seated);
   openxr_host_new_frame(&h);
   openxr_frame_run(&f);
   openxr_host_place_kept(&h);
   CHECK(!openxr_host_has_given(&h),
         "kept views placed after a new core frame");
   t_frame_free(&rt, &s, &f);
   t_host_up(&rt, &s, &sc, &f, &h);
   openxr_host_set_space(&h, &space, &w, &ht);
   t_resume(&h, &seated);
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_frame_run(&f);
   openxr_host_place_kept(&h);
   CHECK(!openxr_host_has_given(&h), "kept views placed in a space set since");
   t_frame_free(&rt, &s, &f);

   /* No STAGE: none kept, and none placed. */
   t_stage_missing = true;
   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   t_keep(&h, &none);
   CHECK(!none.valid && none.session, "seated views kept without a STAGE");
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_resume(&h, &seated);
   CHECK(!openxr_host_has_given(&h), "kept views placed without a STAGE");
   t_stage_missing = false;
   t_in_stage_set  = false;

   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

/* openxr_host's one record: a stopping host keeps it, the next host's
 * resume places its views and spends it, and a new core frame before
 * that start drops it. */
static void test_host_record(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   openxr_projection_t p;
   struct retro_vr_eye_state eyes[2];
   enum retro_vr_reference_space space;
   unsigned w     = 0;
   unsigned ht    = 0;
   uint32_t flags = 0;

   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_view[0].pose.position.x = -0.25f;
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   openxr_host_keep(&h);
   CHECK(     openxr_host_record.valid
         && openxr_host_record.session == h.session_serial
         && t_near(openxr_host_record.views[0].pose.position.x, -0.25f),
         "a stopping host's views are not in the record");
   t_frame_free(&rt, &s, &f);

   t_host_up(&rt, &s, &sc, &f, &h);
   t_view[0].pose.position.x = 0.5f;
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_resume(&h);
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(     openxr_host_has_given(&h) && p.active
         && t_near(p.pose[0].position.x, -0.25f),
         "the next host did not place the record's views");
   CHECK(!openxr_host_record.session,
         "a resume left the record for a later start");
   t_frame_free(&rt, &s, &f);

   /* A start that gave up leaves nothing for the next. */
   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   openxr_host_keep(&h);
   t_frame_free(&rt, &s, &f);
   openxr_host_forget();
   CHECK(!openxr_host_record.session, "a forget left the record");
   t_host_up(&rt, &s, &sc, &f, &h);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_resume(&h);
   CHECK(!openxr_host_has_given(&h), "a resume placed a forgotten record");
   t_frame_free(&rt, &s, &f);

   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   openxr_host_keep(&h);
   t_frame_free(&rt, &s, &f);
   t_host_up(&rt, &s, &sc, &f, &h);
   openxr_frame_run(&f);
   openxr_host_new_frame(&h);
   CHECK(!openxr_host_record.session, "a new core frame left the record");

   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

static int t_binding;

/* A space asked for again keeps the views given in it; another space,
 * session or a record kept for an earlier driver start does not. */
static void test_host_same_space(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   struct retro_vr_eye_state eyes[2];
   enum retro_vr_reference_space space;
   unsigned w     = 0;
   unsigned ht    = 0;
   unsigned made  = 0;
   uint32_t flags = 0;
   int seq;

   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   seq  = retro_atomic_load_acquire_int(&s.track_seq);
   made = t_spaces_made;
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   CHECK(     openxr_host_set_space(&h, &space, &w, &ht)
         && space == RETRO_VR_REFERENCE_SPACE_STAGE && w == 1728 && ht == 1824
         && t_spaces_made == made
         && retro_atomic_load_acquire_int(&s.track_seq) == seq
         && openxr_host_has_given(&h),
         "the same space asked again was remade");
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   CHECK(     t_spaces_made > made
         && retro_atomic_load_acquire_int(&s.track_seq) != seq
         && !openxr_host_has_given(&h),
         "a different space kept the views of the old one");
   /* A recenter is the sample's: the space asked again stays. */
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   seq  = retro_atomic_load_acquire_int(&s.track_seq);
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_host_set_space(&h, &space, &w, &ht);
   CHECK(     retro_atomic_load_acquire_int(&s.track_seq) == seq
         && openxr_host_has_given(&h),
         "a recenter remade the space asked again");
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(     (flags & RETRO_VR_FRAME_RECENTERED)
         && retro_atomic_load_acquire_int(&s.track_seq) != seq,
         "the sample did not remake the seated space at a recenter");
   /* No STAGE: it can only be seated again, so the space stays. */
   t_stage_missing = true;
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   seq  = retro_atomic_load_acquire_int(&s.track_seq);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   CHECK(     space == RETRO_VR_REFERENCE_SPACE_LOCAL
         && retro_atomic_load_acquire_int(&s.track_seq) == seq
         && openxr_host_has_given(&h),
         "a seated fallback was remade for the same ask");
   t_stage_missing = false;
   t_frame_free(&rt, &s, &f);

   /* A new session has no track space to keep. */
   t_host_up(&rt, &s, &sc, &f, &h);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_session_destroy(&s);
   openxr_session_create(&s, &t_binding);
   openxr_host_session_created(&h);
   made  = t_spaces_made;
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   CHECK(t_spaces_made > made, "a new session kept the last one's space");
   t_frame_free(&rt, &s, &f);

   /* A record kept before a content switch is not the next content's:
    * once a frame is drawn it is dropped, and a cached reinit then
    * resumes nothing. */
   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_view[0].pose.position.x = -0.25f;
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   openxr_host_keep(&h);
   CHECK(openxr_host_record.valid,
         "the views of the first content were not kept");
   t_frame_free(&rt, &s, &f);
   t_host_up(&rt, &s, &sc, &f, &h);
   retro_atomic_store_release_int(&s.running, 1);
   openxr_frame_run(&f);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_view[0].pose.position.x = 0.5f;
   openxr_host_sample(&h, &flags);
   openxr_host_eye_state(&h, eyes);
   openxr_host_new_frame(&h);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_resume(&h);
   openxr_host_place_kept(&h);
   flags = 0;
   openxr_host_sample(&h, &flags);
   CHECK(     openxr_host_has_given(&h) && !(flags & RETRO_VR_FRAME_RECENTERED)
         && t_near(h.given[0].pose.position.x, 0.5f),
         "a stale record was resumed after a content switch");
   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

static bool t_enabled_has(const char *name)
{
   unsigned i;
   for (i = 0; i < t_num_enabled; i++)
      if (string_is_equal(t_enabled[i], name))
         return true;
   return false;
}

static void test_runtime_platform(void)
{
   openxr_runtime_t rt;
   XrLoaderInitInfoBaseHeaderKHR loader;
   int next = 0;

   memset(&loader, 0, sizeof(loader));
   loader.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;

   /* The loader starts with what the platform gave, and the instance is
    * told of the platform and asks for its extension. */
   t_runtime_defaults();
   t_num_exts = 4;
   t_exts[3]  = "XR_KHR_android_create_instance";
   memset(&rt, 0, sizeof(rt));
   rt.platform.loader        = &loader;
   rt.platform.instance_next = &next;
   rt.platform.instance_ext  = "XR_KHR_android_create_instance";
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_OK, "instance on a platform");
   CHECK(t_loader_calls == 1 && t_loader_info == &loader,
         "loader started %u times", t_loader_calls);
   CHECK(t_instance_next == &next, "the platform's chain not passed on");
   CHECK(t_num_enabled == 4 && t_enabled_has("XR_KHR_android_create_instance"),
         "%u extensions, the platform's %s", t_num_enabled,
         t_enabled_has("XR_KHR_android_create_instance") ? "asked" : "missing");
   openxr_runtime_deinit(&rt);

   /* An extension the runtime does not list is not asked for. */
   t_runtime_defaults();
   memset(&rt, 0, sizeof(rt));
   rt.platform.instance_ext = "XR_KHR_android_create_instance";
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   CHECK(t_num_enabled == 3 && !t_instance_next && !t_loader_calls,
         "an unlisted platform extension was asked for");
   openxr_runtime_deinit(&rt);

   /* A loader that does not start: no runtime, no instance asked for. */
   t_runtime_defaults();
   t_loader_res  = XR_ERROR_RUNTIME_FAILURE;
   t_num_enabled = 99;
   memset(&rt, 0, sizeof(rt));
   rt.platform.loader = &loader;
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_UNAVAILABLE && t_num_enabled == 99,
         "an instance after the loader failed");
   CHECK(LOGGED("[OpenXR] The loader could not be started (-2).\n"),
         "log: %s", t_log);
   openxr_runtime_deinit(&rt);

   /* A loader without its start function. */
   t_runtime_defaults();
   t_missing = "xrInitializeLoaderKHR";
   memset(&rt, 0, sizeof(rt));
   rt.platform.loader = &loader;
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_UNAVAILABLE, "no start function");
   openxr_runtime_deinit(&rt);

   /* Blend: OPAQUE wherever it is listed; else the runtime's first. */
   t_runtime_defaults();
   t_modes[0]  = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
   t_modes[1]  = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
   t_num_modes = 2;
   memset(&rt, 0, sizeof(rt));
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   openxr_runtime_find_system(&rt);
   CHECK(rt.blend_mode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
         "blend %d with OPAQUE second", (int)rt.blend_mode);
   openxr_runtime_deinit(&rt);
   t_runtime_defaults();
   t_modes[0] = XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
   memset(&rt, 0, sizeof(rt));
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   openxr_runtime_find_system(&rt);
   CHECK(rt.blend_mode == XR_ENVIRONMENT_BLEND_MODE_ADDITIVE,
         "blend %d without OPAQUE", (int)rt.blend_mode);
   openxr_runtime_deinit(&rt);
}

/* The size cap, no stereo views, missing functions, and the notice. */
static void test_runtime_gaps(void)
{
   openxr_runtime_t rt;

   /* The largest side is held to 4096 even when the headset takes more. */
   t_runtime_defaults();
   t_max_w = 8192;
   t_max_h = 8192;
   memset(&rt, 0, sizeof(rt));
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   openxr_runtime_find_system(&rt);
   CHECK(rt.max_dim == 4096, "max_dim %u", rt.max_dim);
   openxr_runtime_deinit(&rt);

   /* No stereo view configuration: no headset. */
   t_runtime_defaults();
   t_views = 0;
   memset(&rt, 0, sizeof(rt));
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   CHECK(openxr_runtime_find_system(&rt) == OPENXR_RUNTIME_UNAVAILABLE
         && LOGGED("[OpenXR] No headset (no stereo view configuration).\n"),
         "no stereo views: %s", t_log);
   openxr_runtime_deinit(&rt);

   /* An instance that cannot be destroyed is a failure. */
   t_runtime_defaults();
   t_missing = "xrDestroyInstance";
   memset(&rt, 0, sizeof(rt));
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_FAILED, "made without xrDestroyInstance");
   rt.instance = XR_NULL_HANDLE;

   /* A loader without xrCreateInstance: no runtime. */
   t_runtime_defaults();
   t_missing = "xrCreateInstance";
   memset(&rt, 0, sizeof(rt));
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_UNAVAILABLE
         && LOGGED("[OpenXR] No runtime (the loader lacks xrCreateInstance or xrEnumerateInstanceExtensionProperties).\n"),
         "no xrCreateInstance: %s", t_log);

   /* The notice goes to the message queue by its id. */
   t_runtime_defaults();
   openxr_runtime_notify(MSG_OPENXR_FAILED);
   CHECK(t_notified == (int)MSG_OPENXR_FAILED, "notified %d", t_notified);
}

static void test_frame_inline(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_frame_begun_t b;
   video_xr_quad_set_t set;
   video_xr_pose_t anchor;
   openxr_projection_t p;
   XrPosef origin;
   enum retro_vr_reference_space space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   int64_t anchor_ns, step_ns = 0;
   unsigned n = 0, waits, ends, i;
   int ticks;
   int cb_user = 0;

   t_frame_fresh(&rt, &s, &sc, &f);
   CHECK(openxr_frame_start_inline(&f) && !f.thread
         && openxr_session_alive(&s), "in-line start");

   /* Not running: no wait, nothing to end. */
   waits = t_waits;
   ends  = t_ends;
   CHECK(!openxr_frame_begin(&f, &b) && !b.begun && t_waits == waits,
         "waited while the session is not running");
   openxr_frame_end(&f, &b);
   CHECK(t_ends == ends, "ended a frame that was not begun");

   /* The begin polls: READY begins the session, then its first frame,
    * with the lock around xrBeginFrame alone, and no tick. */
   rt.refresh_ext = true;
   openxr_frame_request_rate(&f, 90.0f);
   openxr_frame_set_pacing(&f, 1);
   ticks           = retro_atomic_load_acquire_int(&f.tick_seq);
   t_rate_requests = 0;
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   t_locks = t_unlocks = t_frame_unlocked = 0;
   CHECK(openxr_frame_begin(&f, &b) && b.waited && b.begun
         && openxr_session_running(&s), "the first in-line frame");
   CHECK(t_locks == 2 && t_unlocks == 2 && !t_frame_unlocked,
         "locks %u/%u, %u frame calls outside them", t_locks, t_unlocks,
         t_frame_unlocked);
   CHECK(retro_atomic_load_acquire_int(&f.tick_seq) == ticks,
         "an in-line frame ticked");
   CHECK(t_rate_requests == 1, "an in-line frame asked %u rates, want 1",
         t_rate_requests);
   CHECK(b.period == 13888889 && openxr_frame_predicted_time(&f) == b.display_time,
         "period %lld, published time", (long long)b.period);
   CHECK(!b.render, "drawn while not visible");

   /* The end, under the lock again: the published layers while visible. */
   openxr_frame_end(&f, &b);
   CHECK(t_last_layer_count == 0 && t_locks == 3 && t_unlocks == 3
         && !t_frame_unlocked, "the end's lock");
   openxr_slot_create(&sc, 1, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[1].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads     = 1;
   set.quads[0].slot = 1;
   openxr_frame_publish(&f, &set);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   CHECK(openxr_frame_begin(&f, &b) && b.render, "not drawn while focused");
   openxr_frame_end(&f, &b);
   CHECK(t_last_layer_count == 1, "%u layers", t_last_layer_count);

   /* What openxr_host reads keeps up as on the XR thread: the period
    * once the frames agree, and a recenter at the next frame. */
   for (n = 0; n < VIDEO_XR_PERIODS; n++)
   {
      openxr_frame_begin(&f, &b);
      openxr_frame_end(&f, &b);
   }
   CHECK(retro_atomic_load_acquire_int(&f.period_ns) == 13888889,
         "period %d after %u in-line frames",
         retro_atomic_load_acquire_int(&f.period_ns), (unsigned)VIDEO_XR_PERIODS);
   CHECK(t_rate_requests == 1, "the in-line frames asked %u rates, want 1",
         t_rate_requests);
   t_head_tracked       = true;
   memset(&t_head, 0, sizeof(t_head));
   t_head.orientation.w = 1.0f;
   t_head.position.x    = 0.25f;
   t_head.position.y    = 1.5f;
   openxr_frame_request_recenter(&f);
   openxr_frame_begin(&f, &b);
   openxr_frame_end(&f, &b);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.0f, 1.0f, 0.25f, 1.5f, 0.0f),
         "an in-line frame did not recenter: %.2f,%.2f,%.2f",
         anchor.position.x, anchor.position.y, anchor.position.z);
   t_head_tracked = false;

   /* No XR-thread ticks to wait on: no pacing, and the idle clock's
    * anchor is left alone. */
   anchor_ns = f.pace_anchor_ns;
   openxr_frame_pace_wait(&f);
   CHECK(f.pace_mode == 0 && f.pace_anchor_ns == anchor_ns,
         "an in-line host paced on ticks: mode %u", f.pace_mode);

   /* A failed begin submits nothing, is logged once, and the next
    * frame recovers. */
   t_begin_frame_res = XR_ERROR_RUNTIME_FAILURE;
   t_reset_log();
   ends = t_ends;
   CHECK(!openxr_frame_begin(&f, &b) && b.waited && !b.begun, "failed begin");
   openxr_frame_end(&f, &b);
   CHECK(!openxr_frame_begin(&f, &b) && b.waited && !b.begun,
         "second failed begin");
   openxr_frame_end(&f, &b);
   CHECK(t_ends == ends, "ended a frame that was not begun");
   CHECK(t_logged_times("[OpenXR] xrBeginFrame failed (-2).\n") == 1,
         "logged %u times", t_logged_times("[OpenXR] xrBeginFrame failed (-2).\n"));
   t_begin_frame_res = XR_SUCCESS;
   CHECK(openxr_frame_begin(&f, &b) && b.begun, "no recovery after a failed begin");
   openxr_frame_end(&f, &b);
   CHECK(t_ends == ends + 1 && !f.frame_failed, "recovered frame not ended");

   /* A failed wait begins nothing, and is logged once. */
   t_wait_frame_res = XR_ERROR_RUNTIME_FAILURE;
   t_reset_log();
   CHECK(!openxr_frame_begin(&f, &b) && !b.waited && !b.begun, "failed wait");
   openxr_frame_end(&f, &b);
   CHECK(!openxr_frame_begin(&f, &b), "second failed wait");
   openxr_frame_end(&f, &b);
   CHECK(t_logged_times("[OpenXR] xrWaitFrame failed (-2).\n") == 1,
         "logged %u times", t_logged_times("[OpenXR] xrWaitFrame failed (-2).\n"));
   t_wait_frame_res = XR_SUCCESS;

   /* With no frame, the core keeps a period on the clock. */
   retro_atomic_store_release_int(&f.period_ns, 5000000);
   memset(&b, 0, sizeof(b));
   openxr_frame_end(&f, &b);
   /* A late frame restarts the schedule, so a stall only costs a retry. */
   for (i = 0; i < 20; i++)
   {
      anchor_ns = f.pace_anchor_ns;
      openxr_frame_end(&f, &b);
      step_ns = f.pace_anchor_ns - anchor_ns;
      CHECK(step_ns != 10000000, "the idle clock used the 10 ms fallback");
      if (step_ns == 5000000)
         break;
   }
   CHECK(i < 20, "the idle clock never moved by the measured period: %lld ns",
         (long long)step_ns);

   openxr_frame_stop_inline(&f);
   CHECK(!openxr_session_alive(&s), "alive after the in-line stop");
   openxr_slot_destroy(&sc, 1);
   t_frame_free(&rt, &s, &f);

   /* The XR thread and an in-line host exclude each other. */
   t_frame_fresh(&rt, &s, &sc, &f);
   CHECK(openxr_frame_start(&f) && f.thread, "XR thread start");
   CHECK(!openxr_frame_start_inline(&f) && !f.inline_host,
         "an in-line host started beside the XR thread");
   openxr_frame_stop(&f);
   CHECK(openxr_frame_start_inline(&f), "in-line start after the thread");
   CHECK(!openxr_frame_start(&f) && !f.thread,
         "the XR thread started beside an in-line host");
   openxr_frame_stop_inline(&f);
   t_frame_free(&rt, &s, &f);

   /* Without an in-line host, begin and end do nothing. */
   t_frame_fresh(&rt, &s, &sc, &f);
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   waits = t_waits;
   ends  = t_ends;
   CHECK(!openxr_frame_begin(&f, &b) && !b.waited && t_waits == waits
         && !openxr_session_running(&s), "begin without an in-line host");
   b.begun = true;
   openxr_frame_end(&f, &b);
   CHECK(t_ends == ends, "end without an in-line host");
   t_frame_free(&rt, &s, &f);

   /* A deinit stops the in-line host. */
   t_frame_fresh(&rt, &s, &sc, &f);
   openxr_frame_start_inline(&f);
   openxr_frame_deinit(&f);
   CHECK(!openxr_session_alive(&s) && !f.inline_host,
         "an in-line host left running by the deinit");
   t_frame_free(&rt, &s, &f);

   /* A runtime that takes fewer layers than a frame has: the last
    * screens go, never the menu or the host's layers over it. */
   t_max_layers = 3;
   t_frame_fresh(&rt, &s, &sc, &f);
   t_max_layers = 0;
   for (i = 1; i <= 2; i++)
   {
      openxr_slot_create(&sc, i, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
            t_images_cb, &cb_user, &n);
      retro_atomic_store_release_int(&sc.slots[i].content, 1);
   }
   openxr_slot_create(&sc, VIDEO_XR_MENU_SLOT, 44, false,
         VIDEO_SCALE_PACK(64, 64), 1, t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[VIDEO_XR_MENU_SLOT].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads     = 3;
   set.quads[0].slot = 1;
   set.quads[1].slot = 2;
   set.quads[2].kind = VIDEO_XR_QUAD_MENU;
   set.quads[2].slot = VIDEO_XR_MENU_SLOT;
   openxr_frame_publish(&f, &set);
   f.extra_layers = t_extra_layers;
   openxr_frame_start_inline(&f);
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   CHECK(openxr_frame_begin(&f, &b), "limited frame begin");
   b.render = true;
   openxr_frame_end(&f, &b);
   CHECK(t_last_layer_count == 3, "%u layers where the runtime takes 3",
         t_last_layer_count);
   {
      XrCompositionLayerQuad q[4];
      const XrCompositionLayerBaseHeader *kept[4];
      memset(q, 0, sizeof(q));
      for (i = 0; i < 4; i++)
         kept[i] = (const XrCompositionLayerBaseHeader*)&q[i];
      q[0].subImage.swapchain = sc.slots[1].swapchains[0];
      q[1].subImage.swapchain = sc.slots[2].swapchains[0];
      q[2].subImage.swapchain = sc.slots[VIDEO_XR_MENU_SLOT].swapchains[0];
      CHECK(     openxr_frame_fit(&f, kept, 4, 0, 3) == 3
            && kept[0] == (const XrCompositionLayerBaseHeader*)&q[0]
            && kept[1] == (const XrCompositionLayerBaseHeader*)&q[2]
            && kept[2] == (const XrCompositionLayerBaseHeader*)&q[3],
            "not the second screen dropped");
   }
   openxr_frame_stop_inline(&f);
   openxr_slot_destroy(&sc, 1);
   openxr_slot_destroy(&sc, 2);
   openxr_slot_destroy(&sc, VIDEO_XR_MENU_SLOT);
   t_frame_free(&rt, &s, &f);

   /* A projection published with its slot's release between the begin
    * and the end goes out first. */
   t_frame_fresh(&rt, &s, &sc, &f);
   memset(&origin, 0, sizeof(origin));
   origin.orientation.w = 1.0f;
   openxr_session_swap_track_space(&s, &space, &origin, NULL, NULL);
   openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
         VIDEO_SCALE_PACK(1728, 1824), 2, t_images_cb, &cb_user, &n);
   openxr_slot_create(&sc, 1, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[1].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads     = 1;
   set.quads[0].slot = 1;
   openxr_frame_publish(&f, &set);
   openxr_frame_start_inline(&f);
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   CHECK(openxr_frame_begin(&f, &b), "projection frame begin");
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   memset(&p, 0, sizeof(p));
   p.active                = true;
   p.width                 = 1600;
   p.height                = 1800;
   p.pose[0].orientation.w = 1.0f;
   p.pose[1].orientation.w = 1.0f;
   p.track_seq             = retro_atomic_load_acquire_int(&s.track_seq);
   openxr_session_lock(&s);
   retro_atomic_store_release_int(&sc.slots[OPENXR_PROJECTION_SLOT].content, 1);
   openxr_frame_publish_projection(&f, &p);
   openxr_session_unlock(&s);
   b.render = true;
   openxr_frame_end(&f, &b);
   CHECK(t_last_layer_count == 2
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_layer_types[1] == XR_TYPE_COMPOSITION_LAYER_QUAD,
         "projection: %u layers, the first %d", t_last_layer_count,
         (int)t_layer_types[0]);
   openxr_frame_stop_inline(&f);
   openxr_slot_destroy(&sc, 1);
   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);
   t_frame_free(&rt, &s, &f);
}

static void test_binding_gles(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_gles_images_t li;
   uint32_t tex[OPENXR_MAX_IMAGES][2];
   unsigned n        = 0;
   int dummy_binding = 0;

   t_session_fresh(&rt, &s);

   /* The runtime is asked, as it must be before the session. */
   t_gles_reqs_res = XR_SUCCESS;
   t_reset_log();
   CHECK(openxr_binding_gles_requirements(&rt)
         && LOGGED("[OpenXR] OpenGL ES 3.0 to 3.2.\n"), "requirements: %s", t_log);
   t_gles_reqs_res = XR_ERROR_RUNTIME_FAILURE;
   t_reset_log();
   CHECK(!openxr_binding_gles_requirements(&rt)
         && LOGGED("[OpenXR] xrGetOpenGLESGraphicsRequirementsKHR failed (-2).\n"),
         "a failed answer: %s", t_log);
   t_gles_reqs_res = XR_SUCCESS;
   t_missing       = "xrGetOpenGLESGraphicsRequirementsKHR";
   CHECK(!openxr_binding_gles_requirements(&rt), "without the function");
   t_missing       = NULL;

   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);
   memset(&sc, 0, sizeof(sc));
   openxr_swapchains_load(&sc, &rt, &s);

   /* RGBA8 first; sRGB only with a warning; neither: none. */
   t_formats[0] = OPENXR_GLES_SRGB8_ALPHA8;
   t_formats[1] = OPENXR_GLES_RGBA8;
   openxr_swapchains_list_formats(&sc);
   CHECK(openxr_binding_gles_format(&sc) == OPENXR_GLES_RGBA8, "RGBA8 not chosen");
   t_formats[1] = OPENXR_GLES_SRGB8_ALPHA8;
   openxr_swapchains_list_formats(&sc);
   t_reset_log();
   CHECK(openxr_binding_gles_format(&sc) == OPENXR_GLES_SRGB8_ALPHA8
         && LOGGED("[OpenXR] Only an sRGB swapchain format is offered; the output may look washed out.\n"),
         "sRGB: %s", t_log);
   t_formats[0] = 50;
   t_formats[1] = 44;
   openxr_swapchains_list_formats(&sc);
   CHECK(!openxr_binding_gles_format(&sc), "a format from nothing");

   /* Each layer's images land as its textures. */
   memset(tex, 0, sizeof(tex));
   li.sc        = &sc;
   li.tex       = tex;
   t_fill_gles  = true;
   t_gles_base  = 0x400;
   t_sc_images  = 3;
   t_sc_images_second = 0;
   CHECK(openxr_slot_create(&sc, 2, OPENXR_GLES_RGBA8, false,
            VIDEO_SCALE_PACK(64, 64), 2, openxr_binding_gles_images, &li, &n)
         && n == 3, "slot with GLES images: %u", n);
   CHECK(     tex[0][0] == 0x500 && tex[2][0] == 0x502
         && tex[0][1] == 0x600 && tex[2][1] == 0x602,
         "textures %x %x %x %x", tex[0][0], tex[2][0], tex[0][1], tex[2][1]);
   t_fill_gles = false;
   openxr_slot_destroy(&sc, 2);
   openxr_session_destroy(&s);
   openxr_runtime_deinit(&rt);
}

/* vulkan_openxr.c's Vulkan calls are all the laser dot's fill. */
#define T_VK(name) PFN_##name vulkan_symbol_wrapper_##name;
T_VK(vkGetDeviceQueue) T_VK(vkCreateBuffer) T_VK(vkDestroyBuffer)
T_VK(vkGetBufferMemoryRequirements) T_VK(vkGetPhysicalDeviceMemoryProperties)
T_VK(vkAllocateMemory) T_VK(vkFreeMemory) T_VK(vkBindBufferMemory)
T_VK(vkMapMemory) T_VK(vkUnmapMemory) T_VK(vkCreateCommandPool)
T_VK(vkDestroyCommandPool) T_VK(vkAllocateCommandBuffers)
T_VK(vkBeginCommandBuffer) T_VK(vkEndCommandBuffer)
T_VK(vkCmdPipelineBarrier) T_VK(vkCmdCopyBufferToImage) T_VK(vkQueueSubmit)
T_VK(vkCreateFence) T_VK(vkDestroyFence) T_VK(vkWaitForFences)

/* Any call here is a dot being filled: it fails at the first buffer. */
static unsigned t_vk_calls;
static void VKAPI_PTR t_vkGetDeviceQueue(VkDevice device,
      uint32_t family, uint32_t index, VkQueue *queue)
{
   t_vk_calls++;
}
static VkResult VKAPI_PTR t_vkCreateBuffer(VkDevice device,
      const VkBufferCreateInfo *info, const VkAllocationCallbacks *alloc,
      VkBuffer *buffer)
{
   return VK_ERROR_INITIALIZATION_FAILED;
}

/* The video driver's side: the host it was given, and exits asked. */
static video_xr_host_t t_xr_host;
static unsigned t_exit_requests;
void video_driver_set_xr_host(const video_xr_host_t *host)
{
   if (host)
      t_xr_host = *host;
   else
      memset(&t_xr_host, 0, sizeof(t_xr_host));
}
void video_driver_headset_exit_request(void) { t_exit_requests++; }

/* What the headset's own context hears. */
static unsigned t_p_ended;
static void t_p_session_ended(void *u) { t_p_ended++; }

/* What the headset input hears, and how many sessions had been
 * destroyed when it heard one go. */
static unsigned t_in_created, t_in_destroying, t_in_destroyed_at;
static openxr_input_handles_t t_in_handles;
static void t_in_created_fn(void *user, const openxr_input_handles_t *h)
{
   t_in_created++;
   t_in_handles = *h;
}
static void t_in_destroying_fn(void *user)
{
   t_in_destroying++;
   t_in_destroyed_at = t_sessions_destroyed;
}

static void t_push_shown(void)
{
   t_num_events = t_next_event = 0;
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   t_push_state(T_SESSION, XR_SESSION_STATE_SYNCHRONIZED);
   t_push_state(T_SESSION, XR_SESSION_STATE_VISIBLE);
   t_push_state(T_SESSION, XR_SESSION_STATE_FOCUSED);
}

/* The Quest's Vulkan host: vulkan_openxr.c in-line, each headset frame
 * begun and ended inside the video frame. */
static void test_vulkan_inline(void)
{
   vulkan_openxr_platform_t platform;
   XrLoaderInitInfoBaseHeaderKHR loader;
   openxr_input_hooks_t hooks;
   struct retro_vr_eye_state eyes[2];
   enum retro_vr_reference_space space;
   openxr_projection_t p;
   vulkan_openxr_t *xr;
   unsigned waits, begins, ends, instances, i, discarded;
   unsigned sc_made, sc_destroyed;
   int kept;
   VkInstance vk_inst   = (VkInstance)(uintptr_t)0x50;
   VkPhysicalDevice gpu = (VkPhysicalDevice)(uintptr_t)0x58;
   VkDevice dev         = (VkDevice)(uintptr_t)0x60;
   slock_t *lock        = slock_new();
   unsigned w           = 0;
   unsigned ht          = 0;
   uint32_t flags       = 0;
   const unsigned shown = VIDEO_XR_HEADSET_PRESENTING
      | VIDEO_XR_HEADSET_FOCUSED | VIDEO_XR_HEADSET_TRACKING;

   memset(&loader, 0, sizeof(loader));
   loader.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
   memset(&platform, 0, sizeof(platform));
   platform.runtime.loader     = &loader;
   platform.ended              = t_p_session_ended;
   memset(&hooks, 0, sizeof(hooks));
   hooks.session_created       = t_in_created_fn;
   hooks.session_destroying    = t_in_destroying_fn;
   openxr_frame_set_input_hooks(&hooks);
   t_in_created = t_in_destroying = 0;
   t_runtime_defaults();
   t_begun = t_ended_sessions = 0;
   t_sessions_made = t_sessions_destroyed = 0;
   t_spaces_made = t_spaces_destroyed = 0;
   t_sc_wait    = XR_SUCCESS;
   sc_made      = t_sc_made;
   sc_destroyed = t_sc_destroyed;
   instances = t_destroyed_instances;
   t_vk_calls = 0;
   vulkan_symbol_wrapper_vkGetDeviceQueue = t_vkGetDeviceQueue;
   vulkan_symbol_wrapper_vkCreateBuffer   = t_vkCreateBuffer;

   /* The platform makes the runtime in-line, from its loader. */
   vulkan_openxr_set_platform(&platform);
   xr = vulkan_openxr_new(false, VK_API_VERSION_1_1);
   CHECK(xr && vulkan_openxr_inline(xr) && t_loader_calls == 1,
         "not in-line on the platform: loader started %u times",
         t_loader_calls);
   if (!xr)
   {
      openxr_frame_set_input_hooks(NULL);
      vulkan_openxr_set_platform(NULL);
      slock_free(lock);
      return;
   }
   CHECK(     vulkan_openxr_start(xr, vk_inst, gpu, dev, 0, lock)
         && t_sessions_made == 1 && t_in_created == 1
         && t_in_handles.session == T_SESSION && t_xr_host.user == xr,
         "start: %u sessions, the input told of %u", t_sessions_made,
         t_in_created);
   /* The dot as the desktop's, which the stand-in fails at its first
    * buffer: its swapchain goes with it. */
   CHECK(     !openxr_frame_cursor(&xr->frame) && t_vk_calls == 1
         && t_sc_made == sc_made + 1 && t_sc_destroyed == sc_destroyed + 1
         && LOGGED("[OpenXR] No laser cursor (vkCreateBuffer, "),
         "the in-line host's dot: %u Vulkan calls, %u swapchains made, "
         "%u destroyed, log: %s", t_vk_calls, t_sc_made - sc_made,
         t_sc_destroyed - sc_destroyed, t_log);

   /* The first begin begins the session; each begin then waits for and
    * begins one headset frame, a second begin does nothing, and each
    * end ends it. */
   t_push_shown();
   for (i = 0; i < 3; i++)
   {
      waits  = t_waits;
      begins = t_begins;
      ends   = t_ends;
      vulkan_openxr_begin_frame(xr);
      vulkan_openxr_begin_frame(xr);
      CHECK(     t_waits == waits + 1 && t_begins == begins + 1
            && t_ends == ends,
            "frame %u's begin: %u waits, %u begins, %u ends", i,
            t_waits - waits, t_begins - begins, t_ends - ends);
      vulkan_openxr_end_frame(xr, 1);
      CHECK(     t_waits == waits + 1 && t_begins == begins + 1
            && t_ends == ends + 1,
            "frame %u's end: %u waits, %u begins, %u ends", i,
            t_waits - waits, t_begins - begins, t_ends - ends);
   }
   /* An interval of 3 shows the core frame for two more headset
    * frames, after its own. */
   waits        = t_waits;
   begins       = t_begins;
   ends         = t_ends;
   discarded    = t_discarded;
   t_frame_open = false;
   vulkan_openxr_begin_frame(xr);
   vulkan_openxr_end_frame(xr, 3);
   CHECK(     t_waits == waits + 3 && t_begins == begins + 3
         && t_ends == ends + 3 && t_discarded == discarded,
         "an interval of 3: %u waits, %u begins, %u ends, %u discarded",
         t_waits - waits, t_begins - begins, t_ends - ends,
         t_discarded - discarded);
   CHECK(t_begun == 1 && t_xr_host.state(t_xr_host.user) == shown,
         "%u sessions begun, state %x", t_begun,
         t_xr_host.state(t_xr_host.user));
   /* A headset the driver shows nothing (HDR output) gives no HMD. */
   vulkan_openxr_set_hidden(xr, true);
   CHECK(!(t_xr_host.state(t_xr_host.user) & (VIDEO_XR_HEADSET_PRESENTING
         | VIDEO_XR_HEADSET_TRACKING)), "state %x while hidden",
         t_xr_host.state(t_xr_host.user));
   vulkan_openxr_set_hidden(xr, false);

   /* The runtime ends the session: the platform hears it, nothing asks
    * the window to take over, and no frame call follows. */
   t_num_events = t_next_event = 0;
   t_push_state(T_SESSION, XR_SESSION_STATE_STOPPING);
   t_push_state(T_SESSION, XR_SESSION_STATE_IDLE);
   t_push_state(T_SESSION, XR_SESSION_STATE_EXITING);
   waits  = t_waits;
   begins = t_begins;
   ends   = t_ends;
   for (i = 0; i < 3; i++)
   {
      vulkan_openxr_begin_frame(xr);
      vulkan_openxr_end_frame(xr, 0);
   }
   CHECK(     t_ended_sessions == 1 && t_p_ended == 1 && !t_exit_requests
         && !t_xr_host.state(t_xr_host.user),
         "EXITING: %u ended, the platform told %u times, %u exits asked",
         t_ended_sessions, t_p_ended, t_exit_requests);
   CHECK(     t_waits == waits && t_begins == begins && t_ends == ends,
         "after EXITING: %u waits, %u begins, %u ends", t_waits - waits,
         t_begins - begins, t_ends - ends);

   /* A stop keeps the session, and the input on it. A restart makes the
    * session again, the input told of the old one going before it went,
    * and its frames run: the only frame calls since EXITING. */
   vulkan_openxr_stop(xr);
   CHECK(!t_in_destroying, "a stop took the input off a kept session");
   CHECK(     vulkan_openxr_start(xr, vk_inst, gpu, dev, 0, lock)
         && t_sessions_made == 2 && t_sessions_destroyed == 1
         && t_in_created == 2 && t_in_destroying == 1
         && !t_in_destroyed_at && t_xr_host.user == xr,
         "restart: %u sessions made, %u destroyed, the input told of %u "
         "and %u gone (after %u destroyed)", t_sessions_made,
         t_sessions_destroyed, t_in_created, t_in_destroying,
         t_in_destroyed_at);
   t_push_shown();
   vulkan_openxr_begin_frame(xr);
   vulkan_openxr_end_frame(xr, 0);
   CHECK(     t_begun == 2 && t_waits == waits + 1 && t_begins == begins + 1
         && t_ends == ends + 1,
         "the restarted session's frame: %u waits, %u begins, %u ends",
         t_waits - waits, t_begins - begins, t_ends - ends);

   /* Everything goes at free. */
   vulkan_openxr_free(xr);
   CHECK(     t_sessions_destroyed == t_sessions_made
         && t_in_destroying == t_in_created
         && t_in_destroyed_at == t_sessions_destroyed - 1
         && t_spaces_destroyed == t_spaces_made
         && t_destroyed_instances == instances + 1 && !t_xr_host.user,
         "free: %u of %u sessions, %u of %u spaces, %u instances, the "
         "input told of %u of %u going, after session %u",
         t_sessions_destroyed, t_sessions_made, t_spaces_destroyed,
         t_spaces_made, t_destroyed_instances - instances, t_in_destroying,
         t_in_created, t_in_destroyed_at);

   /* A stop while the session shows the core's views keeps them, and
    * free's stop keeps what that one did. */
   memset(t_view, 0, sizeof(t_view));
   t_view[0].pose.orientation.w = 1.0f;
   t_view[1].pose.orientation.w = 1.0f;
   memset(&t_head, 0, sizeof(t_head));
   t_head.orientation.w         = 1.0f;
   t_views_tracked              = true;
   t_head_tracked               = true;
   xr = vulkan_openxr_new(false, VK_API_VERSION_1_1);
   CHECK(xr && vulkan_openxr_start(xr, vk_inst, gpu, dev, 0, lock),
         "a kept-views host did not start");
   if (!xr)
      goto done;
   t_push_shown();
   vulkan_openxr_begin_frame(xr);
   vulkan_openxr_end_frame(xr, 0);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   t_xr_host.set_space(t_xr_host.user, &space, &w, &ht);
   t_view[0].pose.position.x = -0.25f;
   t_xr_host.sample(t_xr_host.user, &flags);
   t_xr_host.eye_state(t_xr_host.user, eyes);
   vulkan_openxr_stop(xr);
   CHECK(     openxr_host_record.valid
         && t_near(openxr_host_record.views[0].pose.position.x, -0.25f),
         "a stop did not keep the views the core was given");
   vulkan_openxr_free(xr);
   CHECK(     openxr_host_record.valid
         && t_near(openxr_host_record.views[0].pose.position.x, -0.25f),
         "free's stop replaced the kept views");

   /* After a reinit, a resume with no new core frame places them, not
    * the head sampled since; a new frame drops them. */
   xr = vulkan_openxr_new(false, VK_API_VERSION_1_1);
   CHECK(xr && vulkan_openxr_start(xr, vk_inst, gpu, dev, 0, lock),
         "a resumed host did not start");
   if (!xr)
      goto done;
   t_push_shown();
   vulkan_openxr_begin_frame(xr);
   vulkan_openxr_end_frame(xr, 0);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   t_xr_host.set_space(t_xr_host.user, &space, &w, &ht);
   t_view[0].pose.position.x = 0.5f;
   t_xr_host.sample(t_xr_host.user, &flags);
   if (t_xr_host.resume)
      t_xr_host.resume(t_xr_host.user);
   vulkan_openxr_place_kept(xr);
   openxr_host_projection(&xr->host, 400, 240, &p);
   CHECK(     vulkan_openxr_has_given(xr) && p.active
         && t_near(p.pose[0].position.x, -0.25f),
         "a resume did not place the kept views: x %.2f",
         p.pose[0].position.x);
   vulkan_openxr_new_frame(xr);
   CHECK(!vulkan_openxr_has_given(xr), "kept views outlived a new core frame");

   /* A new core frame drops a record kept for the next start. */
   t_xr_host.eye_state(t_xr_host.user, eyes);
   vulkan_openxr_stop(xr);
   vulkan_openxr_start(xr, vk_inst, gpu, dev, 0, lock);
   kept = openxr_host_record.session;
   vulkan_openxr_new_frame(xr);
   CHECK(kept && !openxr_host_record.session,
         "a new core frame left the record (%d kept)", kept);
   vulkan_openxr_free(xr);

   /* A start that gave up leaves no record for the next. */
   xr = vulkan_openxr_new(false, VK_API_VERSION_1_1);
   CHECK(xr && vulkan_openxr_start(xr, vk_inst, gpu, dev, 0, lock),
         "a dropped host did not start");
   if (xr)
   {
      t_push_shown();
      vulkan_openxr_begin_frame(xr);
      vulkan_openxr_end_frame(xr, 0);
      space = RETRO_VR_REFERENCE_SPACE_STAGE;
      t_xr_host.set_space(t_xr_host.user, &space, &w, &ht);
      t_xr_host.sample(t_xr_host.user, &flags);
      t_xr_host.eye_state(t_xr_host.user, eyes);
      vulkan_openxr_drop(xr);
      CHECK(!openxr_host_record.session, "a drop left the record");
   }
   openxr_host_record.session = 1;
   t_missing = "xrCreateInstance";
   xr = vulkan_openxr_new(false, VK_API_VERSION_1_1);
   t_missing = NULL;
   CHECK(!xr && !openxr_host_record.session,
         "a failed new left the record");
   vulkan_openxr_free(xr);

done:
   memset(&openxr_host_record, 0, sizeof(openxr_host_record));
   t_views_tracked = false;
   t_head_tracked  = false;
   openxr_frame_set_input_hooks(NULL);
   vulkan_openxr_set_platform(NULL);
   slock_free(lock);
}

static void test_input_hooks(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_input_hooks_t hooks;
   int user = 0;

   t_frame_fresh(&rt, &s, &sc, &f);

   /* No input: nothing to call, and no layers of its. */
   openxr_frame_set_input_hooks(NULL);
   openxr_frame_input_attach(&f);
   CHECK(!f.extra_layers && !f.input_attached, "attached without hooks");
   openxr_frame_input_detach(&f);

   /* The session's handles, and its layers over the screens. */
   memset(&hooks, 0, sizeof(hooks));
   hooks.session_created    = t_in_created_fn;
   hooks.session_destroying = t_in_destroying_fn;
   hooks.frame_layers       = t_extra_layers;
   hooks.user               = &user;
   openxr_frame_set_input_hooks(&hooks);
   t_in_created = t_in_destroying = 0;
   memset(&t_in_handles, 0, sizeof(t_in_handles));
   openxr_frame_input_attach(&f);
   CHECK(t_in_created == 1 && t_in_handles.frame == &f
         && t_in_handles.instance == rt.instance
         && t_in_handles.session == s.session
         && t_in_handles.get_proc == rt.GetInstanceProcAddr
         && t_in_handles.frame_controller,
         "handles: %u calls, frame %d, instance %d, session %d, proc %d, "
         "frame controller %d", t_in_created, t_in_handles.frame == &f,
         t_in_handles.instance == rt.instance,
         t_in_handles.session == s.session,
         t_in_handles.get_proc == rt.GetInstanceProcAddr,
         (int)t_in_handles.frame_controller);
   CHECK(f.extra_layers == t_extra_layers && f.extra_user == &user,
         "the input's layers are not the frame's");
   /* Once a session: a second would leak the first's action sets. */
   openxr_frame_input_attach(&f);
   CHECK(t_in_created == 1, "attached %u times", t_in_created);
   openxr_session_set_alive(&s, true);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 1 && t_extra_cap == OPENXR_MAX_EXTRA_LAYERS,
         "%u layers with the input's, cap %u", t_last_layer_count,
         t_extra_cap);

   /* Detached once, however often asked; its layers go with it. */
   openxr_frame_input_detach(&f);
   openxr_frame_input_detach(&f);
   CHECK(t_in_destroying == 1 && !f.extra_layers && !f.extra_user,
         "detached %u times, layers %d", t_in_destroying,
         f.extra_layers != NULL);
   openxr_frame_run(&f);
   CHECK(t_last_layer_count == 0, "%u layers after the detach",
         t_last_layer_count);

   /* A host's dot is not the input's to touch. */
   f.cursor = (XrSwapchain)(uintptr_t)0x777;
   openxr_frame_input_attach(&f);
   openxr_frame_input_detach(&f);
   CHECK(openxr_frame_cursor(&f) == (XrSwapchain)(uintptr_t)0x777,
         "the dot is not the host's after a detach");
   f.cursor = XR_NULL_HANDLE;

   openxr_frame_set_input_hooks(NULL);
   t_frame_free(&rt, &s, &f);
}

/* A filled swapchain's binding: what its fill was given, and fills made
 * with the session's lock held. */
static unsigned t_fills, t_fills_locked;
static uint32_t t_fill_index;
static void    *t_fill_user;
static bool     t_fill_fails;
static bool t_fill_fn(void *user, uint32_t index, const char **step,
      int *code)
{
   t_fills++;
   t_fill_index = index;
   t_fill_user  = user;
   if (t_locks > t_unlocks)
      t_fills_locked++;
   if (!t_fill_fails)
      return true;
   *step = "the stand-in's fill";
   *code = -7;
   return false;
}

/* An image that never changes, as the laser's dot: made in the first
 * format the session lists, waited for at most 100 ms, filled outside
 * the lock, released. Any failure leaves no swapchain. */
static void test_swapchain_filled(void)
{
   static const int64_t formats[3] = { 37, 44, 50 };
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   XrSwapchain chain;
   const char *step  = NULL;
   int code          = 0;
   int dummy_binding = 0;
   int cb_user       = 0;
   int fill_user     = 0;
   unsigned dims     = VIDEO_SCALE_PACK(32, 32);

   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   CHECK(openxr_session_create(&s, &dummy_binding), "session create");
   memset(&sc, 0, sizeof(sc));
   CHECK(openxr_swapchains_load(&sc, &rt, &s), "load");
   openxr_swapchains_list_formats(&sc);
   t_sc_images        = 3;
   t_sc_images_second = 0;
   t_sc_wait          = XR_SUCCESS;
   t_sc_release       = XR_SUCCESS;
   t_sc_wait_timeout  = -1;
   t_sc_made = t_sc_destroyed = t_sc_acquires = t_sc_releases = 0;
   t_sc_unlocked      = 0;
   t_listed_got       = -1;
   t_listed[0]        = 0;
   t_fills = t_fills_locked = 0;
   t_fill_fails       = false;
   t_locks = t_unlocks = 0;

   chain = openxr_swapchains_create_filled(&sc, formats, 3, dims,
         t_images_cb, &cb_user, t_fill_fn, &fill_user, &step, &code);
   CHECK(chain && t_sc_made == 1 && !t_sc_destroyed,
         "made %u, destroyed %u", t_sc_made, t_sc_destroyed);
   CHECK(     t_ci_format == 44 && !t_ci_mutable
         && t_ci_width == 32 && t_ci_height == 32,
         "create info: format %d mutable %d %ux%u", (int)t_ci_format,
         (int)t_ci_mutable, t_ci_width, t_ci_height);
   CHECK(     t_listed[0] == 3 && t_listed_sc[0] == chain
         && t_listed_user[0] == &cb_user, "the binding listed no images");
   CHECK(     t_sc_acquires == 1 && t_sc_wait_timeout == 100000000
         && t_sc_releases == 1,
         "%u acquired, a %lld ns wait, %u released", t_sc_acquires,
         (long long)t_sc_wait_timeout, t_sc_releases);
   CHECK(     t_fills == 1 && t_fill_index == 1 && t_fill_user == &fill_user
         && !t_fills_locked,
         "%u fills of image %u, %u under the lock", t_fills, t_fill_index,
         t_fills_locked);
   CHECK(!t_sc_unlocked && t_locks && t_locks == t_unlocks,
         "%u runtime calls outside the lock", t_sc_unlocked);
   openxr_session_lock(&s);
   sc.DestroySwapchain(chain);
   openxr_session_unlock(&s);

   /* No format the session lists: nothing is made. */
   t_sc_made = t_fills = 0;
   step      = NULL;
   chain = openxr_swapchains_create_filled(&sc, formats, 1, dims,
         t_images_cb, &cb_user, t_fill_fn, &fill_user, &step, &code);
   CHECK(!chain && !t_sc_made && !t_fills && step,
         "with no format: made %u, filled %u", t_sc_made, t_fills);

   /* An image still busy after the wait, an index past the images the
    * binding got, a failed fill, a failed release: none left. */
   t_sc_made = t_sc_destroyed = t_fills = 0;
   t_sc_wait = XR_TIMEOUT_EXPIRED;
   chain = openxr_swapchains_create_filled(&sc, formats, 3, dims,
         t_images_cb, &cb_user, t_fill_fn, &fill_user, &step, &code);
   t_sc_wait = XR_SUCCESS;
   CHECK(     !chain && t_sc_made == 1 && t_sc_destroyed == 1 && !t_fills
         && code == XR_TIMEOUT_EXPIRED,
         "a busy image: made %u, destroyed %u, filled %u, code %d",
         t_sc_made, t_sc_destroyed, t_fills, code);
   t_sc_made = t_sc_destroyed = 0;
   t_listed_got = 1;
   chain = openxr_swapchains_create_filled(&sc, formats, 3, dims,
         t_images_cb, &cb_user, t_fill_fn, &fill_user, &step, &code);
   t_listed_got = -1;
   CHECK(!chain && t_sc_destroyed == t_sc_made && t_sc_made && !t_fills,
         "index 1 of 1 image: made %u, destroyed %u, filled %u", t_sc_made,
         t_sc_destroyed, t_fills);
   t_sc_made = t_sc_destroyed = 0;
   t_fill_fails = true;
   chain = openxr_swapchains_create_filled(&sc, formats, 3, dims,
         t_images_cb, &cb_user, t_fill_fn, &fill_user, &step, &code);
   t_fill_fails = false;
   CHECK(     !chain && t_sc_destroyed == t_sc_made && t_sc_made
         && step && !strcmp(step, "the stand-in's fill") && code == -7,
         "a failed fill: made %u, destroyed %u, step %s, code %d",
         t_sc_made, t_sc_destroyed, step ? step : "none", code);
   t_sc_made = t_sc_destroyed = 0;
   t_sc_release = XR_ERROR_RUNTIME_FAILURE;
   chain = openxr_swapchains_create_filled(&sc, formats, 3, dims,
         t_images_cb, &cb_user, t_fill_fn, &fill_user, &step, &code);
   t_sc_release = XR_SUCCESS;
   CHECK(     !chain && t_sc_destroyed == t_sc_made && t_sc_made
         && code == XR_ERROR_RUNTIME_FAILURE,
         "a failed release: made %u, destroyed %u, code %d", t_sc_made,
         t_sc_destroyed, code);
   CHECK(!t_sc_unlocked && !t_fills_locked,
         "%u runtime calls outside the lock, %u fills in it",
         t_sc_unlocked, t_fills_locked);

   openxr_session_destroy(&s);
   openxr_runtime_deinit(&rt);
}

static void test_screen_placement(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   video_xr_quad_set_t set, shown;
   XrCompositionLayerQuad layers[VIDEO_XR_MAX_QUADS];
   const XrCompositionLayerBaseHeader *ptrs[VIDEO_XR_MAX_QUADS];
   XrSpace space = XR_NULL_HANDLE;
   unsigned n    = 0;
   unsigned i;
   int seq;
   int cb_user   = 0;

   t_frame_fresh(&rt, &s, &sc, &f);
   openxr_slot_create(&sc, 0, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[0].content, 1);
   /* Screen 0 as a driver places it: from an identity anchor. */
   memset(&set, 0, sizeof(set));
   set.num_quads = 1;
   video_xr_pose_identity(&set.quads[0].pose);
   set.quads[0].pose.position.z = -1.8f;
   set.quads[0].width           = 1.6f;
   set.quads[0].height          = 0.9f;
   openxr_frame_publish(&f, &set);
   openxr_session_set_alive(&s, true);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);

   /* Before any frame: where the driver put them, in LOCAL. */
   openxr_frame_get_shown_quads(&f, &shown, &space);
   CHECK(space == s.local_space
         && t_pose_is(&shown.quads[0].pose, 0.0f, 1.0f, 0.0f, 0.0f, -1.8f),
         "before a frame: at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);

   /* Fixed in Room: at the anchor a recenter moved. */
   t_head_tracked       = true;
   memset(&t_head, 0, sizeof(t_head));
   t_head.orientation.y = 0.70710678f;
   t_head.orientation.w = 0.70710678f;
   t_head.position.x    = 0.5f;
   t_head.position.y    = 0.2f;
   t_head.position.z    = 0.3f;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &space);
   CHECK(space == s.local_space
         && t_pose_is(&shown.quads[0].pose, 0.70710678f, 0.70710678f,
            -1.3f, 0.2f, 0.3f),
         "Fixed: at %.2f,%.2f,%.2f", shown.quads[0].pose.position.x,
         shown.quads[0].pose.position.y, shown.quads[0].pose.position.z);
   CHECK(openxr_frame_layers(&f, layers, ptrs) == 1
         && layers[0].space == s.local_space
         && t_near(layers[0].pose.position.x, -1.3f)
         && t_near(layers[0].pose.position.z, 0.3f)
         && t_near(layers[0].pose.orientation.y, 0.70710678f),
         "Fixed: the layer at %.2f,%.2f", layers[0].pose.position.x,
         layers[0].pose.position.z);
   openxr_frame_get_quads(&f, &shown);
   CHECK(!memcmp(&shown, &set, sizeof(set)),
         "the driver's quads were changed");

   /* Follow Head: in VIEW, where the driver put them; published once in
    * the frame. */
   seq = retro_atomic_load_acquire_int(&f.tracked_seq);
   openxr_frame_set_placement(&f, VIDEO_OPENXR_PLACEMENT_FOLLOW);
   openxr_frame_run(&f);
   CHECK(retro_atomic_load_acquire_int(&f.tracked_seq) == seq + 2,
         "%d tracked publishes in a frame",
         (retro_atomic_load_acquire_int(&f.tracked_seq) - seq) / 2);
   openxr_frame_get_shown_quads(&f, &shown, &space);
   CHECK(space == s.view_space
         && t_pose_is(&shown.quads[0].pose, 0.0f, 1.0f, 0.0f, 0.0f, -1.8f),
         "Follow: at %.2f,%.2f,%.2f in %s", shown.quads[0].pose.position.x,
         shown.quads[0].pose.position.y, shown.quads[0].pose.position.z,
         space == s.view_space ? "VIEW" : "another space");
   CHECK(openxr_frame_layers(&f, layers, ptrs) == 1
         && layers[0].space == s.view_space
         && t_near(layers[0].pose.position.z, -1.8f),
         "Follow: the layer is not in VIEW in front of the eyes");

   /* Lazy Follow starts at the anchor; a 20 degree turn leaves it. */
   openxr_frame_set_placement(&f, VIDEO_OPENXR_PLACEMENT_LAZY);
   t_head.orientation.y = 0.81915204f;   /* 110 degrees */
   t_head.orientation.w = 0.57357644f;
   for (i = 0; i < 10; i++)
      openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &space);
   CHECK(space == s.local_space
         && t_pose_is(&shown.quads[0].pose, 0.70710678f, 0.70710678f,
            -1.3f, 0.2f, 0.3f),
         "Lazy: a 20 degree turn moved them to %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);
   /* Back to 0 degrees, 90 from them: they glide there, not jump. */
   t_head.orientation.y = 0.0f;
   t_head.orientation.w = 1.0f;
   openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &space);
   CHECK(   !t_pose_is(&shown.quads[0].pose, 0.70710678f, 0.70710678f,
               -1.3f, 0.2f, 0.3f)
         && !t_pose_is(&shown.quads[0].pose, 0.0f, 1.0f, 0.5f, 0.2f, -1.5f),
         "Lazy: no glide, at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);
   for (i = 0; i < 400; i++)
      openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &space);
   CHECK(t_pose_is(&shown.quads[0].pose, 0.0f, 1.0f, 0.5f, 0.2f, -1.5f),
         "Lazy: they ended at %.2f,%.2f,%.2f, want 0.50,0.20,-1.50",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);
   /* A recenter snaps them to its anchor. */
   memset(&t_head.position, 0, sizeof(t_head.position));
   t_head.orientation.y = 0.70710678f;
   t_head.orientation.w = 0.70710678f;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &space);
   CHECK(t_pose_is(&shown.quads[0].pose, 0.70710678f, 0.70710678f,
            -1.8f, 0.0f, 0.0f),
         "Lazy: the recenter left them at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);
   /* So does the runtime's; a head it cannot find moves nothing. */
   openxr_frame_local_changed(&f);
   t_head_tracked = false;
   for (i = 0; i < 10; i++)
      openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &space);
   CHECK(t_pose_is(&shown.quads[0].pose, 0.0f, 1.0f, 0.0f, 0.0f, -1.8f),
         "Lazy: after the runtime's recenter at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);

   openxr_slot_destroy(&sc, 0);
   t_frame_free(&rt, &s, &f);
}

/* The XR thread's frame at an unlock, and how many showed the screens
 * away from t_xr_want. */
static video_xr_pose_t t_xr_want;
static unsigned t_xr_off;
static void t_xr_frame_screens(void)
{
   t_xr_frame();
   if (!t_pose_is(&t_xr->screens.pose, t_xr_want.orientation.y,
            t_xr_want.orientation.w, t_xr_want.position.x,
            t_xr_want.position.y, t_xr_want.position.z))
      t_xr_off++;
}

/* A recenter moves the screens in the frame it moves a seated world:
 * they stay at the anchor its space was made at until the space is made
 * again, in Fixed in Room and Lazy Follow alike. A standing world stays,
 * so they take the new anchor at once. Only Lazy Follow locates the
 * head. */
static void test_screen_placement_seated(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   video_xr_quad_set_t set, shown;
   XrCompositionLayerQuad layers[VIDEO_XR_MAX_QUADS];
   const XrCompositionLayerBaseHeader *ptrs[VIDEO_XR_MAX_QUADS];
   enum retro_vr_reference_space space;
   XrSpace in       = XR_NULL_HANDLE;
   unsigned w       = 0;
   unsigned ht      = 0;
   unsigned n       = 0;
   unsigned i;
   unsigned locates;
   int cb_user      = 0;

   t_host_up(&rt, &s, &sc, &f, &h);
   openxr_slot_create(&sc, 0, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[0].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads = 1;
   video_xr_pose_identity(&set.quads[0].pose);
   set.quads[0].pose.position.z = -1.8f;
   set.quads[0].width           = 1.6f;
   set.quads[0].height          = 0.9f;
   openxr_frame_publish(&f, &set);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_frame_run(&f);

   /* Fixed in Room: the recenter's own frame leaves them with the
    * world, and the frame the space is made again in moves both. */
   t_host_recenter_at(&f, 0.5f);
   openxr_frame_get_shown_quads(&f, &shown, &in);
   CHECK(t_pose_is(&shown.quads[0].pose, 0.0f, 1.0f, 0.0f, 0.0f, -1.8f),
         "Fixed: before the seated world, at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);
   CHECK(     openxr_frame_layers(&f, layers, ptrs) == 1
         && t_near(layers[0].pose.position.x, 0.0f)
         && t_near(layers[0].pose.position.z, -1.8f),
         "Fixed: the layer before the seated world, at %.2f,%.2f",
         layers[0].pose.position.x, layers[0].pose.position.z);
   openxr_host_place_kept(&h);
   openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &in);
   CHECK(t_pose_is(&shown.quads[0].pose, 0.70710678f, 0.70710678f,
            -1.3f, 0.0f, 0.0f),
         "Fixed: not with the seated world, at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);

   /* Lazy Follow: held, however far the head goes, until the world
    * moves; then at the recenter's anchor. */
   openxr_frame_set_placement(&f, VIDEO_OPENXR_PLACEMENT_LAZY);
   openxr_frame_run(&f);
   t_host_recenter_at(&f, 0.25f);
   t_head.position.x = 3.0f;
   for (i = 0; i < 5; i++)
      openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &in);
   CHECK(t_pose_is(&shown.quads[0].pose, 0.70710678f, 0.70710678f,
            -1.3f, 0.0f, 0.0f),
         "Lazy: before the seated world, at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);
   openxr_host_place_kept(&h);
   openxr_frame_run(&f);
   openxr_frame_get_shown_quads(&f, &shown, &in);
   CHECK(t_pose_is(&shown.quads[0].pose, 0.70710678f, 0.70710678f,
            -1.55f, 0.0f, 0.0f),
         "Lazy: not with the seated world, at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);

   locates = t_head_locates;
   openxr_frame_run(&f);
   CHECK(t_head_locates == locates + 1, "Lazy: %u head locates in a frame",
         t_head_locates - locates);
   locates = t_head_locates;
   openxr_frame_set_placement(&f, VIDEO_OPENXR_PLACEMENT_FIXED);
   openxr_frame_run(&f);
   openxr_frame_set_placement(&f, VIDEO_OPENXR_PLACEMENT_FOLLOW);
   openxr_frame_run(&f);
   CHECK(t_head_locates == locates,
         "%u head locates in a Fixed and a Follow frame",
         t_head_locates - locates);

   /* Standing: at the recenter's anchor in its own frame. */
   openxr_frame_set_placement(&f, VIDEO_OPENXR_PLACEMENT_FIXED);
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_frame_run(&f);
   t_host_recenter_at(&f, 0.5f);
   openxr_frame_get_shown_quads(&f, &shown, &in);
   CHECK(t_pose_is(&shown.quads[0].pose, 0.70710678f, 0.70710678f,
            -1.3f, 0.0f, 0.0f),
         "standing: not at the recenter's anchor, at %.2f,%.2f,%.2f",
         shown.quads[0].pose.position.x, shown.quads[0].pose.position.y,
         shown.quads[0].pose.position.z);

   /* No STAGE: a headset frame in the seated fallback already has the
    * screens at the anchor. */
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_frame_get_anchor(&f, &t_xr_want);
   t_stage_missing = true;
   t_xr            = &f;
   t_xr_frames     = 0;
   t_xr_off        = 0;
   t_at_unlock     = t_xr_frame_screens;
   space           = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   t_at_unlock     = NULL;
   t_stage_missing = false;
   CHECK(space == RETRO_VR_REFERENCE_SPACE_LOCAL && t_xr_frames && !t_xr_off,
         "no STAGE: %u of %u headset frames at the fallback's makes showed "
         "the screens away from the anchor", t_xr_off, t_xr_frames);

   openxr_slot_destroy(&sc, 0);
   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

/* An in-line host's video frame, in vulkan.c's and gl2's order: a
 * running core's sample, the begin, the driver's Recenter request, then
 * the new frame's projection or, paused, place_kept, and the end. */
static void t_inline_frame(openxr_frame_t *f, openxr_host_t *h,
      bool running, bool recenter)
{
   openxr_frame_begun_t b;
   openxr_projection_t p;
   struct retro_vr_eye_state eyes[2];
   uint32_t flags = 0;
   if (running)
   {
      openxr_host_sample(h, &flags);
      openxr_host_eye_state(h, eyes);
   }
   openxr_frame_begin(f, &b);
   if (recenter)
      openxr_frame_request_recenter(f);
   if (running)
   {
      openxr_host_new_frame(h);
      openxr_host_projection(h, 400, 240, &p);
      openxr_frame_publish_projection(f, &p);
   }
   else
      openxr_host_place_kept(h);
   openxr_frame_end(f, &b);
}

/* In-line hosts remake a seated space at the next frame's sample while
 * the core runs, and at place_kept, after the begin, while it is paused.
 * Either way the first frame whose screens move at a recenter is the
 * first whose world moves, in Fixed in Room and Lazy Follow. */
static void test_screen_placement_inline(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   openxr_frame_begun_t b;
   video_xr_quad_set_t set;
   enum retro_vr_reference_space space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   unsigned w       = 0;
   unsigned ht      = 0;
   unsigned n       = 0;
   unsigned pass, i;
   int cb_user      = 0;

   t_host_up(&rt, &s, &sc, &f, &h);
   openxr_frame_start_inline(&f);
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   openxr_frame_begin(&f, &b);
   openxr_frame_end(&f, &b);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   openxr_slot_create(&sc, 0, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[0].content, 1);
   openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
         VIDEO_SCALE_PACK(1728, 1824), 2, t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(
         &sc.slots[OPENXR_PROJECTION_SLOT].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads = 1;
   video_xr_pose_identity(&set.quads[0].pose);
   set.quads[0].pose.position.z = -1.8f;
   set.quads[0].width           = 1.6f;
   set.quads[0].height          = 0.9f;
   openxr_frame_publish(&f, &set);
   openxr_host_set_space(&h, &space, &w, &ht);

   /* The head a quarter metre aside each time, unturned: no glide. */
   for (pass = 0; pass < 4; pass++)
   {
      XrSpace world;
      float x;
      bool running   = !(pass & 1);
      int screens_at = -1;
      int world_at   = -1;
      openxr_frame_set_placement(&f, (pass < 2)
            ? VIDEO_OPENXR_PLACEMENT_FIXED : VIDEO_OPENXR_PLACEMENT_LAZY);
      t_inline_frame(&f, &h, true, false);
      t_inline_frame(&f, &h, running, false);
      CHECK(t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION,
            "pass %u: no projection before the recenter", pass);
      world             = t_proj.space;
      x                 = t_quad.pose.position.x;
      t_head.position.x = 0.25f * (float)(pass + 1);
      for (i = 0; i < 4; i++)
      {
         t_inline_frame(&f, &h, running, i == 0);
         if (screens_at < 0 && !t_near(t_quad.pose.position.x, x))
            screens_at = (int)i;
         if (     world_at < 0 && t_proj.space != world
               && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
            world_at = (int)i;
      }
      CHECK(screens_at >= 0 && screens_at == world_at,
            "%s %s: the screens moved in frame %d, the world in frame %d",
            (pass < 2) ? "Fixed" : "Lazy", running ? "running" : "paused",
            screens_at, world_at);
      CHECK(t_near(t_quad.pose.position.x, t_head.position.x),
            "pass %u: the screens at %.2f, want %.2f", pass,
            t_quad.pose.position.x, t_head.position.x);
   }

   openxr_slot_destroy(&sc, 0);
   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);
   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

static void test_frame_repeat(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_frame_begun_t b;
   video_xr_quad_set_t set;
   retro_time_t t0;
   openxr_projection_t proj;
   XrPosef origin;
   enum retro_vr_reference_space space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   unsigned waits, ends, recenters;
   unsigned n  = 0;
   int cb_user = 0;

   t_frame_fresh(&rt, &s, &sc, &f);
   openxr_frame_start_inline(&f);
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   openxr_frame_begin(&f, &b);
   openxr_frame_end(&f, &b);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   openxr_slot_create(&sc, 1, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[1].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads     = 1;
   set.quads[0].slot = 1;
   video_xr_pose_identity(&set.quads[0].pose);
   openxr_frame_publish(&f, &set);

   /* A core frame, then two more headset frames showing it. */
   openxr_frame_begin(&f, &b);
   openxr_frame_end(&f, &b);
   waits = t_waits;
   ends  = t_ends;
   openxr_frame_repeat(&f, 2);
   CHECK(t_waits == waits + 2 && t_ends == ends + 2
         && t_last_layer_count == 1,
         "repeats: %u waits, %u ends, %u layers", t_waits - waits,
         t_ends - ends, t_last_layer_count);
   waits = t_waits;
   openxr_frame_repeat(&f, 0);
   CHECK(t_waits == waits, "a repeat with none asked");

   /* A repeat shows the projection too, until the track space moves. */
   memset(&origin, 0, sizeof(origin));
   origin.orientation.w = 1.0f;
   openxr_session_swap_track_space(&s, &space, &origin, NULL, NULL);
   openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
         VIDEO_SCALE_PACK(1728, 1824), 2, t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(
         &sc.slots[OPENXR_PROJECTION_SLOT].content, 1);
   memset(&proj, 0, sizeof(proj));
   proj.active                = true;
   proj.width                 = 1600;
   proj.height                = 1800;
   proj.pose[0].orientation.w = 1.0f;
   proj.pose[0].position.x    = -0.032f;
   proj.pose[1].orientation.w = 1.0f;
   proj.pose[1].position.x    = 0.032f;
   proj.track_seq             = retro_atomic_load_acquire_int(&s.track_seq);
   openxr_frame_publish_projection(&f, &proj);
   openxr_frame_begin(&f, &b);
   openxr_frame_end(&f, &b);
   memset(t_proj_views, 0, sizeof(t_proj_views));
   openxr_frame_repeat(&f, 1);
   CHECK(t_last_layer_count == 2
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_near(t_proj_views[0].pose.position.x, -0.032f),
         "a repeat without the projection: %u layers, the first %d",
         t_last_layer_count, (int)t_layer_types[0]);
   openxr_session_swap_track_space(&s, &space, &origin, NULL, NULL);
   openxr_frame_repeat(&f, 1);
   CHECK(t_last_layer_count == 1
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_QUAD,
         "a repeat showed a projection from the replaced space");
   openxr_frame_clear_projection(&f);
   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);

   /* A recenter asked for in a core frame waits for the next one. */
   t_head_tracked       = true;
   memset(&t_head, 0, sizeof(t_head));
   t_head.orientation.w = 1.0f;
   t_head.position.x    = 0.25f;
   openxr_frame_begin(&f, &b);
   recenters = openxr_frame_recenters(&f);
   openxr_frame_request_recenter(&f);
   openxr_frame_end(&f, &b);
   openxr_frame_repeat(&f, 1);
   CHECK(openxr_frame_recenters(&f) == recenters
         && retro_atomic_load_acquire_int(&f.recenter),
         "a repeat took the recenter");
   openxr_frame_begin(&f, &b);
   openxr_frame_end(&f, &b);
   CHECK(openxr_frame_recenters(&f) == recenters + 1
         && !retro_atomic_load_acquire_int(&f.recenter),
         "the next core frame did not take the recenter");
   t_head_tracked = false;

   /* The session down: each repeat is a period on the clock, after the
    * one that restarts its stale schedule. */
   retro_atomic_store_release_int(&s.running, 0);
   retro_atomic_store_release_int(&f.period_ns, 5000000);
   waits = t_waits;
   openxr_frame_repeat(&f, 1);
   t0    = (retro_time_t)(f.pace_anchor_ns / 1000);
   openxr_frame_repeat(&f, 2);
   CHECK(t_waits == waits
         && cpu_features_get_time_usec() - t0 >= 9000,
         "repeats with the session down: %u waits, %lld us",
         t_waits - waits, (long long)(cpu_features_get_time_usec() - t0));

   openxr_frame_stop_inline(&f);
   openxr_slot_destroy(&sc, 1);
   t_frame_free(&rt, &s, &f);
}

/* At an interval of 2 each core frame's repeat is submitted too. From a
 * recenter asked for in a core frame, no submit moves the screens
 * without the seated world or the world without them, running or
 * paused, and a Lazy glide holds from the recenter until the world
 * moves, then snaps. */
static void test_screen_placement_repeat(void)
{
   static const char *const names[6] = {
      "Fixed running", "Fixed paused", "Lazy running", "Lazy paused",
      "Lazy gliding running", "Lazy gliding paused" };
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_host_t h;
   openxr_frame_begun_t b;
   video_xr_quad_set_t set;
   enum retro_vr_reference_space space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   unsigned w       = 0;
   unsigned ht      = 0;
   unsigned n       = 0;
   unsigned pass, i, k;
   int cb_user      = 0;

   t_host_up(&rt, &s, &sc, &f, &h);
   openxr_frame_start_inline(&f);
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   openxr_frame_begin(&f, &b);
   openxr_frame_end(&f, &b);
   retro_atomic_store_release_int(&s.state, XR_SESSION_STATE_FOCUSED);
   openxr_slot_create(&sc, 0, 44, false, VIDEO_SCALE_PACK(64, 64), 1,
         t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(&sc.slots[0].content, 1);
   openxr_slot_create(&sc, OPENXR_PROJECTION_SLOT, 44, false,
         VIDEO_SCALE_PACK(1728, 1824), 2, t_images_cb, &cb_user, &n);
   retro_atomic_store_release_int(
         &sc.slots[OPENXR_PROJECTION_SLOT].content, 1);
   memset(&set, 0, sizeof(set));
   set.num_quads = 1;
   video_xr_pose_identity(&set.quads[0].pose);
   set.quads[0].pose.position.z = -1.8f;
   set.quads[0].width           = 1.6f;
   set.quads[0].height          = 0.9f;
   openxr_frame_publish(&f, &set);
   openxr_host_set_space(&h, &space, &w, &ht);

   for (pass = 0; pass < 6; pass++)
   {
      XrSpace world;
      float x, want;
      int recenters;
      bool running   = !(pass & 1);
      bool turned    = pass >= 4;
      bool glided    = false;
      int screens_at = -1;
      int world_at   = -1;
      openxr_frame_set_placement(&f, (pass < 2)
            ? VIDEO_OPENXR_PLACEMENT_FIXED : VIDEO_OPENXR_PLACEMENT_LAZY);
      t_inline_frame(&f, &h, true, false);
      openxr_frame_repeat(&f, 1);
      t_inline_frame(&f, &h, running, false);
      openxr_frame_repeat(&f, 1);
      if (turned)
      {
         /* A quarter turn, then back: a repeat glides, the world stays. */
         t_head.orientation.y = (pass == 4) ? 0.70710678f : 0.0f;
         t_head.orientation.w = (pass == 4) ? 0.70710678f : 1.0f;
         world                = t_proj.space;
         for (i = 0; i < 2; i++)
         {
            t_inline_frame(&f, &h, running, false);
            x = t_quad.pose.position.x;
            openxr_frame_repeat(&f, 1);
            if (!t_near(t_quad.pose.position.x, x))
               glided = true;
         }
         CHECK(glided && t_proj.space == world,
               "%s: no repeat glided, or the world moved", names[pass]);
      }
      else
         t_head.position.x = 0.25f * (float)(pass + 1);
      want      = t_head.position.x - ((pass == 4) ? 1.8f : 0.0f);
      world     = t_proj.space;
      x         = t_quad.pose.position.x;
      recenters = openxr_frame_recenters(&f);
      /* Even: a core frame, the first asking for the recenter. Odd: its
       * repeat. */
      for (k = 0; k < 8; k++)
      {
         bool taken, quads, moved;
         if (k & 1)
            openxr_frame_repeat(&f, 1);
         else
            t_inline_frame(&f, &h, running, k == 0);
         taken = openxr_frame_recenters(&f) != recenters;
         quads = !t_near(t_quad.pose.position.x, x);
         moved = t_proj.space != world;
         CHECK(t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION,
               "%s, submit %u: no projection", names[pass], k);
         CHECK(quads == moved || (turned && !taken && !moved),
               "%s, %s %u: the screens %s, the world %s", names[pass],
               (k & 1) ? "repeat" : "core frame", k / 2,
               quads ? "moved" : "stayed", moved ? "moved" : "stayed");
         if (screens_at < 0 && quads && (taken || !turned))
            screens_at = (int)k;
         if (world_at < 0 && moved)
            world_at = (int)k;
         x     = t_quad.pose.position.x;
         world = t_proj.space;
      }
      CHECK(world_at >= 0 && screens_at == world_at,
            "%s: the screens moved at submit %d, the world at %d",
            names[pass], screens_at, world_at);
      CHECK(t_near(t_quad.pose.position.x, want),
            "%s: the screens at %.2f, want %.2f", names[pass],
            t_quad.pose.position.x, want);
   }

   openxr_slot_destroy(&sc, 0);
   openxr_slot_destroy(&sc, OPENXR_PROJECTION_SLOT);
   t_views_tracked = false;
   t_head_tracked  = false;
   t_frame_free(&rt, &s, &f);
}

int main(void)
{
   test_runtime();
   test_session();
   test_swapchain();
   test_frame();
   test_frame_pacing();
   test_frame_lost();
   test_track_space();
   test_tracking();
   test_projection();
   test_host();
   test_host_remake();
   test_host_recenter_paused();
   test_host_recenter_locked();
   test_host_kept();
   test_host_record();
   test_host_same_space();
   test_runtime_platform();
   test_runtime_gaps();
   test_frame_inline();
   test_binding_gles();
   test_vulkan_inline();
   test_input_hooks();
   test_swapchain_filled();
   test_screen_placement();
   test_screen_placement_seated();
   test_screen_placement_inline();
   test_frame_repeat();
   test_screen_placement_repeat();
   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   puts("PASS openxr_core_test");
   return 0;
}
