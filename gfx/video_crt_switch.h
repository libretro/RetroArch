/* CRT SwitchRes Core 
 * Copyright (C) 2018 Ben Templeman.
 *
 * RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

static int ben_core_width;
static int ben_core_height;
static int ben_tmp_width;
static int ben_tmp_height;
static int orig_width;			
static int orig_height;
static float ben_core_hz;
static int set_ben_core_hz;
static float ben_tmp_core_hz;


void switch_res_core(int width, int height, float hz);
void check_first_run();
void screen_setup_aspect(int width, int height);
void switch_res_crt(int width, int height);
void aspect_ratio_switch(int width,int height);
void switch_crt_hz();
void switch_res(int width, int height, int f_restore);
void video_restore();