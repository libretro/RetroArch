/* The shared OpenXR units (gfx/common/openxr_*.c), as they ship,
 * against a runtime the test stands in for. Each unit's xr calls go
 * through the proc lookup it is given, so the stand-in answers them. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <openxr/openxr.h>
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
static uint32_t    t_max_w           = 8192;
static uint32_t    t_max_h           = 2048;
static const char *t_missing;          /* a function the stand-in lacks */
static const char *t_enabled[8];
static unsigned    t_num_enabled;
static unsigned    t_destroyed_instances;

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
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_enum_views(XrInstance i, XrSystemId s,
      XrViewConfigurationType t, uint32_t cap, uint32_t *count,
      XrViewConfigurationView *v)
{
   *count = t_views;
   if (cap >= 1 && t_views >= 1)
      v[0].recommendedImageRectWidth = t_rec_width;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_enum_modes(XrInstance i, XrSystemId s,
      XrViewConfigurationType t, uint32_t cap, uint32_t *count,
      XrEnvironmentBlendMode *m)
{
   *count = 1;
   if (cap)
      m[0] = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_refresh_rates(XrSession s,
      uint32_t cap, uint32_t *count, float *r) { *count = 0; return XR_SUCCESS; }
static XRAPI_ATTR XrResult XRAPI_CALL t_request_rate(XrSession s, float hz)
{ return XR_SUCCESS; }

#define T_SESSION ((XrSession)(uintptr_t)0x30)
#define T_OTHER   ((XrSession)(uintptr_t)0x31)
static XrEventDataBuffer t_events[8];
static unsigned t_num_events, t_next_event;
static XrResult t_poll_result = XR_SUCCESS;  /* after the queue runs dry */
static unsigned t_begun, t_ended_sessions, t_locks, t_unlocks;
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
      const XrSessionCreateInfo *ci, XrSession *s) { *s = T_SESSION; return XR_SUCCESS; }
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_session(XrSession s) { return XR_SUCCESS; }
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
static XRAPI_ATTR XrResult XRAPI_CALL t_create_space(XrSession s,
      const XrReferenceSpaceCreateInfo *ci, XrSpace *out)
{
   if (t_space_fail_at && t_spaces_made + 1 == t_space_fail_at)
      return XR_ERROR_RUNTIME_FAILURE;
   *out = (XrSpace)(uintptr_t)(0x40 + (++t_spaces_made));
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_space(XrSpace s) { t_spaces_destroyed++; return XR_SUCCESS; }
static XRAPI_ATTR XrResult XRAPI_CALL t_locate_views(XrSession s,
      const XrViewLocateInfo *li, XrViewState *vs, uint32_t cap, uint32_t *n, XrView *v)
{ *n = 0; return XR_ERROR_RUNTIME_FAILURE; }
static bool    t_head_tracked;   /* xrLocateSpace finds the head at t_head */
/* Location flags it clears, as a runtime that lost the head does. */
static XrSpaceLocationFlags t_head_lost;
static XrPosef t_head;
static XRAPI_ATTR XrResult XRAPI_CALL t_locate_space(XrSpace a, XrSpace b,
      XrTime t, XrSpaceLocation *loc)
{
   if (!t_head_tracked)
      return XR_ERROR_RUNTIME_FAILURE;
   loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;
   loc->pose          = t_head;
   loc->locationFlags &= ~t_head_lost;
   return XR_SUCCESS;
}

/* the session's hooks, counted */
static unsigned t_h_ended, t_h_exiting, t_h_local;
static XrSessionState t_h_last_state;
static void t_hook_state(void *u, XrSessionState s)
{ t_h_last_state = s; t_state_seq = ++t_seq; }
static void t_hook_ended(void *u) { t_h_ended++; t_ended_seq = ++t_seq; }
static void t_hook_exiting(void *u) { t_h_exiting++; t_exiting_seq = ++t_seq; }
static void t_hook_local(void *u) { t_h_local++; }
static void t_hook_lock(void *u) { t_locks++; }
static void t_hook_unlock(void *u) { t_unlocks++; }

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
static XRAPI_ATTR XrResult XRAPI_CALL t_enum_images(XrSwapchain s,
      uint32_t cap, uint32_t *n, XrSwapchainImageBaseHeader *imgs)
{
   /* Only count queries reach here: the binding lists the images. */
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
static uint32_t t_last_layer_count;
static unsigned t_extra_cap;
static XrResult t_wait_frame_res = XR_SUCCESS;
static XrResult t_end_frame_res  = XR_SUCCESS;
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
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_end_frame(XrSession s,
      const XrFrameEndInfo *ei)
{
   t_ends++;
   t_last_layer_count = ei->layerCount;
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

/* ---- the units, as they ship ---- */
#include "../../../gfx/common/openxr_runtime.c"
#include "../../../gfx/common/openxr_session.c"
#include "../../../gfx/common/openxr_swapchain.c"
#include "../../../gfx/common/openxr_frame.c"

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
   t_max_w           = 8192;
   t_max_h           = 2048;
   t_missing         = NULL;
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
   s->hooks.lock          = t_hook_lock;
   s->hooks.unlock        = t_hook_unlock;
   t_num_events = t_next_event = 0;
   t_poll_result = XR_SUCCESS;
   t_begun = t_ended_sessions = t_locks = t_unlocks = 0;
   t_spaces_made = t_spaces_destroyed = t_space_fail_at = 0;
   t_h_ended = t_h_exiting = t_h_local = 0;
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
   CHECK(t_begun == 1 && s.running, "not begun");
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
   CHECK(t_ended_sessions == 1 && !s.running, "STOPPING did not end it");
   CHECK(t_end_in_lock, "xrEndSession ran outside the lock");
   CHECK(!openxr_session_visible(&s), "visible while stopping");

   /* Another session's events are not this one's. */
   t_session_fresh(&rt, &s);
   openxr_session_load(&s, &rt);
   openxr_session_create(&s, &dummy_binding);
   t_push_state(T_OTHER, XR_SESSION_STATE_READY);
   t_push_space(T_OTHER, XR_REFERENCE_SPACE_TYPE_LOCAL);
   openxr_session_poll(&s);
   CHECK(t_begun == 0 && t_h_local == 0, "acted on another session's events");

   /* LOCAL recentered for this session; STAGE is not LOCAL. */
   t_push_space(T_SESSION, XR_REFERENCE_SPACE_TYPE_STAGE);
   t_push_space(T_SESSION, XR_REFERENCE_SPACE_TYPE_LOCAL);
   openxr_session_poll(&s);
   CHECK(t_h_local == 1, "local_changed %u times", t_h_local);

   /* EXITING: ended once, exiting every time. */
   openxr_session_set_alive(&s, true);
   t_push_state(T_SESSION, XR_SESSION_STATE_EXITING);
   openxr_session_poll(&s);
   CHECK(t_h_ended == 1 && t_h_exiting == 1, "exiting: ended %u exiting %u",
         t_h_ended, t_h_exiting);
   CHECK(s.ended && !openxr_session_alive(&s) && !s.lost, "exit state");
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
   CHECK(s.ended && !s.lost && t_h_ended == 1, "loss pending");
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
   CHECK(s.lost && s.ended, "XR_ERROR_INSTANCE_LOST not lost");

   /* Reset forgets an ended session but not a lost instance (the host
    * drops the runtime on it); destroy frees the spaces. */
   openxr_session_reset(&s);
   CHECK(!s.session && !s.ended && s.lost, "reset keeps lost");
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
   openxr_frame_request_recenter(&f);
   openxr_frame_run(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.70710678f, 0.70710678f, 0.25f, 1.5f, -0.5f)
         && !retro_atomic_load_acquire_int(&f.recenter),
         "recentered at %.2f,%.2f,%.2f yaw %.3f", anchor.position.x,
         anchor.position.y, anchor.position.z, anchor.orientation.y);
   openxr_frame_request_recenter(&f);
   openxr_frame_local_changed(&f);
   openxr_frame_get_anchor(&f, &anchor);
   CHECK(t_pose_is(&anchor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
         "the runtime's recenter kept the anchor");
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
   t_head_tracked = false;

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
      CHECK(!openxr_session_alive(&s) && !s.running && s.lost == cases[i].lost,
            "%u: alive %d running %d lost %d", i,
            (int)openxr_session_alive(&s), (int)s.running, (int)s.lost);
      CHECK(t_logged_times(cases[i].line) == 1, "%u: logged %u times: %s",
            i, t_logged_times(cases[i].line), t_log);
      t_frame_free(&rt, &s, &f);
   }
}

int main(void)
{
   test_runtime();
   test_session();
   test_swapchain();
   test_frame();
   test_frame_pacing();
   test_frame_lost();
   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   puts("PASS openxr_core_test");
   return 0;
}
