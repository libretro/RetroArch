/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

/* Libretro port by Brad Parker,
   Original source code by Dan Zaidan: https://danzaidan.itch.io/
   Original license:
   "You can do whatever you want with the code, but I am providing it as is without any warranty whatsoever."
 */

#include <stdlib.h>
#include <string.h>
#include <libretro.h>
#include <retro_math.h>
#include <retro_inline.h>
#include <retro_endianness.h>

#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define GONG_CORE_PREFIX(s) libretro_gong_##s
#else
#define GONG_CORE_PREFIX(s) s
#endif

#define WIDTH 356
#define HEIGHT 200
#define MAX_PLAYERS 2
#define STATE_SIZE 4096 /* can be anything as long as it's large enough to hold everything */

static retro_log_printf_t GONG_CORE_PREFIX(log_cb);
static retro_video_refresh_t GONG_CORE_PREFIX(video_cb);
static retro_input_poll_t GONG_CORE_PREFIX(input_poll_cb);
static retro_input_state_t GONG_CORE_PREFIX(input_state_cb);
static retro_audio_sample_t GONG_CORE_PREFIX(audio_cb);
static retro_audio_sample_batch_t GONG_CORE_PREFIX(audio_batch_cb);
static retro_environment_t GONG_CORE_PREFIX(environ_cb);

static const char *GONG_CORE_PREFIX(valid_extensions) = "gong";

static unsigned char *video_buf = NULL;

enum
{
   B_MOVE_UP,
   B_MOVE_DOWN,
   B_SPEED_UP,
   B_COUNT /* This should always be in the bottom */
};

/* any changes here must be handled in serialization code too */
typedef struct
{
   union { float f; unsigned u; } py;
   union { float f; unsigned u; } dpy;
} Player;

/* any changes here must be handled in serialization code too */
typedef struct
{
   int half_transition_count;
   bool ended_down;
} Game_Button_State;

/* any changes here must be handled in serialization code too */
typedef struct
{
   Game_Button_State buttons[B_COUNT];
   float last_dt; /* not in savestate */
} Game_Input;

typedef struct
{
   uint16_t input;
   uint16_t not_input;
   uint16_t realinput;
   int16_t analogYLeft;
   int16_t analogYRight;
} retro_inputs;

/* any changes here must be handled in serialization code too */
typedef struct
{
   unsigned version;
   unsigned player1_score;
   unsigned player2_score;
   union { float f; unsigned u; } player2_speed;
   union { float f; unsigned u; } ball_px;
   union { float f; unsigned u; } ball_py;
   union { float f; unsigned u; } ball_dpx;
   union { float f; unsigned u; } ball_dpy;
   union { float f; unsigned u; } ball_speed;
   union { float f; unsigned u; } current_play_points;
   float refresh; /* not in savestate */
   bool is_initialized;
   bool player2_human;
   uint16_t previnput[MAX_PLAYERS];
   Game_Input g_input[MAX_PLAYERS];
   Player player[MAX_PLAYERS];
} State;

typedef struct
{
   /* Pixels are always 32-bit wide, memory order XX BB GG RR */
   int width;
   int height;
   int pitch;
   void *memory;
} Game_Offscreen_Buffer;

static State *g_state = NULL;
static Game_Offscreen_Buffer game_buffer = {0};
static void game_update_and_render(Game_Input *input, Game_Offscreen_Buffer *draw_buffer);

static const struct retro_controller_description pads1[] = {
   { "Joypad", RETRO_DEVICE_JOYPAD },
   { NULL, 0 },
};

static const struct retro_controller_description pads2[] = {
   { "Joypad", RETRO_DEVICE_JOYPAD },
   { NULL, 0 },
};

static const struct retro_controller_info ports[] = {
   { pads1, 1 },
   { pads2, 1 },
   { 0 },
};

struct retro_input_descriptor desc[] = {
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y,    "Left Analog Y" },
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y,    "Right Analog Y" },

   { 0 },
};

static void check_variables(void)
{
   struct retro_variable var        = {0};

   var.key = "gong_refresh";
   if (GONG_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int i;
      g_state->refresh = atoi(var.value);

      for (i = 0; i < (int)(sizeof(g_state->g_input) / sizeof(g_state->g_input[0])); i++)
         g_state->g_input[i].last_dt = 1.0f / g_state->refresh;
   }

   var.key = "gong_player2";
   if (GONG_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strncmp(var.value, "CPU", 3))
         g_state->player2_human = false;
      else if (!strncmp(var.value, "Human", 5))
         g_state->player2_human = true;
   }
}

static void save_state(void *data, size_t size)
{
   int i = 0;
   int j = 0;
   unsigned char *buf = (unsigned char*)data;
   unsigned version = swap_if_little32(g_state->version);
   unsigned player1_score = swap_if_little32(g_state->player1_score);
   unsigned player2_score = swap_if_little32(g_state->player2_score);
   unsigned player2_speed = swap_if_little32(g_state->player2_speed.u);
   unsigned ball_px = swap_if_little32(g_state->ball_px.u);
   unsigned ball_py = swap_if_little32(g_state->ball_py.u);
   unsigned ball_dpx = swap_if_little32(g_state->ball_dpx.u);
   unsigned ball_dpy = swap_if_little32(g_state->ball_dpy.u);
   unsigned ball_speed = swap_if_little32(g_state->ball_speed.u);
   unsigned current_play_points = swap_if_little32(g_state->current_play_points.u);
   unsigned is_initialized = g_state->is_initialized;
   unsigned player2_human = g_state->player2_human;

   (void)size;

   memcpy(buf, &version, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &player1_score, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &player2_score, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &player2_speed, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &ball_px, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &ball_py, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &ball_dpx, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &ball_dpy, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &ball_speed, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &current_play_points, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &is_initialized, sizeof(unsigned));
   buf += sizeof(unsigned);

   memcpy(buf, &player2_human, sizeof(unsigned));
   buf += sizeof(unsigned);

   /* previnput */
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      uint16_t previnput = swap_if_little16(g_state->previnput[i]);
      memcpy(buf, &previnput, sizeof(uint16_t));
      buf += sizeof(uint16_t);
   }

   /* g_input */
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      for (j = 0; j < B_COUNT; j++)
      {
         int half_transition_count = swap_if_little32(g_state->g_input[i].buttons[j].half_transition_count);
         unsigned ended_down = g_state->g_input[i].buttons[j].ended_down;

         memcpy(buf, &half_transition_count, sizeof(int));
         buf += sizeof(int);

         memcpy(buf, &ended_down, sizeof(unsigned));
         buf += sizeof(unsigned);
      }
   }

   /* player */
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      unsigned py = swap_if_little32((unsigned)g_state->player[i].py.u);
       
      memcpy(buf, &py, sizeof(unsigned));
      buf += sizeof(uint16_t);
   }
}

static void load_state(const void *data, size_t size)
{
   int i = 0;
   int j = 0;
   const unsigned char *buf = (const unsigned char*)data;

   (void)size;

   memset(g_state, 0, sizeof(*g_state));

   g_state->version = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->player1_score = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->player2_score = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->player2_speed.u = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->ball_px.u = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->ball_py.u = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->ball_dpx.u = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->ball_dpy.u = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->ball_speed.u = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->current_play_points.u = swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->is_initialized = (bool)swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   g_state->player2_human = (bool)swap_if_little32(*(unsigned*)buf);
   buf += sizeof(unsigned);

   /* previnput */
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      uint16_t previnput = swap_if_little16(*(uint16_t*)buf);
      g_state->previnput[i] = previnput;
      buf += sizeof(uint16_t);
   }

   /* g_input */
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      for (j = 0; j < B_COUNT; j++)
      {
         int half_transition_count;
         bool ended_down;

         half_transition_count = (int)swap_if_little32(*(unsigned*)buf);

         g_state->g_input[i].buttons[j].half_transition_count = half_transition_count;
         buf += sizeof(int);

         ended_down = (bool)swap_if_little32(*(unsigned*)buf);

         g_state->g_input[i].buttons[j].ended_down = ended_down;
         buf += sizeof(unsigned);
      }
   }

   /* player */
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      g_state->player[i].py.u = swap_if_little32(*(unsigned*)buf);
      buf += sizeof(unsigned);

      g_state->player[i].dpy.u = swap_if_little32(*(unsigned*)buf);
      buf += sizeof(unsigned);
   }

   check_variables();
}

static INLINE bool is_down(Game_Button_State state)
{
   return state.ended_down;
}

void GONG_CORE_PREFIX(retro_get_system_info)(struct retro_system_info *info)
{
   info->library_name     = "gong";
   info->library_version  = "v1.0";
   info->need_fullpath    = false;
   info->block_extract    = false;
   info->valid_extensions = GONG_CORE_PREFIX(valid_extensions);
}

void GONG_CORE_PREFIX(retro_get_system_av_info)(struct retro_system_av_info *info)
{
   info->geometry.base_width   = WIDTH;
   info->geometry.base_height  = HEIGHT;
   info->geometry.max_width    = WIDTH;
   info->geometry.max_height   = HEIGHT;
   info->geometry.aspect_ratio = 16.0f / 9.0f;
   info->timing.fps            = g_state->refresh;
   info->timing.sample_rate    = 44100.0;
}

void GONG_CORE_PREFIX(retro_init)(void)
{
   struct retro_log_callback log;

   g_state = (State*)calloc(1, sizeof(*g_state));

   if (GONG_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      GONG_CORE_PREFIX(log_cb) = log.log;
   else
      GONG_CORE_PREFIX(log_cb) = NULL;

   video_buf = (unsigned char*)calloc(1, WIDTH * HEIGHT * sizeof(unsigned));

   game_buffer.width  = WIDTH;
   game_buffer.height = HEIGHT;
   game_buffer.pitch  = WIDTH * sizeof(unsigned);
   game_buffer.memory = video_buf;
}

void GONG_CORE_PREFIX(retro_deinit)(void)
{
   if (video_buf)
      free(video_buf);

   video_buf = NULL;
   game_buffer.memory = NULL;

   if (g_state)
      free(g_state);
}

void GONG_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   bool no_content = true;

   static const struct retro_variable vars[] = {
      { "gong_player2", "Player 2; CPU|Human" },
      { "gong_refresh", "Video Refresh Rate (restart); 60|70|72|75|100|119|120|140|144" },
      { NULL, NULL },
   };

   GONG_CORE_PREFIX(environ_cb) = cb;

   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void GONG_CORE_PREFIX(retro_set_video_refresh)(retro_video_refresh_t cb)
{
   GONG_CORE_PREFIX(video_cb) = cb;
}

void GONG_CORE_PREFIX(retro_set_audio_sample)(retro_audio_sample_t cb)
{
   GONG_CORE_PREFIX(audio_cb) = cb;
}

void GONG_CORE_PREFIX(retro_set_audio_sample_batch)(retro_audio_sample_batch_t cb)
{
   GONG_CORE_PREFIX(audio_batch_cb) = cb;
}

void GONG_CORE_PREFIX(retro_set_input_poll)(retro_input_poll_t cb)
{
   GONG_CORE_PREFIX(input_poll_cb) = cb;
}

void GONG_CORE_PREFIX(retro_set_input_state)(retro_input_state_t cb)
{
   GONG_CORE_PREFIX(input_state_cb) = cb;
}

void GONG_CORE_PREFIX(retro_set_controller_port_device)(unsigned a, unsigned b)
{
}

void GONG_CORE_PREFIX(retro_reset)(void)
{
   memset(g_state, 0, sizeof(*g_state));
   check_variables();
}

size_t GONG_CORE_PREFIX(retro_serialize_size)(void)
{
   return STATE_SIZE;
}

bool GONG_CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   if (size != STATE_SIZE)
      return false;

   save_state(data, size);
   return true;
}

bool GONG_CORE_PREFIX(retro_unserialize)(const void *data, size_t size)
{
   if (size != STATE_SIZE)
      return false;

   load_state(data, size);
   return true;
}

void GONG_CORE_PREFIX(retro_cheat_reset)(void)
{
}

void GONG_CORE_PREFIX(retro_cheat_set)(unsigned a, bool b, const char * c)
{
}

bool GONG_CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;

   check_variables();

   if (!GONG_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (GONG_CORE_PREFIX(log_cb))
         GONG_CORE_PREFIX(log_cb)(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return false;
   }

   return true;
}

bool GONG_CORE_PREFIX(retro_load_game_special)(unsigned a, const struct retro_game_info *b, size_t c)
{
   return false;
}

void GONG_CORE_PREFIX(retro_unload_game)(void)
{
}

unsigned GONG_CORE_PREFIX(retro_get_region)(void)
{
   return RETRO_REGION_NTSC;
}

void* GONG_CORE_PREFIX(retro_get_memory_data)(unsigned id)
{
   return NULL;
}

size_t GONG_CORE_PREFIX(retro_get_memory_size)(unsigned id)
{
   return 0;
}

static void process_joypad(Game_Button_State *new_state, bool is_down)
{
   if (new_state->ended_down != is_down)
   {
      new_state->ended_down = is_down;
      new_state->half_transition_count += 1;
   }
}

static bool is_key_up_or_down(int16_t input, int16_t not_input, int key)
{
   if (input & (1 << key) || not_input & (1 << key))
      return true;

   return false;
}

void GONG_CORE_PREFIX(retro_run)(void)
{
   int i = 0;
   int port = 0;
   bool updated = false;
   retro_inputs inputs[MAX_PLAYERS] = {{0}};

   if (GONG_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();

   GONG_CORE_PREFIX(input_poll_cb)();

   for (port = 0; port < MAX_PLAYERS; port++)
   {
      for (i = 0; i < 16; i++)
      {
         if (GONG_CORE_PREFIX(input_state_cb)(port, RETRO_DEVICE_JOYPAD, 0, i))
         {
            inputs[port].realinput |= 1 << i;
         }
      }

      inputs[port].analogYLeft = GONG_CORE_PREFIX(input_state_cb)(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 5000.0f;
      inputs[port].analogYRight = GONG_CORE_PREFIX(input_state_cb)(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / 5000.0f;

      if (inputs[port].analogYLeft > 0)
         inputs[port].realinput |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
      else if (inputs[port].analogYRight > 0)
         inputs[port].realinput |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);

      if (inputs[port].analogYLeft < 0)
         inputs[port].realinput |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
      else if (inputs[port].analogYRight < 0)
         inputs[port].realinput |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);

      inputs[port].input = inputs[port].realinput & ~g_state->previnput[port];
      inputs[port].not_input = g_state->previnput[port] & ~inputs[port].realinput;

      if (is_key_up_or_down(inputs[port].input, inputs[port].not_input, RETRO_DEVICE_ID_JOYPAD_UP))
         process_joypad(&g_state->g_input[port].buttons[B_MOVE_UP], inputs[port].realinput & (1 << RETRO_DEVICE_ID_JOYPAD_UP));
      else if (is_key_up_or_down(inputs[port].input, inputs[port].not_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
         process_joypad(&g_state->g_input[port].buttons[B_MOVE_DOWN], inputs[port].realinput & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN));
      else if (is_key_up_or_down(inputs[port].input, inputs[port].not_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
         process_joypad(&g_state->g_input[port].buttons[B_MOVE_DOWN], inputs[port].realinput & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN));

      if (is_key_up_or_down(inputs[port].input, inputs[port].not_input, RETRO_DEVICE_ID_JOYPAD_A))
         process_joypad(&g_state->g_input[port].buttons[B_SPEED_UP], inputs[port].realinput & (1 << RETRO_DEVICE_ID_JOYPAD_A));
      else if (is_key_up_or_down(inputs[port].input, inputs[port].not_input, RETRO_DEVICE_ID_JOYPAD_B))
         process_joypad(&g_state->g_input[port].buttons[B_SPEED_UP], inputs[port].realinput & (1 << RETRO_DEVICE_ID_JOYPAD_B));
      else if (is_key_up_or_down(inputs[port].input, inputs[port].not_input, RETRO_DEVICE_ID_JOYPAD_X))
         process_joypad(&g_state->g_input[port].buttons[B_SPEED_UP], inputs[port].realinput & (1 << RETRO_DEVICE_ID_JOYPAD_X));
      else if (is_key_up_or_down(inputs[port].input, inputs[port].not_input, RETRO_DEVICE_ID_JOYPAD_Y))
         process_joypad(&g_state->g_input[port].buttons[B_SPEED_UP], inputs[port].realinput & (1 << RETRO_DEVICE_ID_JOYPAD_Y));

      g_state->previnput[port] = inputs[port].realinput;
   }

   game_update_and_render(g_state->g_input, &game_buffer);

   GONG_CORE_PREFIX(video_cb)(video_buf, WIDTH, HEIGHT, WIDTH * sizeof(uint32_t));
}

unsigned GONG_CORE_PREFIX(retro_api_version)(void)
{
   return RETRO_API_VERSION;
}

static void draw_rect_in_pixels(Game_Offscreen_Buffer *buffer, unsigned color, int min_x, int min_y, int max_x, int max_y)
{
   int y;

   min_x = MAX(min_x, 0);
   min_y = MAX(min_y, 0);
   max_x = MIN(max_x, buffer->width);
   max_y = MIN(max_y, buffer->height);

   for (y = min_y; y < max_y; y++)
   {
      int x;

      for (x = min_x; x < max_x; x++)
      {
         unsigned *pixel = (unsigned*)((unsigned char*)buffer->memory + ((buffer->width * (buffer->height - y - 1) + x) * sizeof(unsigned)));

         *pixel++ = color;
      }
   }
}

static void clear(Game_Offscreen_Buffer *buffer, unsigned color)
{
   draw_rect_in_pixels(buffer, color, 0, 0, buffer->width, buffer->height);
}

static void draw_rect(Game_Offscreen_Buffer *buffer, unsigned color, float x, float y, float half_size_x, float half_size_y)
{
   /* @Hardcoded to always keep the playing field area on screen, no matter the aspect ratio */
   float scale = .01f;
   float relative_axis = (float)buffer->height;
   int min_x, min_y, max_x, max_y;

   if ((float)buffer->width / (float)buffer->height < 1.77f)
   {
      relative_axis = (float)buffer->width;
      scale = .0056f;
   }

   half_size_x *= relative_axis * scale;
   half_size_y *= relative_axis * scale;
   x *= relative_axis * scale;
   y *= relative_axis * scale;

   x = x + buffer->width / 2;
   y = y + buffer->height / 2;

   min_x = (unsigned)(x - half_size_x);
   min_y = (unsigned)(y - half_size_y);
   max_x = (unsigned)(x + half_size_x);
   max_y = (unsigned)(y + half_size_y);

   draw_rect_in_pixels(buffer, color, min_x, min_y, max_x, max_y);
}

static void draw_number(Game_Offscreen_Buffer *buffer, unsigned number, unsigned color, float x, float y)
{
   float at_x = x;

   do {
      unsigned alg = number % 10;

      number /= 10;

      switch (alg)
      {
         case 0:
         {
            draw_rect(buffer, color, at_x - 2.f, y, .5f, 4.f);
            draw_rect(buffer, color, at_x + 2.f, y, .5f, 4.f);
            draw_rect(buffer, color, at_x, y + 4.f, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y - 4.f, 2.5f, .5f);
            break;
         }

         case 1:
         {
            draw_rect(buffer, color, at_x + 2.f, y, .5f, 4.5f);
            break;
         }

         case 2:
         {
            draw_rect(buffer, color, at_x - 2.f, y - 2.f, .5f, 2.f);
            draw_rect(buffer, color, at_x + 2.f, y + 2.f, .5f, 2.f);
            draw_rect(buffer, color, at_x, y + 4.f, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y - 4.f, 2.5f, .5f);
            break;
         }

         case 3:
         {
            draw_rect(buffer, color, at_x + 2.f, y, .5f, 4.f);
            draw_rect(buffer, color, at_x, y + 4.f, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y - 4.f, 2.5f, .5f);
            break;
         };

         case 4:
         {
            draw_rect(buffer, color, at_x, y, 2.5f, .5f);
            draw_rect(buffer, color, at_x + 2.f, y, .5f, 4.5f);
            draw_rect(buffer, color, at_x - 2.f, y+2.5f, .5f, 2.f);
            break;
         };

         case 5:
         {
            draw_rect(buffer, color, at_x + 2.f, y-2.f, .5f, 2.f);
            draw_rect(buffer, color, at_x - 2.f, y+2.f, .5f, 2.f);
            draw_rect(buffer, color, at_x, y + 4.f, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y - 4.f, 2.5f, .5f);
            break;
         };

         case 6:
         {
            draw_rect(buffer, color, at_x + 2.f, y-2.f, .5f, 2.f);
            draw_rect(buffer, color, at_x - 2.f, y, .5f, 4.f);
            draw_rect(buffer, color, at_x, y + 4.f, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y - 4.f, 2.5f, .5f);
            break;
         };

         case 7:
         {
            draw_rect(buffer, color, at_x + 2.f, y, .5f, 4.5f);
            draw_rect(buffer, color, at_x, y + 4.f, 2.5f, .5f);
            break;
         };

         case 8:
         {
            draw_rect(buffer, color, at_x - 2.f, y, .5f, 4.f);
            draw_rect(buffer, color, at_x + 2.f, y, .5f, 4.f);
            draw_rect(buffer, color, at_x, y + 4.f, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y - 4.f, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y, 2.5f, .5f);
            break;
         };

         case 9:
         {
            draw_rect(buffer, color, at_x - 2.f, y + 2.f, .5f, 2.f);
            draw_rect(buffer, color, at_x + 2.f, y, .5f, 4.f);
            draw_rect(buffer, color, at_x, y + 4.f, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y, 2.5f, .5f);
            draw_rect(buffer, color, at_x, y - 4.f, 2.5f, .5f);
            break;
         };

         default:
           break;
      }

      at_x -= 7.f;
   } while(number > 0);
}

static void game_update_and_render(Game_Input *input, Game_Offscreen_Buffer *draw_buffer)
{
   const float initial_ball_speed = 80.f;
   float playing_field_x = 85.f;
   float playing_field_y = 48.f;
   float player_size_x = 2.5f;
   float player_size_y = 10.f;
   int i;

   if (!g_state->is_initialized)
   {
      g_state->is_initialized = 1;
      g_state->ball_px.f = 0;
      g_state->ball_py.f = 0;
      g_state->ball_dpx.f = initial_ball_speed;
      g_state->ball_dpy.f = 0;
      g_state->current_play_points.f = 10.f;
      g_state->player2_speed.f = 80.f;
   }

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      float speed = 80.f;

      if (i == 1 && !g_state->player2_human)
        break;

      g_state->player[i].dpy.f = 0.f;

      if (is_down(input[i].buttons[B_SPEED_UP]))
         speed = 150.f;

      if (is_down(input[i].buttons[B_MOVE_UP]))
      {
         if (g_state->player[i].py.f < playing_field_y - player_size_y)
         {
            g_state->player[i].dpy.f = speed;
         }

         if (g_state->player[i].py.f < -playing_field_y + player_size_y)
         {
            g_state->player[i].py.f = -playing_field_y + player_size_y;
            g_state->player[i].dpy.f = 0.f;
         }
      }
      if (is_down(input[i].buttons[B_MOVE_DOWN]))
      {
         if (g_state->player[i].py.f > -playing_field_y + player_size_y)
         {
            g_state->player[i].dpy.f = -speed;
         }

         if (g_state->player[i].py.f < -playing_field_y + player_size_y)
         {
            g_state->player[i].py.f = -playing_field_y + player_size_y;
            g_state->player[i].dpy.f = 0.f;
         }
      }

      g_state->player[i].py.f += g_state->player[i].dpy.f * input->last_dt;
   }

   if (!g_state->player2_human)
   {
      g_state->player[1].dpy.f = (g_state->ball_py.f - g_state->player[1].py.f) * 100.f;
      g_state->player[1].dpy.f = MIN(g_state->player[1].dpy.f, g_state->player2_speed.f);
      g_state->player[1].dpy.f = MAX(g_state->player[1].dpy.f, -g_state->player2_speed.f);
      g_state->player[1].py.f += g_state->player[1].dpy.f * input->last_dt;

      if (g_state->player[1].py.f < -playing_field_y + player_size_y)
      {
         g_state->player[1].py.f = -playing_field_y + player_size_y;
         g_state->player[1].dpy.f = 0.f;
      }

      if (g_state->player[1].py.f > playing_field_y - player_size_y)
      {
         g_state->player[1].py.f = playing_field_y - player_size_y;
         g_state->player[1].dpy.f = 0.f;
      }
   }

   g_state->ball_px.f += g_state->ball_dpx.f * input->last_dt;

   if (g_state->ball_dpx.f > 0)
   {
      g_state->ball_dpx.f += 10.f * input->last_dt;
   }
   else
   {
      g_state->ball_dpx.f -= 10.f * input->last_dt;
   }

   g_state->ball_py.f += g_state->ball_dpy.f * input->last_dt;

   if (g_state->ball_py.f > playing_field_y - 1.f)
   {
      g_state->ball_py.f = playing_field_y - 1.f;
      g_state->ball_dpy.f *= -1.f;
   }
   else if (g_state->ball_py.f < -playing_field_y + 1)
   {
      g_state->ball_py.f = -playing_field_y + 1.f;
      g_state->ball_dpy.f *= -1;
   }

   if (g_state->ball_px.f > 80.f - 2.5f - 1.f) /* @Hardcoded */
   {
      if ((g_state->ball_py.f >= (g_state->player[1].py.f - 10.f)) && (g_state->ball_py.f <= (g_state->player[1].py.f + 10.f)))
      {
         g_state->ball_dpx.f *= -1.f;
         g_state->ball_px.f = 80.f - 2.5f - 1.f; /* @Hardcoded */
         g_state->ball_dpy.f = (g_state->ball_py.f - g_state->player[1].py.f) + g_state->player[1].dpy.f;
         ++g_state->current_play_points.f;
      }
      else if (g_state->ball_px.f >= playing_field_x - 1)
      {
         g_state->ball_px.f = 0;
         g_state->ball_py.f = 0;
         g_state->ball_dpy.f = 0;
         g_state->ball_dpx.f = -initial_ball_speed;
         g_state->player2_score += (unsigned)g_state->current_play_points.f;
         g_state->current_play_points.f = 10.f;
      }
   }
   else if (g_state->ball_px.f < -80 + 2.5f + 1.f) /* @Hardcoded */
   {
      if ((g_state->ball_py.f >= (g_state->player[0].py.f - 10.f)) && (g_state->ball_py.f <= (g_state->player[0].py.f + 10.f)))
      {
         g_state->ball_dpx.f *= -1.f;
         g_state->ball_px.f = -80 + 2.5f + 1.f; /* @Hardcoded */
         g_state->ball_dpy.f = (g_state->ball_py.f - g_state->player[0].py.f) + g_state->player[0].dpy.f;
         ++g_state->current_play_points.f;
      }
      else if (g_state->ball_px.f <= -playing_field_x + 1)
      {
         g_state->ball_px.f = 0;
         g_state->ball_py.f = 0;
         g_state->ball_dpy.f = 0;
         g_state->ball_dpx.f = initial_ball_speed;
         g_state->player1_score += (unsigned)g_state->current_play_points.f;
         g_state->current_play_points.f = 10.f;

         if (!g_state->player2_human)
            g_state->player2_speed.f += g_state->current_play_points.f * 0.01f;
      }
   }

   clear(draw_buffer, 0x021077);
   draw_rect(draw_buffer, 0x000530, 0.f, 0.f, playing_field_x, playing_field_y);

   draw_rect(draw_buffer, 0x00ffff, -80.f, g_state->player[0].py.f, player_size_x, player_size_y);
   draw_rect(draw_buffer, 0x00ffff, 80.f, g_state->player[1].py.f, player_size_x, player_size_y);

   draw_rect(draw_buffer, 0xffff00, g_state->ball_px.f, g_state->ball_py.f, 1.f, 1.f);

   draw_number(draw_buffer, (unsigned)g_state->current_play_points.f, 0xaaaaaa, 0.f, 38.f);
   draw_number(draw_buffer, g_state->player1_score, 0xff6611, 20.f, 38.f);
   draw_number(draw_buffer, g_state->player2_score, 0xff6611, -20.f, 38.f);
}
