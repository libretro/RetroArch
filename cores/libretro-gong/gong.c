/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2018 - Brad Parker
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
#include <libretro.h>
#include <retro_math.h>
#include <retro_inline.h>

#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define GONG_CORE_PREFIX(s) libretro_gong_##s
#else
#define GONG_CORE_PREFIX(s) s
#endif

#define WIDTH 356
#define HEIGHT 200
#define FPS (60000.0f / 1000.0f)

static retro_log_printf_t GONG_CORE_PREFIX(log_cb);
static retro_video_refresh_t GONG_CORE_PREFIX(video_cb);
static retro_input_poll_t GONG_CORE_PREFIX(input_poll_cb);
static retro_input_state_t GONG_CORE_PREFIX(input_state_cb);
static retro_audio_sample_t GONG_CORE_PREFIX(audio_cb);
static retro_audio_sample_batch_t GONG_CORE_PREFIX(audio_batch_cb);
static retro_environment_t GONG_CORE_PREFIX(environ_cb);

static const char *GONG_CORE_PREFIX(valid_extensions) = "gong";

static float player1_py = 0.0f;
static float player1_dpy = 0.0f;
static float player2_py = 0.0f;
static float player2_dpy = 0.0f;
static float player2_speed = 0.0f;
static float ball_px = 0.0f;
static float ball_py = 0.0f;
static float ball_dpx = 0.0f;
static float ball_dpy = 0.0f;
static float ball_speed = 0.0f;
static bool is_initialized = 0;
static unsigned player1_score = 0;
static unsigned player2_score = 0;
static float current_play_points = 0.0f;

static unsigned char *video_buf = NULL;

enum
{
   B_MOVE_UP,
   B_MOVE_DOWN,
   B_SPEED_UP,
   B_COUNT /* This should always be in the bottom */
};

typedef struct
{
   int half_transition_count;
   bool ended_down;
} Game_Button_State;

typedef struct
{
   Game_Button_State buttons[B_COUNT];
   float last_dt;
} Game_Input;

static Game_Input g_input = {0};

typedef struct
{
   /* Pixels are always 32-bit wide, memory order XX BB GG RR */
   int width;
   int height;
   int pitch;
   void *memory;
} Game_Offscreen_Buffer;

static Game_Offscreen_Buffer game_buffer = {0};

static void game_update_and_render(Game_Input *input, Game_Offscreen_Buffer *draw_buffer);

static const struct retro_controller_description pads[] = {
   { "Joypad", RETRO_DEVICE_JOYPAD },
   { NULL, 0 },
};

static const struct retro_controller_info ports[] = {
   { pads, 1 },
   { 0 },
};

struct retro_input_descriptor desc[] = {
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y,    "Left Analog Y" },
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y,    "Right Analog Y" },

   { 0 },
};

static INLINE bool pressed(Game_Button_State state)
{
   return state.half_transition_count > 1 ||
      (state.half_transition_count == 1 && state.ended_down);
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
   info->timing.fps            = FPS;
   info->timing.sample_rate    = 44100.0;
}

void GONG_CORE_PREFIX(retro_init)(void)
{
   struct retro_log_callback log;

   if (GONG_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      GONG_CORE_PREFIX(log_cb) = log.log;
   else
      GONG_CORE_PREFIX(log_cb) = NULL;

   video_buf = (unsigned char*)calloc(1, WIDTH * HEIGHT * sizeof(unsigned));

   game_buffer.width = WIDTH;
   game_buffer.height = HEIGHT;
   game_buffer.pitch = WIDTH * sizeof(unsigned);
   game_buffer.memory = video_buf;

   g_input.last_dt = 1.0f / FPS;
}

void GONG_CORE_PREFIX(retro_deinit)(void)
{
   if (video_buf)
      free(video_buf);

   video_buf = NULL;
   game_buffer.memory = NULL;
}

void GONG_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   bool no_content = true;

   static const struct retro_variable vars[] = {
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
   player1_py = 0.0f;
   player1_dpy = 0.0f;
   player2_py = 0.0f;
   player2_dpy = 0.0f;
   player2_speed = 0.0f;
   player1_score = 0;
   player2_score = 0;
   is_initialized = 0;
}

size_t GONG_CORE_PREFIX(retro_serialize_size)(void)
{
   return 0;
}

bool GONG_CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool GONG_CORE_PREFIX(retro_unserialize)(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
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
   uint16_t input         = 0;
   uint16_t not_input     = 0;
   static uint16_t previnput = 0;
   uint16_t realinput     = 0;
   int i = 0;
   int16_t analogYLeft1 = 0;
   int16_t analogYRight1 = 0;

   GONG_CORE_PREFIX(input_poll_cb)();

   for (i = 0; i < 16; i++)
   {
      if (GONG_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0, i))
      {
         realinput |= 1 << i;
      }
   }

   analogYLeft1 = GONG_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 5000.0f;
   analogYRight1 = GONG_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / 5000.0f;

   if (analogYLeft1 > 0)
      realinput |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
   else if (analogYRight1 > 0)
      realinput |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);

   if (analogYLeft1 < 0)
      realinput |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
   else if (analogYRight1 < 0)
      realinput |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);

   input = realinput & ~previnput;
   not_input = previnput & ~realinput;

   if (is_key_up_or_down(input, not_input, RETRO_DEVICE_ID_JOYPAD_UP))
      process_joypad(&g_input.buttons[B_MOVE_UP], realinput & (1 << RETRO_DEVICE_ID_JOYPAD_UP));
   else if (is_key_up_or_down(input, not_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
      process_joypad(&g_input.buttons[B_MOVE_DOWN], realinput & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN));
   else if (is_key_up_or_down(input, not_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
      process_joypad(&g_input.buttons[B_MOVE_DOWN], realinput & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN));

   if (is_key_up_or_down(input, not_input, RETRO_DEVICE_ID_JOYPAD_A))
      process_joypad(&g_input.buttons[B_SPEED_UP], realinput & (1 << RETRO_DEVICE_ID_JOYPAD_A));
   else if (is_key_up_or_down(input, not_input, RETRO_DEVICE_ID_JOYPAD_B))
      process_joypad(&g_input.buttons[B_SPEED_UP], realinput & (1 << RETRO_DEVICE_ID_JOYPAD_B));
   else if (is_key_up_or_down(input, not_input, RETRO_DEVICE_ID_JOYPAD_X))
      process_joypad(&g_input.buttons[B_SPEED_UP], realinput & (1 << RETRO_DEVICE_ID_JOYPAD_X));
   else if (is_key_up_or_down(input, not_input, RETRO_DEVICE_ID_JOYPAD_Y))
      process_joypad(&g_input.buttons[B_SPEED_UP], realinput & (1 << RETRO_DEVICE_ID_JOYPAD_Y));

   previnput = realinput;

   game_update_and_render(&g_input, &game_buffer);

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

   if (!is_initialized)
   {
      is_initialized = 1;
      ball_px = 0;
      ball_py = 0;
      ball_dpx = initial_ball_speed;
      ball_dpy = 0;
      current_play_points = 10.f;
      player2_speed = 80.f;
   }

   {
      float speed = 80.f;
      player1_dpy = 0.f;
      
      if (is_down(input->buttons[B_SPEED_UP]))
         speed = 150.f;

      if (is_down(input->buttons[B_MOVE_UP]))
      {
         if (player1_py < playing_field_y - player_size_y)
         {
            player1_dpy = speed;
         }

         if (player1_py < -playing_field_y + player_size_y)
         {
            player1_py = -playing_field_y + player_size_y;
            player1_dpy = 0.f;
         }
      }
      if (is_down(input->buttons[B_MOVE_DOWN]))
      {
         if (player1_py > -playing_field_y + player_size_y)
         {
            player1_dpy = -speed;
         }

         if (player1_py < -playing_field_y + player_size_y)
         {
            player1_py = -playing_field_y + player_size_y;
            player1_dpy = 0.f;
         }
      }

      player1_py += player1_dpy * input->last_dt;
   }

   {
      player2_dpy = (ball_py - player2_py) * 100.f;
      player2_dpy = MIN(player2_dpy, player2_speed);
      player2_dpy = MAX(player2_dpy, -player2_speed);
      player2_py += player2_dpy * input->last_dt;

      if (player2_py < -playing_field_y + player_size_y)
      {
         player2_py = -playing_field_y + player_size_y;
         player2_dpy = 0.f;
      }

      if (player2_py > playing_field_y - player_size_y)
      {
         player2_py = playing_field_y - player_size_y;
         player2_dpy = 0.f;
      }
   }

   ball_px += ball_dpx * input->last_dt;

   if (ball_dpx > 0)
   {
      ball_dpx += 10.f * input->last_dt;
   }
   else
   {
      ball_dpx -= 10.f * input->last_dt;
   }

   ball_py += ball_dpy * input->last_dt;

   if (ball_py > playing_field_y - 1.f)
   {
      ball_py = playing_field_y - 1.f;
      ball_dpy *= -1.f;
   }
   else if (ball_py < -playing_field_y + 1)
   {
      ball_py = -playing_field_y + 1.f;
      ball_dpy *= -1;
   }

   if (ball_px > 80.f - 2.5f - 1.f) /* @Hardcoded */
   {
      if ((ball_py >= (player2_py - 10.f)) && (ball_py <= (player2_py + 10.f)))
      {
         ball_dpx *= -1.f;
         ball_px = 80.f - 2.5f - 1.f; /* @Hardcoded */
         ball_dpy = (ball_py - player2_py) + player2_dpy;
         ++current_play_points;
      }
      else if (ball_px >= playing_field_x - 1)
      {
         ball_px = 0;
         ball_py = 0;
         ball_dpy = 0;
         ball_dpx = -initial_ball_speed;
         player2_score += (unsigned)current_play_points;
         current_play_points = 10.f;
      }
   }
   else if (ball_px < -80 + 2.5f + 1.f) /* @Hardcoded */
   {
      if ((ball_py >= (player1_py - 10.f)) && (ball_py <= (player1_py + 10.f)))
      {
         ball_dpx *= -1.f;
         ball_px = -80 + 2.5f + 1.f; /* @Hardcoded */
         ball_dpy = (ball_py - player1_py) + player1_dpy;
         ++current_play_points;
      }
      else if (ball_px <= -playing_field_x + 1)
      {
         ball_px = 0;
         ball_py = 0;
         ball_dpy = 0;
         ball_dpx = initial_ball_speed;
         player1_score += (unsigned)current_play_points;
         current_play_points = 10.f;
         player2_speed += current_play_points * 0.01f;
      }
   }

   clear(draw_buffer, 0x021077);
   draw_rect(draw_buffer, 0x000530, 0.f, 0.f, playing_field_x, playing_field_y);

   draw_rect(draw_buffer, 0x00ffff, -80.f, player1_py, player_size_x, player_size_y);
   draw_rect(draw_buffer, 0x00ffff, 80.f, player2_py, player_size_x, player_size_y);

   draw_rect(draw_buffer, 0xffff00, ball_px, ball_py, 1.f, 1.f);

   draw_number(draw_buffer, (unsigned)current_play_points, 0xaaaaaa, 0.f, 38.f);
   draw_number(draw_buffer, player1_score, 0xff6611, 20.f, 38.f);
   draw_number(draw_buffer, player2_score, 0xff6611, -20.f, 38.f);
}
