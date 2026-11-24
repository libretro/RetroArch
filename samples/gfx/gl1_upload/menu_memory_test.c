#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>
#include <memalign.h>
#include <gfx/video_frame.h>
#include "../../../gfx/video_defines.h"
#include "memory_type.inc"
#include "flags.inc"
typedef struct {
   gl1_menu_mem_t *menu_mem;
   unsigned char *menu_frame;
   const unsigned char *menu_source;
   size_t menu_frame_cap;
   unsigned flags,menu_dims,menu_pitch,menu_bits;
   bool frame_menu_linear_filter;
} gl1_t;
static unsigned copies;
static bool fail_alloc;
static void *test_alloc(size_t alignment,size_t size)
{return fail_alloc?NULL:memalign_alloc(alignment,size);}
static void *tracked_copy(void *dst,const void *src,size_t size)
{copies++;return memcpy(dst,src,size);}
#define memcpy tracked_copy
#define memalign_alloc test_alloc
#include "memory.inc"
#undef memcpy
#undef memalign_alloc
int main(void)
{
   gl1_t gl;uintptr_t ids[VIDEO_THREAD_MENU_MEM],extra;
   unsigned i,frame,rgb;uint8_t *pixels[VIDEO_THREAD_MENU_MEM];
   uint8_t fallback[5*3*4];
   memset(&gl,0,sizeof(gl));memset(fallback,39,sizeof(fallback));
   assert(!gl1_menu_mem_alloc(&gl,0,&extra)&&!extra);
   fail_alloc=true;assert(!gl1_menu_mem_alloc(&gl,60,&extra)&&!extra&&!gl.menu_mem);fail_alloc=false;
   for(i=0;i<VIDEO_THREAD_MENU_MEM;i++)
   {
      pixels[i]=(uint8_t*)gl1_menu_mem_alloc(&gl,sizeof(fallback),&ids[i]);
      assert(pixels[i]&&!((uintptr_t)pixels[i]&63));
      assert(gl1_menu_mem_ready(&gl,ids[i]));
   }
   assert(!gl1_menu_mem_alloc(&gl,1,&extra)&&!extra);
   gl1_set_texture_frame(&gl,pixels[0],true,VIDEO_SCALE_PACK(10,6),1);
   assert(!gl.menu_source&&!gl.menu_frame&&!copies);
   for(frame=0;frame<36;frame++)
   {
      unsigned slot=frame%VIDEO_THREAD_MENU_MEM;
      assert(gl1_menu_mem_ready(&gl,ids[slot]));
      memset(pixels[slot],(int)frame,sizeof(fallback));
      rgb=frame&1;gl.frame_menu_linear_filter=rgb!=0;
      gl1_set_texture_frame(&gl,pixels[slot],rgb!=0,VIDEO_SCALE_PACK(5,3),1);
      assert(gl.menu_source==pixels[slot]&&!gl.menu_frame&&!gl.menu_frame_cap&&!copies);
      assert(gl.menu_bits==(rgb?32u:16u)&&gl.menu_pitch==5*(rgb?4u:2u));
      assert(!!(gl.flags&GL1_FLAG_MENU_SMOOTH)==!!rgb);
      for(i=0;i<VIDEO_THREAD_MENU_MEM;i++)
         assert(gl1_menu_mem_ready(&gl,ids[i])==(i!=slot));
      /* No replacement: cached redraws must keep their source pinned. */
      assert(gl.menu_source[0]==frame&&!gl1_menu_mem_ready(&gl,ids[slot]));
   }
   gl1_set_texture_frame(&gl,fallback,true,VIDEO_SCALE_PACK(5,3),1);
   assert(copies==1&&gl.menu_source==gl.menu_frame&&!memcmp(gl.menu_source,fallback,sizeof(fallback)));
   for(i=0;i<VIDEO_THREAD_MENU_MEM;i++)assert(gl1_menu_mem_ready(&gl,ids[i]));
   gl1_set_texture_frame(&gl,pixels[0],true,VIDEO_SCALE_PACK(5,3),1);
   gl1_menu_mem_free(&gl,ids[0]);
   assert(!gl.menu_source&&gl1_menu_mem_ready(&gl,ids[0]));
   pixels[0]=(uint8_t*)gl1_menu_mem_alloc(&gl,sizeof(fallback)*4,&ids[0]);
   assert(pixels[0]);memset(pixels[0],72,sizeof(fallback)*4);
   gl1_set_texture_frame(&gl,pixels[0],true,VIDEO_SCALE_PACK(10,6),1);
   assert(gl.menu_source==pixels[0]&&gl.menu_dims==VIDEO_SCALE_PACK(10,6)&&copies==1);
   gl1_set_texture_frame(&gl,NULL,false,0,1);assert(gl.menu_source==pixels[0]);
   for(i=0;i<VIDEO_THREAD_MENU_MEM;i++)gl1_menu_mem_free(&gl,ids[i]);
   assert(!gl.menu_mem&&!gl.menu_source);free(gl.menu_frame);
   puts("PASS GL1 menu loans: no copies, bounded storage, retained redraws, fallback, resize and retirement");return 0;
}
