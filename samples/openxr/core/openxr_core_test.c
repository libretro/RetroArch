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
#ifdef HAVE_OPENXR_GL
/* The desktop GL binding's native half: its headers, read once here. */
#include <EGL/egl.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#define XR_USE_PLATFORM_EGL
#define XR_USE_PLATFORM_XLIB
#else
typedef unsigned int EGLenum; /* the header wants EGL types */
#endif
#define XR_USE_GRAPHICS_API_OPENGL
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
static XrResult t_rate_result = XR_SUCCESS;
static float t_rate_applied;
static XRAPI_ATTR XrResult XRAPI_CALL t_request_rate(XrSession s, float hz)
{
   t_rate_requests++;
   if (XR_SUCCEEDED(t_rate_result))
      t_rate_applied = hz;
   return t_rate_result;
}
/* The levels last asked per domain, and how many asks. */
static unsigned t_perf_calls;
static unsigned t_perf_fail;
static XrPerfSettingsLevelEXT t_perf_cpu, t_perf_gpu;
static XRAPI_ATTR XrResult XRAPI_CALL t_perf_level(XrSession s,
      XrPerfSettingsDomainEXT domain, XrPerfSettingsLevelEXT level)
{
   t_perf_calls++;
   if (t_perf_fail & (domain == XR_PERF_SETTINGS_DOMAIN_CPU_EXT ? 1 : 2))
      return XR_ERROR_RUNTIME_FAILURE;
   if (domain == XR_PERF_SETTINGS_DOMAIN_CPU_EXT)
      t_perf_cpu = level;
   else if (domain == XR_PERF_SETTINGS_DOMAIN_GPU_EXT)
      t_perf_gpu = level;
   return XR_SUCCESS;
}

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
static XrResult t_create_session_res = XR_SUCCESS;
static XRAPI_ATTR XrResult XRAPI_CALL t_create_session(XrInstance i,
      const XrSessionCreateInfo *ci, XrSession *s)
{
   if (t_create_session_res != XR_SUCCESS)
      return t_create_session_res;
   t_sessions_made++;
   *s = T_SESSION;
   return XR_SUCCESS;
}
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
   XrSpaceVelocity *vel = (XrSpaceVelocity*)loc->next;
   if (vel && vel->type == XR_TYPE_SPACE_VELOCITY)
      vel->velocityFlags = 0;
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
static uint32_t t_sc_refuse_width;   /* a width the runtime refuses */
static XRAPI_ATTR XrResult XRAPI_CALL t_create_sc(XrSession s,
      const XrSwapchainCreateInfo *ci, XrSwapchain *out)
{
   if (t_sc_refuse_width && ci->width == t_sc_refuse_width)
      return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
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
static bool     t_fill_gl;      /* the same for desktop GL images */
static bool     t_gl_wrong_type; /* a GL list asked with another type */
static uint32_t t_gles_base;
static XRAPI_ATTR XrResult XRAPI_CALL t_enum_images(XrSwapchain s,
      uint32_t cap, uint32_t *n, XrSwapchainImageBaseHeader *imgs)
{
   uint32_t i;
   if (cap && imgs && t_fill_gl)
   {
      XrSwapchainImageOpenGLKHR *gl = (XrSwapchainImageOpenGLKHR*)imgs;
      t_gles_base += 0x100;
      for (i = 0; i < cap && i < t_sc_images; i++)
      {
         if (gl[i].type != XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR)
            t_gl_wrong_type = true;
         gl[i].image = t_gles_base + i;
      }
      *n = i;
      return XR_SUCCESS;
   }
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
/* Busy as well: one swapchain, and every one made from a count on. */
static XrSwapchain t_sc_busy;
static uint32_t t_sc_busy_from;
static XRAPI_ATTR XrResult XRAPI_CALL t_wait(XrSwapchain s,
      const XrSwapchainImageWaitInfo *wi)
{
   t_sc_wait_timeout = wi->timeout;
   if (     (t_sc_busy && s == t_sc_busy)
         || (t_sc_busy_from
            && (uint32_t)((uintptr_t)s - 0x100) >= t_sc_busy_from))
      return XR_TIMEOUT_EXPIRED;
   return t_sc_wait;
}
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

static XrResult t_gl_reqs_res = XR_SUCCESS;
static unsigned t_gl_reqs_asked;
static XRAPI_ATTR XrResult XRAPI_CALL t_gl_reqs(XrInstance i, XrSystemId s,
      XrGraphicsRequirementsOpenGLKHR *r)
{
   t_gl_reqs_asked++;
   r->minApiVersionSupported = XR_MAKE_VERSION(4, 3, 0);
   r->maxApiVersionSupported = XR_MAKE_VERSION(4, 6, 0);
   return t_gl_reqs_res;
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
   { "xrPerfSettingsSetPerformanceLevelEXT", (PFN_xrVoidFunction)t_perf_level },
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
   { "xrGetOpenGLGraphicsRequirementsKHR",   (PFN_xrVoidFunction)t_gl_reqs },
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
#include "../../../gfx/common/openxr_ring.c"
#include "../../../gfx/common/openxr_host.c"
#include "../../../gfx/common/openxr_binding_gles.c"
#include "../../../gfx/common/openxr_binding_gl.c"
#ifdef HAVE_OPENXR_GL
#include "../../../gfx/common/gl_openxr.c"
#endif
#include "../../../gfx/common/vulkan_openxr.c"

/* The input's hands: where and when the host had them located. */
static XrSpace t_hands_in;
static XrTime  t_hands_at;
static bool t_locate_hands(void *user, XrSpace base, XrTime time,
      struct retro_vr_controller_poses *out)
{
   t_hands_in = base;
   t_hands_at = time;
   out->hands[RETRO_VR_HAND_RIGHT].flags          = RETRO_VR_HAND_ACTIVE;
   out->hands[RETRO_VR_HAND_RIGHT].grip.position[0] = 0.3f;
   return true;
}

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

static void test_runtime_binding_ext(void)
{
   openxr_runtime_t rt;
   unsigned i, matches;

   t_runtime_defaults();
   t_exts[0] = "XR_KHR_opengl_enable";
   t_exts[3] = "XR_MNDX_egl_enable";
   t_exts[4] = "XR_TEST_platform";
   t_num_exts = 5;
   memset(&rt, 0, sizeof(rt));
   rt.binding_ext = "XR_MNDX_egl_enable";
   rt.platform.instance_ext = "XR_TEST_platform";
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc,
            "XR_KHR_opengl_enable") == OPENXR_RUNTIME_OK,
         "instance with two required extensions");
   CHECK(t_num_enabled == 5, "enabled %u of 5 extensions", t_num_enabled);
   matches = 0;
   for (i = 0; i < t_num_enabled; i++)
      if (string_is_equal(t_enabled[i], "XR_MNDX_egl_enable"))
         matches++;
   CHECK(matches == 1, "binding extension enabled %u times", matches);
   CHECK(rt.refresh_ext && rt.frame_controller,
         "binding extension displaced optional extensions");
   CHECK(string_is_equal(t_enabled[0], "XR_KHR_opengl_enable"),
         "graphics extension displaced");
   CHECK(t_num_enabled && string_is_equal(t_enabled[t_num_enabled - 1],
            "XR_TEST_platform"),
         "platform extension displaced");
   openxr_runtime_deinit(&rt);

   t_runtime_defaults();
   t_exts[0] = "XR_KHR_opengl_enable";
   memset(&rt, 0, sizeof(rt));
   rt.binding_ext = "XR_MNDX_egl_enable";
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc,
            "XR_KHR_opengl_enable") == OPENXR_RUNTIME_UNAVAILABLE,
         "accepted a runtime without the binding extension");
   CHECK(rt.instance == XR_NULL_HANDLE, "made an instance without the binding");
   CHECK(LOGGED("The runtime lacks XR_MNDX_egl_enable."),
         "missing binding extension not diagnosed");
   openxr_runtime_deinit(&rt);

   t_runtime_defaults();
   t_exts[0] = "XR_KHR_opengl_enable";
   t_num_exts = 1;
   memset(&rt, 0, sizeof(rt));
   rt.binding_ext = "XR_KHR_opengl_enable";
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc,
            "XR_KHR_opengl_enable") == OPENXR_RUNTIME_OK,
         "binding equal to graphics extension refused");
   CHECK(t_num_enabled == 1, "duplicate required extension enabled");
   openxr_runtime_deinit(&rt);

   t_runtime_defaults();
   memset(&rt, 0, sizeof(rt));
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc,
            "XR_KHR_vulkan_enable2") == OPENXR_RUNTIME_OK,
         "NULL binding changed Vulkan negotiation");
   CHECK(t_num_enabled == 3 && rt.refresh_ext && rt.frame_controller,
         "NULL binding changed enabled extensions");
   openxr_runtime_deinit(&rt);
   t_runtime_defaults();
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

/* The latched seqlock the frame's shared state goes through: a writer
 * stopped part way through a publication holds no reader up - they
 * read the last whole one - and readers on two threads, with a writer
 * going as fast as it can, never read two publications mixed. */
#define T_LATCH_N 24
static retro_atomic_int_t t_latch_seq;
static retro_atomic_int_t t_latch_words[2 * T_LATCH_N];
static retro_atomic_int_t t_latch_done;
static unsigned t_latch_mixed[2], t_latch_back[2];

static void t_latch_writer(void *data)
{
   int v[T_LATCH_N];
   int n, i;
   (void)data;
   for (n = 1; n <= 100000; n++)
   {
      for (i = 0; i < T_LATCH_N; i++)
         v[i] = n;
      openxr_seq_publish(&t_latch_seq, t_latch_words, v, sizeof(v));
   }
   retro_atomic_store_release_int(&t_latch_done, 1);
}

static void t_latch_reader(void *data)
{
   unsigned who = *(unsigned*)data;
   int last     = 0;
   while (!retro_atomic_load_acquire_int(&t_latch_done))
   {
      int v[T_LATCH_N];
      int i;
      openxr_seq_read(&t_latch_seq, t_latch_words, v, sizeof(v));
      for (i = 1; i < T_LATCH_N; i++)
         if (v[i] != v[0])
            t_latch_mixed[who]++;
      if (v[0] < last)
         t_latch_back[who]++;
      last = v[0];
   }
}

static void test_seq_latch(void)
{
   int a[T_LATCH_N], b[T_LATCH_N], got[T_LATCH_N];
   unsigned i, who[2] = { 0, 1 };
   sthread_t *w, *r0, *r1;

   for (i = 0; i < T_LATCH_N; i++)
   {
      a[i] = (int)i + 1;
      b[i] = (int)i + 100;
   }
   openxr_seq_publish(&t_latch_seq, t_latch_words, a, sizeof(a));
   openxr_seq_read(&t_latch_seq, t_latch_words, got, sizeof(got));
   CHECK(!memcmp(a, got, sizeof(a)), "latch: a publication not read back");

   /* a writer stopped half way through copy 0: the reader goes on with
    * the last whole publication, at once */
   {
      int s = retro_atomic_load_relaxed_int(&t_latch_seq);
      retro_atomic_store_release_int(&t_latch_seq, s + 1);
      for (i = 0; i < T_LATCH_N / 2; i++)
         retro_atomic_store_relaxed_int(&t_latch_words[i], -1);
      openxr_seq_read(&t_latch_seq, t_latch_words, got, sizeof(got));
      CHECK(!memcmp(a, got, sizeof(a)),
            "latch: a writer stopped part way through was read");
      retro_atomic_store_release_int(&t_latch_seq, s);
   }
   openxr_seq_publish(&t_latch_seq, t_latch_words, b, sizeof(b));
   openxr_seq_read(&t_latch_seq, t_latch_words, got, sizeof(got));
   CHECK(!memcmp(b, got, sizeof(b)), "latch: the next publication not read back");

   /* one writer, two readers */
   for (i = 0; i < T_LATCH_N; i++)
      a[i] = 0;
   openxr_seq_publish(&t_latch_seq, t_latch_words, a, sizeof(a));
   retro_atomic_store_release_int(&t_latch_done, 0);
   r0 = sthread_create(t_latch_reader, &who[0]);
   r1 = sthread_create(t_latch_reader, &who[1]);
   w  = sthread_create(t_latch_writer, NULL);
   CHECK(r0 && r1 && w, "latch: no threads");
   if (w)
      sthread_join(w);
   else
      retro_atomic_store_release_int(&t_latch_done, 1);
   if (r0)
      sthread_join(r0);
   if (r1)
      sthread_join(r1);
   CHECK(!t_latch_mixed[0] && !t_latch_mixed[1],
         "latch: a read mixed two publications (%u, %u)",
         t_latch_mixed[0], t_latch_mixed[1]);
   CHECK(!t_latch_back[0] && !t_latch_back[1],
         "latch: a read went back to an older publication");
   openxr_seq_read(&t_latch_seq, t_latch_words, got, sizeof(got));
   CHECK(got[0] == 100000 && got[T_LATCH_N - 1] == 100000,
         "latch: the newest was not read once the writer stopped");
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
   unsigned token = 0;
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
   CHECK(openxr_host_sample(&h, 0, &flags) && !flags, "first sample: flags %u",
         (unsigned)flags);
   CHECK(     s.track_space && t_space_type == XR_REFERENCE_SPACE_TYPE_LOCAL
         && t_located_in == s.track_space
         && t_located_at == openxr_frame_predicted_time(&f) + 13888889,
         "sampled at %lld, want the predicted time plus a period",
         (long long)t_located_at);
   CHECK(openxr_host_head_pose(&h, &head) && t_near(head.position[1], 1.25f),
         "head not as sampled");

   /* The hands are located only when asked for, then with the head: no
    * input, none; the input's, in the same space for the same time. */
   {
      struct retro_vr_controller_poses hands;
      memset(&hands, 0xff, sizeof(hands));
      CHECK(     openxr_host_controller_poses(&h, &hands)
            && hands.hands[0].flags == 0 && hands.hands[1].flags == 0,
            "hands without an input");
      f.locate_hands = t_locate_hands;
      t_hands_at     = 0;
      openxr_host_sample(&h, 0, &flags);
      CHECK(t_hands_at == 0, "a sample located the hands unasked");
      CHECK(openxr_host_locate_hands(&h), "the hands were not located");
      CHECK(     t_hands_in == s.track_space && t_hands_at == t_located_at
            && openxr_host_controller_poses(&h, &hands)
            && hands.hands[1].flags == RETRO_VR_HAND_ACTIVE
            && t_near(hands.hands[1].grip.position[0], 0.3f),
            "the hands were not sampled with the head");
      f.locate_hands = NULL;
   }

   /* The views the core was given are the projection's: handed out
    * with a token, given when the frame drawn from them is drawn. */
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(!p.active, "a projection before the core was given its eyes");
   CHECK(     openxr_host_eye_state(&h, eyes, &token)
         && t_near(eyes[1].position[0], 0.032f), "eyes not as sampled");
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(!p.active, "eyes handed out were shown before a frame drew them");
   {
      /* Overwritten since: four more handed out, none given */
      unsigned stale = token, i, t;
      for (i = 0; i < 4; i++)
         openxr_host_eye_state(&h, eyes, &t);
      openxr_host_give(&h, stale);
      openxr_host_projection(&h, 400, 240, &p);
      CHECK(!p.active, "eyes overwritten since were given");
      token = t;
   }
   openxr_host_give(&h, token);
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(     p.active && p.width == 400 && p.height == 240
         && p.track_seq == retro_atomic_load_acquire_int(&s.track_seq)
         && t_near(p.pose[0].position.x, -0.032f)
         && t_near(p.fov[1].angleRight, 0.8f),
         "the projection is not of the views the core was given");

   /* Frames already handed over: a period further for each */
   openxr_host_sample(&h, 1, &flags);
   CHECK(t_located_at == openxr_frame_predicted_time(&f)
         + 2 * (XrTime)retro_atomic_load_acquire_int(&f.period_ns),
         "sampled at %lld one frame ahead", (long long)t_located_at);

   /* Without a measured period, the predicted time alone. */
   retro_atomic_store_release_int(&f.period_ns, 0);
   openxr_host_sample(&h, 0, &flags);
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
   CHECK(     openxr_host_sample(&h, 0, &flags)
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
   openxr_host_sample(&h, 0, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");
   /* A sample that fails still tells of the recenter it took, for the
    * caller to keep until poses answer. */
   t_head.position.x = 0.5f;
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   t_views_tracked   = false;
   flags             = 0;
   CHECK(     !openxr_host_sample(&h, 0, &flags)
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
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made,
         "a recenter of standing content: flags %u, %u spaces made",
         (unsigned)flags, t_spaces_made - made);
   openxr_host_stage_changed(&h);
   flags = 0;
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED), "a STAGE move was not told");
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a STAGE move told twice");

   /* Seated content hears nothing of STAGE. */
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_stage_changed(&h);
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
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
   CHECK(!openxr_host_sample(&h, 0, &flags) && !t_located_in,
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
   CHECK(openxr_host_sample(&h, 0, &flags) && !flags,
         "a recenter told for the anchor the space was made at");
   t_host_recenter_at(&f, 0.5f);
   openxr_frame_get_anchor(&f, &anchor);
   made  = t_spaces_made;
   flags = 0;
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made + 1
         && t_near(t_space_pose.position.x, anchor.position.x),
         "STAGE-asked LOCAL content not remade: flags %u, %u made, x %.2f",
         (unsigned)flags, t_spaces_made - made, t_space_pose.position.x);
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
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
   openxr_host_sample(&h, 0, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED),
         "a recenter told for a remake that failed");
   t_space_fail_at = 0;
   flags           = 0;
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED)
         && t_near(t_space_pose.position.x, anchor.position.x),
         "a failed remake was not retried: flags %u", (unsigned)flags);
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");

   /* The runtime's own recenter moves the LOCAL origin: the space is
    * remade at identity. */
   openxr_frame_local_changed(&f);
   made  = t_spaces_made;
   flags = 0;
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made + 1
         && t_near(t_space_pose.position.x, 0.0f)
         && t_near(t_space_pose.orientation.w, 1.0f),
         "the runtime's recenter: flags %u, %u made", (unsigned)flags,
         t_spaces_made - made);
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
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
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made + 1
         && t_space_type == XR_REFERENCE_SPACE_TYPE_STAGE,
         "a new session: flags %u, %u made, type %d", (unsigned)flags,
         t_spaces_made - made, (int)t_space_type);
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
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
   unsigned token = 0;
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED) && t_spaces_made == made,
         "the next sample: flags %u, %u made", (unsigned)flags,
         t_spaces_made - made);
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED), "a recenter told twice");

   /* A remake that fails moves nothing and tells nothing; the sample
    * tries again. */
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED),
         "a recenter told for a remake that failed");
   t_space_fail_at = 0;
   flags           = 0;
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED)
         && retro_atomic_load_acquire_int(&s.track_seq) != seq,
         "a failed remake was not retried: flags %u", (unsigned)flags);

   /* Views and a projection from a space replaced since stay there. */
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
   made = t_spaces_made;
   seq  = retro_atomic_load_acquire_int(&s.track_seq);
   t_host_recenter_at(&f, 0.25f);
   openxr_host_place_kept(&h);
   CHECK(     t_spaces_made == made
         && retro_atomic_load_acquire_int(&s.track_seq) == seq
         && openxr_host_has_given(&h),
         "a recenter moved a standing space while paused");
   flags = 0;
   CHECK(     openxr_host_sample(&h, 0, &flags)
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
   unsigned token = 0;
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
      openxr_host_sample(&h, 0, &flags);
      openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
         openxr_host_sample(&h, 0, &flags);
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
   unsigned token = 0;
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
   openxr_host_sample(&h, 0, &flags);
   t_keep(&h, &kept);
   CHECK(!kept.valid && kept.session,
         "views kept before the core was given any");
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   t_resume(&h, &kept);
   openxr_host_projection(&h, 400, 240, &p);
   CHECK(     openxr_host_has_given(&h) && p.active
         && p.track_seq == retro_atomic_load_acquire_int(&s.track_seq)
         && t_near(p.pose[0].position.x, -0.25f),
         "the kept views are not the next STAGE space's projection");
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
   CHECK(!(flags & RETRO_VR_FRAME_RECENTERED),
         "a STAGE core was told of a recenter after a reinit");
   /* A new core frame was not drawn from them, but from eyes it asked
    * for since. */
   openxr_host_new_frame(&h);
   CHECK(!openxr_host_has_given(&h), "kept views outlived a new core frame");
   t_resume(&h, &kept);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
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
   CHECK(     openxr_host_sample(&h, 0, &flags)
         && (flags & RETRO_VR_FRAME_RECENTERED),
         "a seated core was not told its origin moved");
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   unsigned token = 0;
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   unsigned token = 0;
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
   seq  = retro_atomic_load_acquire_int(&s.track_seq);
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_host_set_space(&h, &space, &w, &ht);
   CHECK(     retro_atomic_load_acquire_int(&s.track_seq) == seq
         && openxr_host_has_given(&h),
         "a recenter remade the space asked again");
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
   CHECK(     (flags & RETRO_VR_FRAME_RECENTERED)
         && retro_atomic_load_acquire_int(&s.track_seq) != seq,
         "the sample did not remake the seated space at a recenter");
   /* No STAGE: it can only be seated again, so the space stays. */
   t_stage_missing = true;
   space = RETRO_VR_REFERENCE_SPACE_STAGE;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
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
   openxr_host_sample(&h, 0, &flags);
   openxr_host_eye_state(&h, eyes, &token);
   openxr_host_give(&h, token);
   openxr_host_new_frame(&h);
   space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   openxr_host_set_space(&h, &space, &w, &ht);
   openxr_host_resume(&h);
   openxr_host_place_kept(&h);
   flags = 0;
   openxr_host_sample(&h, 0, &flags);
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

/* Performance Boost: nothing asked while off from the start, CPU boost
 * once on, both domains back at sustained high (the runtime's default)
 * once off again, one ask per change, and nothing without the
 * extension or its function. */
static void test_frame_perf(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_frame_begun_t b;
   unsigned i;

   /* The extension is asked for when offered, dropped without its
    * function. */
   t_runtime_defaults();
   t_exts[t_num_exts++] = XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME;
   memset(&rt, 0, sizeof(rt));
   CHECK(openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2")
         == OPENXR_RUNTIME_OK, "instance with performance settings");
   CHECK(rt.perf_settings && rt.PerfSettingsSetPerformanceLevelEXT,
         "performance settings not loaded");
   openxr_runtime_deinit(&rt);
   t_runtime_defaults();
   t_exts[t_num_exts++] = XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME;
   t_missing = "xrPerfSettingsSetPerformanceLevelEXT";
   memset(&rt, 0, sizeof(rt));
   openxr_runtime_create_instance(&rt, t_get_proc, "XR_KHR_vulkan_enable2");
   CHECK(!rt.perf_settings, "performance settings kept without the function");
   openxr_runtime_deinit(&rt);

   t_frame_fresh(&rt, &s, &sc, &f);
   rt.perf_settings                      = true;
   rt.PerfSettingsSetPerformanceLevelEXT = t_perf_level;
   video_xr_set_perf_boost(false);
   t_perf_calls = 0;
   t_perf_cpu   = t_perf_gpu = (XrPerfSettingsLevelEXT)0;
   CHECK(openxr_frame_start_inline(&f), "in-line start");
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   for (i = 0; i < 3; i++)
   {
      openxr_frame_begin(&f, &b);
      openxr_frame_end(&f, &b);
   }
   CHECK(t_perf_calls == 0, "%u asks while off from the start", t_perf_calls);

   video_xr_set_perf_boost(true);
   for (i = 0; i < 3; i++)
   {
      openxr_frame_begin(&f, &b);
      openxr_frame_end(&f, &b);
   }
   CHECK(t_perf_calls == 2
         && t_perf_cpu == XR_PERF_SETTINGS_LEVEL_BOOST_EXT
         && t_perf_gpu == XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT,
         "on: %u asks, CPU %d, GPU %d", t_perf_calls, (int)t_perf_cpu,
         (int)t_perf_gpu);

   video_xr_set_perf_boost(false);
   for (i = 0; i < 3; i++)
   {
      openxr_frame_begin(&f, &b);
      openxr_frame_end(&f, &b);
   }
   CHECK(t_perf_calls == 4
         && t_perf_cpu == XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT
         && t_perf_gpu == XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT,
         "off again: %u asks, CPU %d, GPU %d", t_perf_calls,
         (int)t_perf_cpu, (int)t_perf_gpu);

   /* Without the extension: never asked. */
   rt.perf_settings = false;
   video_xr_set_perf_boost(true);
   openxr_frame_begin(&f, &b);
   openxr_frame_end(&f, &b);
   CHECK(t_perf_calls == 4, "asked without the extension");
   rt.perf_settings = true;
   for (i = 1; i <= 3; i++)
   {
      unsigned calls, applied = 3 ^ i;
      video_xr_set_perf_boost(false);
      openxr_frame_ask_perf(&f);
      video_xr_set_perf_boost(true);
      t_perf_fail = i;
      calls = t_perf_calls;
      openxr_frame_ask_perf(&f);
      CHECK(t_perf_calls == calls + 2 && f.perf_applied == applied,
            "partial failure %u lost applied domain state", i);
      calls = t_perf_calls;
      t_reset_log();
      openxr_frame_ask_perf(&f);
      CHECK(t_perf_calls == calls && !t_log[0],
            "failed performance request retried or logged before deadline");
      f.perf_retry_usec = 0;
      openxr_frame_ask_perf(&f);
      CHECK(t_perf_calls == calls + (i == 3 ? 2 : 1)
            && f.perf_applied == applied && !t_log[0],
            "retry repeated successful domain or warning");
      t_perf_fail = 0;
      calls = t_perf_calls;
      f.perf_retry_usec = 0;
      openxr_frame_ask_perf(&f);
      CHECK(t_perf_calls == calls + (i == 3 ? 2 : 1)
            && f.perf_applied == 3
            && t_perf_cpu == XR_PERF_SETTINGS_LEVEL_BOOST_EXT
            && t_perf_gpu == XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT,
            "failed performance request did not recover");
      calls = t_perf_calls;
      openxr_frame_ask_perf(&f);
      CHECK(t_perf_calls == calls, "completed request repeated");
   }
   video_xr_set_perf_boost(false);
   t_perf_fail = 1;
   openxr_frame_ask_perf(&f);
   CHECK(f.perf_applied == 2, "failed boost removal lost GPU success");
   video_xr_set_perf_boost(true);
   t_perf_fail = 0;
   openxr_frame_ask_perf(&f);
   CHECK(f.perf_applied == 3 && f.asked_perf == 1,
         "new setting was delayed by old retry deadline");
   openxr_frame_stop_inline(&f);
   CHECK(openxr_frame_start_inline(&f) && f.asked_perf == -1
         && !f.perf_applied && !f.perf_retry_usec,
         "session restart retained performance retry state");
   openxr_frame_ask_perf(&f);
   CHECK(f.perf_applied == 3, "session restart did not reapply boost");
   video_xr_set_perf_boost(false);
   t_frame_free(&rt, &s, &f);
}

static void test_frame_rate_retry(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   unsigned calls;

   t_frame_fresh(&rt, &s, &sc, &f);
   rt.refresh_ext = true;
   rt.RequestDisplayRefreshRateFB = t_request_rate;
   CHECK(openxr_frame_start_inline(&f), "rate retry in-line start");
   t_rate_requests = 0;
   t_rate_applied = 72.0f;
   t_rate_result = XR_ERROR_RUNTIME_FAILURE;
   openxr_frame_request_rate(&f, 90.0f);
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 1 && f.asked_rate == 0.0f
         && t_rate_applied == 72.0f, "failed rate cached as applied");
   t_reset_log();
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 1 && !t_log[0], "rate retry ignored deadline");
   f.rate_retry_usec = 0;
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 2 && !t_log[0], "rate retry missing or logged again");
   t_rate_result = XR_SUCCESS;
   f.rate_retry_usec = 0;
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 3 && f.asked_rate == 90.0f
         && t_rate_applied == 90.0f, "rate retry did not recover");
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 3, "accepted rate requested again");

   t_rate_result = XR_ERROR_RUNTIME_FAILURE;
   openxr_frame_request_rate(&f, 120.0f);
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 4 && f.asked_rate == 90.0f,
         "changed rate delayed or replaced accepted rate on failure");
   openxr_frame_request_rate(&f, 90.0f);
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 4, "return to accepted rate requested it again");
   openxr_frame_request_rate(&f, 120.0f);
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 5, "return to failed rate retained old deadline");
   openxr_frame_request_rate(&f, 0.0f);
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 5 && f.requested_rate == 0.0f,
         "disabled rate did not cancel retries");

   openxr_frame_stop_inline(&f);
   CHECK(openxr_frame_start_inline(&f) && !f.asked_rate
         && !f.requested_rate && !f.rate_retry_usec,
         "session restart retained rate state");
   t_rate_result = XR_SUCCESS;
   openxr_frame_request_rate(&f, 90.0f);
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == 6 && f.asked_rate == 90.0f,
         "session restart did not reapply rate");
   calls = t_rate_requests;
   openxr_frame_request_rate(&f, 120.0f);
   rt.refresh_ext = false;
   openxr_frame_ask_rate(&f);
   rt.refresh_ext = true;
   rt.RequestDisplayRefreshRateFB = NULL;
   openxr_frame_ask_rate(&f);
   CHECK(t_rate_requests == calls, "rate requested without extension or function");
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

   /* sRGB when the copy can keep the values; else RGBA8 first; sRGB
    * only with a warning; neither: none. */
   t_formats[0] = OPENXR_GLES_SRGB8_ALPHA8;
   t_formats[1] = OPENXR_GLES_RGBA8;
   openxr_swapchains_list_formats(&sc);
   CHECK(openxr_binding_gles_format(&sc, true) == OPENXR_GLES_SRGB8_ALPHA8,
         "sRGB not chosen with write control");
   CHECK(openxr_binding_gles_format(&sc, false) == OPENXR_GLES_RGBA8, "RGBA8 not chosen");
   t_formats[0] = OPENXR_GLES_RGBA8;
   t_formats[1] = 44;
   openxr_swapchains_list_formats(&sc);
   CHECK(openxr_binding_gles_format(&sc, true) == OPENXR_GLES_RGBA8,
         "RGBA8 not chosen without sRGB on offer");
   t_formats[0] = OPENXR_GLES_SRGB8_ALPHA8;
   t_formats[1] = OPENXR_GLES_SRGB8_ALPHA8;
   openxr_swapchains_list_formats(&sc);
   t_reset_log();
   CHECK(openxr_binding_gles_format(&sc, false) == OPENXR_GLES_SRGB8_ALPHA8
         && LOGGED("[OpenXR] Only an sRGB swapchain format is offered; the output may look washed out.\n"),
         "sRGB: %s", t_log);
   t_formats[0] = 50;
   t_formats[1] = 44;
   openxr_swapchains_list_formats(&sc);
   CHECK(!openxr_binding_gles_format(&sc, true), "a format from nothing");

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

static void test_binding_gl(void)
{
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_gl_images_t gi;
   uint32_t tex[OPENXR_MAX_IMAGES][2];
   unsigned n        = 0;
   int dummy_binding = 0;

   t_session_fresh(&rt, &s);

   /* Asked of the runtime, and held against the context's version. */
   t_gl_reqs_res   = XR_SUCCESS;
   t_gl_reqs_asked = 0;
   t_reset_log();
   CHECK(openxr_binding_gl_requirements(&rt, 4, 6)
         && t_gl_reqs_asked == 1
         && LOGGED("[OpenXR] OpenGL 4.3 to 4.6.\n"), "requirements: %s", t_log);
   CHECK(openxr_binding_gl_requirements(&rt, 4, 7),
         "a version past the tested one refused");
   t_reset_log();
   CHECK(!openxr_binding_gl_requirements(&rt, 3, 3)
         && LOGGED("[OpenXR] The runtime needs OpenGL 4.3; the context is 3.3.\n"),
         "an old context accepted: %s", t_log);
   CHECK(!openxr_binding_gl_requirements(&rt, 4, 2), "4.2 accepted");
   t_gl_reqs_res = XR_ERROR_RUNTIME_FAILURE;
   t_reset_log();
   CHECK(!openxr_binding_gl_requirements(&rt, 4, 6)
         && LOGGED("[OpenXR] xrGetOpenGLGraphicsRequirementsKHR failed (-2).\n"),
         "a failed answer: %s", t_log);
   t_gl_reqs_res = XR_SUCCESS;
   t_missing     = "xrGetOpenGLGraphicsRequirementsKHR";
   t_reset_log();
   CHECK(!openxr_binding_gl_requirements(&rt, 4, 6)
         && LOGGED("[OpenXR] The runtime lacks functions headset output needs.\n"),
         "without the function: %s", t_log);
   t_missing     = NULL;

   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);
   memset(&sc, 0, sizeof(sc));
   openxr_swapchains_load(&sc, &rt, &s);

   /* sRGB first, whatever the runtime's order; RGBA8 alone; else none. */
   t_formats[0] = OPENXR_GL_RGBA8;
   t_formats[1] = OPENXR_GL_SRGB8_ALPHA8;
   openxr_swapchains_list_formats(&sc);
   CHECK(openxr_binding_gl_format(&sc) == 0x8c43, "sRGB8_ALPHA8 not chosen");
   t_formats[0] = OPENXR_GL_SRGB8_ALPHA8;
   t_formats[1] = OPENXR_GL_RGBA8;
   openxr_swapchains_list_formats(&sc);
   CHECK(openxr_binding_gl_format(&sc) == 0x8c43, "sRGB8_ALPHA8 not first");
   t_formats[0] = 0x8c41;   /* SRGB8: no alpha */
   t_formats[1] = OPENXR_GL_RGBA8;
   openxr_swapchains_list_formats(&sc);
   t_reset_log();
   CHECK(openxr_binding_gl_format(&sc) == 0x8058
         && LOGGED("[OpenXR] Only a linear swapchain format is offered; the output may look washed out.\n"),
         "RGBA8 alone: %s", t_log);
   t_formats[0] = 50;
   t_formats[1] = 44;
   openxr_swapchains_list_formats(&sc);
   t_reset_log();
   CHECK(!openxr_binding_gl_format(&sc)
         && LOGGED("[OpenXR] No swapchain format for OpenGL.\n"),
         "a format from nothing: %s", t_log);

   /* Each layer's images land as its textures, asked as GL images. */
   memset(tex, 0, sizeof(tex));
   gi.sc            = &sc;
   gi.tex           = tex;
   t_fill_gl        = true;
   t_gl_wrong_type  = false;
   t_gles_base      = 0x400;
   t_sc_images      = 3;
   t_sc_images_second = 0;
   CHECK(openxr_slot_create(&sc, 2, 0x8c43, false,
            VIDEO_SCALE_PACK(64, 64), 2, openxr_binding_gl_images, &gi, &n)
         && n == 3, "slot with GL images: %u", n);
   CHECK(     tex[0][0] == 0x500 && tex[2][0] == 0x502
         && tex[0][1] == 0x600 && tex[2][1] == 0x602,
         "textures %x %x %x %x", tex[0][0], tex[2][0], tex[0][1], tex[2][1]);
   CHECK(!t_gl_wrong_type, "GL images asked as another type");
   openxr_slot_destroy(&sc, 2);
   /* The swapchain rules hold for GL: too many, or layers that differ. */
   t_sc_images = OPENXR_MAX_IMAGES + 1;
   CHECK(!openxr_slot_create(&sc, 2, 0x8c43, false,
            VIDEO_SCALE_PACK(64, 64), 1, openxr_binding_gl_images, &gi, &n),
         "more images than a slot holds");
   t_sc_images        = 3;
   t_sc_images_second = 2;
   CHECK(!openxr_slot_create(&sc, 2, 0x8c43, false,
            VIDEO_SCALE_PACK(64, 64), 2, openxr_binding_gl_images, &gi, &n),
         "layers with different image counts");
   t_sc_images_second = 0;
   t_fill_gl          = false;
   openxr_slot_destroy(&sc, 2);
   openxr_session_destroy(&s);
   openxr_runtime_deinit(&rt);
}

#ifdef HAVE_OPENXR_GL
static gfx_ctx_proc_t t_gl_proc(const char *name) { return NULL; }

static void test_binding_gl_chain(void)
{
   openxr_gl_binding_t b;
   gfx_ctx_shared_context_t ctx;
   const XrGraphicsBindingEGLMNDX *egl;
   const XrGraphicsBindingOpenGLXlibKHR *glx;

   memset(&ctx, 0, sizeof(ctx));
   ctx.kind             = GFX_CTX_SHARED_EGL;
   ctx.display          = (void*)0x10;
   ctx.config           = (void*)0x20;
   ctx.context          = (void*)0x30;
   ctx.surface          = (void*)0x40;
   ctx.get_proc_address = t_gl_proc;
   memset(&b, 0x5a, sizeof(b));
   egl = (const XrGraphicsBindingEGLMNDX*)openxr_binding_gl_chain(&b, &ctx);
   CHECK(egl && egl->type == XR_TYPE_GRAPHICS_BINDING_EGL_MNDX && !egl->next
         && egl->display == (EGLDisplay)0x10 && egl->config == (EGLConfig)0x20
         && egl->context == (EGLContext)0x30
         && egl->getProcAddress == (PFN_xrEglGetProcAddressMNDX)t_gl_proc,
         "EGL binding");

   memset(&ctx, 0, sizeof(ctx));
   ctx.kind      = GFX_CTX_SHARED_GLX;
   ctx.display   = (void*)0x11;
   ctx.config    = (void*)0x21;
   ctx.context   = (void*)0x31;
   ctx.drawable  = 0x41;
   ctx.visual_id = 0x29b;
   memset(&b, 0x5a, sizeof(b));
   glx = (const XrGraphicsBindingOpenGLXlibKHR*)openxr_binding_gl_chain(&b, &ctx);
   CHECK(glx && glx->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR
         && !glx->next && glx->xDisplay == (Display*)0x11
         && glx->glxFBConfig == (GLXFBConfig)0x21
         && glx->glxContext == (GLXContext)0x31
         && glx->glxDrawable == 0x41 && glx->visualid == 0x29b,
         "GLX binding");

   memset(&ctx, 0, sizeof(ctx));
   CHECK(!openxr_binding_gl_chain(&b, &ctx), "a binding for no context");
}

#endif

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
   unsigned token = 0;
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
   t_xr_host.sample(t_xr_host.user, 0, &flags);
   t_xr_host.eye_state(t_xr_host.user, eyes, &token);
   t_xr_host.give(t_xr_host.user, token);
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
   t_xr_host.sample(t_xr_host.user, 0, &flags);
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
   t_xr_host.eye_state(t_xr_host.user, eyes, &token);
   t_xr_host.give(t_xr_host.user, token);
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
      t_xr_host.sample(t_xr_host.user, 0, &flags);
      t_xr_host.eye_state(t_xr_host.user, eyes, &token);
      t_xr_host.give(t_xr_host.user, token);
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
   unsigned token = 0;
   uint32_t flags = 0;
   if (running)
   {
      openxr_host_sample(h, 0, &flags);
      openxr_host_eye_state(h, eyes, &token);
      openxr_host_give(h, token);
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

enum t_thread_hook_case
{
   T_THREAD_FRAME,
   T_THREAD_STOPPED,
   T_THREAD_WAIT_FAILED,
   T_THREAD_BEGIN_FAILED,
   T_THREAD_START_FAILED,
   T_THREAD_START_EMPTY
};

typedef struct t_thread_hooks
{
   openxr_frame_t *frame;
   slock_t *lock;
   retro_atomic_int_t starts, fills, stops, errors;
   enum t_thread_hook_case which;
   unsigned phase;
   int ticks;
   XrTime time;
} t_thread_hooks_t;

static t_thread_hooks_t *t_active_thread_hooks;

static void t_thread_lock(void *user)
{
   t_thread_hooks_t *t = (t_thread_hooks_t*)user;
   slock_lock(t->lock);
   t_hook_lock(NULL);
}

static void t_thread_unlock(void *user)
{
   t_thread_hooks_t *t = (t_thread_hooks_t*)user;
   t_hook_unlock(NULL);
   slock_unlock(t->lock);
}

static bool t_thread_start(void *user)
{
   static int binding;
   t_thread_hooks_t *t = (t_thread_hooks_t*)user;
   retro_atomic_fetch_add_int(&t->starts, 1);
   if (t->which == T_THREAD_START_EMPTY)
      return false;
   if (!openxr_session_create(t->frame->session, &binding))
      return false;
   openxr_frame_session_created(t->frame);
   return t->which != T_THREAD_START_FAILED;
}

static void t_thread_fill(void *user, XrTime time)
{
   t_thread_hooks_t *t = (t_thread_hooks_t*)user;
   openxr_session_lock(t->frame->session);
   if (t->phase != 1 || time != t->time || t_locks - t_unlocks != 1)
      retro_atomic_fetch_or_int(&t->errors, 1);
   /* The core is woken after the host takes what it drew: a frame it
    * finished first would be shown a headset frame early. */
   if (retro_atomic_load_acquire_int(&t->frame->tick_seq) != t->ticks)
      retro_atomic_fetch_or_int(&t->errors, 8);
   t->phase = 2;
   retro_atomic_fetch_add_int(&t->fills, 1);
   openxr_session_unlock(t->frame->session);
}

static void t_thread_stop(void *user)
{
   t_thread_hooks_t *t = (t_thread_hooks_t*)user;
   openxr_session_destroy(t->frame->session);
   retro_atomic_fetch_add_int(&t->stops, 1);
}

static XRAPI_ATTR XrResult XRAPI_CALL t_thread_wait_frame(XrSession s,
      const XrFrameWaitInfo *wi, XrFrameState *state)
{
   t_thread_hooks_t *t = t_active_thread_hooks;
   if (t->which == T_THREAD_WAIT_FAILED)
   {
      retro_atomic_store_release_int(&t->frame->quit, 1);
      return XR_ERROR_RUNTIME_FAILURE;
   }
   t_wait_frame(s, wi, state);
   t->time  = state->predictedDisplayTime;
   t->phase = 1;
   t->ticks = retro_atomic_load_acquire_int(&t->frame->tick_seq);
   return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL t_thread_begin_frame(XrSession s,
      const XrFrameBeginInfo *bi)
{
   t_thread_hooks_t *t = t_active_thread_hooks;
   if (     t->phase != 2
         || retro_atomic_load_acquire_int(&t->frame->tick_seq)
            != t->ticks + 1)
      retro_atomic_fetch_or_int(&t->errors, 2);
   t->phase = 3;
   if (t->which == T_THREAD_BEGIN_FAILED)
   {
      retro_atomic_store_release_int(&t->frame->quit, 1);
      return XR_ERROR_RUNTIME_FAILURE;
   }
   return t_begin_frame(s, bi);
}

static XRAPI_ATTR XrResult XRAPI_CALL t_thread_end_frame(XrSession s,
      const XrFrameEndInfo *ei)
{
   t_thread_hooks_t *t = t_active_thread_hooks;
   XrResult result = t_end_frame(s, ei);
   if (t->phase != 3)
      retro_atomic_fetch_or_int(&t->errors, 4);
   retro_atomic_store_release_int(&t->frame->quit, 1);
   return result;
}

static void test_frame_thread_hooks(void)
{
   enum t_thread_hook_case which;
   for (which = T_THREAD_FRAME; which <= T_THREAD_START_EMPTY; which++)
   {
      openxr_runtime_t rt;
      openxr_session_t s;
      openxr_swapchains_t sc;
      openxr_frame_t f;
      t_thread_hooks_t t;
      bool started, restarted = false;
      unsigned made, destroyed, begins, ends;
      int expected_fill = which == T_THREAD_FRAME || which == T_THREAD_BEGIN_FAILED;

      t_frame_fresh(&rt, &s, &sc, &f);
      openxr_session_destroy(&s);
      memset(&t, 0, sizeof(t));
      t.frame = &f;
      t.which = which;
      t.lock  = slock_new();
      CHECK(t.lock, "no graphics lock");
      if (!t.lock)
      {
         t_frame_free(&rt, &s, &f);
         return;
      }
      retro_atomic_int_init(&t.starts, 0);
      retro_atomic_int_init(&t.fills, 0);
      retro_atomic_int_init(&t.stops, 0);
      retro_atomic_int_init(&t.errors, 0);
      s.hooks.lock        = t_thread_lock;
      s.hooks.unlock      = t_thread_unlock;
      s.hooks.user        = &t;
      f.thread_hooks.start = t_thread_start;
      f.thread_hooks.fill  = t_thread_fill;
      f.thread_hooks.stop  = t_thread_stop;
      f.thread_hooks.user  = &t;
      f.WaitFrame         = t_thread_wait_frame;
      f.BeginFrame        = t_thread_begin_frame;
      f.EndFrame          = t_thread_end_frame;
      openxr_frame_set_pacing(&f, 1);
      t_active_thread_hooks = &t;
      if (which != T_THREAD_STOPPED)
      {
         t_push_state(T_SESSION, XR_SESSION_STATE_READY);
         t_push_state(T_SESSION, XR_SESSION_STATE_FOCUSED);
      }
      made      = t_sessions_made;
      destroyed = t_sessions_destroyed;
      begins    = t_begins;
      ends      = t_ends;
      started   = openxr_frame_start(&f);
      if (started)
         restarted = openxr_frame_start(&f);
      /* The stand-in ends a frame itself; give that thread a bounded run. */
      if (started && which != T_THREAD_STOPPED)
      {
         retro_time_t deadline = cpu_features_get_time_usec() + 2000000;
         while (!retro_atomic_load_acquire_int(&t.stops)
               && cpu_features_get_time_usec() < deadline)
            retro_sleep(1);
      }
      openxr_frame_stop(&f);
      CHECK(started == (which < T_THREAD_START_FAILED) && !restarted,
            "case %d: wrong startup result", which);
      CHECK(!f.thread && !s.session && !openxr_session_alive(&s),
            "case %d: thread or session escaped", which);
      CHECK(retro_atomic_load_acquire_int(&t.starts) == 1
            && retro_atomic_load_acquire_int(&t.stops) == 1,
            "case %d: start/stop count", which);
      CHECK(retro_atomic_load_acquire_int(&t.fills) == expected_fill,
            "case %d: fill count", which);
      CHECK(!retro_atomic_load_acquire_int(&t.errors),
            "case %d: hook order/lock errors", which);
      CHECK(t_sessions_made == made + (which != T_THREAD_START_EMPTY)
            && t_sessions_destroyed == destroyed + (which != T_THREAD_START_EMPTY),
            "case %d: session cleanup", which);
      CHECK(t_begins == begins + (which == T_THREAD_FRAME)
            && t_ends == ends + (which == T_THREAD_FRAME),
            "case %d: submitted a failed or stopped frame", which);
      openxr_frame_stop(&f);
      CHECK(retro_atomic_load_acquire_int(&t.stops) == 1,
            "case %d: repeated stop called the hook", which);
      t_frame_free(&rt, &s, &f);
      slock_free(t.lock);
      t_active_thread_hooks = NULL;
   }
}

static void test_frame_carried_seq(void)
{
   static int binding;
   openxr_runtime_t rt;
   openxr_session_t s;
   openxr_swapchains_t sc;
   openxr_frame_t f;
   openxr_projection_t p, got;

   t_frame_fresh(&rt, &s, &sc, &f);
   memset(&p, 0, sizeof(p));
   p.active             = true;
   p.track_seq          = 10;
   p.width              = 400;
   p.height             = 240;
   p.pose[0].position.x = -0.03f;
   p.pose[1].position.x = 0.03f;
   p.fov[0].angleLeft   = -0.75f;
   p.fov[1].angleRight  = 0.8f;
   openxr_session_lock(&s);
   openxr_frame_publish_projection(&f, &p);
   openxr_frame_move_projection(&f, 10, 11);
   CHECK(openxr_frame_carried_seq(&f, 10) == 11, "first carry missing");
   openxr_frame_move_projection(&f, 11, 12);
   CHECK(openxr_frame_carried_seq(&f, 10) == 12
         && openxr_frame_carried_seq(&f, 11) == 12
         && openxr_frame_carried_seq(&f, 9) == 9,
         "consecutive carries not composed");
   openxr_seq_read(&f.proj_seq, f.proj_words, &got, sizeof(got));
   p.track_seq = 12;
   CHECK(!memcmp(&p, &got, sizeof(p)), "carry changed projection poses or size");
   openxr_frame_clear_projection(&f);
   openxr_frame_move_projection(&f, 12, 13);
   CHECK(openxr_frame_carried_seq(&f, 10) == 13,
         "carry lost a queued projection while none was published");
   openxr_frame_move_projection(&f, 20, 21);
   CHECK(openxr_frame_carried_seq(&f, 20) == 21
         && openxr_frame_carried_seq(&f, 10) == 10,
         "unrelated space was included in carry");
   openxr_session_unlock(&s);
   openxr_session_reset(&s);
   openxr_session_create(&s, &binding);
   openxr_frame_session_created(&f);
   openxr_session_lock(&s);
   CHECK(openxr_frame_carried_seq(&f, 20) == 20,
         "new session kept a carry mapping");
   openxr_session_unlock(&s);
   t_frame_free(&rt, &s, &f);
}

static void t_ring_permutation(const openxr_ring_t *r)
{
   int back   = openxr_ring_back(r);
   int front  = openxr_ring_front(r);
   int middle = retro_atomic_load_acquire_int(&r->middle) & 3;
   CHECK(back >= 0 && back < OPENXR_RING_SIZE, "back index %d", back);
   CHECK(front >= 0 && front < OPENXR_RING_SIZE, "front index %d", front);
   CHECK(middle >= 0 && middle < OPENXR_RING_SIZE, "middle index %d", middle);
   CHECK(back != front && back != middle && front != middle,
         "ring indices overlap: %d/%d/%d", back, middle, front);
}

static void test_ring_indices(void)
{
   openxr_ring_t r;
   unsigned i;
   int front = -1;
   int published;
   int held;
   int back;

   openxr_ring_init(&r);
   t_ring_permutation(&r);
   CHECK(!openxr_ring_take(&r, &front), "new ring has a publication");
   CHECK(front == -1, "empty take changed its output");
   published = openxr_ring_back(&r);
   back = openxr_ring_publish(&r);
   CHECK(back == openxr_ring_back(&r),
         "publish did not return the next back index");
   t_ring_permutation(&r);
   CHECK(openxr_ring_take(&r, &front), "publication missing");
   CHECK(front == published && front == openxr_ring_front(&r),
         "take did not acquire the published record");
   CHECK(!openxr_ring_take(&r, &front), "publication taken twice");

   held = front;
   for (i = 0; i < 101; i++)
   {
      published = openxr_ring_back(&r);
      CHECK(published != held, "producer acquired the held front");
      CHECK(openxr_ring_publish(&r) != held,
            "publish returned the held front");
      t_ring_permutation(&r);
   }
   CHECK(openxr_ring_take(&r, &front), "latest publication missing");
   CHECK(front == published, "take did not acquire the latest record");
   CHECK(!openxr_ring_take(&r, &front), "latest publication taken twice");
   t_ring_permutation(&r);
}

#define T_RING_RECORDS 200000
typedef struct t_ring_threads
{
   openxr_ring_t ring;
   struct { unsigned a, b; } records[OPENXR_RING_SIZE];
   retro_atomic_int_t done;
} t_ring_threads_t;

static void t_ring_produce(void *data)
{
   t_ring_threads_t *t = (t_ring_threads_t*)data;
   unsigned seq;
   for (seq = 1; seq <= T_RING_RECORDS; seq++)
   {
      int back = openxr_ring_back(&t->ring);
      t->records[back].a = seq;
      t->records[back].b = seq;
      openxr_ring_publish(&t->ring);
   }
   retro_atomic_store_release_int(&t->done, 1);
}

static void t_ring_consume(t_ring_threads_t *t, unsigned *last)
{
   int front;
   unsigned a, b;
   if (!openxr_ring_take(&t->ring, &front))
      return;
   CHECK(front >= 0 && front < OPENXR_RING_SIZE, "taken index %d", front);
   if (front < 0 || front >= OPENXR_RING_SIZE)
      return;
   a = t->records[front].a;
   b = t->records[front].b;
   CHECK(a == b, "torn record: %u/%u", a, b);
   CHECK(a > *last, "sequence %u follows %u", a, *last);
   *last = a;
}

static void test_ring_threads(void)
{
   t_ring_threads_t t;
   sthread_t *thread;
   unsigned last = 0;
   memset(t.records, 0, sizeof(t.records));
   openxr_ring_init(&t.ring);
   retro_atomic_int_init(&t.done, 0);
   thread = sthread_create(t_ring_produce, &t);
   CHECK(thread, "no ring producer thread");
   if (!thread)
      return;
   while (!retro_atomic_load_acquire_int(&t.done))
      t_ring_consume(&t, &last);
   t_ring_consume(&t, &last);
   sthread_join(thread);
   CHECK(last == T_RING_RECORDS, "final sequence %u", last);
}

#ifdef HAVE_OPENXR_GL
/* ---- the desktop GL host, on a GL that only keeps books ---- */

/* Which thread made a call: the test's (the video thread) or the XR
 * thread. */
static uintptr_t t_video_thread;
static bool t_on_xr(void) { return sthread_get_current_thread_id() != t_video_thread; }

#define T_GL_NAMES 0x2000
#define T_GL_SYNCS 1024
#define T_GL_TRACE 8192
enum t_gl_op
{
   T_GL_WAIT = 1, T_GL_FENCE, T_GL_DELSYNC, T_GL_COPY, T_GL_GENTEX,
   T_GL_DELTEX, T_GL_STORAGE, T_GL_BLIT
};
typedef struct t_gl_call { bool xr; unsigned op, a, b; } t_gl_call_t;
typedef struct t_gl_sync { bool alive, xr_made, xr_deleted; unsigned deletes; } t_gl_sync_t;
static slock_t *t_gl_lock;
static unsigned t_gl_next_tex, t_gl_next_sync, t_gl_next_fbo;
static unsigned t_tex_content[T_GL_NAMES];
static bool t_tex_alive[T_GL_NAMES];
static unsigned t_tex_dims[T_GL_NAMES];
static t_gl_sync_t t_syncs[T_GL_SYNCS];
static int t_fbos_alive;
static unsigned t_gl_bound[2];            /* GL_TEXTURE_2D, per thread */
static unsigned t_gl_read_att, t_gl_draw_att, t_gl_read_fbo, t_gl_draw_fbo;
static t_gl_call_t t_gl_calls[T_GL_TRACE];
static unsigned t_gl_num_calls;
static unsigned t_gl_copies, t_gl_xr_wrong;
static GLenum t_gl_error;
static int t_gl_version;
static const char *t_gl_missing;

static void t_gl_reset(void)
{
   if (!t_gl_lock)
      t_gl_lock = slock_new();
   t_video_thread = sthread_get_current_thread_id();
   t_gl_next_tex  = 0x1000;
   t_gl_next_sync = 1;
   t_gl_next_fbo  = 1;
   memset(t_tex_content, 0, sizeof(t_tex_content));
   memset(t_tex_alive, 0, sizeof(t_tex_alive));
   memset(t_tex_dims, 0, sizeof(t_tex_dims));
   memset(t_syncs, 0, sizeof(t_syncs));
   memset(t_gl_bound, 0, sizeof(t_gl_bound));
   t_fbos_alive   = 0;
   t_gl_num_calls = t_gl_copies = t_gl_xr_wrong = 0;
   t_gl_read_att  = t_gl_draw_att = t_gl_read_fbo = t_gl_draw_fbo = 0;
   t_gl_error     = GL_NO_ERROR;
   t_gl_version   = 46;
   t_gl_missing   = NULL;
}

static void t_gl_trace(unsigned op, unsigned a, unsigned b)
{
   if (t_gl_num_calls < T_GL_TRACE)
   {
      t_gl_calls[t_gl_num_calls].xr = t_on_xr();
      t_gl_calls[t_gl_num_calls].op = op;
      t_gl_calls[t_gl_num_calls].a  = a;
      t_gl_calls[t_gl_num_calls].b  = b;
      t_gl_num_calls++;
   }
}

static void APIENTRY t_glGenTextures(GLsizei n, GLuint *names)
{
   GLsizei i;
   slock_lock(t_gl_lock);
   for (i = 0; i < n; i++)
   {
      names[i] = t_gl_next_tex++;
      t_tex_alive[names[i]] = true;
      t_gl_trace(T_GL_GENTEX, names[i], 0);
   }
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glDeleteTextures(GLsizei n, const GLuint *names)
{
   GLsizei i;
   slock_lock(t_gl_lock);
   for (i = 0; i < n; i++)
      if (names[i] && names[i] < T_GL_NAMES)
      {
         t_tex_alive[names[i]] = false;
         t_gl_trace(T_GL_DELTEX, names[i], 0);
      }
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glBindTexture(GLenum target, GLuint name)
{ t_gl_bound[t_on_xr()] = name; }
static void APIENTRY t_glTexStorage2D(GLenum target, GLsizei levels,
      GLenum format, GLsizei w, GLsizei h)
{
   unsigned name = t_gl_bound[t_on_xr()];
   slock_lock(t_gl_lock);
   if (name < T_GL_NAMES)
      t_tex_dims[name] = VIDEO_SCALE_PACK(w, h);
   t_gl_trace(T_GL_STORAGE, name, levels);
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glTexImage2D(GLenum target, GLint level,
      GLint format, GLsizei w, GLsizei h, GLint border, GLenum fmt,
      GLenum type, const void *data)
{ t_glTexStorage2D(target, 1, (GLenum)format, w, h); }
static void APIENTRY t_glTexParameteri(GLenum target, GLenum pname, GLint v) { }
static void APIENTRY t_glTexSubImage2D(GLenum target, GLint level,
      GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type,
      const void *data) { }
static void APIENTRY t_glPixelStorei(GLenum pname, GLint v) { }
static void APIENTRY t_glGetIntegerv(GLenum pname, GLint *v)
{
   switch (pname)
   {
      case GL_MAJOR_VERSION:      *v = t_gl_version / 10; break;
      case GL_MINOR_VERSION:      *v = t_gl_version % 10; break;
      case GL_TEXTURE_BINDING_2D: *v = (GLint)t_gl_bound[t_on_xr()]; break;
      default:                    *v = 0; break;
   }
}
static const GLubyte *APIENTRY t_glGetStringi(GLenum name, GLuint i)
{ return NULL; }
static GLenum APIENTRY t_glGetError(void)
{
   GLenum e   = t_gl_error;
   t_gl_error = GL_NO_ERROR;
   return e;
}
static GLsync APIENTRY t_glFenceSync(GLenum cond, GLbitfield flags)
{
   unsigned id;
   slock_lock(t_gl_lock);
   id                  = t_gl_next_sync++;
   t_syncs[id].alive   = true;
   t_syncs[id].xr_made = t_on_xr();
   t_gl_trace(T_GL_FENCE, id, 0);
   slock_unlock(t_gl_lock);
   return (GLsync)(uintptr_t)id;
}
static void APIENTRY t_glWaitSync(GLsync sync, GLbitfield flags,
      GLuint64 timeout)
{
   unsigned id = (unsigned)(uintptr_t)sync;
   slock_lock(t_gl_lock);
   if (!id || id >= T_GL_SYNCS || !t_syncs[id].alive)
      t_gl_xr_wrong++;
   t_gl_trace(T_GL_WAIT, id, 0);
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glDeleteSync(GLsync sync)
{
   unsigned id = (unsigned)(uintptr_t)sync;
   if (!id)
      return;
   slock_lock(t_gl_lock);
   if (id < T_GL_SYNCS)
   {
      t_syncs[id].alive      = false;
      t_syncs[id].xr_deleted = t_on_xr();
      t_syncs[id].deletes++;
   }
   t_gl_trace(T_GL_DELSYNC, id, 0);
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glFlush(void) { }
static void APIENTRY t_glFinish(void) { }
static void APIENTRY t_glCopyImageSubData(GLuint src, GLenum st, GLint sl,
      GLint sx, GLint sy, GLint sz, GLuint dst, GLenum dt, GLint dl,
      GLint dx, GLint dy, GLint dz, GLsizei w, GLsizei h, GLsizei d)
{
   slock_lock(t_gl_lock);
   if (!t_on_xr())
      t_gl_xr_wrong++;
   if (src < T_GL_NAMES && dst < T_GL_NAMES)
      t_tex_content[dst] = t_tex_content[src];
   t_gl_copies++;
   t_gl_trace(T_GL_COPY, src, dst);
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glGenFramebuffers(GLsizei n, GLuint *names)
{
   GLsizei i;
   slock_lock(t_gl_lock);
   for (i = 0; i < n; i++)
      names[i] = t_gl_next_fbo++;
   t_fbos_alive += n;
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glDeleteFramebuffers(GLsizei n, const GLuint *names)
{
   GLsizei i;
   slock_lock(t_gl_lock);
   for (i = 0; i < n; i++)
      if (names[i])
         t_fbos_alive--;
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glBindFramebuffer(GLenum target, GLuint fbo)
{
   if (target == GL_READ_FRAMEBUFFER || target == GL_FRAMEBUFFER)
      t_gl_read_fbo = fbo;
   if (target == GL_DRAW_FRAMEBUFFER || target == GL_FRAMEBUFFER)
      t_gl_draw_fbo = fbo;
}
static void APIENTRY t_glFramebufferTexture2D(GLenum target, GLenum att,
      GLenum textarget, GLuint tex, GLint level)
{
   if (target == GL_READ_FRAMEBUFFER)
      t_gl_read_att = tex;
   else
      t_gl_draw_att = tex;
}
static GLenum APIENTRY t_glCheckFramebufferStatus(GLenum target)
{ return GL_FRAMEBUFFER_COMPLETE; }
static void APIENTRY t_glBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1,
      GLint sy1, GLint dx0, GLint dy0, GLint dx1, GLint dy1,
      GLbitfield mask, GLenum filter)
{
   slock_lock(t_gl_lock);
   if (t_gl_read_att < T_GL_NAMES && t_gl_draw_att < T_GL_NAMES)
      t_tex_content[t_gl_draw_att] = t_tex_content[t_gl_read_att];
   t_gl_copies++;
   t_gl_trace(T_GL_BLIT, t_gl_read_att, t_gl_draw_att);
   slock_unlock(t_gl_lock);
}
static void APIENTRY t_glDisable(GLenum cap) { }

static const struct { const char *name; gfx_ctx_proc_t fn; } t_gl_procs[] = {
   { "glGenTextures",            (gfx_ctx_proc_t)t_glGenTextures },
   { "glDeleteTextures",         (gfx_ctx_proc_t)t_glDeleteTextures },
   { "glBindTexture",            (gfx_ctx_proc_t)t_glBindTexture },
   { "glTexStorage2D",           (gfx_ctx_proc_t)t_glTexStorage2D },
   { "glTexImage2D",             (gfx_ctx_proc_t)t_glTexImage2D },
   { "glTexParameteri",          (gfx_ctx_proc_t)t_glTexParameteri },
   { "glTexSubImage2D",          (gfx_ctx_proc_t)t_glTexSubImage2D },
   { "glPixelStorei",            (gfx_ctx_proc_t)t_glPixelStorei },
   { "glGetIntegerv",            (gfx_ctx_proc_t)t_glGetIntegerv },
   { "glGetStringi",             (gfx_ctx_proc_t)t_glGetStringi },
   { "glGetError",               (gfx_ctx_proc_t)t_glGetError },
   { "glFenceSync",              (gfx_ctx_proc_t)t_glFenceSync },
   { "glWaitSync",               (gfx_ctx_proc_t)t_glWaitSync },
   { "glDeleteSync",             (gfx_ctx_proc_t)t_glDeleteSync },
   { "glFlush",                  (gfx_ctx_proc_t)t_glFlush },
   { "glFinish",                 (gfx_ctx_proc_t)t_glFinish },
   { "glCopyImageSubData",       (gfx_ctx_proc_t)t_glCopyImageSubData },
   { "glGenFramebuffers",        (gfx_ctx_proc_t)t_glGenFramebuffers },
   { "glDeleteFramebuffers",     (gfx_ctx_proc_t)t_glDeleteFramebuffers },
   { "glBindFramebuffer",        (gfx_ctx_proc_t)t_glBindFramebuffer },
   { "glFramebufferTexture2D",   (gfx_ctx_proc_t)t_glFramebufferTexture2D },
   { "glCheckFramebufferStatus", (gfx_ctx_proc_t)t_glCheckFramebufferStatus },
   { "glBlitFramebuffer",        (gfx_ctx_proc_t)t_glBlitFramebuffer },
   { "glDisable",                (gfx_ctx_proc_t)t_glDisable },
   { NULL, NULL }
};

static gfx_ctx_proc_t t_gl_get_proc(const char *name)
{
   unsigned i;
   if (t_gl_missing && string_is_equal(name, t_gl_missing))
      return NULL;
   for (i = 0; t_gl_procs[i].name; i++)
      if (string_is_equal(name, t_gl_procs[i].name))
         return t_gl_procs[i].fn;
   return NULL;
}

/* The context driver's shared context. */
static bool t_shared_fail_new, t_shared_fail_bind;
static unsigned t_shared_news, t_shared_frees, t_shared_binds,
      t_shared_unbinds, t_shared_wrong_thread;
static bool t_shared_new(void *data, gfx_ctx_shared_context_t *out)
{
   memset(out, 0, sizeof(*out));
   if (t_shared_fail_new)
      return false;
   if (t_on_xr())
      t_shared_wrong_thread++;
   t_shared_news++;
   out->kind             = GFX_CTX_SHARED_EGL;
   out->display          = (void*)0x10;
   out->config           = (void*)0x20;
   out->context          = (void*)0x30;
   out->get_proc_address = t_gl_get_proc;
   return true;
}
static bool t_shared_bind(gfx_ctx_shared_context_t *ctx, bool bind)
{
   if (!t_on_xr())
      t_shared_wrong_thread++;
   if (!bind)
   {
      t_shared_unbinds++;
      return true;
   }
   if (t_shared_fail_bind)
      return false;
   t_shared_binds++;
   return true;
}
static void t_shared_free(gfx_ctx_shared_context_t *ctx)
{
   if (t_on_xr())
      t_shared_wrong_thread++;
   if (ctx->kind)
      t_shared_frees++;
   memset(ctx, 0, sizeof(*ctx));
}
static gfx_ctx_driver_t t_gl_ctx_driver;

/* One headset frame at a time: the XR thread waits for the test's
 * permit; the test waits for the frame's end. */
static slock_t *t_gate_lock;
static scond_t *t_gate_cond;
static unsigned t_gate_permits, t_gate_done;
static bool t_gate_all;
static XRAPI_ATTR XrResult XRAPI_CALL t_gate_wait_frame(XrSession s,
      const XrFrameWaitInfo *wi, XrFrameState *st)
{
   unsigned spins = 0;
   slock_lock(t_gate_lock);
   while (!t_gate_permits && !t_gate_all && spins++ < 5000)
      scond_wait_timeout(t_gate_cond, t_gate_lock, 1000);
   if (t_gate_permits)
      t_gate_permits--;
   slock_unlock(t_gate_lock);
   return t_wait_frame(s, wi, st);
}
static XRAPI_ATTR XrResult XRAPI_CALL t_gate_end_frame(XrSession s,
      const XrFrameEndInfo *ei)
{
   XrResult res = t_end_frame(s, ei);
   slock_lock(t_gate_lock);
   t_gate_done++;
   scond_broadcast(t_gate_cond);
   slock_unlock(t_gate_lock);
   return res;
}
static bool t_gate_step(unsigned n)
{
   unsigned target;
   bool ok = true;
   slock_lock(t_gate_lock);
   target          = t_gate_done + n;
   t_gate_permits += n;
   scond_broadcast(t_gate_cond);
   while (ok && t_gate_done < target)
      ok = scond_wait_timeout(t_gate_cond, t_gate_lock, 3000000);
   slock_unlock(t_gate_lock);
   return ok;
}
static void t_gate_set_all(bool all)
{
   slock_lock(t_gate_lock);
   t_gate_all     = all;
   t_gate_permits = 0;
   scond_broadcast(t_gate_cond);
   slock_unlock(t_gate_lock);
}

static void t_gl_host_setup(void)
{
   if (!t_gate_lock)
   {
      t_gate_lock = slock_new();
      t_gate_cond = scond_new();
   }
   t_gate_set_all(false);
   t_runtime_defaults();
   t_exts[0]  = "XR_KHR_opengl_enable";
   t_exts[3]  = "XR_MNDX_egl_enable";
   t_num_exts = 4;
   t_gl_reset();
   memset(&t_gl_ctx_driver, 0, sizeof(t_gl_ctx_driver));
   t_gl_ctx_driver.shared_context_new  = t_shared_new;
   t_gl_ctx_driver.shared_context_bind = t_shared_bind;
   t_gl_ctx_driver.shared_context_free = t_shared_free;
   t_shared_fail_new = t_shared_fail_bind = false;
   t_shared_news = t_shared_frees = t_shared_binds = t_shared_unbinds = 0;
   t_shared_wrong_thread = 0;
   t_formats[0]         = OPENXR_GL_SRGB8_ALPHA8;
   t_formats[1]         = OPENXR_GL_RGBA8;
   t_more_formats       = 0;
   t_fill_gl            = true;
   t_gl_wrong_type      = false;
   t_gles_base          = 0x400;
   t_sc_images          = 3;
   t_sc_images_second   = 0;
   t_sc_wait            = XR_SUCCESS;
   t_sc_busy            = XR_NULL_HANDLE;
   t_sc_busy_from       = 0;
   t_sc_release         = XR_SUCCESS;
   t_sc_refuse_width    = 0;
   t_create_session_res = XR_SUCCESS;
   t_gl_reqs_res        = XR_SUCCESS;
   t_wait_frame_res = t_begin_frame_res = t_end_frame_res = XR_SUCCESS;
   t_num_events = t_next_event = 0;
   t_poll_result = XR_SUCCESS;
   t_push_state(T_SESSION, XR_SESSION_STATE_READY);
   t_push_state(T_SESSION, XR_SESSION_STATE_FOCUSED);
   memset(&t_xr_host, 0, sizeof(t_xr_host));
}

static gl_openxr_t *t_gl_host_new(void)
{
   gl_openxr_t *xr;
   t_gl_host_setup();
   if (!(xr = gl_openxr_new(&t_gl_ctx_driver, NULL)))
      return NULL;
   xr->frame.WaitFrame = t_gate_wait_frame;
   xr->frame.EndFrame  = t_gate_end_frame;
   return xr;
}

static void t_gl_host_free(gl_openxr_t *xr)
{
   t_gate_set_all(true);
   gl_openxr_free(xr);
   t_gate_set_all(false);
   t_fill_gl = false;
}

/* What the headset showed at its last frame: the quads' count and the
 * first quad's swapchain, or a projection first. */
static unsigned t_gl_shown(XrSwapchain *first)
{
   unsigned n;
   slock_lock(t_gate_lock);
   n = t_last_layer_count;
   if (first)
      *first = t_quad.subImage.swapchain;
   slock_unlock(t_gate_lock);
   return n;
}

/* The video thread's draw: slot's targets marked gen, published with a
 * quad for slot and one for another slot, which the slot's image must
 * not bring with it. */
static bool t_gl_draw(gl_openxr_t *xr, unsigned slot, unsigned dims,
      unsigned layers, unsigned gen, const openxr_projection_t *p)
{
   unsigned l;
   uint32_t tex[2] = { 0, 0 };
   video_xr_quad_set_t set;
   if (!gl_openxr_slot_target(xr, slot, dims, layers, tex))
      return false;
   slock_lock(t_gl_lock);
   for (l = 0; l < layers; l++)
      if (tex[l] < T_GL_NAMES)
         t_tex_content[tex[l]] = gen * 10 + l;
   slock_unlock(t_gl_lock);
   memset(&set, 0, sizeof(set));
   if (slot != OPENXR_PROJECTION_SLOT)
   {
      set.quads[0].slot               = slot;
      set.quads[0].kind               = (slot == VIDEO_XR_MENU_SLOT)
         ? VIDEO_XR_QUAD_MENU : VIDEO_XR_QUAD_SCREEN;
      set.quads[0].width              = 1.0f;
      set.quads[0].height             = 1.0f;
      set.quads[0].pose.orientation.w = 1.0f;
      set.quads[1]                    = set.quads[0];
      set.quads[1].slot               = (slot + 1) % VIDEO_XR_MAX_SLOTS;
      set.slots[slot].dims            = dims;
      set.slots[slot].layers          = layers;
      set.num_quads                   = 2;
   }
   gl_openxr_slot_publish(xr, slot, &set, p);
   return true;
}

/* The image the XR thread last copied into for slot's layer. */
static unsigned t_gl_image_content(gl_openxr_t *xr, unsigned slot,
      unsigned layer)
{
   unsigned c;
   uint32_t image = xr->slots[slot].images[1][layer];
   slock_lock(t_gl_lock);
   c = (image < T_GL_NAMES) ? t_tex_content[image] : 0;
   slock_unlock(t_gl_lock);
   return c;
}

/* Each fence deleted once, by the thread that made it; none alive. */
static void t_gl_books_closed(const char *name)
{
   unsigned i, wrong = 0, alive = 0, textures = 0;
   for (i = 1; i < t_gl_next_sync && i < T_GL_SYNCS; i++)
   {
      if (t_syncs[i].alive)
         alive++;
      else if (t_syncs[i].deletes != 1
            || t_syncs[i].xr_deleted != t_syncs[i].xr_made)
         wrong++;
   }
   for (i = 0x1000; i < t_gl_next_tex && i < T_GL_NAMES; i++)
      if (t_tex_alive[i])
         textures++;
   CHECK(!alive && !wrong && !textures && !t_fbos_alive,
         "%s: %u fences alive, %u deleted wrongly, %u textures, %d "
         "framebuffers", name, alive, wrong, textures, t_fbos_alive);
}

/* The video thread waited on the XR thread's copy fence before it
 * deleted or reallocated a texture the XR thread had copied from, or
 * the XR thread retired that fence first (after its glFinish at stop). */
static void t_gl_reuse_waited(const char *name)
{
   unsigned i, j, k, bad = 0;
   for (i = 0; i < t_gl_num_calls; i++)
   {
      unsigned tex, fence = 0;
      if (!t_gl_calls[i].xr || t_gl_calls[i].op != T_GL_COPY)
         continue;
      tex = t_gl_calls[i].a;
      for (j = i + 1; j < t_gl_num_calls; j++)
         if (t_gl_calls[j].xr && t_gl_calls[j].op == T_GL_FENCE)
         {
            fence = t_gl_calls[j].a;
            break;
         }
      for (k = j; fence && k < t_gl_num_calls; k++)
      {
         if (     t_gl_calls[k].a == fence
               && (   (!t_gl_calls[k].xr && t_gl_calls[k].op == T_GL_WAIT)
                   || (t_gl_calls[k].xr && t_gl_calls[k].op == T_GL_DELSYNC)))
            break;
         if (     !t_gl_calls[k].xr && t_gl_calls[k].op == T_GL_DELTEX
               && t_gl_calls[k].a == tex)
         {
            bad++;
            break;
         }
      }
   }
   CHECK(!bad, "%s: %u textures deleted before the copy from them", name,
         bad);
}

static void test_gl_host_start_failure(void)
{
   gl_openxr_t *xr;
   unsigned made;

   /* No shared contexts in the context driver, or none it could make. */
   t_gl_host_setup();
   memset(&t_gl_ctx_driver, 0, sizeof(t_gl_ctx_driver));
   t_reset_log();
   CHECK(!gl_openxr_new(&t_gl_ctx_driver, NULL)
         && LOGGED("[OpenXR] The context driver has no shared contexts; no headset output.\n"),
         "a driver without shared contexts: %s", t_log);
   t_gl_ctx_driver.shared_context_new  = t_shared_new;
   t_gl_ctx_driver.shared_context_bind = t_shared_bind;
   t_gl_ctx_driver.shared_context_free = t_shared_free;
   t_shared_fail_new = true;
   t_reset_log();
   CHECK(!gl_openxr_new(&t_gl_ctx_driver, NULL)
         && LOGGED("[OpenXR] No context could share the window's; no headset output.\n"),
         "no shared context: %s", t_log);
   t_shared_fail_new = false;
   t_gl_missing      = "glFenceSync";
   t_reset_log();
   CHECK(!gl_openxr_new(&t_gl_ctx_driver, NULL)
         && LOGGED("[OpenXR] The GL context lacks functions headset output needs.\n")
         && t_shared_frees == t_shared_news,
         "a context without fences: %s", t_log);
   t_gl_missing = NULL;
   t_gl_version = 33;
   CHECK(!gl_openxr_new(&t_gl_ctx_driver, NULL)
         && t_shared_frees == t_shared_news, "a context too old");
   t_fill_gl = false;

   /* The XR thread cannot take the context: no session, nothing left. */
   xr = t_gl_host_new();
   CHECK(xr, "host");
   if (!xr)
      return;
   made               = t_sessions_made;
   t_shared_fail_bind = true;
   CHECK(!gl_openxr_start(xr), "started without its context");
   CHECK(t_sessions_made == made && !t_xr_host.user, "bind failure: a "
         "session %u or a host", t_sessions_made - made);
   t_gl_host_free(xr);
   CHECK(t_shared_frees == 1 && !t_shared_wrong_thread,
         "bind failure: context freed %u, wrong threads %u", t_shared_frees,
         t_shared_wrong_thread);
   t_gl_books_closed("bind failure");

   /* The runtime refuses the session: unbound on its thread, nothing left. */
   xr = t_gl_host_new();
   if (!xr)
      return;
   t_create_session_res = XR_ERROR_GRAPHICS_DEVICE_INVALID;
   CHECK(!gl_openxr_start(xr), "started without a session");
   CHECK(t_shared_binds == 1 && t_shared_unbinds == 1
         && !t_shared_wrong_thread && !t_xr_host.user,
         "session failure: binds %u unbinds %u wrong %u", t_shared_binds,
         t_shared_unbinds, t_shared_wrong_thread);
   t_gl_host_free(xr);
   t_gl_books_closed("session failure");

   /* A session without a format it can use: destroyed on its thread. */
   xr = t_gl_host_new();
   if (!xr)
      return;
   t_formats[0] = 50;
   t_formats[1] = 44;
   made         = t_sessions_destroyed;
   CHECK(!gl_openxr_start(xr), "started without a format");
   CHECK(t_sessions_destroyed == made + 1 && !xr->session.session
         && t_shared_unbinds == t_shared_binds,
         "format failure: session destroyed %u, unbinds %u of %u",
         t_sessions_destroyed - made, t_shared_unbinds, t_shared_binds);
   t_gl_host_free(xr);
   t_gl_books_closed("format failure");
}

static void test_gl_host_records(void)
{
   unsigned i, copies, waits_on_xr;
   XrSwapchain shown = XR_NULL_HANDLE;
   gl_openxr_t *xr   = t_gl_host_new();
   CHECK(xr, "host");
   if (!xr)
      return;
   CHECK(gl_openxr_start(xr) && t_xr_host.user == xr, "start");
   CHECK(t_shared_binds == 1 && !t_shared_wrong_thread, "bound %u, wrong %u",
         t_shared_binds, t_shared_wrong_thread);

   /* Nothing published: nothing copied, nothing shown. */
   CHECK(t_gate_step(2), "frames");
   CHECK(!t_gl_copies && !t_gl_shown(NULL), "copied %u before a record",
         t_gl_copies);

   /* A record: waited on, copied, released and shown with its quad
    * alone. */
   CHECK(t_gl_draw(xr, 0, VIDEO_SCALE_PACK(64, 32), 1, 1, NULL), "draw 1");
   CHECK(t_gate_step(1), "frame");
   CHECK(t_gl_copies == 1 && t_gl_image_content(xr, 0, 0) == 10,
         "copies %u, image %u", t_gl_copies, t_gl_image_content(xr, 0, 0));
   CHECK(t_gl_shown(&shown) == 1
         && shown == xr->sc.slots[0].swapchains[0],
         "shown %u layers", t_gl_shown(NULL));
   for (i = 0; i < t_gl_num_calls && t_gl_calls[i].op != T_GL_COPY; i++) { }
   CHECK(i > 0 && i < t_gl_num_calls && t_gl_calls[i - 1].op == T_GL_WAIT
         && t_gl_calls[i - 1].xr && !t_syncs[t_gl_calls[i - 1].a].xr_made,
         "the copy did not follow a wait on the draw's fence");
   CHECK(i + 1 < t_gl_num_calls && t_gl_calls[i + 1].op == T_GL_FENCE
         && t_gl_calls[i + 1].xr, "no fence after the copy");

   /* No copy without a new record. */
   CHECK(t_gate_step(3), "frames");
   CHECK(t_gl_copies == 1 && t_gl_shown(NULL) == 1, "copies %u",
         t_gl_copies);

   /* Three records before a frame: the newest alone is copied. */
   CHECK(     t_gl_draw(xr, 0, VIDEO_SCALE_PACK(64, 32), 1, 2, NULL)
         && t_gl_draw(xr, 0, VIDEO_SCALE_PACK(64, 32), 1, 3, NULL)
         && t_gl_draw(xr, 0, VIDEO_SCALE_PACK(64, 32), 1, 4, NULL),
         "draws 2-4");
   CHECK(t_gate_step(1), "frame");
   CHECK(t_gl_copies == 2 && t_gl_image_content(xr, 0, 0) == 40,
         "copies %u, image %u", t_gl_copies, t_gl_image_content(xr, 0, 0));

   /* Records reused: the video thread waits on the copies' fences. */
   for (i = 5; i < 12; i++)
   {
      t_gl_draw(xr, 0, VIDEO_SCALE_PACK(64, 32), 1, i, NULL);
      t_gate_step(1);
   }
   copies      = t_gl_copies;
   waits_on_xr = 0;
   for (i = 0; i < t_gl_num_calls; i++)
      if (     !t_gl_calls[i].xr && t_gl_calls[i].op == T_GL_WAIT
            && t_syncs[t_gl_calls[i].a].xr_made)
         waits_on_xr++;
   CHECK(copies == 9 && t_gl_image_content(xr, 0, 0) == 110 && waits_on_xr,
         "copies %u, image %u, waits on copies %u", copies,
         t_gl_image_content(xr, 0, 0), waits_on_xr);
   CHECK(!t_gl_xr_wrong, "%u calls on the wrong thread or fence",
         t_gl_xr_wrong);
   t_gl_host_free(xr);
   CHECK(t_shared_unbinds == 1 && t_shared_frees == 1
         && !t_shared_wrong_thread && !t_xr_host.user,
         "free: unbinds %u frees %u wrong %u", t_shared_unbinds,
         t_shared_frees, t_shared_wrong_thread);
   t_gl_reuse_waited("records");
   t_gl_books_closed("records");
}

static void test_gl_host_resize(void)
{
   unsigned made;
   gl_openxr_t *xr = t_gl_host_new();
   CHECK(xr, "host");
   if (!xr)
      return;
   CHECK(gl_openxr_start(xr), "start");
   CHECK(t_gl_draw(xr, 1, VIDEO_SCALE_PACK(64, 64), 2, 1, NULL)
         && t_gate_step(1), "first size");
   CHECK(t_ci_width == 64 && t_gl_image_content(xr, 1, 1) == 11
         && t_gl_shown(NULL) == 1, "64: width %u, image %u",
         (unsigned)t_ci_width, t_gl_image_content(xr, 1, 1));

   /* A new size waits for its image: the slot shows nothing while the
    * runtime is busy, never the new size's quad over the old pixels. */
   t_sc_wait = XR_TIMEOUT_EXPIRED;
   CHECK(t_gl_draw(xr, 1, VIDEO_SCALE_PACK(128, 64), 2, 2, NULL)
         && t_gate_step(1), "second size");
   CHECK(t_ci_width == 128 && !t_gl_shown(NULL)
         && t_gl_image_content(xr, 1, 0) != 20,
         "128 busy: width %u, shown %u", (unsigned)t_ci_width,
         t_gl_shown(NULL));
   t_sc_wait = XR_SUCCESS;
   CHECK(t_gate_step(1), "frame");
   CHECK(t_gl_image_content(xr, 1, 0) == 20 && t_gl_shown(NULL) == 1,
         "128: image %u, shown %u", t_gl_image_content(xr, 1, 0),
         t_gl_shown(NULL));

   /* A size the runtime refuses: left out, said, not asked again. */
   t_sc_refuse_width = 4096;
   CHECK(t_gl_draw(xr, 1, VIDEO_SCALE_PACK(4096, 64), 2, 3, NULL)
         && t_gate_step(1), "refused size");
   made = t_sc_made;
   CHECK(!t_gl_shown(NULL)
         && gl_openxr_slot_refused(xr, 1) == VIDEO_SCALE_PACK(4096, 64),
         "refused: shown %u, refused %x", t_gl_shown(NULL),
         gl_openxr_slot_refused(xr, 1));
   CHECK(t_gl_draw(xr, 1, VIDEO_SCALE_PACK(4096, 64), 2, 4, NULL)
         && t_gate_step(2), "refused again");
   CHECK(t_sc_made == made, "a refused size asked again");
   CHECK(t_gl_draw(xr, 1, VIDEO_SCALE_PACK(256, 64), 2, 5, NULL)
         && t_gate_step(1), "another size");
   CHECK(t_gl_shown(NULL) == 1 && !gl_openxr_slot_refused(xr, 1)
         && t_gl_image_content(xr, 1, 1) == 51,
         "after refusal: shown %u, image %u", t_gl_shown(NULL),
         t_gl_image_content(xr, 1, 1));
   t_gl_host_free(xr);
   t_gl_reuse_waited("resize");
   t_gl_books_closed("resize");
}

static void test_gl_host_empty_record(void)
{
   const unsigned menu = VIDEO_XR_MENU_SLOT;
   gl_openxr_t *xr     = t_gl_host_new();
   CHECK(xr, "host");
   if (!xr)
      return;
   CHECK(gl_openxr_start(xr), "start");
   CHECK(t_gl_draw(xr, menu, VIDEO_SCALE_PACK(64, 64), 1, 1, NULL)
         && t_gate_step(1), "menu");
   CHECK(t_gl_shown(NULL) == 1 && (t_quad.layerFlags
            & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT),
         "menu shown %u", t_gl_shown(NULL));

   /* Closed: gone at the next frame. */
   gl_openxr_slot_forget(xr, menu);
   CHECK(t_gate_step(1) && !t_gl_shown(NULL), "closed menu shown");

   /* Reopened while the runtime is busy: never the old image. */
   t_sc_wait = XR_TIMEOUT_EXPIRED;
   CHECK(t_gl_draw(xr, menu, VIDEO_SCALE_PACK(64, 64), 1, 2, NULL)
         && t_gate_step(2), "reopen");
   CHECK(!t_gl_shown(NULL) && t_gl_image_content(xr, menu, 0) == 10,
         "the old menu came back: shown %u", t_gl_shown(NULL));
   t_sc_wait = XR_SUCCESS;
   CHECK(t_gate_step(1) && t_gl_shown(NULL) == 1
         && t_gl_image_content(xr, menu, 0) == 20, "reopened: image %u",
         t_gl_image_content(xr, menu, 0));

   /* Drawn then closed before a frame: closed, in order. */
   CHECK(t_gl_draw(xr, menu, VIDEO_SCALE_PACK(64, 64), 1, 3, NULL),
         "draw 3");
   gl_openxr_slot_forget(xr, menu);
   CHECK(t_gate_step(1) && !t_gl_shown(NULL), "closed after a draw shown");
   CHECK(t_gl_draw(xr, menu, VIDEO_SCALE_PACK(64, 64), 1, 4, NULL)
         && t_gate_step(1) && t_gl_shown(NULL) == 1
         && t_gl_image_content(xr, menu, 0) == 40, "reopened again");
   t_gl_host_free(xr);
   t_gl_books_closed("empty record");
}

/* The screen the tracked eyes are on yields to them: the headset shows
 * one or the other, never neither, whichever image the runtime holds. */
static void test_gl_host_yield(void)
{
   const unsigned slot = OPENXR_PROJECTION_SLOT;
   enum retro_vr_reference_space space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   XrPosef origin;
   openxr_projection_t p;
   gl_openxr_t *xr = t_gl_host_new();
   CHECK(xr, "host");
   if (!xr)
      return;
   CHECK(gl_openxr_start(xr), "start");
   memset(&origin, 0, sizeof(origin));
   origin.orientation.w = 1.0f;
   openxr_session_swap_track_space(&xr->session, &space, &origin, NULL, NULL);
   memset(&p, 0, sizeof(p));
   p.active    = true;
   p.track_seq = retro_atomic_load_acquire_int(&xr->session.track_seq);
   p.width     = 64;
   p.height    = 48;
   p.pose[0].orientation.w = p.pose[1].orientation.w = 1.0f;

   CHECK(t_gl_draw(xr, 0, VIDEO_SCALE_PACK(64, 64), 1, 1, NULL)
         && t_gate_step(1) && t_gl_shown(NULL) == 1, "screen");

   /* The eyes' new images are busy: the screen stays until they show. */
   t_sc_busy_from = t_sc_created_total + 1;
   CHECK(t_gl_draw(xr, slot, VIDEO_SCALE_PACK(64, 48), 2, 2, &p),
         "eyes");
   gl_openxr_slot_yield(xr, 0);
   CHECK(t_gate_step(2), "frames");
   CHECK(     t_gl_shown(NULL) == 1
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_QUAD,
         "the screen went before the eyes showed: %u layers, type %d",
         t_gl_shown(NULL), (int)t_layer_types[0]);
   t_sc_busy_from = 0;
   CHECK(t_gate_step(1), "frame");
   CHECK(     t_gl_shown(NULL) == 1
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION,
         "eyes for the screen: %u layers, type %d", t_gl_shown(NULL),
         (int)t_layer_types[0]);

   /* Back to the screen while its image is busy: the eyes stay until
    * the screen shows. */
   t_sc_busy = xr->sc.slots[0].swapchains[0];
   CHECK(t_gl_draw(xr, 0, VIDEO_SCALE_PACK(64, 64), 1, 3, NULL),
         "screen again");
   gl_openxr_slot_forget(xr, slot);
   CHECK(t_gate_step(2), "frames");
   CHECK(     t_gl_shown(NULL) == 1
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION,
         "the eyes went before the screen showed: %u layers, type %d",
         t_gl_shown(NULL), (int)t_layer_types[0]);
   t_sc_busy = XR_NULL_HANDLE;
   CHECK(t_gate_step(1), "frame");
   CHECK(     t_gl_shown(NULL) == 1
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_QUAD
         && t_gl_image_content(xr, 0, 0) == 30,
         "the screen for the eyes: %u layers, type %d", t_gl_shown(NULL),
         (int)t_layer_types[0]);
   t_gl_host_free(xr);
   t_gl_books_closed("yield");
}

static openxr_frame_t *t_gl_carry_frame;
static void t_gl_carry(void *user, int from, int to)
{
   openxr_frame_move_projection(t_gl_carry_frame, from, to);
}

static void test_gl_host_projection(void)
{
   const unsigned slot = OPENXR_PROJECTION_SLOT;
   const unsigned dims = VIDEO_SCALE_PACK(96, 80);
   enum retro_vr_reference_space space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   XrPosef origin;
   openxr_projection_t p, got;
   int seq;
   gl_openxr_t *xr = t_gl_host_new();
   CHECK(xr, "host");
   if (!xr)
      return;
   CHECK(gl_openxr_start(xr), "start");
   memset(&origin, 0, sizeof(origin));
   origin.orientation.w = 1.0f;
   CHECK(openxr_session_swap_track_space(&xr->session, &space, &origin,
            NULL, NULL),
         "track space");
   seq = retro_atomic_load_acquire_int(&xr->session.track_seq);

   memset(&p, 0, sizeof(p));
   p.active                 = true;
   p.track_seq              = seq;
   p.width                  = 64;
   p.height                 = 48;
   p.pose[0].orientation.w  = p.pose[1].orientation.w = 1.0f;
   p.pose[0].position.x     = -0.03f;
   p.pose[1].position.x     = 0.03f;
   p.fov[0].angleLeft       = p.fov[1].angleLeft = -0.7f;

   /* A quad's slot takes no projection. */
   CHECK(t_gl_draw(xr, 2, VIDEO_SCALE_PACK(32, 32), 1, 1, &p)
         && t_gate_step(1), "screen");
   CHECK(t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_QUAD,
         "a screen's record brought a projection");

   /* Copied, then shown with the poses it was drawn from. */
   CHECK(t_gl_draw(xr, slot, dims, 2, 2, &p) && t_gate_step(1), "eyes");
   CHECK(     t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_proj_views[0].pose.position.x == -0.03f
         && t_proj_views[1].subImage.imageRect.extent.width == 64
         && t_gl_image_content(xr, slot, 1) == 21,
         "projection: type %d, x %f, image %u", (int)t_layer_types[0],
         t_proj_views[0].pose.position.x, t_gl_image_content(xr, slot, 1));

   /* Not copied yet: the old image keeps the old poses. */
   t_sc_wait            = XR_TIMEOUT_EXPIRED;
   p.pose[0].position.x = -0.05f;
   CHECK(t_gl_draw(xr, slot, dims, 2, 3, &p) && t_gate_step(1), "busy");
   CHECK(     t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_proj_views[0].pose.position.x == -0.03f
         && t_gl_image_content(xr, slot, 0) == 20,
         "new poses over the old image: x %f",
         t_proj_views[0].pose.position.x);
   t_sc_wait = XR_SUCCESS;
   CHECK(t_gate_step(1) && t_proj_views[0].pose.position.x == -0.05f
         && t_gl_image_content(xr, slot, 0) == 30, "copied: x %f",
         t_proj_views[0].pose.position.x);

   /* Two recenters while a record waits: it follows them both. */
   t_sc_wait            = XR_TIMEOUT_EXPIRED;
   p.pose[0].position.x = -0.07f;
   CHECK(t_gl_draw(xr, slot, dims, 2, 4, &p) && t_gate_step(1), "queued");
   t_gl_carry_frame = &xr->frame;
   CHECK(openxr_session_swap_track_space(&xr->session, &space, &origin,
            t_gl_carry, NULL)
         && openxr_session_swap_track_space(&xr->session, &space, &origin,
            t_gl_carry, NULL), "recenters");
   t_sc_wait = XR_SUCCESS;
   CHECK(t_gate_step(1), "frame");
   openxr_seq_read(&xr->frame.proj_seq, xr->frame.proj_words, &got,
         sizeof(got));
   CHECK(     got.track_seq == seq + 2 && got.pose[0].position.x == -0.07f
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_proj_views[0].pose.position.x == -0.07f,
         "carried: seq %d of %d, x %f", got.track_seq, seq + 2,
         t_proj_views[0].pose.position.x);

   /* Another eye size, its image not copied yet: no projection, rather
    * than the new size's extent over the old pixels, or the old one's
    * over images the runtime has none of. */
   t_sc_wait = XR_TIMEOUT_EXPIRED;
   p.width   = 100;
   p.height  = 90;
   CHECK(t_gl_draw(xr, slot, VIDEO_SCALE_PACK(128, 96), 2, 5, &p)
         && t_gate_step(1), "resized, busy");
   CHECK(t_ci_width == 128
         && t_layer_types[0] != XR_TYPE_COMPOSITION_LAYER_PROJECTION,
         "resized, busy: width %u, type %d", (unsigned)t_ci_width,
         (int)t_layer_types[0]);
   t_sc_wait = XR_SUCCESS;
   CHECK(     t_gate_step(1)
         && t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && t_proj_views[0].subImage.imageRect.extent.width == 100
         && t_proj_views[1].subImage.imageRect.extent.height == 90
         && t_gl_image_content(xr, slot, 1) == 51,
         "resized: type %d, extent %d, image %u", (int)t_layer_types[0],
         (int)t_proj_views[0].subImage.imageRect.extent.width,
         t_gl_image_content(xr, slot, 1));

   /* A size the runtime refuses: no projection, and the size is said,
    * so the driver shows the eyes' screen as a quad again. */
   t_sc_refuse_width = 4096;
   CHECK(t_gl_draw(xr, slot, VIDEO_SCALE_PACK(4096, 96), 2, 6, &p)
         && t_gate_step(1), "refused size");
   CHECK(     t_layer_types[0] != XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && gl_openxr_slot_refused(xr, slot) == VIDEO_SCALE_PACK(4096, 96),
         "refused: type %d, refused %x", (int)t_layer_types[0],
         gl_openxr_slot_refused(xr, slot));
   t_sc_refuse_width = 0;
   CHECK(t_gl_draw(xr, slot, dims, 2, 7, &p) && t_gate_step(1), "eyes again");
   CHECK(     t_layer_types[0] == XR_TYPE_COMPOSITION_LAYER_PROJECTION
         && !gl_openxr_slot_refused(xr, slot)
         && t_gl_image_content(xr, slot, 0) == 70,
         "after refusal: type %d, image %u", (int)t_layer_types[0],
         t_gl_image_content(xr, slot, 0));

   /* Cleared in order: no projection from the next frame. */
   gl_openxr_slot_forget(xr, slot);
   CHECK(t_gate_step(1)
         && t_layer_types[0] != XR_TYPE_COMPOSITION_LAYER_PROJECTION,
         "a cleared projection shown");
   t_gl_host_free(xr);
   t_gl_books_closed("projection");
}
#endif

int main(void)
{
   test_runtime();
   test_runtime_binding_ext();
   test_ring_indices();
   test_ring_threads();
   test_session();
   test_swapchain();
   test_seq_latch();
   test_frame();
   test_frame_thread_hooks();
   test_frame_carried_seq();
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
   test_frame_perf();
   test_frame_rate_retry();
   test_binding_gles();
   test_binding_gl();
#ifdef HAVE_OPENXR_GL
   test_binding_gl_chain();
#endif
   test_vulkan_inline();
   test_input_hooks();
   test_swapchain_filled();
   test_screen_placement();
   test_screen_placement_seated();
   test_screen_placement_inline();
   test_frame_repeat();
   test_screen_placement_repeat();
#ifdef HAVE_OPENXR_GL
   /* Last: they leave the stand-in's formats and extensions as GL's. */
   test_gl_host_start_failure();
   test_gl_host_records();
   test_gl_host_resize();
   test_gl_host_empty_record();
   test_gl_host_projection();
   test_gl_host_yield();
#endif
   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   puts("PASS openxr_core_test");
   return 0;
}
