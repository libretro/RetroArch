/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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


static int g_player = 1;
static int g_joypad = 1;
static char *g_in_path = NULL;
static char *g_out_path = NULL;

static void print_help(void)
{
   puts("==================");
   puts("ssnes-joyconfig");
   puts("==================");
   puts("");
   puts("-p/--player: Which player to configure for (1 or 2).");
   puts("-j/--joypad: Which joypad to use when configuring (1 or 2).");
   puts("-i/--input: Input file to configure with. Binds will be added on or overwritten.");
   puts("\tIf not selected, an empty config will be used as a base.");
   puts("-o/--output: Output file to write to. If not selected, config file will be dumped to stdout.");
   puts("-h/--help: This help.");
}

struct bind
{
   char *keystr;
   char *confbtn[2];
   char *confaxis[2];
};

#define BIND(x, k) { x, { "input_player1_" #k "_btn", "input_player2_" #k "_btn" }, {"input_player1_" #k "_axis", "input_player2_" #k "_axis"}},
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
};

void get_binds(config_file_t *conf, int player, int joypad)
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
      fprintf(stderr, "Cannot find joystick number %d, only have %d joysticks available ...\n", joypad + 1, num);
      exit(1);
   }

   joystick = SDL_JoystickOpen(joypad);
   if (!joystick)
   {
      fprintf(stderr, "Cannot open joystick.\n");
      exit(1);
   }

   SDL_JoystickUpdate();

   int last_axis = 0xFF;
   int num_axes = SDL_JoystickNumAxes(joystick);
   int initial_axes[num_axes];
   for (int i = 0; i < num_axes; i++)
      initial_axes[i] = SDL_JoystickGetAxis(joystick, i);


   fprintf(stderr, "Configuring binds for player #%d on joypad #%d (%s)\n", player + 1, joypad + 1, SDL_JoystickName(joypad));
   fprintf(stderr, "Press Ctrl-C to exit early.\n");
   fprintf(stderr, "\n");

   for (unsigned i = 0; i < sizeof(binds) / sizeof(struct bind); i++)
   {
      fprintf(stderr, "%s\n", binds[i].keystr);

      bool done = false;
      SDL_Event event;
      int value;
      const char *quark;

      while (SDL_WaitEvent(&event) && !done)
      {
         switch (event.type)
         {
            case SDL_JOYBUTTONDOWN:
               fprintf(stderr, "\tJoybutton pressed: %d\n", (int)event.jbutton.button);
               done = true;
               config_set_int(conf, binds[i].confbtn[player], event.jbutton.button);
               break;

            case SDL_JOYAXISMOTION:
               if (abs(event.jaxis.value) > 20000 && 
                     abs((int)event.jaxis.value - initial_axes[event.jaxis.axis]) > 20000 && 
                     event.jaxis.axis != last_axis)
               {
                  last_axis = event.jaxis.axis;
                  fprintf(stderr, "\tJoyaxis moved: Axis %d, Value %d\n", (int)event.jaxis.axis, (int)event.jaxis.value);
                  done = true;

                  char buf[8];
                  snprintf(buf, sizeof(buf), event.jaxis.value > 0 ? "+%d" : "-%d", event.jaxis.axis);
                  config_set_string(conf, binds[i].confaxis[player], buf);
               }
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
               config_set_string(conf, binds[i].confbtn[player], buf);
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
   char optstring[] = "i:o:p:j:h";
   struct option opts[] = {
      { "input", 1, NULL, 'i' },
      { "output", 1, NULL, 'o' },
      { "player", 1, NULL, 'p' },
      { "joypad", 1, NULL, 'j' },
      { "help", 0, NULL, 'h' },
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

         case 'j':
            g_joypad = strtol(optarg, NULL, 0);
            if (g_joypad < 1)
            {
               fprintf(stderr, "Joypad number can't be less than 1!\n");
               exit(1);
            }
            else if (g_joypad > 2)
            {
               fprintf(stderr, "Joypad number can't be over 2! (1 or 2 allowed)\n");
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
            else if (g_player > 2)
            {
               fprintf(stderr, "Player number must be 1 or 2.\n");
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
   get_binds(conf, g_player - 1, g_joypad - 1);
   config_file_write(conf, g_out_path);
   config_file_free(conf);
   if (g_in_path)
      free(g_in_path);
   if (g_out_path)
      free(g_out_path);
   return 0;
}
