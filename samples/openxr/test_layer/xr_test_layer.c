/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Test-only OpenXR API layer for RetroArch's headset output.
 *
 * Records every xrEndFrame's layers as JSON lines, copies each swapchain
 * image as the application releases it, and writes the images a frame
 * shows to PNG every Nth frame. A script file overrides answers the
 * runtime gives: the head pose, session state events, the controllers'
 * actions and hand poses. Haptics, suggested bindings and synced
 * action sets are recorded.
 *
 * Loaded through xr_test_layer_frame.json it also offers
 * XR_VALVE_frame_controller_interaction, as the Steam Frame's runtime
 * does: the extension is taken out before the runtime sees it, and the
 * layer accepts that profile's suggested bindings itself.
 *
 * It can also run the headset slower than the runtime (Monado's null
 * compositor is fixed at 20 Hz) and offer XR_FB_display_refresh_rate
 * rates of its own; the requests it gets are recorded.
 *
 *   RA_XR_LAYER_OUT         directory for frames.jsonl and snap_*.png
 *   RA_XR_LAYER_SNAP_EVERY  write images every Nth frame; 0 never
 *   RA_XR_LAYER_SCRIPT      script file, re-read when it changes
 *   RA_XR_LAYER_QUAD_INDEX0 1: quads show array layer 0 whatever their
 *                           imageArrayIndex, as SteamVR on the Steam
 *                           Frame does
 *
 * Script lines:
 *   head <x> <y> <z> <yaw>  the VIEW space's pose in LOCAL, yaw in degrees
 *   head off                the runtime's own pose again
 *   fail session            xrCreateSession fails
 *   fail waitframe          xrWaitFrame reports XR_ERROR_SESSION_LOST
 *   fail instance           xrPollEvent reports XR_ERROR_INSTANCE_LOST
 *                           while a session exists
 *   state <n>               queue session state n: while a session
 *                           exists, each xrPollEvent reports the oldest
 *   space <n>               the next xrPollEvent reports reference space
 *                           type n changing (2, LOCAL: a runtime recenter)
 *   divide <k>              the application sees one runtime frame in k,
 *                           a headset at 1/k of the runtime's rate; the
 *                           others are begun and left for its
 *                           xrBeginFrame to discard
 *   rates <hz> [<hz> ...]   xrEnumerateDisplayRefreshRatesFB lists these,
 *                           and a request for one of them sets divide;
 *                           rates off: the runtime's own
 *   action <set>/<name>[@left|@right] <x> [<y>]
 *                           an action's state while its set is synced:
 *                           boolean x != 0, float x, vector2f x y
 *   aim <left|right> <px> <py> <pz> <tx> <ty> <tz>
 *                           that hand's pose spaces at p, -Z towards t
 *   aim <left|right> off    that hand untracked
 *
 * A changed script (a new mtime, inode or size) is applied line by
 * line. head, fail, divide and rates stay in effect until a later line
 * of the same kind replaces them (head off, fail off). state and space
 * fire once per change of the file. Every read starts from no actions
 * and no aims: a script is the whole controller state. Action states
 * and hand poses never come from the runtime.
 */

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <zlib.h>

#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_loader_negotiation.h>

#define MAX_CHAINS 32
#define MAX_IMAGES 8
#define MAX_SPACES 64
#define MAX_FRAME_LAYERS 64
#define MAX_RATES 8

#define MAX_SETS        16
#define FRAME_EXT       "XR_VALVE_frame_controller_interaction"
#define FRAME_PROFILE   "/interaction_profiles/valve/frame_controller_valve"
#define MAX_ACTIONS     128
#define MAX_SCRIPTED    64
#define MAX_HAND_SPACES 32
#define ACTION_NAME     (XR_MAX_ACTION_SET_NAME_SIZE + XR_MAX_ACTION_NAME_SIZE)

struct action_rec
{
   XrAction handle;
   XrActionSet set;
   XrActionType type;
   char name[ACTION_NAME];   /* "set/action" */
};

struct scripted
{
   char name[ACTION_NAME];
   int hand;                 /* 0 any, 1 left, 2 right */
   float v[2];
};

struct chain
{
   XrSwapchain handle;
   int id;
   uint32_t width, height, layers;
   int64_t format;
   VkImage images[MAX_IMAGES];
   uint32_t num_images;
   uint32_t acquired[MAX_IMAGES];   /* indices, oldest first */
   uint32_t num_acquired;
   unsigned char *pixels;           /* last released image, all layers */
   bool has_pixels;
   VkBuffer buffer;
   VkDeviceMemory memory;
   VkDeviceSize size;
};

static struct
{
   pthread_mutex_t lock;
   FILE *out;
   char dir[1024];
   unsigned snap_every;
   bool quad_index0;
   char script[1024];
   struct timespec script_mtime;
   ino_t script_ino;
   off_t script_size;
   bool head_set;
   bool fail_session;
   bool fail_waitframe;
   bool fail_instance;
   int inject_states[8];     /* oldest first */
   unsigned num_inject_states;
   int inject_space;
   XrSession session;
   XrPosef head;
   uint64_t frames;
   XrDuration period;
   XrDuration runtime_period;  /* the runtime's own */
   unsigned divide;            /* one runtime frame in divide */
   float rates[MAX_RATES];     /* offered in the runtime's place */
   unsigned num_rates;

   VkPhysicalDevice gpu;
   VkDevice device;
   VkQueue queue;
   uint32_t queue_family;
   VkCommandPool pool;
   VkCommandBuffer cmd;
   VkFence fence;

   struct chain chains[MAX_CHAINS];
   int next_id;
   XrSpace spaces[MAX_SPACES];
   XrReferenceSpaceType space_types[MAX_SPACES];
   unsigned num_spaces;

   PFN_xrGetInstanceProcAddr gipa;
   PFN_xrCreateSession CreateSession;
   PFN_xrDestroySession DestroySession;
   PFN_xrPollEvent PollEvent;
   PFN_xrCreateReferenceSpace CreateReferenceSpace;
   PFN_xrLocateSpace LocateSpace;
   PFN_xrCreateSwapchain CreateSwapchain;
   PFN_xrDestroySwapchain DestroySwapchain;
   PFN_xrEnumerateSwapchainImages EnumerateSwapchainImages;
   PFN_xrAcquireSwapchainImage AcquireSwapchainImage;
   PFN_xrReleaseSwapchainImage ReleaseSwapchainImage;
   PFN_xrWaitFrame WaitFrame;
   PFN_xrBeginFrame BeginFrame;
   PFN_xrEndFrame EndFrame;

   XrInstance instance;
   bool frame;               /* the application enabled FRAME_EXT */
   struct
   {
      XrActionSet handle;
      char name[XR_MAX_ACTION_SET_NAME_SIZE];
   } sets[MAX_SETS];
   unsigned num_sets;
   struct action_rec actions[MAX_ACTIONS];
   unsigned num_actions;
   struct
   {
      XrSpace space;
      int hand;
   } hand_spaces[MAX_HAND_SPACES];
   unsigned num_hand_spaces;
   XrActionSet synced[MAX_SETS];
   unsigned num_synced;
   char synced_names[512];
   struct scripted scripted[MAX_SCRIPTED];
   unsigned num_scripted;
   bool aim_set[3];
   XrPosef aim[3];
   int hand_logged[3];       /* 0 never, 1 untracked, 2 tracked */

   PFN_xrPathToString PathToString;
   PFN_xrCreateActionSet CreateActionSet;
   PFN_xrDestroyActionSet DestroyActionSet;
   PFN_xrCreateAction CreateAction;
   PFN_xrCreateActionSpace CreateActionSpace;
   PFN_xrDestroySpace DestroySpace;
   PFN_xrSyncActions SyncActions;
   PFN_xrGetActionStateBoolean GetActionStateBoolean;
   PFN_xrGetActionStateFloat GetActionStateFloat;
   PFN_xrGetActionStateVector2f GetActionStateVector2f;
   PFN_xrGetActionStatePose GetActionStatePose;
   PFN_xrApplyHapticFeedback ApplyHapticFeedback;
   PFN_xrStopHapticFeedback StopHapticFeedback;
   PFN_xrSuggestInteractionProfileBindings SuggestInteractionProfileBindings;
   PFN_xrEnumerateDisplayRefreshRatesFB EnumerateDisplayRefreshRatesFB;
   PFN_xrRequestDisplayRefreshRateFB RequestDisplayRefreshRateFB;
} L = { .lock = PTHREAD_MUTEX_INITIALIZER };

static long long now_us(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

static void layer_open(void)
{
   const char *dir   = getenv("RA_XR_LAYER_OUT");
   const char *snap  = getenv("RA_XR_LAYER_SNAP_EVERY");
   const char *scr   = getenv("RA_XR_LAYER_SCRIPT");
   const char *idx0  = getenv("RA_XR_LAYER_QUAD_INDEX0");
   char path[1100];
   if (dir && *dir && !L.out)
   {
      snprintf(L.dir, sizeof(L.dir), "%s", dir);
      snprintf(path, sizeof(path), "%s/frames.jsonl", dir);
      L.out = fopen(path, "w");
   }
   L.snap_every  = snap ? (unsigned)strtoul(snap, NULL, 10) : 0;
   L.quad_index0 = idx0 && atoi(idx0) != 0;
   if (scr)
      snprintf(L.script, sizeof(L.script), "%s", scr);
}

static struct chain *find_chain(XrSwapchain h)
{
   int i;
   for (i = 0; i < MAX_CHAINS; i++)
      if (L.chains[i].handle == h)
         return &L.chains[i];
   return NULL;
}

static int space_type(XrSpace s)
{
   unsigned i;
   for (i = 0; i < L.num_spaces; i++)
      if (L.spaces[i] == s)
         return (int)L.space_types[i];
   return -1;
}

/* ---- Actions and hands ---- */

/* 1 left, 2 right, 0 anything else. Caller holds L.lock. */
static int hand_of(XrPath path)
{
   char s[XR_MAX_PATH_LENGTH];
   uint32_t n = 0;
   if (     path == XR_NULL_PATH || !L.PathToString
         || XR_FAILED(L.PathToString(L.instance, path, sizeof(s), &n, s)))
      return 0;
   if (!strcmp(s, "/user/hand/left"))
      return 1;
   if (!strcmp(s, "/user/hand/right"))
      return 2;
   return 0;
}

static const char *hand_name(int hand)
{
   return hand == 1 ? "left" : (hand == 2 ? "right" : "any");
}

static const char *set_name(XrActionSet s)
{
   unsigned i;
   for (i = 0; i < L.num_sets; i++)
      if (L.sets[i].handle == s)
         return L.sets[i].name;
   return "?";
}

static const struct action_rec *find_action(XrAction a)
{
   unsigned i;
   for (i = 0; i < L.num_actions; i++)
      if (L.actions[i].handle == a)
         return &L.actions[i];
   return NULL;
}

static bool set_synced(XrActionSet s)
{
   unsigned i;
   for (i = 0; i < L.num_synced; i++)
      if (L.synced[i] == s)
         return true;
   return false;
}

/* The script's value for an action asked for a hand, while its set is
 * synced; NULL reads inactive. Caller holds L.lock. */
static const struct scripted *scripted_value(XrAction action, XrPath sub)
{
   unsigned i;
   int hand;
   const struct action_rec *a = find_action(action);
   if (!a || !set_synced(a->set))
      return NULL;
   hand = hand_of(sub);
   for (i = 0; i < L.num_scripted; i++)
      if (     !strcmp(L.scripted[i].name, a->name)
            && (!hand || !L.scripted[i].hand || L.scripted[i].hand == hand))
         return &L.scripted[i];
   return NULL;
}

/* 1 or 2 for a hand's pose action space, 0 for any other space. */
static int hand_space(XrSpace s)
{
   unsigned i;
   for (i = 0; i < L.num_hand_spaces; i++)
      if (L.hand_spaces[i].space == s)
         return L.hand_spaces[i].hand;
   return 0;
}

/* ---- Script ---- */

static void script_head(const char *args)
{
   float x, y, z, yaw;
   if (!strncmp(args, "off", 3))
   {
      L.head_set = false;
      return;
   }
   if (sscanf(args, "%f %f %f %f", &x, &y, &z, &yaw) != 4)
      return;
   L.head.position.x    = x;
   L.head.position.y    = y;
   L.head.position.z    = z;
   L.head.orientation.x = 0.0f;
   L.head.orientation.y = sinf(yaw * (float)M_PI / 360.0f);
   L.head.orientation.z = 0.0f;
   L.head.orientation.w = cosf(yaw * (float)M_PI / 360.0f);
   L.head_set           = true;
}

static void script_fail(const char *args)
{
   L.fail_session   = !strncmp(args, "session", 7);
   L.fail_waitframe = !strncmp(args, "waitframe", 9);
   L.fail_instance  = !strncmp(args, "instance", 8);
}

/* Queued until a session exists, so a script may lead with states. */
static void script_state(const char *args)
{
   if (L.num_inject_states
         < sizeof(L.inject_states) / sizeof(L.inject_states[0]))
      L.inject_states[L.num_inject_states++] = atoi(args);
}

static void script_space(const char *args)
{
   L.inject_space = atoi(args);
}

static void script_divide(const char *args)
{
   int k    = atoi(args);
   L.divide = (k > 1) ? (unsigned)k : 1;
}

static void script_rates(const char *args)
{
   char *end;
   const char *p = args;
   L.num_rates   = 0;
   if (!strncmp(args, "off", 3))
      return;
   while (L.num_rates < MAX_RATES)
   {
      float r = strtof(p, &end);
      if (end == p || r <= 0.0f)
         break;
      L.rates[L.num_rates++] = r;
      p = end;
   }
}

static void script_reset(void)
{
   L.num_scripted = 0;
   L.aim_set[1]   = false;
   L.aim_set[2]   = false;
}

static int parse_hand(const char *s)
{
   if (!strncmp(s, "left", 4))
      return 1;
   if (!strncmp(s, "right", 5))
      return 2;
   return 0;
}

static void script_action(const char *args)
{
   char name[ACTION_NAME];
   float x = 0.0f, y = 0.0f;
   char *at;
   struct scripted *s;
   if (     L.num_scripted >= MAX_SCRIPTED
         || sscanf(args, "%127s %f %f", name, &x, &y) < 2)
      return;
   s = &L.scripted[L.num_scripted++];
   memset(s, 0, sizeof(*s));
   if ((at = strchr(name, '@')))
   {
      *at     = '\0';
      s->hand = parse_hand(at + 1);
   }
   snprintf(s->name, sizeof(s->name), "%s", name);
   s->v[0] = x;
   s->v[1] = y;
}

static void script_aim(const char *args)
{
   char side[16];
   float p[3], t[3], fx, fy, fz, len, yaw, pitch;
   int hand;
   if (sscanf(args, "%15s", side) != 1 || !(hand = parse_hand(side)))
      return;
   if (strstr(args, "off"))
   {
      L.aim_set[hand] = false;
      return;
   }
   if (sscanf(args, "%15s %f %f %f %f %f %f", side, &p[0], &p[1], &p[2],
            &t[0], &t[1], &t[2]) != 7)
      return;
   fx  = t[0] - p[0];
   fy  = t[1] - p[1];
   fz  = t[2] - p[2];
   len = sqrtf(fx * fx + fy * fy + fz * fz);
   if (len < 1e-6f)
      return;
   yaw   = atan2f(-fx / len, -fz / len);
   pitch = asinf(fy / len);
   /* Pitch about +X, then yaw about +Y: -Z points along (f). */
   L.aim[hand].orientation.x = cosf(yaw / 2.0f) * sinf(pitch / 2.0f);
   L.aim[hand].orientation.y = sinf(yaw / 2.0f) * cosf(pitch / 2.0f);
   L.aim[hand].orientation.z = -sinf(yaw / 2.0f) * sinf(pitch / 2.0f);
   L.aim[hand].orientation.w = cosf(yaw / 2.0f) * cosf(pitch / 2.0f);
   L.aim[hand].position.x    = p[0];
   L.aim[hand].position.y    = p[1];
   L.aim[hand].position.z    = p[2];
   L.aim_set[hand]           = true;
}

static const struct
{
   const char *name;
   void (*run)(const char *args);
} script_cmds[] = {
   { "head", script_head },
   { "fail", script_fail },
   { "state", script_state },
   { "action", script_action },
   { "aim", script_aim },
   { "space", script_space },
   { "divide", script_divide },
   { "rates", script_rates },
};

/* Caller holds L.lock. */
static void script_poll(void)
{
   size_t i;
   char line[256];
   struct stat st;
   FILE *f;
   if (!L.script[0] || stat(L.script, &st) != 0)
      return;
   /* A quick rewrite can keep the mtime; run.py replaces the file, so
    * the inode changes too. */
   if (     st.st_mtim.tv_sec  == L.script_mtime.tv_sec
         && st.st_mtim.tv_nsec == L.script_mtime.tv_nsec
         && st.st_ino          == L.script_ino
         && st.st_size         == L.script_size)
      return;
   L.script_mtime = st.st_mtim;
   L.script_ino   = st.st_ino;
   L.script_size  = st.st_size;
   if (!(f = fopen(L.script, "r")))
      return;
   script_reset();
   while (fgets(line, sizeof(line), f))
      for (i = 0; i < sizeof(script_cmds) / sizeof(script_cmds[0]); i++)
      {
         size_t n = strlen(script_cmds[i].name);
         if (!strncmp(line, script_cmds[i].name, n)
               && (line[n] == ' ' || line[n] == '\n' || !line[n]))
            script_cmds[i].run(line + n + (line[n] == ' '));
      }
   fclose(f);
}

/* ---- Copies and PNGs ---- */

static bool rgba8(int64_t format, bool *bgra)
{
   switch (format)
   {
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_R8G8B8A8_SRGB:
         *bgra = false;
         return true;
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_SRGB:
         *bgra = true;
         return true;
      default:
         break;
   }
   return false;
}

static bool ensure_cmd(void)
{
   VkCommandPoolCreateInfo pi = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
   VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
   VkFenceCreateInfo fi = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
   if (L.cmd)
      return true;
   pi.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pi.queueFamilyIndex = L.queue_family;
   if (vkCreateCommandPool(L.device, &pi, NULL, &L.pool) != VK_SUCCESS)
      return false;
   ai.commandPool        = L.pool;
   ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   ai.commandBufferCount = 1;
   if (vkAllocateCommandBuffers(L.device, &ai, &L.cmd) != VK_SUCCESS)
      return false;
   return vkCreateFence(L.device, &fi, NULL, &L.fence) == VK_SUCCESS;
}

static bool ensure_buffer(struct chain *c, VkDeviceSize size)
{
   uint32_t i;
   VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
   VkMemoryAllocateInfo mi = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkMemoryRequirements req;
   VkPhysicalDeviceMemoryProperties props;
   VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   if (c->buffer && c->size == size)
      return true;
   if (c->buffer)
   {
      vkDestroyBuffer(L.device, c->buffer, NULL);
      vkFreeMemory(L.device, c->memory, NULL);
      c->buffer = VK_NULL_HANDLE;
   }
   free(c->pixels);
   if (!(c->pixels = malloc(size)))
      return false;
   bi.size        = size;
   bi.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   if (vkCreateBuffer(L.device, &bi, NULL, &c->buffer) != VK_SUCCESS)
      return false;
   vkGetBufferMemoryRequirements(L.device, c->buffer, &req);
   vkGetPhysicalDeviceMemoryProperties(L.gpu, &props);
   for (i = 0; i < props.memoryTypeCount; i++)
      if ((req.memoryTypeBits & (1u << i))
            && (props.memoryTypes[i].propertyFlags & want) == want)
         break;
   if (i == props.memoryTypeCount)
      return false;
   mi.allocationSize  = req.size;
   mi.memoryTypeIndex = i;
   if (vkAllocateMemory(L.device, &mi, NULL, &c->memory) != VK_SUCCESS)
      return false;
   vkBindBufferMemory(L.device, c->buffer, c->memory, 0);
   c->size = size;
   return true;
}

/* Caller holds L.lock. Submitting on the application's queue is safe
 * because RetroArch calls xrReleaseSwapchainImage under its queue lock,
 * and hello_xr is single-threaded. */
static void capture(struct chain *c, uint32_t index)
{
   bool bgra;
   void *map;
   VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
   VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
   VkBufferMemoryBarrier bb = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
   VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
   VkBufferImageCopy region;
   VkDeviceSize size = (VkDeviceSize)c->width * c->height * 4 * c->layers;

   if (!L.device || !L.snap_every || index >= c->num_images
         || !rgba8(c->format, &bgra))
      return;
   if (!ensure_cmd() || !ensure_buffer(c, size))
      return;

   vkResetCommandBuffer(L.cmd, 0);
   bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkBeginCommandBuffer(L.cmd, &bi);
   b.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   b.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
   b.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   b.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   b.image               = c->images[index];
   b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   b.subresourceRange.levelCount = 1;
   b.subresourceRange.layerCount = c->layers;
   vkCmdPipelineBarrier(L.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
   memset(&region, 0, sizeof(region));
   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.layerCount = c->layers;
   region.imageExtent.width           = c->width;
   region.imageExtent.height          = c->height;
   region.imageExtent.depth           = 1;
   vkCmdCopyImageToBuffer(L.cmd, c->images[index],
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, c->buffer, 1, &region);
   /* The fence wait alone does not make the copy visible to the host. */
   bb.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
   bb.dstAccessMask       = VK_ACCESS_HOST_READ_BIT;
   bb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   bb.buffer              = c->buffer;
   bb.size                = VK_WHOLE_SIZE;
   vkCmdPipelineBarrier(L.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &bb, 0, NULL);
   b.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
   b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   b.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   b.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   vkCmdPipelineBarrier(L.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL,
         1, &b);
   vkEndCommandBuffer(L.cmd);

   si.commandBufferCount = 1;
   si.pCommandBuffers    = &L.cmd;
   vkResetFences(L.device, 1, &L.fence);
   if (vkQueueSubmit(L.queue, 1, &si, L.fence) != VK_SUCCESS)
      return;
   vkWaitForFences(L.device, 1, &L.fence, VK_TRUE, UINT64_MAX);
   if (vkMapMemory(L.device, c->memory, 0, size, 0, &map) != VK_SUCCESS)
      return;
   memcpy(c->pixels, map, size);
   vkUnmapMemory(L.device, c->memory);
   c->has_pixels = true;
}

static void put_be32(unsigned char *p, uint32_t v)
{
   p[0] = (unsigned char)(v >> 24);
   p[1] = (unsigned char)(v >> 16);
   p[2] = (unsigned char)(v >> 8);
   p[3] = (unsigned char)v;
}

static void png_chunk(FILE *f, const char *type,
      const unsigned char *data, uint32_t len)
{
   unsigned char b[4];
   uLong crc = crc32(0L, (const Bytef*)type, 4);
   if (len)
      crc = crc32(crc, data, len);
   put_be32(b, len);
   fwrite(b, 1, 4, f);
   fwrite(type, 1, 4, f);
   if (len)
      fwrite(data, 1, len, f);
   put_be32(b, (uint32_t)crc);
   fwrite(b, 1, 4, f);
}

static void png_write(const char *path, const unsigned char *src,
      uint32_t w, uint32_t h, bool bgra)
{
   static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
   unsigned char ihdr[13];
   size_t stride = (size_t)w * 4 + 1;
   uLongf zlen   = compressBound(stride * h);
   unsigned char *raw = malloc(stride * h);
   unsigned char *z   = malloc(zlen);
   uint32_t x, y;
   FILE *f;
   if (!raw || !z)
      goto end;
   for (y = 0; y < h; y++)
   {
      unsigned char *row = raw + y * stride;
      row[0] = 0;
      for (x = 0; x < w; x++)
      {
         const unsigned char *p = src + ((size_t)y * w + x) * 4;
         row[1 + x * 4 + 0] = bgra ? p[2] : p[0];
         row[1 + x * 4 + 1] = p[1];
         row[1 + x * 4 + 2] = bgra ? p[0] : p[2];
         row[1 + x * 4 + 3] = p[3];
      }
   }
   if (compress2(z, &zlen, raw, stride * h, 6) != Z_OK)
      goto end;
   if (!(f = fopen(path, "wb")))
      goto end;
   put_be32(ihdr, w);
   put_be32(ihdr + 4, h);
   ihdr[8]  = 8;   /* bits */
   ihdr[9]  = 6;   /* RGBA */
   ihdr[10] = 0;
   ihdr[11] = 0;
   ihdr[12] = 0;
   fwrite(sig, 1, 8, f);
   png_chunk(f, "IHDR", ihdr, 13);
   png_chunk(f, "IDAT", z, (uint32_t)zlen);
   png_chunk(f, "IEND", NULL, 0);
   fclose(f);
end:
   free(raw);
   free(z);
}

/* Caller holds L.lock. */
static void snap(uint64_t n, XrSwapchain h, uint32_t layer)
{
   bool bgra;
   char path[1200];
   struct chain *c = find_chain(h);
   if (!c || !c->has_pixels || layer >= c->layers || !rgba8(c->format, &bgra))
      return;
   snprintf(path, sizeof(path), "%s/snap_%llu_sc%d_l%u.png", L.dir,
         (unsigned long long)n, c->id, layer);
   png_write(path, c->pixels + (size_t)c->width * c->height * 4 * layer,
         c->width, c->height, bgra);
}

/* ---- Hooks ---- */

static XRAPI_ATTR XrResult XRAPI_CALL layer_CreateSession(XrInstance instance,
      const XrSessionCreateInfo *info, XrSession *session)
{
   bool fail;
   XrResult res;
   const XrBaseInStructure *p;
   pthread_mutex_lock(&L.lock);
   script_poll();
   fail = L.fail_session;
   pthread_mutex_unlock(&L.lock);
   if (fail)
      return XR_ERROR_RUNTIME_FAILURE;
   if (XR_FAILED(res = L.CreateSession(instance, info, session)))
      return res;
   pthread_mutex_lock(&L.lock);
   for (p = (const XrBaseInStructure*)info->next; p; p = p->next)
      if (p->type == XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR)
      {
         const XrGraphicsBindingVulkanKHR *b =
            (const XrGraphicsBindingVulkanKHR*)p;
         L.session      = *session;
         L.gpu          = b->physicalDevice;
         L.device       = b->device;
         L.queue_family = b->queueFamilyIndex;
         vkGetDeviceQueue(b->device, b->queueFamilyIndex, b->queueIndex,
               &L.queue);
      }
   pthread_mutex_unlock(&L.lock);
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_DestroySession(XrSession session)
{
   int i;
   pthread_mutex_lock(&L.lock);
   /* Before the application destroys its device. */
   if (L.device)
   {
      for (i = 0; i < MAX_CHAINS; i++)
         if (L.chains[i].buffer)
         {
            vkDestroyBuffer(L.device, L.chains[i].buffer, NULL);
            vkFreeMemory(L.device, L.chains[i].memory, NULL);
            L.chains[i].buffer = VK_NULL_HANDLE;
            L.chains[i].size   = 0;
         }
      if (L.fence)
         vkDestroyFence(L.device, L.fence, NULL);
      if (L.pool)
         vkDestroyCommandPool(L.device, L.pool, NULL);
      L.fence  = VK_NULL_HANDLE;
      L.pool   = VK_NULL_HANDLE;
      L.cmd    = VK_NULL_HANDLE;
      L.device = VK_NULL_HANDLE;
   }
   /* Its spaces and swapchains go with it, and the runtime may hand their
    * handles out again. */
   for (i = 0; i < MAX_CHAINS; i++)
   {
      free(L.chains[i].pixels);
      memset(&L.chains[i], 0, sizeof(L.chains[i]));
   }
   L.num_spaces      = 0;
   L.num_hand_spaces = 0;
   L.session         = XR_NULL_HANDLE;
   pthread_mutex_unlock(&L.lock);
   return L.DestroySession(session);
}

/* A scripted reference space change into ev, once, while a session
 * exists. */
static bool take_space_change(XrEventDataBuffer *ev)
{
   int type;
   XrEventDataReferenceSpaceChangePending *c =
      (XrEventDataReferenceSpaceChangePending*)ev;
   pthread_mutex_lock(&L.lock);
   type           = L.session ? L.inject_space : 0;
   L.inject_space = 0;
   if (type)
   {
      memset(ev, 0, sizeof(*ev));
      c->type               = XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING;
      c->session            = L.session;
      c->referenceSpaceType = (XrReferenceSpaceType)type;
      c->poseInPreviousSpace.orientation.w = 1.0f;
      if (L.out)
      {
         fprintf(L.out, "{\"ev\":\"space\",\"space\":%d,\"t_us\":%lld}\n",
               type, now_us());
         fflush(L.out);
      }
   }
   pthread_mutex_unlock(&L.lock);
   return type != 0;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_PollEvent(XrInstance instance,
      XrEventDataBuffer *ev)
{
   int inject;
   bool lost;
   XrResult res;
   pthread_mutex_lock(&L.lock);
   script_poll();
   lost           = L.session && L.fail_instance;
   inject         = 0;
   if (L.session && L.num_inject_states)
   {
      inject = L.inject_states[0];
      L.num_inject_states--;
      memmove(L.inject_states, L.inject_states + 1,
            L.num_inject_states * sizeof(L.inject_states[0]));
   }
   pthread_mutex_unlock(&L.lock);
   if (lost)
      return XR_ERROR_INSTANCE_LOST;
   if (inject)
   {
      XrEventDataSessionStateChanged *s = (XrEventDataSessionStateChanged*)ev;
      memset(ev, 0, sizeof(*ev));
      s->type    = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
      s->session = L.session;
      s->state   = (XrSessionState)inject;
      res        = XR_SUCCESS;
   }
   else if (take_space_change(ev))
      res = XR_SUCCESS;
   else
      res = L.PollEvent(instance, ev);
   if (res == XR_SUCCESS && ev->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
   {
      const XrEventDataSessionStateChanged *s =
         (const XrEventDataSessionStateChanged*)ev;
      pthread_mutex_lock(&L.lock);
      if (L.out)
      {
         fprintf(L.out, "{\"ev\":\"state\",\"state\":%d,\"t_us\":%lld}\n",
               (int)s->state, now_us());
         fflush(L.out);
      }
      pthread_mutex_unlock(&L.lock);
   }
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_CreateReferenceSpace(XrSession session,
      const XrReferenceSpaceCreateInfo *info, XrSpace *space)
{
   XrResult res = L.CreateReferenceSpace(session, info, space);
   if (XR_SUCCEEDED(res))
   {
      pthread_mutex_lock(&L.lock);
      if (L.num_spaces < MAX_SPACES)
      {
         L.spaces[L.num_spaces]      = *space;
         L.space_types[L.num_spaces] = info->referenceSpaceType;
         L.num_spaces++;
      }
      pthread_mutex_unlock(&L.lock);
   }
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_LocateSpace(XrSpace space,
      XrSpace base, XrTime time, XrSpaceLocation *loc)
{
   int hand;
   XrResult res = L.LocateSpace(space, base, time, loc);
   pthread_mutex_lock(&L.lock);
   script_poll();
   hand = hand_space(space);
   if (     XR_SUCCEEDED(res) && L.head_set
         && space_type(space) == XR_REFERENCE_SPACE_TYPE_VIEW
         && space_type(base)  == XR_REFERENCE_SPACE_TYPE_LOCAL)
   {
      loc->pose          = L.head;
      loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
         | XR_SPACE_LOCATION_POSITION_VALID_BIT
         | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT
         | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
   }
   else if (XR_SUCCEEDED(res) && hand)
   {
      /* Hands are only where the script puts them. */
      bool tracked = L.aim_set[hand]
         && space_type(base) == XR_REFERENCE_SPACE_TYPE_LOCAL;
      if (tracked)
      {
         loc->pose          = L.aim[hand];
         loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
            | XR_SPACE_LOCATION_POSITION_VALID_BIT
            | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT
            | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
      }
      else
         loc->locationFlags = 0;
      if (L.hand_logged[hand] != (tracked ? 2 : 1))
      {
         L.hand_logged[hand] = tracked ? 2 : 1;
         if (L.out)
         {
            fprintf(L.out, "{\"ev\":\"hand\",\"hand\":\"%s\",\"tracked\":%s,"
                  "\"t_us\":%lld}\n", hand_name(hand),
                  tracked ? "true" : "false", now_us());
            fflush(L.out);
         }
      }
   }
   pthread_mutex_unlock(&L.lock);
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_CreateSwapchain(XrSession session,
      const XrSwapchainCreateInfo *info, XrSwapchain *sc)
{
   int i;
   XrResult res;
   XrSwapchainCreateInfo ci = *info;
   /* So released images can be copied back. */
   ci.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
   res = L.CreateSwapchain(session, &ci, sc);
   if (XR_FAILED(res))
      return res;
   pthread_mutex_lock(&L.lock);
   for (i = 0; i < MAX_CHAINS; i++)
      if (!L.chains[i].handle)
      {
         struct chain *c = &L.chains[i];
         free(c->pixels);
         memset(c, 0, sizeof(*c));
         c->handle = *sc;
         c->id     = L.next_id++;
         c->width  = info->width;
         c->height = info->height;
         c->layers = info->arraySize;
         c->format = info->format;
         if (L.out)
         {
            fprintf(L.out, "{\"ev\":\"swapchain\",\"sc\":%d,\"w\":%u,\"h\":%u,"
                  "\"layers\":%u,\"format\":%lld}\n", c->id, c->width,
                  c->height, c->layers, (long long)c->format);
            fflush(L.out);
         }
         break;
      }
   pthread_mutex_unlock(&L.lock);
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_DestroySwapchain(XrSwapchain sc)
{
   struct chain *c;
   pthread_mutex_lock(&L.lock);
   if ((c = find_chain(sc)))
   {
      if (c->buffer && L.device)
      {
         vkDestroyBuffer(L.device, c->buffer, NULL);
         vkFreeMemory(L.device, c->memory, NULL);
      }
      free(c->pixels);
      memset(c, 0, sizeof(*c));
   }
   pthread_mutex_unlock(&L.lock);
   return L.DestroySwapchain(sc);
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_EnumerateSwapchainImages(
      XrSwapchain sc, uint32_t cap, uint32_t *count,
      XrSwapchainImageBaseHeader *images)
{
   uint32_t i;
   struct chain *c;
   XrResult res = L.EnumerateSwapchainImages(sc, cap, count, images);
   if (XR_FAILED(res) || !images || images->type != XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR)
      return res;
   pthread_mutex_lock(&L.lock);
   if ((c = find_chain(sc)))
   {
      const XrSwapchainImageVulkanKHR *vk = (const XrSwapchainImageVulkanKHR*)images;
      c->num_images = *count < MAX_IMAGES ? *count : MAX_IMAGES;
      for (i = 0; i < c->num_images; i++)
         c->images[i] = vk[i].image;
   }
   pthread_mutex_unlock(&L.lock);
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_AcquireSwapchainImage(XrSwapchain sc,
      const XrSwapchainImageAcquireInfo *info, uint32_t *index)
{
   struct chain *c;
   XrResult res = L.AcquireSwapchainImage(sc, info, index);
   if (XR_FAILED(res))
      return res;
   pthread_mutex_lock(&L.lock);
   if ((c = find_chain(sc)) && c->num_acquired < MAX_IMAGES)
      c->acquired[c->num_acquired++] = *index;
   pthread_mutex_unlock(&L.lock);
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_ReleaseSwapchainImage(XrSwapchain sc,
      const XrSwapchainImageReleaseInfo *info)
{
   struct chain *c;
   pthread_mutex_lock(&L.lock);
   if ((c = find_chain(sc)) && c->num_acquired)
   {
      uint32_t index = c->acquired[0];
      memmove(c->acquired, c->acquired + 1,
            (c->num_acquired - 1) * sizeof(c->acquired[0]));
      c->num_acquired--;
      capture(c, index);
      if (L.out)
      {
         fprintf(L.out, "{\"ev\":\"release\",\"sc\":%d,\"t_us\":%lld}\n",
               c->id, now_us());
         fflush(L.out);
      }
   }
   pthread_mutex_unlock(&L.lock);
   return L.ReleaseSwapchainImage(sc, info);
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_WaitFrame(XrSession session,
      const XrFrameWaitInfo *info, XrFrameState *state)
{
   bool fail;
   unsigned i, divide;
   XrResult res;
   pthread_mutex_lock(&L.lock);
   script_poll();
   fail   = L.fail_waitframe;
   divide = L.divide ? L.divide : 1;
   if (fail && L.out)
   {
      fprintf(L.out, "{\"ev\":\"fail\",\"fn\":\"xrWaitFrame\",\"t_us\":%lld}\n",
            now_us());
      fflush(L.out);
   }
   pthread_mutex_unlock(&L.lock);
   if (fail)
      return XR_ERROR_SESSION_LOST;
   res = L.WaitFrame(session, info, state);
   /* A slower headset: the frames in between are begun here, and the
    * application's xrBeginFrame discards them. No Vulkan queue work, so
    * the application's queue lock is not needed. */
   for (i = 1; XR_SUCCEEDED(res) && i < divide; i++)
   {
      XrFrameBeginInfo bi;
      memset(&bi, 0, sizeof(bi));
      bi.type = XR_TYPE_FRAME_BEGIN_INFO;
      if (XR_FAILED(res = L.BeginFrame(session, &bi)))
         break;
      res = L.WaitFrame(session, info, state);
   }
   if (XR_SUCCEEDED(res))
   {
      pthread_mutex_lock(&L.lock);
      L.runtime_period               = state->predictedDisplayPeriod;
      state->predictedDisplayPeriod *= divide;
      L.period                       = state->predictedDisplayPeriod;
      pthread_mutex_unlock(&L.lock);
   }
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_EnumerateDisplayRefreshRatesFB(
      XrSession session, uint32_t cap, uint32_t *count, float *rates)
{
   uint32_t i, n;
   float r[MAX_RATES];
   pthread_mutex_lock(&L.lock);
   script_poll();
   n = L.num_rates;
   memcpy(r, L.rates, sizeof(r));
   pthread_mutex_unlock(&L.lock);
   if (!n)
      return L.EnumerateDisplayRefreshRatesFB
         ? L.EnumerateDisplayRefreshRatesFB(session, cap, count, rates)
         : XR_ERROR_FUNCTION_UNSUPPORTED;
   *count = n;
   if (!cap)
      return XR_SUCCESS;
   if (cap < n)
      return XR_ERROR_SIZE_INSUFFICIENT;
   for (i = 0; i < n; i++)
      rates[i] = r[i];
   return XR_SUCCESS;
}

/* Recorded; with scripted rates the headset runs at the one asked for,
 * when it divides the runtime's rate. */
static XRAPI_ATTR XrResult XRAPI_CALL layer_RequestDisplayRefreshRateFB(
      XrSession session, float hz)
{
   bool scripted;
   unsigned k = 0;
   pthread_mutex_lock(&L.lock);
   scripted = L.num_rates != 0;
   if (scripted && hz > 0.0f && L.runtime_period > 0)
   {
      double runtime_hz = 1e9 / (double)L.runtime_period;
      k = (unsigned)(runtime_hz / hz + 0.5);
      if (k < 1 || fabs(runtime_hz / k - hz) > 0.01 * hz)
         k = 0;
      else
         L.divide = k;
   }
   if (L.out)
   {
      fprintf(L.out, "{\"ev\":\"refresh_request\",\"hz\":%.3f,"
            "\"divide\":%u,\"t_us\":%lld}\n", hz, k, now_us());
      fflush(L.out);
   }
   pthread_mutex_unlock(&L.lock);
   if (scripted)
      return XR_SUCCESS;
   return L.RequestDisplayRefreshRateFB
      ? L.RequestDisplayRefreshRateFB(session, hz)
      : XR_ERROR_FUNCTION_UNSUPPORTED;
}

static const char *eye_name(XrEyeVisibility e)
{
   return e == XR_EYE_VISIBILITY_LEFT ? "left"
      : (e == XR_EYE_VISIBILITY_RIGHT ? "right" : "both");
}

static const char *space_name(XrSpace s)
{
   int t = space_type(s);
   return t == XR_REFERENCE_SPACE_TYPE_LOCAL ? "local"
      : (t == XR_REFERENCE_SPACE_TYPE_VIEW ? "view" : "other");
}

static int chain_id(XrSwapchain h)
{
   struct chain *c = find_chain(h);
   return c ? c->id : -1;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_EndFrame(XrSession session,
      const XrFrameEndInfo *info)
{
   uint32_t i, v;
   uint64_t n;
   bool do_snap;
   XrFrameEndInfo shown;
   XrCompositionLayerQuad quads[MAX_FRAME_LAYERS];
   const XrCompositionLayerBaseHeader *ptrs[MAX_FRAME_LAYERS];
   pthread_mutex_lock(&L.lock);
   script_poll();
   /* Recorded, snapped and submitted as the runtime shows them. */
   if (L.quad_index0 && info->layerCount <= MAX_FRAME_LAYERS)
   {
      for (i = 0; i < info->layerCount; i++)
      {
         ptrs[i] = info->layers[i];
         if (info->layers[i]->type != XR_TYPE_COMPOSITION_LAYER_QUAD)
            continue;
         quads[i] = *(const XrCompositionLayerQuad*)info->layers[i];
         quads[i].subImage.imageArrayIndex = 0;
         ptrs[i]  = (const XrCompositionLayerBaseHeader*)&quads[i];
      }
      shown        = *info;
      shown.layers = ptrs;
      info         = &shown;
   }
   n       = ++L.frames;
   do_snap = L.snap_every && !(n % L.snap_every);
   if (L.out)
   {
      fprintf(L.out, "{\"ev\":\"frame\",\"n\":%llu,\"t_us\":%lld,"
            "\"period_ns\":%lld,\"snap\":%s,\"layers\":[",
            (unsigned long long)n, now_us(), (long long)L.period,
            do_snap ? "true" : "false");
      for (i = 0; i < info->layerCount; i++)
      {
         const XrCompositionLayerBaseHeader *h = info->layers[i];
         if (i)
            fputc(',', L.out);
         if (h->type == XR_TYPE_COMPOSITION_LAYER_QUAD)
         {
            const XrCompositionLayerQuad *q = (const XrCompositionLayerQuad*)h;
            fprintf(L.out, "{\"type\":\"quad\",\"sc\":%d,\"eye\":\"%s\","
                  "\"flags\":%llu,\"space\":\"%s\","
                  "\"pose\":[%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f],"
                  "\"size\":[%.5f,%.5f],\"rect\":[%d,%d,%d,%d],\"layer\":%u}",
                  chain_id(q->subImage.swapchain), eye_name(q->eyeVisibility),
                  (unsigned long long)q->layerFlags, space_name(q->space),
                  q->pose.position.x, q->pose.position.y, q->pose.position.z,
                  q->pose.orientation.x, q->pose.orientation.y,
                  q->pose.orientation.z, q->pose.orientation.w,
                  q->size.width, q->size.height,
                  q->subImage.imageRect.offset.x, q->subImage.imageRect.offset.y,
                  q->subImage.imageRect.extent.width,
                  q->subImage.imageRect.extent.height,
                  q->subImage.imageArrayIndex);
            if (do_snap)
               snap(n, q->subImage.swapchain, q->subImage.imageArrayIndex);
         }
         else if (h->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
         {
            const XrCompositionLayerProjection *p =
               (const XrCompositionLayerProjection*)h;
            fputs("{\"type\":\"projection\",\"views\":[", L.out);
            for (v = 0; v < p->viewCount; v++)
            {
               fprintf(L.out, "%s{\"sc\":%d,\"layer\":%u}", v ? "," : "",
                     chain_id(p->views[v].subImage.swapchain),
                     p->views[v].subImage.imageArrayIndex);
               if (do_snap)
                  snap(n, p->views[v].subImage.swapchain,
                        p->views[v].subImage.imageArrayIndex);
            }
            fputs("]}", L.out);
         }
         else
            fprintf(L.out, "{\"type\":\"other\",\"xr_type\":%d}", (int)h->type);
      }
      fputs("]}\n", L.out);
      fflush(L.out);
   }
   pthread_mutex_unlock(&L.lock);
   return L.EndFrame(session, info);
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_CreateActionSet(XrInstance instance,
      const XrActionSetCreateInfo *info, XrActionSet *set)
{
   XrResult res = L.CreateActionSet(instance, info, set);
   if (XR_SUCCEEDED(res))
   {
      pthread_mutex_lock(&L.lock);
      if (L.num_sets < MAX_SETS)
      {
         L.sets[L.num_sets].handle = *set;
         snprintf(L.sets[L.num_sets].name, sizeof(L.sets[0].name), "%s",
               info->actionSetName);
         L.num_sets++;
      }
      pthread_mutex_unlock(&L.lock);
   }
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_CreateAction(XrActionSet set,
      const XrActionCreateInfo *info, XrAction *action)
{
   XrResult res = L.CreateAction(set, info, action);
   if (XR_SUCCEEDED(res))
   {
      pthread_mutex_lock(&L.lock);
      if (L.num_actions < MAX_ACTIONS)
      {
         struct action_rec *a = &L.actions[L.num_actions++];
         a->handle = *action;
         a->set    = set;
         a->type   = info->actionType;
         snprintf(a->name, sizeof(a->name), "%s/%s", set_name(set),
               info->actionName);
      }
      pthread_mutex_unlock(&L.lock);
   }
   return res;
}

/* Its actions go with it. The layer outlives the instance (nodelete)
 * and the runtime may hand the handles out again, so no record may
 * keep an old handle's meaning. */
static XRAPI_ATTR XrResult XRAPI_CALL layer_DestroyActionSet(XrActionSet set)
{
   unsigned i;
   pthread_mutex_lock(&L.lock);
   for (i = 0; i < L.num_actions; )
   {
      if (L.actions[i].set == set)
         L.actions[i] = L.actions[--L.num_actions];
      else
         i++;
   }
   for (i = 0; i < L.num_sets; i++)
      if (L.sets[i].handle == set)
      {
         L.sets[i] = L.sets[--L.num_sets];
         break;
      }
   for (i = 0; i < L.num_synced; i++)
      if (L.synced[i] == set)
      {
         L.synced[i] = L.synced[--L.num_synced];
         break;
      }
   pthread_mutex_unlock(&L.lock);
   return L.DestroyActionSet(set);
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_CreateActionSpace(XrSession session,
      const XrActionSpaceCreateInfo *info, XrSpace *space)
{
   XrResult res = L.CreateActionSpace(session, info, space);
   if (XR_SUCCEEDED(res))
   {
      pthread_mutex_lock(&L.lock);
      if (L.num_hand_spaces < MAX_HAND_SPACES)
      {
         L.hand_spaces[L.num_hand_spaces].space = *space;
         L.hand_spaces[L.num_hand_spaces].hand  = hand_of(info->subactionPath);
         L.num_hand_spaces++;
      }
      pthread_mutex_unlock(&L.lock);
   }
   return res;
}

/* A handle the runtime reuses must not keep an old space's meaning. */
static XRAPI_ATTR XrResult XRAPI_CALL layer_DestroySpace(XrSpace space)
{
   unsigned i;
   pthread_mutex_lock(&L.lock);
   for (i = 0; i < L.num_hand_spaces; i++)
      if (L.hand_spaces[i].space == space)
      {
         L.hand_spaces[i] = L.hand_spaces[--L.num_hand_spaces];
         break;
      }
   for (i = 0; i < L.num_spaces; i++)
      if (L.spaces[i] == space)
      {
         L.num_spaces--;
         L.spaces[i]      = L.spaces[L.num_spaces];
         L.space_types[i] = L.space_types[L.num_spaces];
         break;
      }
   pthread_mutex_unlock(&L.lock);
   return L.DestroySpace(space);
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_SyncActions(XrSession session,
      const XrActionsSyncInfo *info)
{
   uint32_t i;
   size_t len = 0;
   char names[512];
   XrResult res = L.SyncActions(session, info);
   pthread_mutex_lock(&L.lock);
   script_poll();
   names[0]     = '\0';
   L.num_synced = 0;
   /* An unfocused or failed sync makes nothing active. */
   if (res == XR_SUCCESS)
      for (i = 0; i < info->countActiveActionSets && L.num_synced < MAX_SETS; i++)
      {
         XrActionSet s = info->activeActionSets[i].actionSet;
         L.synced[L.num_synced++] = s;
         if (len < sizeof(names))
            len += (size_t)snprintf(names + len, sizeof(names) - len,
                  "%s\"%s\"", i ? "," : "", set_name(s));
      }
   if (strcmp(names, L.synced_names) && L.out)
   {
      fprintf(L.out, "{\"ev\":\"sync\",\"sets\":[%s],\"result\":%d,"
            "\"t_us\":%lld}\n", names, (int)res, now_us());
      fflush(L.out);
   }
   snprintf(L.synced_names, sizeof(L.synced_names), "%s", names);
   pthread_mutex_unlock(&L.lock);
   return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_GetActionStateBoolean(XrSession session,
      const XrActionStateGetInfo *gi, XrActionStateBoolean *state)
{
   const struct scripted *s;
   (void)session;
   pthread_mutex_lock(&L.lock);
   script_poll();
   s = scripted_value(gi->action, gi->subactionPath);
   state->currentState         = (s && s->v[0] != 0.0f) ? XR_TRUE : XR_FALSE;
   state->changedSinceLastSync = XR_FALSE;
   state->lastChangeTime       = 0;
   state->isActive             = s ? XR_TRUE : XR_FALSE;
   pthread_mutex_unlock(&L.lock);
   return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_GetActionStateFloat(XrSession session,
      const XrActionStateGetInfo *gi, XrActionStateFloat *state)
{
   const struct scripted *s;
   (void)session;
   pthread_mutex_lock(&L.lock);
   script_poll();
   s = scripted_value(gi->action, gi->subactionPath);
   state->currentState         = s ? s->v[0] : 0.0f;
   state->changedSinceLastSync = XR_FALSE;
   state->lastChangeTime       = 0;
   state->isActive             = s ? XR_TRUE : XR_FALSE;
   pthread_mutex_unlock(&L.lock);
   return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_GetActionStateVector2f(XrSession session,
      const XrActionStateGetInfo *gi, XrActionStateVector2f *state)
{
   const struct scripted *s;
   (void)session;
   pthread_mutex_lock(&L.lock);
   script_poll();
   s = scripted_value(gi->action, gi->subactionPath);
   state->currentState.x       = s ? s->v[0] : 0.0f;
   state->currentState.y       = s ? s->v[1] : 0.0f;
   state->changedSinceLastSync = XR_FALSE;
   state->lastChangeTime       = 0;
   state->isActive             = s ? XR_TRUE : XR_FALSE;
   pthread_mutex_unlock(&L.lock);
   return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_GetActionStatePose(XrSession session,
      const XrActionStateGetInfo *gi, XrActionStatePose *state)
{
   int hand;
   (void)session;
   pthread_mutex_lock(&L.lock);
   script_poll();
   hand            = hand_of(gi->subactionPath);
   state->isActive = (hand ? L.aim_set[hand]
         : (L.aim_set[1] || L.aim_set[2])) ? XR_TRUE : XR_FALSE;
   pthread_mutex_unlock(&L.lock);
   return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_ApplyHapticFeedback(XrSession session,
      const XrHapticActionInfo *info, const XrHapticBaseHeader *fb)
{
   pthread_mutex_lock(&L.lock);
   if (L.out && fb->type == XR_TYPE_HAPTIC_VIBRATION)
   {
      const XrHapticVibration *v = (const XrHapticVibration*)fb;
      fprintf(L.out, "{\"ev\":\"haptic\",\"hand\":\"%s\",\"amplitude\":%.4f,"
            "\"duration_ns\":%lld,\"t_us\":%lld}\n",
            hand_name(hand_of(info->subactionPath)), v->amplitude,
            (long long)v->duration, now_us());
      fflush(L.out);
   }
   pthread_mutex_unlock(&L.lock);
   return L.ApplyHapticFeedback(session, info, fb);
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_StopHapticFeedback(XrSession session,
      const XrHapticActionInfo *info)
{
   pthread_mutex_lock(&L.lock);
   if (L.out)
   {
      fprintf(L.out, "{\"ev\":\"haptic_stop\",\"hand\":\"%s\",\"t_us\":%lld}\n",
            hand_name(hand_of(info->subactionPath)), now_us());
      fflush(L.out);
   }
   pthread_mutex_unlock(&L.lock);
   return L.StopHapticFeedback(session, info);
}

static XRAPI_ATTR XrResult XRAPI_CALL layer_SuggestInteractionProfileBindings(
      XrInstance instance, const XrInteractionProfileSuggestedBinding *sb)
{
   char profile[XR_MAX_PATH_LENGTH];
   char path[XR_MAX_PATH_LENGTH];
   uint32_t i;
   uint32_t n = 0;
   XrResult res;
   if (     !L.PathToString
         || XR_FAILED(L.PathToString(instance, sb->interactionProfile,
               sizeof(profile), &n, profile)))
      snprintf(profile, sizeof(profile), "?");
   /* The runtime underneath doesn't know the Frame's profile. */
   if (L.frame && !strcmp(profile, FRAME_PROFILE))
      res = XR_SUCCESS;
   else
      res = L.SuggestInteractionProfileBindings(instance, sb);
   pthread_mutex_lock(&L.lock);
   if (L.out)
   {
      fprintf(L.out, "{\"ev\":\"bindings\",\"profile\":\"%s\",\"count\":%u,"
            "\"result\":%d,\"binds\":[", profile,
            (unsigned)sb->countSuggestedBindings, (int)res);
      for (i = 0; i < sb->countSuggestedBindings; i++)
      {
         const struct action_rec *a =
            find_action(sb->suggestedBindings[i].action);
         if (XR_FAILED(L.PathToString(instance,
                     sb->suggestedBindings[i].binding, sizeof(path), &n,
                     path)))
            snprintf(path, sizeof(path), "?");
         fprintf(L.out, "%s[\"%s\",\"%s\"]", i ? "," : "",
               a ? a->name : "?", path);
      }
      fprintf(L.out, "],\"t_us\":%lld}\n", now_us());
      fflush(L.out);
   }
   pthread_mutex_unlock(&L.lock);
   return res;
}

/* ---- Loader interface ---- */

static XRAPI_ATTR XrResult XRAPI_CALL layer_GetInstanceProcAddr(XrInstance instance,
      const char *name, PFN_xrVoidFunction *fn);

#define HOOK(n) if (!strcmp(name, "xr" #n)) \
   { *fn = (PFN_xrVoidFunction)layer_##n; return XR_SUCCESS; }

static XRAPI_ATTR XrResult XRAPI_CALL layer_GetInstanceProcAddr(XrInstance instance,
      const char *name, PFN_xrVoidFunction *fn)
{
   HOOK(GetInstanceProcAddr)
   HOOK(CreateSession)
   HOOK(DestroySession)
   HOOK(PollEvent)
   HOOK(CreateReferenceSpace)
   HOOK(LocateSpace)
   HOOK(CreateSwapchain)
   HOOK(DestroySwapchain)
   HOOK(EnumerateSwapchainImages)
   HOOK(AcquireSwapchainImage)
   HOOK(ReleaseSwapchainImage)
   HOOK(WaitFrame)
   HOOK(EndFrame)
   HOOK(CreateActionSet)
   HOOK(DestroyActionSet)
   HOOK(CreateAction)
   HOOK(CreateActionSpace)
   HOOK(DestroySpace)
   HOOK(SyncActions)
   HOOK(GetActionStateBoolean)
   HOOK(GetActionStateFloat)
   HOOK(GetActionStateVector2f)
   HOOK(GetActionStatePose)
   HOOK(ApplyHapticFeedback)
   HOOK(StopHapticFeedback)
   HOOK(SuggestInteractionProfileBindings)
   HOOK(EnumerateDisplayRefreshRatesFB)
   HOOK(RequestDisplayRefreshRateFB)
   if (!L.gipa)
      return XR_ERROR_FUNCTION_UNSUPPORTED;
   return L.gipa(instance, name, fn);
}

#undef HOOK

static XRAPI_ATTR XrResult XRAPI_CALL layer_CreateApiLayerInstance(
      const XrInstanceCreateInfo *info, const XrApiLayerCreateInfo *li,
      XrInstance *instance)
{
   XrResult res;
   XrApiLayerCreateInfo next;
   XrInstanceCreateInfo down = *info;
   const char *exts[64];
   uint32_t i;
   if (!li || !li->nextInfo || info->enabledExtensionCount > 64)
      return XR_ERROR_INITIALIZATION_FAILED;
   /* FRAME_EXT is this layer's to offer, never the runtime's. */
   L.frame                    = false;
   down.enabledExtensionCount = 0;
   down.enabledExtensionNames = exts;
   for (i = 0; i < info->enabledExtensionCount; i++)
   {
      if (!strcmp(info->enabledExtensionNames[i], FRAME_EXT))
         L.frame = true;
      else
         exts[down.enabledExtensionCount++] = info->enabledExtensionNames[i];
   }
   next          = *li;
   next.nextInfo = li->nextInfo->next;
   res = li->nextInfo->nextCreateApiLayerInstance(&down, &next, instance);
   if (XR_FAILED(res))
      return res;
   L.gipa = li->nextInfo->nextGetInstanceProcAddr;
#define NEXT(n) L.gipa(*instance, "xr" #n, (PFN_xrVoidFunction*)&L.n)
   NEXT(CreateSession);
   NEXT(DestroySession);
   NEXT(PollEvent);
   NEXT(CreateReferenceSpace);
   NEXT(LocateSpace);
   NEXT(CreateSwapchain);
   NEXT(DestroySwapchain);
   NEXT(EnumerateSwapchainImages);
   NEXT(AcquireSwapchainImage);
   NEXT(ReleaseSwapchainImage);
   NEXT(WaitFrame);
   NEXT(BeginFrame);
   NEXT(EndFrame);
   NEXT(PathToString);
   NEXT(CreateActionSet);
   NEXT(DestroyActionSet);
   NEXT(CreateAction);
   NEXT(CreateActionSpace);
   NEXT(DestroySpace);
   NEXT(SyncActions);
   NEXT(GetActionStateBoolean);
   NEXT(GetActionStateFloat);
   NEXT(GetActionStateVector2f);
   NEXT(GetActionStatePose);
   NEXT(ApplyHapticFeedback);
   NEXT(StopHapticFeedback);
   NEXT(SuggestInteractionProfileBindings);
   NEXT(EnumerateDisplayRefreshRatesFB);
   NEXT(RequestDisplayRefreshRateFB);
#undef NEXT
   L.instance = *instance;
   pthread_mutex_lock(&L.lock);
   layer_open();
   pthread_mutex_unlock(&L.lock);
   return res;
}

__attribute__((visibility("default")))
XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(
      const XrNegotiateLoaderInfo *li, const char *name,
      XrNegotiateApiLayerRequest *req)
{
   (void)name;
   if (     !li || !req
         || li->structType  != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO
         || req->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST
         || li->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION
         || li->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION)
      return XR_ERROR_INITIALIZATION_FAILED;
   req->layerInterfaceVersion  = XR_CURRENT_LOADER_API_LAYER_VERSION;
   req->layerApiVersion        = XR_CURRENT_API_VERSION;
   req->getInstanceProcAddr    = layer_GetInstanceProcAddr;
   req->createApiLayerInstance = layer_CreateApiLayerInstance;
   return XR_SUCCESS;
}
