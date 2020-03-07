/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *  copyright (c) 2016-2017 - Brad Parker
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

#ifndef __FPGA_COMMON_H
#define __FPGA_COMMON_H

#define NUMBER_OF_WRITE_FRAMES 1//XPAR_AXIVDMA_0_NUM_FSTORES
#define STORAGE_SIZE NUMBER_OF_WRITE_FRAMES * ((1920*1080)<<2)
#define FRAME_SIZE (STORAGE_SIZE / NUMBER_OF_WRITE_FRAMES)

#define FB_WIDTH 1920
#define FB_HEIGHT 1080

typedef struct RegOp
{
   int fd;
   void *ptr;
   int only_mmap;
   int only_munmap;
} RegOp;

typedef struct fpga
{
   bool rgb32;
   bool menu_rgb32;
   unsigned menu_width;
   unsigned menu_height;
   unsigned menu_pitch;
   unsigned video_width;
   unsigned video_height;
   unsigned video_pitch;
   unsigned video_bits;
   unsigned menu_bits;

   RegOp regOp;
   volatile unsigned *framebuffer;
   unsigned char *menu_frame;
} fpga_t;

#endif
