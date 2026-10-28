/* A core for two real RetroArch processes in netplay: each frame mixes
 * both players' buttons and sticks into its RAM and logs the frame, the
 * second player's left stick and a CRC of the RAM, so two logs say
 * whether the peers stayed together and whether the stick crossed.
 *
 * NETPLAY_E2E_LOG: the log. NETPLAY_E2E_PAD: bytes a state carries past
 * the RAM, to stand in for a large one. NETPLAY_E2E_DESYNC: frames run
 * after the first state load before the RAM is upset once.
 * NETPLAY_E2E_PACE_MS: milliseconds slept a frame (4 if unset), to make
 * one peer lag the other. NETPLAY_E2E_JUNK: the state carries a count of
 * its own serializations, which differs between peers and never reaches
 * the RAM, as a core that stores a timestamp would.
 *
 * Paced at about 250 frames a second by default: null video runs
 * unthrottled, and the logs would grow by the hundred thousand lines. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <libretro.h>

#ifdef _WIN32
#include <windows.h>
static void pace(unsigned ms) { Sleep(ms); }
#else
#include <time.h>
static void pace(unsigned ms)
{
   struct timespec ts;
   ts.tv_sec  = (time_t)(ms / 1000);
   ts.tv_nsec = (long)(ms % 1000) * 1000000L;
   nanosleep(&ts, NULL);
}
#endif

#define RAM_SIZE 256

static retro_video_refresh_t video_cb;
static retro_input_poll_t    poll_cb;
static retro_input_state_t   input_cb;
static retro_environment_t   env_cb;
static uint8_t  ram[RAM_SIZE];
static uint32_t frame;
static uint32_t pixels[64];
static size_t   pad;
static uint32_t junk;
static uint32_t serials;
static unsigned desync_after;
static unsigned since_load;
static unsigned pace_ms;
static bool     loaded;
static FILE    *log_file;

static uint32_t crc32_of(const uint8_t *p, size_t n)
{
   uint32_t c = 0xffffffffu;
   size_t i;
   int k;
   for (i = 0; i < n; i++)
   {
      c ^= p[i];
      for (k = 0; k < 8; k++)
         c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
   }
   return ~c;
}

static void mix(uint32_t v)
{
   uint32_t i = (frame * 7u + v) % RAM_SIZE;
   ram[i] = (uint8_t)(ram[i] * 31u + v + (v >> 8) + (v >> 16) + (v >> 24));
}

void retro_set_environment(retro_environment_t cb)
{
   bool no_game = true;
   env_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }

void retro_init(void)
{
   const char *p = getenv("NETPLAY_E2E_PAD");
   const char *l = getenv("NETPLAY_E2E_LOG");
   const char *u = getenv("NETPLAY_E2E_DESYNC");
   const char *m = getenv("NETPLAY_E2E_PACE_MS");
   const char *j = getenv("NETPLAY_E2E_JUNK");
   pad      = p ? (size_t)strtoul(p, NULL, 10) : 0;
   desync_after = u ? (unsigned)strtoul(u, NULL, 10) : 0;
   pace_ms  = m ? (unsigned)strtoul(m, NULL, 10) : 4;
   junk     = (j && *j == '1') ? 4 : 0;
   serials  = 0;
   loaded   = false;
   since_load = 0;
   log_file = l ? fopen(l, "w") : NULL;
   memset(ram, 0, sizeof(ram));
   frame    = 0;
}

void retro_deinit(void)
{
   if (log_file)
      fclose(log_file);
   log_file = NULL;
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "netplay_e2e";
   info->library_version  = "1";
   info->valid_extensions = "";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width  = info->geometry.max_width  = 8;
   info->geometry.base_height = info->geometry.max_height = 8;
   info->timing.fps           = 60.0;
   info->timing.sample_rate   = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   if (log_file)
   {
      fprintf(log_file, "device %u %u\n", port, device);
      fflush(log_file);
   }
}

void retro_reset(void) { memset(ram, 0, sizeof(ram)); frame = 0; }

void retro_run(void)
{
   unsigned p;
   int16_t x1 = 0, y1 = 0;

   poll_cb();
   for (p = 0; p < 2; p++)
   {
      int16_t x = input_cb(p, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
      int16_t y = input_cb(p, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
      mix((uint16_t)input_cb(p, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_MASK));
      mix((uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16));
      if (p == 1)
      {
         x1 = x;
         y1 = y;
      }
   }
   mix(frame);
   if (loaded && desync_after && ++since_load == desync_after)
      ram[0] ^= 0x5a;
   frame++;
   if (log_file)
   {
      fprintf(log_file, "frame %u %d %d %08x\n", (unsigned)frame,
            x1, y1, (unsigned)crc32_of(ram, sizeof(ram)));
      fflush(log_file);
   }
   pixels[0] = frame;
   video_cb(pixels, 8, 8, 8 * sizeof(*pixels));
   pace(pace_ms);
}

size_t retro_serialize_size(void) { return 4 + RAM_SIZE + junk + pad; }

bool retro_serialize(void *data, size_t size)
{
   uint8_t *d = (uint8_t*)data;
   size_t i;
   if (size < retro_serialize_size())
      return false;
   memcpy(d, &frame, 4);
   memcpy(d + 4, ram, RAM_SIZE);
   if (junk)
   {
      serials++;
      memcpy(d + 4 + RAM_SIZE, &serials, 4);
   }
   /* the padding follows the RAM, so it compresses no better than a
    * real core's state */
   for (i = 0; i < pad; i++)
      d[4 + RAM_SIZE + junk + i] = (uint8_t)(ram[i % RAM_SIZE] ^ (i * 2654435761u >> 13));
   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   const uint8_t *d = (const uint8_t*)data;
   if (size < 4 + RAM_SIZE)
      return false;
   memcpy(&frame, d, 4);
   memcpy(ram, d + 4, RAM_SIZE);
   if (!loaded && log_file)
   {
      fprintf(log_file, "loaded %u\n", (unsigned)frame);
      fflush(log_file);
   }
   loaded = true;
   return true;
}

void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
bool retro_load_game(const struct retro_game_info *g) { (void)g; return true; }
bool retro_load_game_special(unsigned t, const struct retro_game_info *g,
      size_t n) { (void)t; (void)g; (void)n; return false; }
void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id)
{
   return id == RETRO_MEMORY_SYSTEM_RAM ? ram : NULL;
}
size_t retro_get_memory_size(unsigned id)
{
   return id == RETRO_MEMORY_SYSTEM_RAM ? RAM_SIZE : 0;
}
