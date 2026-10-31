/* Public-libretro snapshot consumer. The generated PS-X reset stub enters
 * Beetle's EXE loader; other cores may require their own firmware. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "libretro.h"
struct snapshot_core_api
{
   unsigned (*retro_api_version)(void);
   void (*retro_get_system_info)(struct retro_system_info *);
   void (*retro_get_system_av_info)(struct retro_system_av_info *);
   unsigned (*retro_get_region)(void);
   void (*retro_set_controller_port_device)(unsigned,unsigned);
   void (*retro_cheat_reset)(void);
   void (*retro_cheat_set)(unsigned,bool,const char *);
   void (*retro_set_environment)(retro_environment_t);
   void (*retro_set_video_refresh)(retro_video_refresh_t);
   void (*retro_set_audio_sample)(retro_audio_sample_t);
   void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
   void (*retro_set_input_poll)(retro_input_poll_t);
   void (*retro_set_input_state)(retro_input_state_t);
   void (*retro_init)(void);
   void (*retro_deinit)(void);
   bool (*retro_load_game)(const struct retro_game_info *);
   void (*retro_unload_game)(void);
   void (*retro_run)(void);
   void (*retro_reset)(void);
   size_t (*retro_serialize_size)(void);
   bool (*retro_serialize)(void *, size_t);
   bool (*retro_unserialize)(const void *, size_t);
   void *(*retro_get_memory_data)(unsigned);
   size_t (*retro_get_memory_size)(unsigned);
};

static bool snapshot_core_close(void *library)
{
#ifdef _WIN32
   return FreeLibrary((HMODULE)library) != 0;
#else
   return dlclose(library) == 0;
#endif
}

static void *snapshot_core_open(struct snapshot_core_api *api, const char *path)
{
   void *library;
#ifdef _WIN32
   library = (void *)LoadLibraryA(path);
#define SNAPSHOT_SYMBOL(name) ((void *)GetProcAddress((HMODULE)library, name))
#else
   library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#define SNAPSHOT_SYMBOL(name) dlsym(library, name)
#endif
   if (!library) return NULL;
   memset(api,0,sizeof(*api));
#define SNAPSHOT_LOAD(name) do { void *symbol = SNAPSHOT_SYMBOL(#name); \
   if (!symbol || sizeof(api->name) != sizeof(symbol)) { snapshot_core_close(library); return NULL; } \
   memcpy(&api->name,&symbol,sizeof(api->name)); } while (0)
   SNAPSHOT_LOAD(retro_api_version); SNAPSHOT_LOAD(retro_get_system_info);
   SNAPSHOT_LOAD(retro_get_system_av_info); SNAPSHOT_LOAD(retro_get_region);
   SNAPSHOT_LOAD(retro_set_controller_port_device); SNAPSHOT_LOAD(retro_cheat_reset); SNAPSHOT_LOAD(retro_cheat_set);
   SNAPSHOT_LOAD(retro_set_environment);
   SNAPSHOT_LOAD(retro_set_video_refresh); SNAPSHOT_LOAD(retro_set_audio_sample);
   SNAPSHOT_LOAD(retro_set_audio_sample_batch); SNAPSHOT_LOAD(retro_set_input_poll);
   SNAPSHOT_LOAD(retro_set_input_state);
   SNAPSHOT_LOAD(retro_init); SNAPSHOT_LOAD(retro_deinit); SNAPSHOT_LOAD(retro_load_game);
   SNAPSHOT_LOAD(retro_unload_game); SNAPSHOT_LOAD(retro_run); SNAPSHOT_LOAD(retro_reset);
   SNAPSHOT_LOAD(retro_serialize_size);
   SNAPSHOT_LOAD(retro_serialize); SNAPSHOT_LOAD(retro_unserialize);
   SNAPSHOT_LOAD(retro_get_memory_data); SNAPSHOT_LOAD(retro_get_memory_size);
#undef SNAPSHOT_LOAD
#undef SNAPSHOT_SYMBOL
   return library;
}

static uint32_t snapshot_read32(const uint8_t *p)
{
   return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* A guest publishes one of two complete records by updating its counter last.
 * GPU/RSP reads precede writes, so a later redraw cannot repair a missing load. */
static int snapshot_fixture_check(const void *memory, size_t bytes, bool rsp)
{
   const uint8_t *ram = (const uint8_t *)memory;
   uint32_t counter, expected, actual;
   unsigned i;
   bool valid = true;
   if (!ram) return 0;
   if (bytes < 0x100080) return -1;
   counter = snapshot_read32(ram + 0x80000);
   if (!counter) return 0;
   expected = 0x4000 | (counter < 2 ? 0 : (counter - 2) & 31);
   expected |= expected << 16;
   actual = snapshot_read32(ram + 0x100040 + (counter & 1) * 16);
   if (actual != expected)
   {
      fprintf(stderr,"retained VRAM at guest frame %u: read %08x, expected %08x\n",counter,actual,expected);
      return -1;
   }
   if (rsp)
      for (i = 1; i < 4; i++)
      {
         actual = snapshot_read32(ram + 0x100040 + (counter & 1) * 16 + i * 4);
         expected = 0x1000 | (counter < 2 ? 0 : (counter - 2) & 31);
         if (actual != expected)
         {
            fprintf(stderr,"retained RSP component %u at guest frame %u: read %08x, expected %08x\n",i,counter,actual,expected);
            valid = false;
         }
      }
   return valid ? 1 : -1;
}


#define REQUIRE(condition) do { if (!(condition)) die(#condition, __LINE__); } while (0)

static struct snapshot_core_api core;

static struct retro_snapshot_interface snapshots;
static enum retro_snapshot_execution_context phase = RETRO_SNAPSHOT_ORDINARY;
static char directory[4096];
static const char *scale = "1x(native)";
static bool variables_changed;
static unsigned pixel_format, input_frame, option_reads, video_calls;
static unsigned creates, releases, saves, loads, speculative_runs;
static bool conformance, check_rsp, check_presented;
static bool ordinary_execution, decline_snapshots;
static bool check_ordinary_load;
static bool fixed_scale;
static unsigned warmup_frames = 20, retained_steps = 32;
static const char *system_directory;
static struct retro_variable options[128];
static unsigned option_hits[128];
static unsigned option_count;

typedef struct image
{
   uint8_t *pixels;
   size_t size;
   unsigned width, height;
   const void *borrowed;
   size_t pitch;
} image_t;
static image_t video;

static void die(const char *condition, unsigned line)
{
   fprintf(stderr, "snapshot_smoke:%u: %s\n", line, condition);
   fflush(NULL);
   _Exit(EXIT_FAILURE);
}

static void load_profile(const char *path)
{
   FILE *file = fopen(path, "r");
   char line[512];
   REQUIRE(file != NULL);
   while (fgets(line, sizeof(line), file))
   {
      char *equal, *key, *value;
      size_t length;
      if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
      length = strcspn(line, "\r\n");
      REQUIRE(length < sizeof(line) - 1); line[length] = 0;
      equal = strchr(line, '='); REQUIRE(equal && equal != line);
      *equal++ = 0;
      if (!strcmp(line, "@frontend"))
      {
         REQUIRE(!strcmp(equal,"ordinary") || !strcmp(equal,"decline"));
         ordinary_execution = true;
         decline_snapshots = !strcmp(equal,"decline");
         continue;
      }
      if (!strcmp(line, "@rsp"))
      {
         REQUIRE(!strcmp(equal,"0") || !strcmp(equal,"1"));
         check_rsp = equal[0] == '1';
         continue;
      }
      if (!strcmp(line, "@ordinary_load"))
      {
         REQUIRE(!strcmp(equal,"0") || !strcmp(equal,"1"));
         check_ordinary_load = equal[0] == '1';
         continue;
      }
      if (!strcmp(line, "@presented"))
      {
         REQUIRE(!strcmp(equal,"0") || !strcmp(equal,"1"));
         check_presented = equal[0] == '1';
         continue;
      }
      REQUIRE(line[0] != '@' && option_count < sizeof(options) / sizeof(options[0]));
      {
         unsigned i;
         for (i = 0; i < option_count; i++) REQUIRE(strcmp(options[i].key,line));
      }
      key = malloc(strlen(line) + 1); value = malloc(strlen(equal) + 1);
      REQUIRE(key && value); strcpy(key,line); strcpy(value,equal);
      options[option_count].key = key; options[option_count++].value = value;
   }
   REQUIRE(!ferror(file) && fclose(file) == 0);
}

static uint32_t read32(const uint8_t *p)
{
   return snapshot_read32(p);
}

static void write32(uint8_t *p, uint32_t value)
{
   unsigned i;
   for (i = 0; i < 4; i++) p[i] = (uint8_t)(value >> (8 * i));
}

static void file_path(char *path, size_t size, const char *name)
{
   int length = snprintf(path, size, "%s/%s", directory, name);
   REQUIRE(length > 0 && (size_t)length < size);
}

/* Never overwrite unrelated content, including a user's real BIOS. */
static void fixture_file(const char *name, const uint8_t *bytes, size_t size)
{
   static char path[4096];
   FILE *file;
   size_t i;
   file_path(path, sizeof(path), name);
   file = fopen(path, "rb");
   if (file)
   {
      for (i = 0; i < size; i++) REQUIRE(fgetc(file) == bytes[i]);
      REQUIRE(fgetc(file) == EOF);
      REQUIRE(fclose(file) == 0);
      return;
   }
   REQUIRE(errno == ENOENT);
   file = fopen(path, "wb");
   REQUIRE(file != NULL);
   REQUIRE(fwrite(bytes, 1, size, file) == size);
   REQUIRE(fclose(file) == 0);
}

/* Small MIPS emitter: registers t0/t1/t7=8/9/15, s1/s2/s3/s5=17/18/19/21. */
static uint32_t program[1024];
static unsigned instructions;
static void emit(uint32_t instruction)
{
   REQUIRE(instructions < sizeof(program) / sizeof(program[0]));
   program[instructions++] = instruction;
}
static void immediate(unsigned reg, uint32_t value)
{
   emit(0x3c000000u | reg << 16 | value >> 16); /* lui */
   emit(0x34000000u | reg << 21 | reg << 16 | (value & 0xffff)); /* ori */
}
static void branch(unsigned reg, unsigned destination)
{
   int displacement = (int)destination - (int)instructions - 1;
   REQUIRE(displacement >= -32768 && displacement <= 32767);
   emit(0x10000000u | reg << 21 | ((uint32_t)displacement & 0xffff)); /* beq r,zero */
   emit(0);
}
static void wait_gpu(unsigned bit)
{
   unsigned loop;
   immediate(15, 1u << bit);
   loop = instructions;
   emit(0x8e280004); emit(0); /* lw t0,4(s1); load delay */
   emit(0x010f4024);         /* and t0,t0,t7 */
   branch(8, loop);
}
static void gpu_word(uint32_t value, unsigned offset)
{
   immediate(8, value);
   emit(0xae280000u | offset); /* sw t0,offset(s1) */
}

static void make_fixture(void)
{
   uint8_t *bios = (uint8_t *)calloc(1, 512 * 1024);
   uint8_t *exe;
   size_t text_size, i;
   unsigned frame, wait;
   REQUIRE(bios != NULL);
   write32(bios, 0x3c08bf00); write32(bios + 4, 0x35081000);
   write32(bios + 8, 0x01000008); /* jump to the core's expansion loader */
   fixture_file("scph5501.bin", bios, 512 * 1024);
   free(bios);

   immediate(17, 0x1f801810); /* GPU ports */
   immediate(8, 0x1f801104); immediate(9, 0x100);
   emit(0xad090000); /* counter0 dot clock keeps the idle scheduler active */
   gpu_word(0, 4); gpu_word(0x03000000, 4); gpu_word(0x08000001, 4);
   gpu_word(0x06000000u | (0x260 + 320 * 8) << 12 | 0x260, 4);
   gpu_word(0x07000000u | (0x10 + 240) << 10 | 0x10, 4);
   gpu_word(0xe3000000, 0); gpu_word(0xe407ffff, 0);
   gpu_word(0xe5000000, 0); gpu_word(0xe6000000, 0);
   wait_gpu(26);
   gpu_word(0x02080808, 0); gpu_word(0, 0); gpu_word(240u << 16 | 320, 0);
   wait_gpu(26);
   gpu_word(0x300000ff, 0); gpu_word(12u << 16 | 3, 0);
   gpu_word(0x0000ff00, 0); gpu_word(40u << 16 | 300, 0);
   gpu_word(0x00ff0000, 0); gpu_word(220u << 16 | 50, 0);
   wait_gpu(26);
   gpu_word(0xa0000000, 0); gpu_word(300u << 16 | 400, 0);
   gpu_word(2u << 16 | 8, 0);
   for (i = 0; i < 8; i++) { wait_gpu(28); gpu_word(0x001f801f, 0); }
   gpu_word(0xe6000002, 0); /* masked red pixels survive a green rectangle */
   wait_gpu(26);
   gpu_word(0x6000ff00, 0); gpu_word(300u << 16 | 400, 0);
   gpu_word(2u << 16 | 8, 0);
   wait_gpu(26);
   gpu_word(0xa0000000, 0); gpu_word(304u << 16 | 420, 0);
   gpu_word(1u << 16 | 2, 0);
   wait_gpu(28); gpu_word(0x40004000, 0);
   immediate(18, 0x1f801070); immediate(21, 0);
   frame = instructions;
   wait = instructions;
   emit(0x8e480000); emit(0); emit(0x31080001); /* lw/nop/andi I_STAT */
   branch(8, wait);
   immediate(8, 0xfffffffe); emit(0xae480000); /* acknowledge vblank */
   /* Read the previous frame's retained pixels before replacing them. The
    * inactive result slot is published by the frame counter at loop end. */
   wait_gpu(26);
   gpu_word(0xc0000000, 0); gpu_word(304u << 16 | 420, 0);
   gpu_word(1u << 16 | 2, 0);
   wait_gpu(27);
   emit(0x8e290000); emit(0); /* lw t1,0(s1); load delay */
   immediate(19, 0xa0100040);
   emit(0x26aa0001); emit(0x314a0001); emit(0x000a5100); /* (s5+1)&1, <<4 */
   emit(0x026a9821); emit(0xae690000); /* addu s3,s3,t2; sw t1,0(s3) */
   wait_gpu(26);
   gpu_word(0xa0000000, 0); gpu_word(304u << 16 | 420, 0);
   gpu_word(1u << 16 | 2, 0);
   wait_gpu(28);
   emit(0x32a8001f); emit(0x35084000); /* raw RGB555 value from s5 */
   emit(0x00084c00); emit(0x01094025); emit(0xae280000); /* pack two pixels */
   wait_gpu(26);
   immediate(9, 0x60000800); emit(0x32a800ff); /* frame-dependent corner */
   emit(0x01094025); emit(0xae280000);
   gpu_word(0, 0); gpu_word(16u << 16 | 16, 0);
   wait_gpu(26);
   /* Blend into retained subpixel edges instead of repainting the whole
    * display. Missing enhanced-pixel restoration must remain observable. */
   immediate(9, 0x22002000); emit(0x32a8001f); emit(0x000840c0);
   emit(0x01094025); emit(0xae280000);
   gpu_word(90u << 16 | 80, 0); gpu_word(125u << 16 | 200, 0);
   gpu_word(180u << 16 | 95, 0);
   wait_gpu(26);
   gpu_word(0xc0000000, 0); gpu_word(300u << 16 | 400, 0);
   gpu_word(2u << 16 | 8, 0);
   immediate(19, 0xa0100000);
   for (i = 0; i < 8; i++)
   {
      wait_gpu(27);
      emit(0x8e290000); emit(0); emit(0xae690000); emit(0x26730004);
   }
   emit(0x26b50001); immediate(19, 0xa0080000); emit(0xae750000);
   emit(0x08000000u | ((0xa0010000u + frame * 4) >> 2 & 0x03ffffff));
   emit(0);
   text_size = (instructions * 4 + 2047) & ~(size_t)2047;
   exe = (uint8_t *)calloc(1, 2048 + text_size);
   REQUIRE(exe != NULL);
   memcpy(exe, "PS-X EXE", 8);
   write32(exe + 0x10, 0xa0010000); write32(exe + 0x18, 0x80010000);
   write32(exe + 0x1c, (uint32_t)text_size); write32(exe + 0x30, 0xa01ffff0);
   for (i = 0; i < instructions; i++) write32(exe + 2048 + 4 * i, program[i]);
   fixture_file("snapshot-history.exe", exe, 2048 + text_size);
   free(exe);
}

static enum retro_snapshot_execution_context RETRO_CALLCONV execution_context(void)
{
   return phase;
}
static void RETRO_CALLCONV core_log(enum retro_log_level level, const char *format, ...)
{
   va_list ap;
   if (level < RETRO_LOG_WARN) return;
   va_start(ap, format); vfprintf(stderr, format, ap); va_end(ap);
}
static bool RETRO_CALLCONV environment(unsigned command, void *data)
{
   switch (command)
   {
      case RETRO_ENVIRONMENT_SET_SNAPSHOT_INTERFACE:
      {
         struct retro_snapshot_interface *iface = data;
         if (decline_snapshots) return false;
         REQUIRE(iface && iface->version == RETRO_SNAPSHOT_INTERFACE_VERSION);
         REQUIRE(iface->size >= sizeof(*iface) && !snapshots.version);
         REQUIRE(iface->create && iface->save && iface->load && iface->release);
         REQUIRE(iface->supported && iface->valid);
         snapshots = *iface;
         iface->get_execution_context = execution_context;
         return true;
      }
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         *(const char **)data = system_directory ? system_directory : directory; return true;
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char **)data = directory; return true;
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
         ((struct retro_log_callback *)data)->log = core_log; return true;
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         pixel_format = *(unsigned *)data;
         return pixel_format <= RETRO_PIXEL_FORMAT_RGB565;
      case RETRO_ENVIRONMENT_GET_VARIABLE:
      {
         struct retro_variable *var = data;
         const char *key = var->key;
         var->value = NULL;
         if (conformance)
         {
            unsigned i;
            for (i = 0; i < option_count; i++)
               if (!strcmp(key,options[i].key))
               {
                  var->value = options[i].value;
                  option_reads++; option_hits[i]++;
                  break;
               }
            return var->value != NULL;
         }
         if (strstr(key, "internal_resolution")) { var->value = scale; option_reads++; }
         else if (strstr(key, "renderer_software_fb")) var->value = "enabled";
         else if (strstr(key, "renderer")) var->value = "software";
         else if (strstr(key, "cpu_dynarec") || strstr(key, "pgxp") ||
                  strstr(key, "frame_duping") || strstr(key, "skip_bios") ||
                  strstr(key, "override_bios")) var->value = "disabled";
         else if (strstr(key, "filter")) var->value = "nearest";
         return true;
      }
      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
         *(bool *)data = variables_changed; variables_changed = false; return true;
      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
         *(int *)data = phase == RETRO_SNAPSHOT_SPECULATIVE ? 0 : 3; return true;
      case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT:
         *(enum retro_savestate_context *)data = RETRO_SAVESTATE_CONTEXT_NORMAL;
         return true;
      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
      case RETRO_ENVIRONMENT_SET_GEOMETRY:
      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
         return true;
      default: return false;
   }
}
static void RETRO_CALLCONV audio_sample(int16_t left, int16_t right)
{
   (void)left; (void)right;
}
static size_t RETRO_CALLCONV audio_batch(const int16_t *data, size_t frames)
{
   (void)data; return frames;
}
static void RETRO_CALLCONV input_poll(void) { }
static int16_t RETRO_CALLCONV input_state(unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   unsigned value = (input_frame / 3) & 1;
   if (port || device != RETRO_DEVICE_JOYPAD || index) return 0;
   return id == RETRO_DEVICE_ID_JOYPAD_MASK ? (int16_t)value : id == 0 ? (int16_t)value : 0;
}
static void RETRO_CALLCONV video_refresh(const void *pixels, unsigned width,
      unsigned height, size_t pitch)
{
   unsigned y, bytes = pixel_format == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
   size_t row, size;
   if (phase == RETRO_SNAPSHOT_SPECULATIVE) return;
   REQUIRE(pixels != RETRO_HW_FRAME_BUFFER_VALID);
   if (!pixels) { video_calls++; return; }
   REQUIRE(width && height && width <= 8192 && height <= 8192);
   row = (size_t)width * bytes; size = (size_t)width * height * 4;
   REQUIRE(row <= pitch && pitch <= 64 * 1024 * 1024 / height && size <= 64 * 1024 * 1024);
   if (size != video.size)
   {
      uint8_t *next = realloc(video.pixels, size);
      REQUIRE(next != NULL); video.pixels = next; video.size = size;
   }
   video.width = width; video.height = height; video_calls++;
   video.borrowed = pixels; video.pitch = pitch;
   for (y = 0; y < height; y++)
   {
      unsigned x;
      for (x = 0; x < width; x++)
      {
         uint32_t color;
         if (bytes == 4)
         {
            memcpy(&color, (const uint8_t *)pixels + y * pitch + x * 4, 4);
            color &= 0xffffff;
         }
         else
         {
            uint16_t value;
            unsigned r, g, b;
            memcpy(&value, (const uint8_t *)pixels + y * pitch + x * 2, 2);
            r = value >> (pixel_format == RETRO_PIXEL_FORMAT_RGB565 ? 11 : 10) & 31;
            g = value >> 5 & (pixel_format == RETRO_PIXEL_FORMAT_RGB565 ? 63 : 31);
            b = value & 31;
            r = (r << 3) | (r >> 2); b = (b << 3) | (b >> 2);
            g = pixel_format == RETRO_PIXEL_FORMAT_RGB565 ? (g << 2) | (g >> 4) : (g << 3) | (g >> 2);
            color = (r << 16) | (g << 8) | b;
         }
         memcpy(video.pixels + ((size_t)y * width + x) * 4, &color, 4);
      }
   }
}

static void *new_snapshot(void)
{
   void *handle = NULL;
   REQUIRE(snapshots.create(&handle) == RETRO_SNAPSHOT_SUCCESS && handle);
   creates++; return handle;
}
static void capture(void *handle)
{
   REQUIRE(snapshots.save(handle) == RETRO_SNAPSHOT_SUCCESS);
   REQUIRE(snapshots.valid(handle)); saves++;
}
static void restore(const void *handle)
{
   unsigned x, y;
   REQUIRE(snapshots.load(handle) == RETRO_SNAPSHOT_SUCCESS); loads++;
   if (check_presented && video.borrowed)
   {
      REQUIRE(pixel_format == RETRO_PIXEL_FORMAT_XRGB8888);
      /* The paused frontend reads the last presented buffer after a load. */
      for (y = 0; y < video.height; y++)
         for (x = 0; x < video.width; x++)
         {
            uint32_t actual, expected;
            memcpy(&actual, (const uint8_t *)video.borrowed + y * video.pitch + x * 4, 4);
            memcpy(&expected, video.pixels + ((size_t)y * video.width + x) * 4, 4);
            REQUIRE((actual & 0xffffff) == expected);
         }
   }
}
static void release(void *handle) { snapshots.release(handle); releases++; }
static void restore_ram(const void *handle, const void *expected, size_t bytes)
{
   restore(handle);
   REQUIRE(!memcmp(expected,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes));
}
static void advance(unsigned frame)
{
   unsigned previous = video_calls;
   input_frame = frame;
   phase = ordinary_execution ? RETRO_SNAPSHOT_ORDINARY : RETRO_SNAPSHOT_CANONICAL;
   core.retro_run();
   REQUIRE(video_calls > previous);
}
static unsigned native_oracle(void)
{
   const uint8_t *ram = core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   unsigned i, counter;
   REQUIRE(ram && core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) >= 0x100020);
   counter = read32(ram + 0x80000);
   if (!counter) fprintf(stderr,"fixture has no completed frame; stage=%u, code=%08x\n",read32(ram+0x80004),read32(ram+0x1000));
   REQUIRE(counter > 0);
   if (!conformance)
      for (i = 0; i < 8; i++) REQUIRE(read32(ram + 0x100000 + i * 4) == 0x03e0801f);
   REQUIRE(snapshot_fixture_check(ram,core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM),check_rsp) == 1);
   return counter;
}

/* Public API observations only: ordinary serialization is not a reference. */
static void conformance_test(void)
{
   void *anchor = new_snapshot(), *other = new_snapshot();
   size_t bytes = core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
   uint8_t *expected, *before, *image = NULL;
   unsigned step, changes = 0, previous_counter = native_oracle();
   size_t image_size = 0;
   REQUIRE(bytes >= 0x100080 && bytes <= 64 * 1024 * 1024);
   expected = malloc(bytes); before = malloc(bytes); REQUIRE(expected && before);
   memcpy(before,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes);
   REQUIRE(!snapshots.valid(other));
   REQUIRE(snapshots.load(other) == RETRO_SNAPSHOT_UNAVAILABLE);
   REQUIRE(!memcmp(before,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes));
   for (step = 0; step < retained_steps; step++)
   {
      unsigned i, counter, width, height, depth = step % 11 == 0 ? 12 : 1 + step % 3;
      memcpy(before,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes);
      capture(anchor);
      REQUIRE(!memcmp(before,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes));
      advance(step); counter = native_oracle();
      REQUIRE(counter > previous_counter); previous_counter = counter;
      memcpy(expected,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes);
      if (image_size == video.size && memcmp(image,video.pixels,video.size)) changes++;
      if (image_size != video.size)
      {
         uint8_t *next = realloc(image,video.size);
         REQUIRE(next); image = next; image_size = video.size;
      }
      REQUIRE(image_size); memcpy(image,video.pixels,image_size); width = video.width; height = video.height;
      capture(other); release(other); REQUIRE(snapshots.valid(anchor)); other = new_snapshot();
      restore_ram(anchor,before,bytes); restore_ram(anchor,before,bytes);
      phase = RETRO_SNAPSHOT_SPECULATIVE;
      for (i = 0; i < depth; i++)
      {
         input_frame = step + i + 1;
         core.retro_run(); speculative_runs++;
      }
      if (depth == 12)
      {
         const uint8_t *ram = core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
         REQUIRE(ram && read32(ram + 0x80000) > counter);
      }
      phase = RETRO_SNAPSHOT_CANONICAL; restore_ram(anchor,before,bytes);
      advance(step); REQUIRE(native_oracle() == counter);
      REQUIRE(!memcmp(expected,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes));
      REQUIRE(video.width == width && video.height == height && video.size == image_size);
      REQUIRE(!memcmp(image,video.pixels,image_size));
   }
   REQUIRE(changes > 0); release(other); release(anchor); free(image); free(before); free(expected);
   puts("PASS public-API continuation: retained GPU reads, CPU counter, RAM, RGB, repeated loads and independent release");
}
static void ordinary(void *bytes, size_t size)
{
   memset(bytes, 0, size);
   REQUIRE(core.retro_serialize(bytes, size));
}
static void determinism(unsigned steps)
{
   void *anchor = new_snapshot(), *empty = new_snapshot();
   size_t state_size = core.retro_serialize_size();
   uint8_t *expected = calloc(1, state_size), *actual = calloc(1, state_size);
   uint8_t *image = NULL;
   size_t image_size = 0;
   unsigned step, image_changes = 0;
   REQUIRE(state_size && state_size < 256 * 1024 * 1024 && expected && actual);
   for (step = 0; step < steps; step++)
   {
      unsigned depth = step % 11 == 0 ? 12 : 1 + step % 3, i;
      unsigned width, height, counter;
      capture(anchor);
      advance(step);
      counter = native_oracle();
      ordinary(expected, state_size);
      if (image_size == video.size && memcmp(image, video.pixels, video.size)) image_changes++;
      if (image_size != video.size)
      {
         uint8_t *next = realloc(image, video.size);
         REQUIRE(next != NULL); image = next; image_size = video.size;
      }
      memcpy(image, video.pixels, image_size); width = video.width; height = video.height;
      REQUIRE(!snapshots.valid(empty));
      REQUIRE(snapshots.load(empty) == RETRO_SNAPSHOT_UNAVAILABLE);
      ordinary(actual, state_size);
      REQUIRE(memcmp(expected, actual, state_size) == 0);
      restore(anchor); restore(anchor); /* captures are non-consuming */
      if (step == 3)
      {
         void *other = new_snapshot();
         capture(other); release(other); REQUIRE(snapshots.valid(anchor));
      }
      phase = RETRO_SNAPSHOT_SPECULATIVE;
      for (i = 0; i < depth; i++) { input_frame = step + i + 1; core.retro_run(); speculative_runs++; }
      phase = RETRO_SNAPSHOT_CANONICAL;
      restore(anchor);
      advance(step);
      REQUIRE(native_oracle() == counter);
      REQUIRE(video.width == width && video.height == height && video.size == image_size);
      REQUIRE(memcmp(image, video.pixels, image_size) == 0);
      ordinary(actual, state_size);
      REQUIRE(memcmp(expected, actual, state_size) == 0);
   }
   REQUIRE(image_changes > 0);
   release(empty); release(anchor); free(expected); free(actual); free(image);
}

static void recovery_and_ordinary_copy(void)
{
   void *anchor = new_snapshot();
   size_t size = core.retro_serialize_size();
   uint8_t *original = malloc(size), *copy = malloc(size), *actual = malloc(size);
   unsigned calls;
   REQUIRE(original && copy && actual);
   capture(anchor); ordinary(original, size);
   memcpy(copy, original, size); memset(original, 0xa5, size); free(original);
   REQUIRE(!core.retro_unserialize("invalid", 7));
   REQUIRE(snapshots.valid(anchor));
   REQUIRE(core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == NULL);
   REQUIRE(core.retro_serialize_size() == 0);
   REQUIRE(!core.retro_serialize(actual, size));
   calls = video_calls; core.retro_run(); REQUIRE(video_calls == calls);
   restore(anchor); native_oracle(); ordinary(actual, size);
   REQUIRE(memcmp(copy, actual, size) == 0);
   advance(123);
   REQUIRE(core.retro_unserialize(copy, size));
   REQUIRE(!snapshots.valid(anchor));
   ordinary(actual, size); REQUIRE(memcmp(copy, actual, size) == 0);
   native_oracle(); release(anchor); free(copy); free(actual);
}

static void ordinary_load_test(void)
{
   size_t size = core.retro_serialize_size();
   size_t bytes = core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
   uint8_t *state = malloc(size), *ram = malloc(bytes);
   void *anchor = ordinary_execution ? NULL : new_snapshot();
   unsigned counter = native_oracle();
   REQUIRE(size && bytes && state && ram);
   if (anchor) capture(anchor);
   ordinary(state,size);
   memcpy(ram,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes);
   REQUIRE(!core.retro_unserialize("invalid",7));
   REQUIRE(core.retro_serialize_size() == size);
   REQUIRE(core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
   REQUIRE(!memcmp(ram,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes));
   if (anchor) { REQUIRE(snapshots.valid(anchor)); restore(anchor); }
   advance(500); REQUIRE(native_oracle() > counter);
   core.retro_reset();
   if (anchor) REQUIRE(!snapshots.valid(anchor));
   REQUIRE(core.retro_unserialize(state,size));
   REQUIRE(!memcmp(ram,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes));
   if (anchor) { REQUIRE(!snapshots.valid(anchor)); release(anchor); }
   free(ram); free(state);
   puts("PASS rejected ordinary load preserves execution; reset invalidates snapshots; copied ordinary state restores");
}

static void transitions(void)
{
   void *anchor = new_snapshot();
   unsigned width = video.width, previous_reads;
   size_t size;
   uint8_t *before, *after;
   capture(anchor);
   previous_reads = option_reads;
   scale = "2x"; variables_changed = true; advance(200);
   REQUIRE(option_reads > previous_reads && video.width == 2 * width);
   REQUIRE(!snapshots.valid(anchor)); native_oracle();
   size = core.retro_serialize_size(); before = malloc(size); after = malloc(size);
   REQUIRE(before && after); ordinary(before, size);
   REQUIRE(snapshots.load(anchor) == RETRO_SNAPSHOT_UNAVAILABLE);
   ordinary(after, size); REQUIRE(memcmp(before, after, size) == 0);
   release(anchor); free(before); free(after);
   determinism(8);
   anchor = new_snapshot(); capture(anchor);
   scale = "1x(native)"; variables_changed = true; advance(201);
   REQUIRE(video.width == width && !snapshots.valid(anchor));
   native_oracle(); release(anchor);
   anchor = new_snapshot(); capture(anchor); core.retro_reset();
   REQUIRE(!snapshots.valid(anchor)); release(anchor);
   for (previous_reads = 0; previous_reads < 20; previous_reads++) advance(previous_reads);
   native_oracle(); determinism(8);
   puts("PASS resolution 1x -> 2x -> 1x, invalid capture rejection, and reset");
}

int main(int argc, char **argv)
{
   struct retro_game_info game;
   struct retro_system_info info;
   void *content = NULL;
   static char scene[4096];
   unsigned i;
   void *library;
   if (argc == 3 && !strcmp(argv[1], "--generate-psx"))
   {
      REQUIRE(strlen(argv[2]) < sizeof(directory)); strcpy(directory,argv[2]);
#ifdef _WIN32
      REQUIRE(CreateDirectoryA(directory,NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
#else
      REQUIRE(mkdir(directory,0700) == 0 || errno == EEXIST);
#endif
      make_fixture(); return EXIT_SUCCESS;
   }
   if (argc == 5 && !strcmp(argv[4], "--fixed-scale"))
   {
      fixed_scale = true; argc = 4;
   }
   if (argc < 3 || argc > 6)
   {
      fprintf(stderr, "Usage: %s CORE_LIBRARY NEW_DIRECTORY [1|4] [--fixed-scale]\n       %s CORE_LIBRARY NEW_DIRECTORY PROFILE CONTENT [SYSTEM_DIRECTORY]\n", argv[0],argv[0]);
      return EXIT_FAILURE;
   }
   REQUIRE(strlen(argv[2]) < sizeof(directory)); strcpy(directory, argv[2]);
   if (argc == 4)
   {
      REQUIRE(!strcmp(argv[3], "1") || !strcmp(argv[3], "4"));
      scale = !strcmp(argv[3], "4") ? "4x" : "1x(native)";
   }
   if (argc >= 5)
   {
      conformance = true; load_profile(argv[3]);
      if (argc == 6) system_directory = argv[5];
   }
#ifdef _WIN32
   REQUIRE(CreateDirectoryA(directory, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
#else
   REQUIRE(mkdir(directory, 0700) == 0 || errno == EEXIST);
#endif
   library = snapshot_core_open(&core,argv[1]); REQUIRE(library);
   REQUIRE(core.retro_api_version() == RETRO_API_VERSION);
   if (!conformance) { make_fixture(); file_path(scene, sizeof(scene), "snapshot-history.exe"); }
   core.retro_set_environment(environment); core.retro_set_video_refresh(video_refresh);
   core.retro_set_audio_sample(audio_sample); core.retro_set_audio_sample_batch(audio_batch);
   core.retro_set_input_poll(input_poll); core.retro_set_input_state(input_state);
   core.retro_init();
   REQUIRE((ordinary_execution || snapshots.version) && phase == RETRO_SNAPSHOT_ORDINARY);
   memset(&game, 0, sizeof(game)); game.path = conformance ? argv[4] : scene;
   memset(&info,0,sizeof(info)); core.retro_get_system_info(&info);
   if (!info.need_fullpath)
   {
      FILE *file = fopen(game.path,"rb");
      long size;
      REQUIRE(file && fseek(file,0,SEEK_END) == 0);
      size = ftell(file); REQUIRE(size > 0 && size <= 64 * 1024 * 1024);
      REQUIRE(fseek(file,0,SEEK_SET) == 0); content = malloc((size_t)size); REQUIRE(content);
      REQUIRE(fread(content,1,(size_t)size,file) == (size_t)size && fclose(file) == 0);
      game.data = content; game.size = (size_t)size;
   }
   REQUIRE(core.retro_load_game(&game));
   REQUIRE(option_reads && (ordinary_execution || snapshots.supported(3)));
   for (i = 0; i < warmup_frames; i++) advance(i);
   if (conformance)
      for (i = 0; i < option_count; i++)
      {
         printf("Option %s=%s; reads=%u\n",options[i].key,options[i].value,option_hits[i]);
         REQUIRE(option_hits[i] > 0);
      }
   native_oracle();
   if (ordinary_execution)
      for (i = 0; i < retained_steps; i++) { advance(i); native_oracle(); }
   else if (conformance) conformance_test(); else determinism(retained_steps);
   if (!conformance && !fixed_scale && !strcmp(scale, "1x(native)"))
   {
      recovery_and_ordinary_copy(); transitions();
   }
   if (check_ordinary_load) ordinary_load_test();
   REQUIRE(creates == releases);
   if (conformance)
   {
      static char path[4096];
      FILE *report;
      fixture_file("final-ram.bin",core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),
                   core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM));
      REQUIRE(video.pixels && video.size);
      fixture_file("final-rgb.bin",video.pixels,video.size);
      file_path(path,sizeof(path),"checks.txt"); report=fopen(path,"w"); REQUIRE(report);
      fprintf(report,"counter=%u\ncreates=%u\nsaves=%u\nloads=%u\n",native_oracle(),creates,saves,loads);
      REQUIRE(!fclose(report));
   }
   core.retro_unload_game(); core.retro_deinit(); free(video.pixels); free(content);
   for (i = 0; i < option_count; i++) { free((void *)options[i].key); free((void *)options[i].value); }
   REQUIRE(snapshot_core_close(library));
   printf("PASS %s: %u speculative frames, %u saves, %u loads, %u owners released\n", conformance ? "shared snapshot fixture" : scale, speculative_runs, saves, loads, releases);
   printf("Observed software image: %ux%u RGB; %u option reads\n",video.width,video.height,option_reads);
   return EXIT_SUCCESS;
}
