/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SDL
#include "SDL.h"
#endif

#include "conf/config_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include "general.h"


static int g_player = 1;
static int g_joypad = 0;
static char *g_in_path = NULL;
static char *g_out_path = NULL;
static bool g_use_misc = false;

static void print_help(void)
{
   puts("==================");
   puts("ssnes-joyconfig");
   puts("==================");
   puts("Usage: ssnes-joyconfig [ -p/--player <1-5> | -j/--joypad <num> | -i/--input <file> | -o/--output <file> | -h/--help ]");
   puts("");
   puts("-p/--player: Which player to configure for (1 up to and including 5).");
   puts("-j/--joypad: Which joypad to use when configuring (first joypad is 0).");
   puts("-i/--input: Input file to configure with. Binds will be added on or overwritten.");
   puts("\tIf not selected, an empty config will be used as a base.");
   puts("-o/--output: Output file to write to. If not selected, config file will be dumped to stdout.");
   puts("-m/--misc: Also configure various keybinds that are not directly SNES related. These configurations are for player 1 only.");
   puts("-h/--help: This help.");
}

struct bind
{
   char *keystr;
   char *confbtn[MAX_PLAYERS];
   char *confaxis[MAX_PLAYERS];
   bool is_misc;
};

#define BIND(x, k) { x, { "input_player1_" #k "_btn", "input_player2_" #k "_btn", "input_player3_" #k "_btn", "input_player4_" #k "_btn", "input_player5_" #k "_btn" }, {"input_player1_" #k "_axis", "input_player2_" #k "_axis", "input_player3_" #k "_axis", "input_player4_" #k "_axis", "input_player5_" #k "_axis"}, false},

#define MISC_BIND(x, k) { x, { "input_" #k "_btn" }, { "input_" #k "_axis" }, true},

static struct bind binds[] = {
   BIND("A button (right)", a)
   BIND("B button (down)", b)
   BIND("X button (top)", x)
   BIND("Y button (left)", y)
   BIND("L button (left shoulder)", l)
   BIND("R button (right shoulder)", r)
   BIND("Start button", start)
   BIND("Select button", select)
   BIND("Left D-pad", left)
   BIND("Up D-pad", up)
   BIND("Right D-pad", right)
   BIND("Down D-pad", down)

   MISC_BIND("Save state", save_state)
   MISC_BIND("Load state", load_state)
   MISC_BIND("Exit emulator", exit_emulator)
   MISC_BIND("Toggle fullscreen", toggle_fullscreen)
   MISC_BIND("Save state slot increase", state_slot_increase)
   MISC_BIND("Save state slot decrease", state_slot_decrease)
   MISC_BIND("Toggle fast forward", toggle_fast_forward)
   MISC_BIND("Audio input rate step up", rate_step_up)
   MISC_BIND("Audio input rate step down", rate_step_down)
   MISC_BIND("Rewind", rewind)
   MISC_BIND("Movie recording toggle", movie_record_toggle)
   MISC_BIND("Pause", pause_toggle)
   MISC_BIND("Reset", reset)
   MISC_BIND("Next shader", shader_next)
   MISC_BIND("Previous shader", shader_prev)
   MISC_BIND("Toggle cheat on/off", cheat_toggle)
   MISC_BIND("Cheat index plus", cheat_index_plus)
   MISC_BIND("Cheat index minus", cheat_index_minus)
   MISC_BIND("Screenshot", screenshot)
   MISC_BIND("DSP config", dsp_config)
};

static void get_binds(config_file_t *conf, int player, int joypad)
{
   if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_VIDEO) < 0)
   {
      fprintf(stderr, "Failed to init joystick subsystem.\n");
      exit(1);
   }
   SDL_Joystick *joystick;
   int num = SDL_NumJoysticks();
   if (joypad >= num)
   {
      fprintf(stderr, "Cannot find joystick at index #%d, only have %d joystick(s) available ...\n", joypad, num);
      exit(1);
   }

   joystick = SDL_JoystickOpen(joypad);
   if (!joystick)
   {
      fprintf(stderr, "Cannot open joystick.\n");
      exit(1);
   }

   int last_axis = 0xFF;
   int last_pos = 0;
   int num_axes = SDL_JoystickNumAxes(joystick);
   int initial_axes[num_axes];

   SDL_PumpEvents();
   SDL_JoystickUpdate();
   for (int i = 0; i < num_axes; i++)
      initial_axes[i] = SDL_JoystickGetAxis(joystick, i);

   fprintf(stderr, "Configuring binds for player #%d on joypad #%d (%s)\n", player + 1, joypad, SDL_JoystickName(joypad));
   fprintf(stderr, "Press Ctrl-C to exit early.\n");
   fprintf(stderr, "\n");

   for (unsigned i = 0; i < sizeof(binds) / sizeof(struct bind) && (g_use_misc || !binds[i].is_misc) ; i++)
   {
      fprintf(stderr, "%s\n", binds[i].keystr);

      bool done = false;
      SDL_Event event;
      int value;
      const char *quark;
      unsigned player_index = binds[i].is_misc ? 0 : player;

      while (SDL_WaitEvent(&event) && !done)
      {
         switch (event.type)
         {
            case SDL_JOYBUTTONDOWN:
               fprintf(stderr, "\tJoybutton pressed: %d\n", (int)event.jbutton.button);
               done = true;
               config_set_int(conf, binds[i].confbtn[player_index], event.jbutton.button);
               break;

            case SDL_JOYAXISMOTION:
               if ( // This is starting to look like Lisp! :D
                     (abs((int)event.jaxis.value - initial_axes[event.jaxis.axis]) > 20000) &&
                     (
                        (event.jaxis.axis != last_axis) || 
                        (
                           (abs(event.jaxis.value) > 20000) && 
                           (abs((int)event.jaxis.value - last_pos) > 20000)
                        )
                     )
                  )
               {
                  last_axis = event.jaxis.axis;
                  last_pos = event.jaxis.value;
                  fprintf(stderr, "\tJoyaxis moved: Axis %d, Value %d\n", (int)event.jaxis.axis, (int)event.jaxis.value);
                  done = true;

                  char buf[8];
                  snprintf(buf, sizeof(buf), event.jaxis.value > 0 ? "+%d" : "-%d", event.jaxis.axis);
                  config_set_string(conf, binds[i].confaxis[player_index], buf);
               }
               break;

            case SDL_KEYDOWN:
               fprintf(stderr, ":V\n");
               break;

            case SDL_JOYHATMOTION:
               value = event.jhat.value;
               if (value & SDL_HAT_UP)
                  quark = "up";
               else if (value & SDL_HAT_DOWN)
                  quark = "down";
               else if (value & SDL_HAT_LEFT)
                  quark = "left";
               else if (value & SDL_HAT_RIGHT)
                  quark = "right";
               else
                  break;

               fprintf(stderr, "\tJoyhat moved: Hat %d, direction %s\n", (int)event.jhat.hat, quark);

               done = true;
               char buf[16];
               snprintf(buf, sizeof(buf), "h%d%s", event.jhat.hat, quark);
               config_set_string(conf, binds[i].confbtn[player_index], buf);
               break;


            case SDL_QUIT:
               goto end;

            default:
               break;
         }
      }
   }

end:
   SDL_JoystickClose(joystick);
   SDL_Quit();
}

static void parse_input(int argc, char *argv[])
{
   char optstring[] = "i:o:p:j:hm";
   struct option opts[] = {
      { "input", 1, NULL, 'i' },
      { "output", 1, NULL, 'o' },
      { "player", 1, NULL, 'p' },
      { "joypad", 1, NULL, 'j' },
      { "help", 0, NULL, 'h' },
      { "misc", 0, NULL, 'm' },
      { NULL, 0, NULL, 0 }
   };

   int option_index = 0;
   for(;;)
   {
      int c = getopt_long(argc, argv, optstring, opts, &option_index);
      if (c == -1)
         break;

      switch (c)
      {
         case 'h':
            print_help();
            exit(0);

         case 'i':
            g_in_path = strdup(optarg);
            break;

         case 'o':
            g_out_path = strdup(optarg);
            break;

         case 'm':
            g_use_misc = true;
            break;

         case 'j':
            g_joypad = strtol(optarg, NULL, 0);
            if (g_joypad < 0)
            {
               fprintf(stderr, "Joypad number can't be negative!\n");
               exit(1);
            }
            break;

         case 'p':
            g_player = strtol(optarg, NULL, 0);
            if (g_player < 1)
            {
               fprintf(stderr, "Player number must be at least 1!\n");
               exit(1);
            }
            else if (g_player > MAX_PLAYERS)
            {
               fprintf(stderr, "Player number must be from 1 to %d.\n", MAX_PLAYERS);
               exit(1);
            }
            break;

         default:
            break;
      }
   }

   if (optind < argc)
   {
      print_help();
      exit(1);
   }

}

// Windows is being bitchy. Cannot include SDL.h with a file that has main() it seems ... It simply won't run at all even with -lSDLmain.
#ifdef _WIN32
int real_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
   parse_input(argc, argv);

   config_file_t *conf = config_file_new(g_in_path);
   if (!conf)
   {
      fprintf(stderr, "Couldn't open config file ...\n");
      return 1;
   }

   const char *index_list[] = { 
      "input_player1_joypad_index", 
      "input_player2_joypad_index", 
      "input_player3_joypad_index", 
      "input_player4_joypad_index", 
      "input_player5_joypad_index"
   };

   config_set_int(conf, index_list[g_player - 1], g_joypad);

   get_binds(conf, g_player - 1, g_joypad);
   config_file_write(conf, g_out_path);
   config_file_free(conf);
   if (g_in_path)
      free(g_in_path);
   if (g_out_path)
      free(g_out_path);
   return 0;
}
