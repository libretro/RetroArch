/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *  Copyright (C) 2011 - Daniel De Matteis
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <debug.h>
#include <xenos/xenos.h>
#include <diskio/dvd.h>
#include <diskio/ata.h>
#include <input/input.h>
#include <console/console.h>
#include <diskio/diskio.h>
#include <usb/usbmain.h>
#include <time/time.h>
#include <ppc/timebase.h>
#include <xenon_soc/xenon_power.h>
#include <elf/elf.h>
#include <dirent.h>

#undef main

int ssnes_main(int argc, char **argv);
static void start_ssnes(const char *path)
{
   char arg0[] = "ssnes";
   char arg1[256];
   strncpy(arg1, path, sizeof(arg1));
   char *argv[3] = { arg0, arg1, NULL };
   ssnes_main(sizeof(argv) / sizeof(argv[0]) - 1, argv);
}

#define FG_COL -1
#define BG_COL 0

#define MAX_FILES 1000
#define STICK_THRESHOLD 25000
#define MAX_DISPLAYED_ENTRIES 20

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static struct dirent entries[MAX_FILES];
static int entrycount;

static void load_dir(const char *path)
{
   DIR *d = opendir(path);
   entrycount = 0;

   if (!d)
      return;

   for (struct dirent *de = readdir(d); de; de = readdir(d))
   {
      if (strcmp(de->d_name, "."))
      {
         memcpy(&entries[entrycount], de, sizeof(struct dirent));
         entrycount++;
      }
   }

   closedir(d);
}

static void append_dir_to_path(char *path, const char *dir)
{
   if (!strcmp(dir, ".."))
   {
      int i = strlen(path);
      int delimcount = 0;

      while (i >= 0 && delimcount < 2)
      {
         if (path[i] == '/')
         {
            delimcount++;

            if (delimcount > 1)
               path[i + 1]= '\0';
         }
         i--;
      }
   }
   else if (!strcmp(dir, "."))
      return;
   else
   {
      strcat(path,  dir);
      strcat(path, "/");
   }
}

int main(void)
{
   const char *s = NULL;
   char path[256];

   int handle;
   struct controller_data_s pad;
   int pos = 0, ppos = -1;

   xenos_init(VIDEO_MODE_AUTO);
   console_init();
   xenon_make_it_faster(XENON_SPEED_FULL);
   usb_init();
   usb_do_poll();
   xenon_ata_init();
   dvd_init();

   handle = -1;
   handle = bdev_enum(handle, &s);
   if (handle < 0)
      return 0;

   strcpy(path, s);
   strcat(path, ":/");	

   load_dir(path);

   for (;;)
   {
      usb_do_poll();		
      get_controller_data(&pad, 0);

      if (pad.s1_y > STICK_THRESHOLD || pad.up)
         pos--;
      if (pad.s1_y < -STICK_THRESHOLD || pad.down)
         pos++;

      if (entrycount && (pos < 0 || pos >= entrycount))
      {
         pos = ppos;
         continue;
      }

      if (pad.logo)
         return 0;

      if (pad.a)
      {
         if (entries[pos].d_type & DT_DIR)
         {
            append_dir_to_path(path,entries[pos].d_name);
            load_dir(path);
            ppos = -1;
            pos = 0;
         }
         else
         {
            char fn[256];
            strcpy(fn, path);
            strcat(fn, entries[pos].d_name);

            printf("%s\n", fn);

            start_ssnes(fn);
         }
      }

      if (pad.select)
      {
         append_dir_to_path(path, "..");
         load_dir(path);
         ppos = -1;
         pos = 0;
      }

      if (pad.b)
      {
         do
         {
            handle = bdev_enum(handle, &s);
         } while (handle < 0);

         strcpy(path, s);
         strcat(path, ":/");
         load_dir(path);
         ppos = -1;
         pos = 0;
      }

      if (ppos == pos)
         continue;

      memset(&pad, 0, sizeof(pad));

      console_set_colors(BG_COL, FG_COL);
      console_clrscr();
      printf("A: select, B: change disk, Back: parent dir, Logo: reload Xell\n\n%s\n\n", path);

      int start = MAX(0, pos - MAX_DISPLAYED_ENTRIES / 2);
      int count = MIN(MAX_DISPLAYED_ENTRIES, entrycount - start);

      for (int i = start; i < start + count; i++)
      {
         struct dirent *de = &entries[i];

         if (i == pos)
            console_set_colors(FG_COL, BG_COL);
         else
            console_set_colors(BG_COL, FG_COL);

         if (de->d_type & DT_DIR)
            console_putch('[');

         s = de->d_name;
         while (*s)
            console_putch(*s++);

         if (de->d_type & DT_DIR)
            console_putch(']');

         console_putch('\r');
         console_putch('\n');
      }

      ppos = pos;

      do
      {
         usb_do_poll();		
         get_controller_data(&pad, 0);
      } while (pad.a || pad.b || pad.select || pad.s1_y > STICK_THRESHOLD || pad.s1_y < -STICK_THRESHOLD);
   }

   return 0;
}

