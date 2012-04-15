#ifndef LIBRETRO_H__
#define LIBRETRO_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#else
#if defined(_MSC_VER) && !defined(__cplusplus)
#define bool unsigned char
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif
#endif

#define RETRO_API_VERSION         1

#define RETRO_DEVICE_MASK         0xff
#define RETRO_DEVICE_NONE         0
#define RETRO_DEVICE_JOYPAD       1
#define RETRO_DEVICE_MOUSE        2
#define RETRO_DEVICE_KEYBOARD     3
#define RETRO_DEVICE_LIGHTGUN     4

#define RETRO_DEVICE_JOYPAD_MULTITAP        ((1 << 8) | RETRO_DEVICE_JOYPAD)
#define RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE   ((1 << 8) | RETRO_DEVICE_LIGHTGUN)
#define RETRO_DEVICE_LIGHTGUN_JUSTIFIER     ((2 << 8) | RETRO_DEVICE_LIGHTGUN)
#define RETRO_DEVICE_LIGHTGUN_JUSTIFIERS    ((3 << 8) | RETRO_DEVICE_LIGHTGUN)

#define RETRO_DEVICE_ID_JOYPAD_B        0
#define RETRO_DEVICE_ID_JOYPAD_Y        1
#define RETRO_DEVICE_ID_JOYPAD_SELECT   2
#define RETRO_DEVICE_ID_JOYPAD_START    3
#define RETRO_DEVICE_ID_JOYPAD_UP       4
#define RETRO_DEVICE_ID_JOYPAD_DOWN     5
#define RETRO_DEVICE_ID_JOYPAD_LEFT     6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT    7
#define RETRO_DEVICE_ID_JOYPAD_A        8
#define RETRO_DEVICE_ID_JOYPAD_X        9
#define RETRO_DEVICE_ID_JOYPAD_L       10
#define RETRO_DEVICE_ID_JOYPAD_R       11

#define RETRO_DEVICE_ID_MOUSE_X      0
#define RETRO_DEVICE_ID_MOUSE_Y      1
#define RETRO_DEVICE_ID_MOUSE_LEFT   2
#define RETRO_DEVICE_ID_MOUSE_RIGHT  3

#define RETRO_DEVICE_ID_LIGHTGUN_X        0
#define RETRO_DEVICE_ID_LIGHTGUN_Y        1
#define RETRO_DEVICE_ID_LIGHTGUN_TRIGGER  2
#define RETRO_DEVICE_ID_LIGHTGUN_CURSOR   3
#define RETRO_DEVICE_ID_LIGHTGUN_TURBO    4
#define RETRO_DEVICE_ID_LIGHTGUN_PAUSE    5
#define RETRO_DEVICE_ID_LIGHTGUN_START    6

#define RETRO_REGION_NTSC  0
#define RETRO_REGION_PAL   1

#define RETRO_MEMORY_MASK        0xff
#define RETRO_MEMORY_SAVE_RAM    0
#define RETRO_MEMORY_RTC         1
#define RETRO_MEMORY_SYSTEM_RAM  2

#define RETRO_MEMORY_SNES_BSX_RAM             ((1 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_BSX_PRAM            ((2 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM  ((3 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM  ((4 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_GAME_BOY_RAM        ((5 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_GAME_BOY_RTC        ((6 << 8) | RETRO_MEMORY_RTC)

#define RETRO_GAME_TYPE_BSX             0x101
#define RETRO_GAME_TYPE_BSX_SLOTTED     0x102
#define RETRO_GAME_TYPE_SUFAMI_TURBO    0x103
#define RETRO_GAME_TYPE_SUPER_GAME_BOY  0x104


// Environment commands.
#define RETRO_ENVIRONMENT_SET_ROTATION  1  // const unsigned * --
                                           // Sets screen rotation of graphics.
                                           // Is only implemented if rotation can be accelerated by hardware.
                                           // Valid values are 0, 1, 2, 3, which rotates screen by 0, 90, 180, 270 degrees
                                           // counter-clockwise respectively.
                                           //
#define RETRO_ENVIRONMENT_GET_OVERSCAN  2  // bool * --
                                           // Boolean value whether or not the implementation should use overscan, or crop away overscan.
                                           //
#define RETRO_ENVIRONMENT_GET_CAN_DUPE  3  // bool * --
                                           // Boolean value whether or not SSNES supports frame duping,
                                           // passing NULL to video frame callback.
                                           //
#define RETRO_ENVIRONMENT_GET_VARIABLE  4  // struct retro_variable * --
                                           // Interface to aquire user-defined information from environment
                                           // that cannot feasibly be supported in a multi-system way.
                                           // Mostly used for obscure,
                                           // specific features that the user can tap into when neseccary.
                                           //
#define RETRO_ENVIRONMENT_SET_VARIABLES 5  // const struct retro_variable * --
                                           // Allows an implementation to signal the environment
                                           // which variables it might want to check for later using GET_VARIABLE.
                                           // 'data' points to an array of retro_variable structs terminated by a { NULL, NULL } element.
                                           // retro_variable::value should contain a human readable description of the key.
                                           //
#define RETRO_ENVIRONMENT_SET_MESSAGE   6  // const struct retro_message * --
                                           // Sets a message to be displayed in implementation-specific manner for a certain amount of 'frames'.

struct retro_message
{
   const char *msg;
   unsigned    frames;
};

struct retro_system_info
{
   const char *library_name;
   const char *library_version;
   const char *valid_extensions;
   bool        need_fullpath;
   bool        block_extract;
};

struct retro_game_geometry
{
   unsigned base_width;    // Nominal video width of game.
   unsigned base_height;   // Nominal video height of game.
   unsigned max_width;     // Maximum possible width of game.
   unsigned max_height;    // Maximum possible height of game.

   float    aspect_ratio;  // Nominal aspect ratio of game. If aspect_ratio is <= 0.0,
                           // an aspect ratio of base_width / base_height is assumed.
                           // A frontend could override this setting if desired.
};

struct retro_system_timing
{
   double fps;             // FPS of video content.
   double sample_rate;     // Sampling rate of audio.
};

struct retro_system_av_info
{
   struct retro_game_geometry geometry;
   struct retro_system_timing timing;
};

struct retro_variable
{
   const char *key;        // Variable to query in RETRO_ENVIRONMENT_GET_VARIABLE.
                           // If NULL, obtains the complete environment string if more complex parsing is necessary.
                           // The environment string is formatted as key-value pairs delimited by semicolons as so:
                           // "key1=value1;key2=value2;..."
   const char *value;      // Value to be obtained. If key does not exist, it is set to NULL.
};

struct retro_game_info
{
   const char *path;       // Path to game, UTF-8 encoded. Usually used as a reference.
                           // May be NULL if rom was loaded from stdin or similar.
                           // SET_NEED_FULLPATH path guaranteed that this path is valid.
   const void *data;       // Memory buffer of loaded game.
                           // If the game is too big to load in one go.
                           // SET_NEED_FULLPATH should be used.
                           // In this case, data and size will be 0,
                           // and game can be loaded from path.
   size_t      size;       // Size of memory buffer.
   const char *meta;       // String of implementation specific meta-data.
};

typedef bool (*retro_environment_t)(unsigned cmd, void *data);
typedef void (*retro_video_refresh_t)(const void *data, unsigned width, unsigned height, size_t pitch);
typedef void (*retro_audio_sample_t)(int16_t left, int16_t right);
typedef size_t (*retro_audio_sample_batch_t)(const int16_t *data, size_t frames);

typedef void (*retro_input_poll_t)(void);
typedef int16_t (*retro_input_state_t)(unsigned port, unsigned device, unsigned index, unsigned id);

void retro_init(void);
void retro_deinit(void);

unsigned retro_api_version(void);

void retro_get_system_info(struct retro_system_info *info);
void retro_get_system_av_info(struct retro_system_av_info *info);

void retro_set_environment(retro_environment_t);
void retro_set_video_refresh(retro_video_refresh_t);
void retro_set_audio_sample(retro_audio_sample_t);
void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
void retro_set_input_poll(retro_input_poll_t);
void retro_set_input_state(retro_input_state_t);

void retro_set_controller_port_device(unsigned port, unsigned device);

void retro_reset(void);
void retro_run(void);

size_t retro_serialize_size(void);
bool retro_serialize(void *data, size_t size);
bool retro_unserialize(const void *data, size_t size);

void retro_cheat_reset(void);
void retro_cheat_set(unsigned index, bool enabled, const char *code);

bool retro_load_game(const struct retro_game_info *game);

bool retro_load_game_special(
  unsigned game_type,
  const struct retro_game_info *info, size_t num_info
);

void retro_unload_game(void);

unsigned retro_get_region(void);

void *retro_get_memory_data(unsigned id);
size_t retro_get_memory_size(unsigned id);

#ifdef __cplusplus
}
#endif

#endif
