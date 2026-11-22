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
#include <streams/file_stream.h>

#include "link_console.h"

#define LINK_CONSOLE_SYMBOLS 18

static struct
{
   link_sched_t *sched;
   link_bus_t *bus;
   link_console_t *consoles;
   const struct link_scenario *scn;
   const char *system_dir;
   unsigned long frame;
   bool keep;                       /* keep this frame's pictures */
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
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         return true;
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

/* The picture as opaque ARGB, for the PNG of a checkpoint */
static void link_console_keep(link_console_t *c, const unsigned char *row,
      unsigned width, unsigned height, size_t pitch)
{
   size_t size = (size_t)width * height;
   unsigned x, y;
   if (size > c->picture_size)
   {
      uint32_t *more = (uint32_t*)realloc(c->picture, size * sizeof(*more));
      if (!more)
         return;
      c->picture      = more;
      c->picture_size = size;
   }
   for (y = 0; y < height; y++, row += pitch)
      for (x = 0; x < width; x++)
      {
         uint32_t *out = &c->picture[(size_t)y * width + x];
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
   c->picture_width  = width;
   c->picture_height = height;
   c->drew           = true;
   c->pictured       = true;
}

/* Hashes the rows of the picture, without any padding past them. NULL is a
 * dupe: the picture stays the previous frame's. */
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
      c->pictured = c->pictured || c->drew_before;
      return;
   }
   if (data == RETRO_HW_FRAME_BUFFER_VALID)
      return;
   xxh64_reset(&h, 0);
   for (y = 0; y < height; y++, row += pitch)
      xxh64_update(&h, row, (size_t)width * c->bytes_per_pixel);
   c->video = xxh64_digest(&h);
   if (link_console_now.keep)
      link_console_keep(c, (const unsigned char*)data, width, height, pitch);
}

static void RETRO_CALLCONV link_console_sample(int16_t left, int16_t right)
{
   link_console_t *c = link_console_current();
   int16_t frame[2];
   if (!c)
      return;
   frame[0] = left;
   frame[1] = right;
   xxh64_update(&c->audio, frame, sizeof(frame));
}

static size_t RETRO_CALLCONV link_console_batch(const int16_t *data, size_t frames)
{
   link_console_t *c = link_console_current();
   if (c && data)
      xxh64_update(&c->audio, data, frames * 2 * sizeof(*data));
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
         mask = link_scenario_mask(&scn->inputs[i], link_console_now.frame);
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return (int16_t)mask;
   return (int16_t)(id < 16 ? (mask >> id) & 1 : 0);
}

void link_console_session(link_sched_t *sched, link_bus_t *bus, link_console_t *consoles,
      const struct link_scenario *scn, const char *system_dir)
{
   link_console_now.sched      = sched;
   link_console_now.bus        = bus;
   link_console_now.consoles   = consoles;
   link_console_now.scn        = scn;
   link_console_now.system_dir = system_dir;
   link_console_now.frame      = 0;
}

void link_console_frame(unsigned long frame, bool keep)
{
   size_t i;
   link_console_now.frame = frame;
   link_console_now.keep  = keep;
   for (i = 0; i < link_console_now.scn->console_count; i++)
   {
      link_console_t *c = &link_console_now.consoles[i];
      c->drew_before    = c->drew;
      c->ran            = false;
      c->drew           = false;
      c->pictured       = false;
   }
}

static bool link_console_symbols(link_console_t *c)
{
   static const char *const names[LINK_CONSOLE_SYMBOLS] = {
      "retro_set_environment", "retro_set_video_refresh", "retro_set_audio_sample",
      "retro_set_audio_sample_batch", "retro_set_input_poll", "retro_set_input_state",
      "retro_init", "retro_deinit", "retro_get_system_info", "retro_run",
      "retro_serialize_size", "retro_serialize", "retro_load_game", "retro_unload_game",
      "retro_get_memory_data", "retro_get_memory_size", "retro_unserialize", "retro_reset"
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
   if (ok && c->loaded && k->save)
      ok = link_console_fill_save(c, k->save);
   link_sched_leave(link_console_now.sched);
   return ok && c->loaded;
}

void link_console_run(void *user)
{
   link_console_t *c = (link_console_t*)user;
   c->ran = true;
   xxh64_reset(&c->audio, 0);
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
   /* No picture of the loaded state has been seen */
   c->video    = 0;
   c->ran      = false;
   c->drew     = false;
   c->pictured = false;
   return ok;
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

bool link_console_declared(const link_console_t *c, const char *key)
{
   size_t i;
   for (i = 0; i < c->variable_count; i++)
      if (!strcmp(c->variables[i].key, key))
         return true;
   return false;
}

void link_console_close(link_console_t *c)
{
   if (c->loaded)
   {
      link_sched_enter(link_console_now.sched, c->index);
      c->api.unload_game();
      link_sched_leave(link_console_now.sched);
      link_bus_console_gone(link_console_now.bus, c->index);
   }
   if (c->started)
   {
      link_sched_enter(link_console_now.sched, c->index);
      c->api.deinit();
      link_sched_leave(link_console_now.sched);
   }
   if (c->lib)
      dylib_close(c->lib);
   link_console_forget(c);
   free(c->rom_data);
   free(c->state);
   free(c->picture);
   free(c->dir);
   free(c->save_dir);
   free(c->core_copy);
   memset(c, 0, sizeof(*c));
}
