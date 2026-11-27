/* One console of the link harness: its own copy of a core, and the
 * callbacks every console shares, which find their console through the
 * scheduler */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define LINK_CONSOLE_GETCWD(buf, len) _getcwd((buf), (int)(len))
#else
#include <unistd.h>
#define LINK_CONSOLE_GETCWD(buf, len) getcwd((buf), (len))
#endif

#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <memory/memdesc.h>
#include <streams/file_stream.h>

#include "link_console.h"
#ifdef HAVE_EGL
#include "link_gl.h"
#endif
#ifdef HAVE_VULKAN
#include "link_vk.h"
#endif

#define LINK_CONSOLE_SYMBOLS 19
#define LINK_CONSOLE_GPU_LINE 256

static struct
{
   link_sched_t *sched;
   link_bus_t *bus;
   link_console_t *consoles;
   const struct link_scenario *scn;
   const char *system_dir;
   const char *require;             /* text a GPU console's renderer must name, or NULL */
   bool validate;                   /* Vulkan consoles have the validation layer */
   uint64_t keep_at, keep_rate;     /* the next checkpoint's instant; rate 0: not known */
   unsigned long keep_steps;        /* the steps to it; 0: none */
} link_console_now;

static link_console_t *link_console_current(void)
{
   int c = link_console_now.sched ? link_sched_console(link_console_now.sched) : -1;
   return c < 0 ? NULL : &link_console_now.consoles[c];
}

static void RETRO_CALLCONV link_console_log(enum retro_log_level level, const char *fmt, ...)
{
   link_console_t *c = link_console_current();
   va_list ap;
   if (level < RETRO_LOG_WARN)
      return;
   if (level == RETRO_LOG_ERROR && c)
      c->errors++;
   if (c)
      fprintf(stderr, "c%u: %s: ", c->index, level == RETRO_LOG_ERROR ? "error" : "warning");
   else
      fprintf(stderr, "core: %s: ", level == RETRO_LOG_ERROR ? "error" : "warning");
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

static bool RETRO_CALLCONV link_console_start_time(struct retro_calendar_time *t)
{
   if (!t)
      return false;
   t->sec        = link_console_now.scn->calendar_sec;
   t->nsec       = 0;
   t->utc_offset = link_console_now.scn->utc_offset;
   return true;
}

static bool RETRO_CALLCONV link_console_current_time(struct retro_calendar_time *t)
{
   (void)t;
   return false;
}

static bool RETRO_CALLCONV link_console_zone(struct retro_calendar_zone *zone)
{
   (void)zone;
   return false;
}

static bool link_console_calendar(struct retro_calendar_time_interface *calendar)
{
   if (!calendar || !calendar->interface_version)
      return false;
   if (calendar->interface_version > RETRO_CALENDAR_TIME_INTERFACE_VERSION)
      calendar->interface_version = RETRO_CALENDAR_TIME_INTERFACE_VERSION;
   calendar->get_start_time   = link_console_start_time;
   calendar->get_current_time = link_console_current_time;
   calendar->get_zone         = link_console_zone;
   return true;
}

static void link_console_forget(link_console_t *c)
{
   size_t i;
   for (i = 0; i < c->variable_count; i++)
   {
      free(c->variables[i].key);
      free(c->variables[i].fallback);
   }
   free(c->variables);
   c->variables      = NULL;
   c->variable_count = 0;
}

/* A new set replaces the old; each key's fallback is its first value */
static bool link_console_declare(link_console_t *c, const struct retro_variable *vars)
{
   size_t n = 0, i;
   if (!vars)
      return false;
   link_console_forget(c);
   while (vars[n].key)
      n++;
   if (!n)
      return true;
   if (!(c->variables = (struct link_console_variable*)calloc(n, sizeof(*c->variables))))
      return false;
   c->variable_count = n;
   for (i = 0; i < n; i++)
   {
      const char *values = vars[i].value ? strstr(vars[i].value, "; ") : NULL;
      size_t len         = 0;
      values             = values ? values + 2 : "";
      while (values[len] && values[len] != '|')
         len++;
      c->variables[i].key      = (char*)malloc(strlen(vars[i].key) + 1);
      c->variables[i].fallback = (char*)malloc(len + 1);
      if (!c->variables[i].key || !c->variables[i].fallback)
         return false;
      strcpy(c->variables[i].key, vars[i].key);
      memcpy(c->variables[i].fallback, values, len);
      c->variables[i].fallback[len] = '\0';
   }
   return true;
}

/* The scenario's value, else the first declared value */
static bool link_console_variable(link_console_t *c, struct retro_variable *var)
{
   const struct link_scenario *scn = link_console_now.scn;
   size_t i;
   if (!var || !var->key)
      return false;
   var->value = NULL;
   for (i = scn->option_count; i-- > 0;)
      if (scn->options[i].console == c->index && !strcmp(scn->options[i].key, var->key))
      {
         var->value = scn->options[i].value;
         return true;
      }
   for (i = 0; i < c->variable_count; i++)
      if (!strcmp(c->variables[i].key, var->key))
      {
         var->value = c->variables[i].fallback;
         return true;
      }
   return false;
}

/* A copy, as a core may pass its descriptors on its stack, completed as
 * RetroArch completes its own; a later call replaces it, and one with none
 * clears it */
static bool link_console_memory_maps(link_console_t *c, const struct retro_memory_map *map)
{
   free(c->descriptors);
   c->descriptors      = NULL;
   c->descriptor_count = 0;
   if (!map || !map->descriptors || !map->num_descriptors)
      return true;
   if (!(c->descriptors = (struct retro_memory_descriptor*)calloc(map->num_descriptors,
         sizeof(*c->descriptors))))
      return false;
   memcpy(c->descriptors, map->descriptors, map->num_descriptors * sizeof(*c->descriptors));
   memdesc_preprocess(c->descriptors, map->num_descriptors);
   c->descriptor_count = map->num_descriptors;
   return true;
}

static bool link_console_pixel_format(link_console_t *c, const enum retro_pixel_format *format)
{
   if (!format)
      return false;
   switch (*format)
   {
      case RETRO_PIXEL_FORMAT_0RGB1555:
      case RETRO_PIXEL_FORMAT_RGB565:
         c->bytes_per_pixel = 2;
         c->format          = *format;
         return true;
      case RETRO_PIXEL_FORMAT_XRGB8888:
         c->bytes_per_pixel = 4;
         c->format          = *format;
         return true;
      default:
         break;
   }
   return false;
}

#ifdef HAVE_EGL
static uintptr_t RETRO_CALLCONV link_console_framebuffer(void)
{
   link_console_t *c = link_console_current();
   return c && c->gl ? link_gl_framebuffer(c->gl) : 0;
}

/* What a console with render=gl is answered: SET_HW_RENDER makes a
 * context, which link_console_start makes current */
static bool link_console_gl_environment(link_console_t *c, unsigned cmd, void *data)
{
   struct retro_hw_render_callback *hw   = (struct retro_hw_render_callback*)data;
   const struct retro_system_av_info *av = (const struct retro_system_av_info*)data;
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_HW_RENDER:
         link_gl_free(c->gl);
         if (!(c->gl = link_gl_new(hw, c->index)))
            return false;
         hw->get_current_framebuffer = link_console_framebuffer;
         hw->get_proc_address        = link_gl_proc;
         c->hw                       = *hw;
         return true;
      case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
         /* false: no other API is on offer */
         if (data)
            *(unsigned*)data = RETRO_HW_CONTEXT_OPENGL_CORE;
         return false;
      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
         if (!av)
            return false;
         return !c->gl_started
            || link_gl_grow(c->gl, av->geometry.max_width, av->geometry.max_height);
      default:
         break;
   }
   return true;
}
#endif

#ifdef HAVE_VULKAN
/* What a console with render=vulkan is answered: SET_HW_RENDER opens the
 * Vulkan loader, and link_console_start makes the device */
static bool link_console_vk_environment(link_console_t *c, unsigned cmd, void *data)
{
   struct retro_hw_render_callback *hw   = (struct retro_hw_render_callback*)data;
   const struct retro_system_av_info *av = (const struct retro_system_av_info*)data;
   struct retro_hw_render_context_negotiation_interface *negotiation =
      (struct retro_hw_render_context_negotiation_interface*)data;
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT:
         if (!negotiation)
            return false;
         negotiation->interface_version = negotiation->interface_type
            == RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN ? link_vk_negotiation() : 0;
         return true;
      case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
         /* Asked when link_console_start makes the device, so one after
          * the core's load is refused as SET_HW_RENDER is */
         if (     !negotiation
               || negotiation->interface_type
                  != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN)
            return false;
         if (c->loaded)
         {
            fprintf(stderr, "link_harness: console %u:"
                  " SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE after its core's load is"
                  " refused\n", c->index);
            c->errors++;
            return false;
         }
         c->negotiation = negotiation;
         return true;
      case RETRO_ENVIRONMENT_SET_HW_RENDER:
         if (hw->context_type != RETRO_HW_CONTEXT_VULKAN)
         {
            fprintf(stderr, "link_harness: console %u: render=vulkan does not serve context"
                  " type %u\n", c->index, (unsigned)hw->context_type);
            return false;
         }
         if (!link_vk_loader(c->index))
            return false;
         c->hw = *hw;
         return true;
      case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
         /* false: no other API is on offer */
         if (data)
            *(unsigned*)data = RETRO_HW_CONTEXT_VULKAN;
         return false;
      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
         /* The largest frame read grows, as an OpenGL framebuffer does */
         if (!av)
            return false;
         if (c->vk_started)
         {
            if (av->geometry.max_width > c->vk_width)
               c->vk_width = av->geometry.max_width;
            if (av->geometry.max_height > c->vk_height)
               c->vk_height = av->geometry.max_height;
         }
         return true;
      default:
         break;
   }
   /* SET_GEOMETRY: the core's image is its own */
   return true;
}
#endif

#if defined(HAVE_EGL) || defined(HAVE_VULKAN)
/* What a console with render= is answered where one without is refused.
 * Its context or device is made at link_console_start: a SET_HW_RENDER
 * before then replaces the one before it, and one after is refused, as is
 * one from a console that is closing, whose context nothing would free. */
static bool link_console_hw_environment(link_console_t *c, unsigned cmd, void *data)
{
   enum link_scenario_render render = link_console_now.scn->consoles[c->index].render;
   if (render == LINK_SCENARIO_RENDER_NONE)
      return false;
   if (cmd == RETRO_ENVIRONMENT_SET_HW_RENDER)
   {
      if (!data || c->closing)
         return false;
      if (c->loaded)
      {
         fprintf(stderr, "link_harness: console %u: SET_HW_RENDER after its core's load"
               " is refused\n", c->index);
         c->errors++;
         return false;
      }
   }
   switch (render)
   {
#ifdef HAVE_EGL
      case LINK_SCENARIO_RENDER_GL:
         return link_console_gl_environment(c, cmd, data);
#endif
#ifdef HAVE_VULKAN
      case LINK_SCENARIO_RENDER_VULKAN:
         return link_console_vk_environment(c, cmd, data);
#endif
      default:
         break;
   }
   return false;
}
#endif

static bool RETRO_CALLCONV link_console_environment(unsigned cmd, void *data)
{
   link_console_t *c = link_console_current();
   if (!c)
      return false;
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_LINK_INTERFACE:
         return link_sched_environment(link_console_now.sched, data);
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         if (!data)
            return false;
         *(const char**)data = link_console_now.system_dir;
         return true;
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         if (!data)
            return false;
         *(const char**)data = c->save_dir;
         return true;
      case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
         if (!data)
            return false;
         *(unsigned*)data = 0;
         return true;
      case RETRO_ENVIRONMENT_SET_VARIABLES:
         return link_console_declare(c, (const struct retro_variable*)data);
      case RETRO_ENVIRONMENT_GET_VARIABLE:
         return link_console_variable(c, (struct retro_variable*)data);
      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
         if (!data)
            return false;
         *(bool*)data = false;
         return true;
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         return link_console_pixel_format(c, (const enum retro_pixel_format*)data);
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
         if (!data)
            return false;
         ((struct retro_log_callback*)data)->log = link_console_log;
         return true;
      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
         return true;
      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         if (!data)
            return false;
         *(bool*)data = true;
         return true;
      case RETRO_ENVIRONMENT_GET_CALENDAR_TIME_INTERFACE:
         return link_console_calendar((struct retro_calendar_time_interface*)data);
      case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
         return link_console_memory_maps(c, (const struct retro_memory_map*)data);
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         return true;
#if defined(HAVE_EGL) || defined(HAVE_VULKAN)
      case RETRO_ENVIRONMENT_SET_HW_RENDER:
      case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
      case RETRO_ENVIRONMENT_SET_GEOMETRY:
      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
         return link_console_hw_environment(c, cmd, data);
#endif
#ifdef HAVE_VULKAN
      case RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT:
      case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
         if (link_console_now.scn->consoles[c->index].render != LINK_SCENARIO_RENDER_VULKAN)
            return false;
         return link_console_vk_environment(c, cmd, data);
      case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
         /* From its context_reset to its context_destroy */
         if (!data || !c->vk_started)
            return false;
         *(const struct retro_hw_render_interface**)data =
            (const struct retro_hw_render_interface*)link_vk_interface(c->vk);
         return true;
#endif
      default:
         break;
   }
   return false;
}

/* A 5- or 6-bit channel widened to eight bits */
static uint32_t link_console_widen(unsigned v, unsigned bits)
{
   return (uint32_t)((v << (8 - bits)) | (v >> (2 * bits - 8)));
}

/* Room for size pixels */
static bool link_console_pixels(struct link_console_picture *p, size_t size)
{
   uint32_t *more;
   if (size <= p->size)
      return true;
   if (!(more = (uint32_t*)realloc(p->argb, size * sizeof(*more))))
      return false;
   p->argb = more;
   p->size = size;
   return true;
}

/* The picture as opaque ARGB, for the PNG of a checkpoint */
static void link_console_keep(link_console_t *c, const unsigned char *row,
      unsigned width, unsigned height, size_t pitch)
{
   struct link_console_picture *p = &c->drawn;
   unsigned x, y;
   if (!link_console_pixels(p, (size_t)width * height))
      return;
   for (y = 0; y < height; y++, row += pitch)
      for (x = 0; x < width; x++)
      {
         uint32_t *out = &p->argb[(size_t)y * width + x];
         unsigned px;
         if (c->format == RETRO_PIXEL_FORMAT_XRGB8888)
         {
            *out = 0xFF000000UL | (((const uint32_t*)(const void*)row)[x] & 0xFFFFFFUL);
            continue;
         }
         px = ((const uint16_t*)(const void*)row)[x];
         if (c->format == RETRO_PIXEL_FORMAT_RGB565)
            *out = 0xFF000000UL | (link_console_widen(px >> 11, 5) << 16)
               | (link_console_widen((px >> 5) & 63, 6) << 8) | link_console_widen(px & 31, 5);
         else
            *out = 0xFF000000UL | (link_console_widen((px >> 10) & 31, 5) << 16)
               | (link_console_widen((px >> 5) & 31, 5) << 8) | link_console_widen(px & 31, 5);
      }
   p->width  = width;
   p->height = height;
   c->drew   = true;
}

#if defined(HAVE_EGL) || defined(HAVE_VULKAN)
/* A frame drawn on the console's GPU, read back here, inside the call that
 * passes it: hashed as the XRGB8888 rows of a software picture, and made
 * opaque where it is kept. false: not read. */
static bool link_console_read(link_console_t *c, unsigned width, unsigned height)
{
   struct link_console_picture *p = &c->drawn;
   size_t size                    = (size_t)width * height;
   const char *bound              = "framebuffer";
   unsigned max_width             = 0;
   unsigned max_height            = 0;
   bool fits                      = true;
   bool read                      = false;
   size_t i;
#ifdef HAVE_EGL
   if (c->gl_started)
   {
      link_gl_size(c->gl, &max_width, &max_height);
      fits = size && width <= max_width && height <= max_height;
   }
#endif
#ifdef HAVE_VULKAN
   /* retro_vulkan_image has no extent to hold the frame to */
   if (c->vk_started)
   {
      max_width  = c->vk_width;
      max_height = c->vk_height;
      bound      = "max geometry";
      fits       = width <= max_width && height <= max_height;
   }
#endif
   if (!fits)
   {
      if (c->oversize)
         return false;
      fprintf(stderr, "link_harness: console %u: a frame of %ux%u is larger than its %s"
            " of %ux%u\n", c->index, width, height, bound, max_width, max_height);
      c->oversize = true;
      c->errors++;
      return false;
   }
   if (!link_console_pixels(p, size))
   {
      fprintf(stderr, "link_harness: console %u: out of memory in its frame %lu\n",
            c->index, c->frame);
      c->errors++;
      return false;
   }
#ifdef HAVE_EGL
   if (c->gl_started)
      read = link_gl_read(c->gl, p->argb, width, height);
#endif
#ifdef HAVE_VULKAN
   if (c->vk_started)
      read = link_vk_read(c->vk, p->argb, width, height);
#endif
   if (!read)
   {
      c->errors++;
      return false;
   }
   c->video = xxh64(p->argb, size * sizeof(*p->argb), 0);
   if (!c->keep)
      return true;
   for (i = 0; i < size; i++)
      p->argb[i] |= 0xFF000000UL;
   p->width  = width;
   p->height = height;
   c->drew   = true;
   return true;
}
#endif

/* Hashes the rows of the picture, without any padding past them. NULL is a
 * dupe: the picture stays the previous frame's. A frame drawn in a
 * framebuffer the console does not have is ignored. */
static void RETRO_CALLCONV link_console_video(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   link_console_t *c        = link_console_current();
   const unsigned char *row = (const unsigned char*)data;
   xxh64_state_t h;
   unsigned y;
   if (!c)
      return;
   if (!data)
   {
      c->duped = true;
#ifdef HAVE_VULKAN
      if (c->vk_started)
         link_vk_dupe(c->vk);
#endif
      return;
   }
   if (data == RETRO_HW_FRAME_BUFFER_VALID)
   {
#ifdef HAVE_EGL
      if (c->gl_started)
         link_console_read(c, width, height);
#endif
#ifdef HAVE_VULKAN
      /* What the core submitted for a frame not read is waited for as a
       * dupe's is */
      if (c->vk_started && !link_console_read(c, width, height))
         link_vk_dupe(c->vk);
#endif
      return;
   }
   xxh64_reset(&h, 0);
   for (y = 0; y < height; y++, row += pitch)
      xxh64_update(&h, row, (size_t)width * c->bytes_per_pixel);
   c->video = xxh64_digest(&h);
   if (c->keep)
      link_console_keep(c, (const unsigned char*)data, width, height, pitch);
}

/* Room for count samples in all */
static bool link_console_room(link_console_t *c, size_t count)
{
   int16_t *more;
   size_t cap = c->kept_cap ? c->kept_cap : 4096;
   if (count <= c->kept_cap)
      return true;
   while (cap < count)
      cap *= 2;
   if (!(more = (int16_t*)realloc(c->kept_audio, cap * sizeof(*more))))
   {
      fprintf(stderr, "link_harness: console %u: out of memory in its frame %lu\n",
            c->index, c->frame);
      c->errors++;
      return false;
   }
   c->kept_audio = more;
   c->kept_cap   = cap;
   return true;
}

/* The frame's audio, hashed and, while its core may be saved inside an
 * advance, kept for a save before the frame ends */
static void link_console_hear(link_console_t *c, const int16_t *data, size_t count)
{
   xxh64_update(&c->audio, data, count * sizeof(*data));
   if (!(link_bus_get_flags(link_console_now.bus, c->index) & RETRO_LINK_SAVE_IN_ADVANCE)
         || !link_console_room(c, c->kept_count + count))
      return;
   memcpy(c->kept_audio + c->kept_count, data, count * sizeof(*data));
   c->kept_count += count;
}

static void RETRO_CALLCONV link_console_sample(int16_t left, int16_t right)
{
   link_console_t *c = link_console_current();
   int16_t frame[2];
   if (!c)
      return;
   frame[0] = left;
   frame[1] = right;
   link_console_hear(c, frame, 2);
}

static size_t RETRO_CALLCONV link_console_batch(const int16_t *data, size_t frames)
{
   link_console_t *c = link_console_current();
   if (c && data)
      link_console_hear(c, data, frames * 2);
   return frames;
}

static void RETRO_CALLCONV link_console_poll(void)
{
}

static int16_t RETRO_CALLCONV link_console_input(unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   link_console_t *c               = link_console_current();
   const struct link_scenario *scn = link_console_now.scn;
   unsigned mask                   = 0;
   size_t i;
   if (!c || (device & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD || index)
      return 0;
   for (i = 0; i < scn->input_count; i++)
      if (scn->inputs[i].console == c->index && scn->inputs[i].port == port)
         mask = link_scenario_mask(&scn->inputs[i], c->frame);
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return (int16_t)mask;
   return (int16_t)(id < 16 ? (mask >> id) & 1 : 0);
}

void link_console_session(link_sched_t *sched, link_bus_t *bus, link_console_t *consoles,
      const struct link_scenario *scn, const char *system_dir, const char *require_renderer,
      bool vk_validate)
{
   link_console_now.sched      = sched;
   link_console_now.bus        = bus;
   link_console_now.consoles   = consoles;
   link_console_now.scn        = scn;
   link_console_now.system_dir = system_dir;
   link_console_now.require    = require_renderer;
   link_console_now.validate   = vk_validate;
   link_console_now.keep_steps = 0;
}

void link_console_step(unsigned long steps, uint64_t at, uint64_t rate)
{
   link_console_now.keep_steps = steps;
   link_console_now.keep_at    = at;
   link_console_now.keep_rate  = rate;
}

/* A checkpoint shows a console's last finished frame, or over a dupe the
 * one before, and the console may be inside the frame after: a frame keeps
 * its picture when the checkpoint comes by the console's edge three frames
 * on, counted in steps where an instant is not known */
static void link_console_frame(link_console_t *c)
{
   uint64_t since, span, rate;
   c->drew  = false;
   c->duped = false;
   if (!link_console_now.keep_steps)
      c->keep = false;
   else if (link_console_now.keep_rate
         && link_bus_console_edge(link_console_now.bus, c->index, &since, &span, &rate)
         && span && span <= (~(uint64_t)0 - since) / 3)
      c->keep = !link_bus_instant_before(since + 3 * span, rate,
            link_console_now.keep_at, link_console_now.keep_rate);
   else
      c->keep = link_console_now.keep_steps <= 3;
}

void link_console_done(void *user)
{
   link_console_t *c = (link_console_t*)user;
   struct link_console_finished *f;
   if (c->finished_count == c->finished_cap)
   {
      size_t cap = c->finished_cap ? c->finished_cap * 2 : 4;
      struct link_console_finished *more = (struct link_console_finished*)realloc(c->finished,
            cap * sizeof(*more));
      if (!more)
      {
         fprintf(stderr, "link_harness: console %u: out of memory in its frame %lu\n",
               c->index, c->frame);
         c->errors++;
         c->frame++;
         return;
      }
      c->finished     = more;
      c->finished_cap = cap;
   }
   f        = &c->finished[c->finished_count++];
   f->frame = c->frame++;
   f->video = c->video;
   f->audio = link_console_audio(c);
   f->state = 0;
   f->saved = link_console_state(c, &f->state);
   if (c->drew)
   {
      struct link_console_picture p = c->shown;
      c->shown = c->drawn;
      c->drawn = p;
   }
   c->pictured    = c->drew || (c->duped && c->drew_before);
   c->drew_before = c->drew;
   c->ran         = true;
}

static bool link_console_symbols(link_console_t *c)
{
   static const char *const names[LINK_CONSOLE_SYMBOLS] = {
      "retro_set_environment", "retro_set_video_refresh", "retro_set_audio_sample",
      "retro_set_audio_sample_batch", "retro_set_input_poll", "retro_set_input_state",
      "retro_init", "retro_deinit", "retro_get_system_info", "retro_run",
      "retro_serialize_size", "retro_serialize", "retro_load_game", "retro_unload_game",
      "retro_get_memory_data", "retro_get_memory_size", "retro_unserialize", "retro_reset",
      "retro_get_system_av_info"
   };
   function_t f[LINK_CONSOLE_SYMBOLS];
   unsigned i;
   for (i = 0; i < LINK_CONSOLE_SYMBOLS; i++)
      if (!(f[i] = dylib_proc(c->lib, names[i])))
      {
         fprintf(stderr, "link_harness: console %u: the core has no %s\n", c->index, names[i]);
         return false;
      }
   c->api.set_environment        = (void (*)(retro_environment_t))f[0];
   c->api.set_video_refresh      = (void (*)(retro_video_refresh_t))f[1];
   c->api.set_audio_sample       = (void (*)(retro_audio_sample_t))f[2];
   c->api.set_audio_sample_batch = (void (*)(retro_audio_sample_batch_t))f[3];
   c->api.set_input_poll         = (void (*)(retro_input_poll_t))f[4];
   c->api.set_input_state        = (void (*)(retro_input_state_t))f[5];
   c->api.init                   = (void (*)(void))f[6];
   c->api.deinit                 = (void (*)(void))f[7];
   c->api.get_system_info        = (void (*)(struct retro_system_info *))f[8];
   c->api.run                    = (void (*)(void))f[9];
   c->api.serialize_size         = (size_t (*)(void))f[10];
   c->api.serialize              = (bool (*)(void *, size_t))f[11];
   c->api.load_game              = (bool (*)(const struct retro_game_info *))f[12];
   c->api.unload_game            = (void (*)(void))f[13];
   c->api.get_memory_data        = (void *(*)(unsigned))f[14];
   c->api.get_memory_size        = (size_t (*)(unsigned))f[15];
   c->api.unserialize            = (bool (*)(const void *, size_t))f[16];
   c->api.reset                  = (void (*)(void))f[17];
   c->api.get_system_av_info     = (void (*)(struct retro_system_av_info *))f[18];
   return true;
}

bool link_console_open(link_console_t *c, unsigned index, const char *out)
{
   const struct link_scenario_console *k = &link_console_now.scn->consoles[index];
   char name[16];
   char *cwd, *path;
   const char *error;
   c->index           = index;
   c->bytes_per_pixel = 2;
   c->format          = RETRO_PIXEL_FORMAT_0RGB1555;
   sprintf(name, "c%u", index);
   if (     !(c->dir = link_scenario_join(out, name))
         || !(c->save_dir = link_scenario_join(c->dir, "save"))
         || !(c->core_copy = link_scenario_join(c->dir, path_basename(k->core))))
      return false;
   if (!path_mkdir(c->save_dir))
   {
      fprintf(stderr, "link_harness: cannot make %s\n", c->save_dir);
      return false;
   }
   if (filestream_copy(k->core, c->core_copy))
   {
      fprintf(stderr, "link_harness: cannot copy %s to %s\n", k->core, c->core_copy);
      return false;
   }
   /* An absolute path, so that Windows looks nowhere else for it */
   if (!(cwd = (char*)malloc(PATH_MAX_LENGTH)))
      return false;
   path = path_is_absolute(c->core_copy) || !LINK_CONSOLE_GETCWD(cwd, PATH_MAX_LENGTH)
      ? NULL : link_scenario_join(cwd, c->core_copy);
   c->lib = dylib_load(path ? path : c->core_copy);
   free(path);
   free(cwd);
   if (!c->lib)
   {
      error = dylib_error();
      fprintf(stderr, "link_harness: cannot load %s: %s\n", c->core_copy, error ? error : "");
      return false;
   }
   return link_console_symbols(c);
}

/* Save RAM from a copy of the save in the console's save folder */
static bool link_console_fill_save(link_console_t *c, const char *save)
{
   char *copy = link_scenario_join(c->save_dir, path_basename(save));
   void *buf  = NULL;
   int64_t len = 0;
   bool ok     = copy && !filestream_copy(save, copy) && filestream_read_file(copy, &buf, &len);
   if (ok)
   {
      unsigned char *ram = (unsigned char*)c->api.get_memory_data(RETRO_MEMORY_SAVE_RAM);
      size_t size        = c->api.get_memory_size(RETRO_MEMORY_SAVE_RAM);
      if (ram && size)
         memcpy(ram, buf, (size_t)len < size ? (size_t)len : size);
   }
   else
      fprintf(stderr, "link_harness: console %u: cannot copy %s\n", c->index, save);
   free(buf);
   free(copy);
   return ok;
}

#if defined(HAVE_EGL) || defined(HAVE_VULKAN)
/* The console's driver, in out/c<n>/gpu.txt */
static bool link_console_gpu(link_console_t *c, const char *line)
{
   char *path = link_scenario_join(c->dir, "gpu.txt");
   bool ok    = path && filestream_write_file(path, line, (int64_t)strlen(line));
   free(path);
   if (!ok)
      fprintf(stderr, "link_harness: console %u: cannot write gpu.txt in %s\n", c->index,
            c->dir);
   return ok;
}
#endif

#ifdef HAVE_EGL
/* After retro_load_game on a console that asked for a context: the context
 * current with a framebuffer of the core's largest picture, on a renderer
 * the run allows, then context_reset */
static bool link_console_start_gl(link_console_t *c)
{
   const char *require = link_console_now.require;
   struct retro_system_av_info av;
   char line[LINK_CONSOLE_GPU_LINE];
   memset(&av, 0, sizeof(av));
   c->api.get_system_av_info(&av);
   if (!link_gl_start(c->gl, av.geometry.max_width, av.geometry.max_height))
      return false;
   if (require && !strstr(link_gl_renderer(c->gl), require))
   {
      fprintf(stderr, "link_harness: console %u: renderer \"%s\" is not %s\n", c->index,
            link_gl_renderer(c->gl), require);
      return false;
   }
   link_gl_describe(c->gl, line, sizeof(line));
   if (!link_console_gpu(c, line))
      return false;
   c->gl_started = true;
   if (c->hw.context_reset)
      c->hw.context_reset();
   return true;
}
#endif

#ifdef HAVE_VULKAN
/* After retro_load_game on a console that asked for Vulkan: an instance
 * and a device of its own, on a renderer the run allows, and no frame read
 * larger than the core's largest picture, then context_reset */
static bool link_console_start_vk(link_console_t *c)
{
   struct retro_system_av_info av;
   char line[LINK_CONSOLE_GPU_LINE];
   memset(&av, 0, sizeof(av));
   c->api.get_system_av_info(&av);
   c->vk_width  = av.geometry.max_width;
   c->vk_height = av.geometry.max_height;
   if (!(c->vk = link_vk_new(c->negotiation, c->index, link_console_now.require,
         link_console_now.validate, &c->errors)))
      return false;
   link_vk_describe(c->vk, line, sizeof(line));
   if (!link_console_gpu(c, line))
      return false;
   c->vk_started = true;
   if (c->hw.context_reset)
      c->hw.context_reset();
   return true;
}
#endif

bool link_console_start(link_console_t *c)
{
   const struct link_scenario_console *k = &link_console_now.scn->consoles[c->index];
   struct retro_system_info info;
   struct retro_game_info game;
   bool ok = true;

   link_sched_enter(link_console_now.sched, c->index);
   c->api.set_environment(link_console_environment);
   c->api.set_video_refresh(link_console_video);
   c->api.set_audio_sample(link_console_sample);
   c->api.set_audio_sample_batch(link_console_batch);
   c->api.set_input_poll(link_console_poll);
   c->api.set_input_state(link_console_input);
   c->api.init();
   c->started = true;

   memset(&info, 0, sizeof(info));
   memset(&game, 0, sizeof(game));
   c->api.get_system_info(&info);
   if (k->rom)
   {
      int64_t len = 0;
      game.path   = k->rom;
      if (!info.need_fullpath)
      {
         if (!filestream_read_file(k->rom, &c->rom_data, &len))
            ok = false;
         game.data = c->rom_data;
         game.size = (size_t)len;
      }
   }
   if (ok && !(c->loaded = c->api.load_game(k->rom ? &game : NULL)))
      fprintf(stderr, "link_harness: console %u: retro_load_game failed\n", c->index);
#ifdef HAVE_EGL
   if (ok && c->loaded && c->gl)
      ok = link_console_start_gl(c);
#endif
#ifdef HAVE_VULKAN
   if (ok && c->loaded && c->hw.context_type == RETRO_HW_CONTEXT_VULKAN)
      ok = link_console_start_vk(c);
#endif
   if (ok && c->loaded && k->save)
      ok = link_console_fill_save(c, k->save);
   link_sched_leave(link_console_now.sched);
   return ok && c->loaded;
}

/* EGL keeps one current context for a thread, whichever cothread's stack
 * is in use, and every console runs on the scheduler's thread: so this may
 * be called on any of them, the main one at a switch into a console and
 * the caller's at link_sched_enter. A console with no context of its own
 * leaves the current one where it is. */
void link_console_enter(void *user)
{
#ifdef HAVE_EGL
   link_console_t *c = (link_console_t*)user;
   if (c->gl_started && !link_gl_enter(c->gl))
      c->errors++;
#else
   (void)user;
#endif
}

void link_console_run(void *user)
{
   link_console_t *c = (link_console_t*)user;
   link_console_frame(c);
   if (c->resuming)
      c->resuming = false;
   else
   {
      xxh64_reset(&c->audio, 0);
      c->kept_count = 0;
   }
   c->api.run();
}

uint64_t link_console_audio(link_console_t *c)
{
   return xxh64_digest(&c->audio);
}

size_t link_console_state_size(link_console_t *c)
{
   size_t size;
   link_sched_enter(link_console_now.sched, c->index);
   size = c->api.serialize_size();
   link_sched_leave(link_console_now.sched);
   return size;
}

bool link_console_serialize(link_console_t *c, void *data, size_t size)
{
   bool ok;
   link_sched_enter(link_console_now.sched, c->index);
   ok = c->api.serialize(data, size);
   link_sched_leave(link_console_now.sched);
   return ok;
}

bool link_console_state(link_console_t *c, uint64_t *hash)
{
   size_t size = link_console_state_size(c);
   if (size > c->state_size)
   {
      unsigned char *more = (unsigned char*)realloc(c->state, size);
      if (!more)
         return false;
      c->state      = more;
      c->state_size = size;
   }
   if (!link_console_serialize(c, c->state, size))
      return false;
   *hash = xxh64(c->state, size, 0);
   return true;
}

bool link_console_unserialize(link_console_t *c, const void *data, size_t len)
{
   bool ok;
   link_sched_enter(link_console_now.sched, c->index);
   ok = c->api.unserialize(data, len);
   link_sched_leave(link_console_now.sched);
   /* No picture of the loaded state has been seen, nor any of its audio */
   c->video        = 0;
   c->ran          = false;
   c->drew         = false;
   c->drew_before  = false;
   c->pictured     = false;
   c->resuming     = false;
   c->kept_count   = 0;
   xxh64_reset(&c->audio, 0);
   return ok;
}

bool link_console_resume(link_console_t *c, const unsigned char *audio, size_t count)
{
   size_t i;
   if (!link_console_room(c, count))
      return false;
   for (i = 0; i < count; i++)
      c->kept_audio[i] = (int16_t)(uint16_t)(audio[2 * i] | (audio[2 * i + 1] << 8));
   c->kept_count = count;
   if (count)
      xxh64_update(&c->audio, c->kept_audio, count * sizeof(*c->kept_audio));
   c->resuming = true;
   return true;
}

void link_console_held(link_console_t *c)
{
   c->ran      = false;
   c->pictured = false;
}

void link_console_reset(link_console_t *c)
{
   link_sched_enter(link_console_now.sched, c->index);
   c->api.reset();
   link_sched_leave(link_console_now.sched);
}

bool link_console_byte(link_console_t *c, unsigned memory, unsigned long offset,
      unsigned *value)
{
   const unsigned char *data;
   size_t size;
   link_sched_enter(link_console_now.sched, c->index);
   data = (const unsigned char*)c->api.get_memory_data(memory);
   size = c->api.get_memory_size(memory);
   link_sched_leave(link_console_now.sched);
   if (!data || offset >= size)
      return false;
   *value = data[offset];
   return true;
}

bool link_console_address(const link_console_t *c, size_t address, unsigned *value)
{
   size_t o;
   const struct retro_memory_descriptor *d = memdesc_find(c->descriptors,
         c->descriptor_count, address, &o);
   if (!d || !d->ptr)
      return false;
   *value = ((const unsigned char*)d->ptr)[d->offset + o];
   return true;
}

bool link_console_declared(const link_console_t *c, const char *key)
{
   size_t i;
   for (i = 0; i < c->variable_count; i++)
      if (!strcmp(c->variables[i].key, key))
         return true;
   return false;
}

unsigned link_console_close(link_console_t *c)
{
   unsigned errors;
   if (c->loaded)
   {
      link_sched_enter(link_console_now.sched, c->index);
#ifdef HAVE_VULKAN
      /* What it handed over is finished with, as wait_sync_index has it */
      if (c->vk_started)
         link_vk_wait(c->vk);
#endif
      if ((c->gl_started || c->vk_started) && c->hw.context_destroy)
         c->hw.context_destroy();
      c->api.unload_game();
      link_sched_leave(link_console_now.sched);
      link_bus_console_gone(link_console_now.bus, c->index);
   }
#ifdef HAVE_EGL
   link_gl_free(c->gl);
#endif
#ifdef HAVE_VULKAN
   link_vk_free(c->vk);
#endif
#if defined(HAVE_EGL) || defined(HAVE_VULKAN)
   /* retro_deinit may still ask for the framebuffer, or for a context */
   c->gl         = NULL;
   c->vk         = NULL;
   c->gl_started = false;
   c->vk_started = false;
   c->closing    = true;
#endif
   if (c->started)
   {
      link_sched_enter(link_console_now.sched, c->index);
      c->api.deinit();
      link_sched_leave(link_console_now.sched);
   }
   if (c->lib)
      dylib_close(c->lib);
   errors = c->errors;
   link_console_forget(c);
   free(c->descriptors);
   free(c->rom_data);
   free(c->state);
   free(c->kept_audio);
   free(c->finished);
   free(c->shown.argb);
   free(c->drawn.argb);
   free(c->dir);
   free(c->save_dir);
   free(c->core_copy);
   memset(c, 0, sizeof(*c));
   return errors;
}
