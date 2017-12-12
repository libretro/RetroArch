/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#ifndef __PSP1_VIDEO_INL_H
#define __PSP1_VIDEO_INL_H

#include <pspge.h>
#include "pspgu.h"

typedef void (*GuCallback)(int);

typedef struct
{
   GuCallback sig;
   GuCallback fin;
   short signal_history[16];
   int signal_offset;
   int kernel_event_flag;
   int ge_callback_id;

   GuSwapBuffersCallback swapBuffersCallback;
   int swapBuffersBehaviour;
} GuSettings;

typedef struct
{
   unsigned int* start;
   unsigned int* current;
   int parent_context;
} GuDisplayList;

typedef struct
{
   GuDisplayList list;
   int scissor_enable;
   int scissor_start[2];
   int scissor_end[2];
   int near_plane;
   int far_plane;
   int depth_offset;
   int fragment_2x;
   int texture_function;
   int texture_proj_map_mode;
   int texture_map_mode;
   int sprite_mode[4];
   unsigned int clear_color;
   unsigned int clear_stencil;
   unsigned int clear_depth;
   int texture_mode;
} GuContext;

typedef struct
{
   int pixel_size;
   int frame_width;
   void* frame_buffer;
   void* disp_buffer;
   void* depth_buffer;
   int depth_width;
   int width;
   int height;
} GuDrawBuffer;

typedef struct
{
   /* row 0 */

   unsigned char enable;	/* Light enable */
   unsigned char type;	   /* Light type   */
   unsigned char xpos;	   /* X position   */
   unsigned char ypos;	   /* Y position   */

   /* row 1 */

   unsigned char zpos;	// Z position
   unsigned char xdir;	// X direction
   unsigned char ydir;	// Y direction
   unsigned char zdir;	// Z direction

   /* row 2 */

   unsigned char ambient;	// Ambient color
   unsigned char diffuse;	// Diffuse color
   unsigned char specular;	// Specular color
   unsigned char constant;	// Constant attenuation

   /* row 3 */

   unsigned char linear;	// Linear attenuation
   unsigned char quadratic;// Quadratic attenuation
   unsigned char exponent;	// Light exponent
   unsigned char cutoff;	// Light cutoff
} GuLightSettings;

extern unsigned int gu_current_frame;
extern GuContext gu_contexts[3];
extern int ge_list_executed[2];
extern void* ge_edram_address;
extern GuSettings gu_settings;
extern GuDisplayList* gu_list;
extern int gu_curr_context;
extern int gu_init;
extern int gu_display_on;
extern int gu_call_mode;
extern int gu_states;
extern GuDrawBuffer gu_draw_buffer;

extern unsigned int* gu_object_stack[];
extern int gu_object_stack_depth;

extern GuLightSettings light_settings[4];

static int tbpcmd_tbl[8] = { 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7 };	/* 0x30A18 */
static int tbwcmd_tbl[8] = { 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf };	/* 0x30A38 */
static int tsizecmd_tbl[8] = { 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf };	/* 0x30A58 */

#define sendCommandi(cmd, argument) *(gu_list->current++) = (cmd << 24) | (argument & 0xffffff)

#define sendCommandiStall(cmd, argument) \
{ \
   sendCommandi(cmd,argument); \
   if (!gu_object_stack_depth && !gu_curr_context) \
      sceGeListUpdateStallAddr(ge_list_executed[0],gu_list->current); \
}

#define __sceGuCopyImage(psm, sx, sy, width, height, srcw, src, dx, dy, destw, dest) \
   sendCommandi(178,((unsigned int)(src)) & 0xffffff); \
   sendCommandi(179,((((unsigned int)(src)) & 0xff000000) >> 8)|(srcw)); \
   sendCommandi(235,((sy) << 10)|(sx)); \
   sendCommandi(180,((unsigned int)(dest)) & 0xffffff); \
   sendCommandi(181,((((unsigned int)(dest)) & 0xff000000) >> 8)| (destw)); \
   sendCommandi(236,((dy) << 10) | (dx)); \
   sendCommandi(238,(((height)-1) << 10)|((width)-1)); \
   sendCommandi(234,((psm) ^ 0x03) ? 0 : 1)

#define __sceGuSync(mode, what) \
   switch (mode) \
   { \
      case 0: return sceGeDrawSync(what); \
      case 3: return sceGeListSync(ge_list_executed[0],what); \
      case 4: return sceGeListSync(ge_list_executed[1],what); \
      default: case 1: case 2: return 0; \
   }

#define __sceGuTexFlush() sendCommandf(203,0.0f)

#define __sceGuTexImage(mipmap, width, height, tbw, tbp) \
   sendCommandi(tbpcmd_tbl[(mipmap)],((unsigned int)(tbp)) & 0xffffff); \
   sendCommandi(tbwcmd_tbl[(mipmap)],((((unsigned int)(tbp)) >> 8) & 0x0f0000)|(tbw)); \
   sendCommandi(tsizecmd_tbl[(mipmap)],(getExp(height) << 8)|(getExp((width)))); \
   __sceGuTexFlush()

#define __sceGuCallList(list) \
{ \
   unsigned int list_addr = (unsigned int)list; \
   if (gu_call_mode == 1) \
   { \
      sendCommandi(14,(list_addr >> 16) | 0x110000); \
      sendCommandi(12,list_addr & 0xffff); \
      sendCommandiStall(0,0); \
   } \
   else \
   { \
      sendCommandi(16,(list_addr >> 8) & 0xf0000); \
      sendCommandiStall(10,list_addr & 0xffffff); \
   } \
}

#define __sceGuFinish_GU_DIRECT() \
   sendCommandi(15,0); \
   sendCommandiStall(12,0); \
   /* go to parent list */ \
   gu_curr_context = gu_list->parent_context; \
   gu_list = &gu_contexts[gu_curr_context].list

void sendCommandf(int cmd, float argument);

void callbackSig(int id, void* arg);
void callbackFin(int id, void* arg);
void resetValues();

#endif
