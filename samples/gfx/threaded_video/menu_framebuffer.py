#!/usr/bin/env python3
"""Execute the shipping menu loan and publication code with copy accounting."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/video_thread_wrapper.c").read_text(encoding="utf-8")
header = (root / "gfx/video_thread_wrapper.h").read_text(encoding="utf-8")
structure = header[header.index("#define VIDEO_THREAD_MENU_MEM"):]
structure = structure[:structure.index("\ntypedef struct thread_video")]
wstart = source.index("static void video_thread_menu_mem_update(")
wend = source.index("\nstatic void thread_update_driver_state(", wstart)
worker = source[wstart:wend].replace("\n#endif", "")
wstart = wend + 1
wend = source.index("\n#ifdef HAVE_OVERLAY", wstart)
worker += source[wstart:wend] + "\n}\n"
start = source.index("static struct video_thread_menu_texture *thread_menu_texture_back(")
end = source.index("\n}\n", source.index("static void thread_set_texture_frame(", start)) + 3
functions = source[start:end]
rgui = (root / "menu/drivers/rgui.c").read_text(encoding="utf-8")
start = rgui.index("static void rgui_set_texture_frame(")
end = rgui.index("\n}\n", rgui.index("static void rgui_set_texture(", start)) + 3
render = rgui[start:end]
frontend = (root / "gfx/video_driver.c").read_text(encoding="utf-8")
start = frontend.index("void *video_driver_get_menu_framebuffer(")
frontend = frontend[start:frontend.index("\n}\n", start) + 3]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <queues/retro_triple_buffer.h>
#include <rthreads/rthreads.h>
#define VIDEO_THREAD_TEXTURE_ENABLE 1
#define VIDEO_THREAD_TEXTURE_FULL_SCREEN 2
#include "gfx/video_defines.h"
STRUCTURE
struct poke;
typedef struct poke video_poke_interface_t;
typedef struct {
   const video_poke_interface_t *poke;
   void *driver_data;
   struct {
      retro_triple_buffer_t frames;
      struct video_thread_menu_texture slot[3];
      struct video_thread_menu_mem mem[VIDEO_THREAD_MENU_MEM];
      size_t refused_size;
      retro_atomic_int_t request_dims, enable;
   } texture;
} thread_video_t;
static thread_video_t *video_thread_thr_capture;
static unsigned copied, allocated;
static bool active, fail_alloc;
static bool video_driver_thread_wrapper_active(void) { return active; }
static void *counted_copy(void *dst, const void *src, size_t size)
{
   copied++;
   return memcpy(dst, src, size);
}
static void *counted_realloc(void *ptr, size_t size)
{
   allocated++;
   return fail_alloc ? NULL : realloc(ptr, size);
}
#define memcpy counted_copy
#define realloc counted_realloc
FUNCTIONS
#undef memcpy
#undef realloc

#define RGUI_UPSCALE_NONE 0
#define RGUI_UPSCALE_AUTO 1
#define GFX_DISP_FLAG_FB_DIRTY 1
typedef struct { uint16_t *data; unsigned width, height; } frame_buf_t;
typedef struct { frame_buf_t frame_buf, upscale_buf; } rgui_t;
typedef struct { unsigned flags, framebuf_dims; } gfx_display_t;
typedef struct { struct { unsigned menu_rgui_internal_upscale_level; } uints; } settings_t;
struct poke {
   void (*set_texture_frame)(void*, const void*, bool, unsigned, float);
   void *(*get_menu_framebuffer)(void*, bool, unsigned, size_t*);
   void *(*menu_mem_alloc)(void*, size_t, uintptr_t*);
   unsigned menu_mem_pitch_align;
   bool (*frame_mem_ready)(void*, uintptr_t);
   void (*frame_mem_free)(void*, uintptr_t);
   void (*set_texture_enable)(void*, bool, bool);
};
typedef struct { void *data; struct poke *poke; } video_driver_state_t;
static gfx_display_t display;
static settings_t settings;
static video_driver_state_t video_driver_st;
static unsigned viewport_dims;
static gfx_display_t *disp_get_ptr(void) { return &display; }
static settings_t *config_get_ptr(void) { return &settings; }
static video_driver_state_t *video_state_get_ptr(void) { return &video_driver_st; }
static void video_driver_get_viewport_info(struct video_viewport *vp)
{
   vp->dims = viewport_dims;
}
#define configuration_set_uint(s, field, value) ((field) = (value))
FRONTEND
RENDER
WORKER

static uint16_t direct_pixels[256];
static unsigned direct_submits;
static void *direct_get(void *data, bool rgb32, unsigned dims, size_t *pitch)
{
   (void)data;
   assert(!rgb32 && dims == VIDEO_SCALE_PACK(8, 8));
   memset(direct_pixels, 0xcd, sizeof(direct_pixels));
   *pitch = 24;
   return direct_pixels;
}
static void direct_submit(void *data, const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   unsigned x, y;
   (void)data;
   assert(frame == direct_pixels && !rgb32 && dims == VIDEO_SCALE_PACK(8, 8));
   assert(alpha == 1.0f);
   for (y = 0; y < 8; y++)
      for (x = 0; x < 12; x++)
         assert(direct_pixels[y * 12 + x] == (x < 8
               ? (uint16_t)(10 + (y / 2) * 4 + x / 2) : 0xcdcd));
   direct_submits++;
}

static void check_upscale(thread_video_t *thr)
{
   struct poke poke;
   rgui_t rgui;
   struct video_thread_menu_texture *taken;
   uint16_t pixels[16];
   unsigned level, i, x, y, width, before;
   memset(&rgui, 0, sizeof(rgui));
   poke.set_texture_frame = thread_set_texture_frame;
   poke.get_menu_framebuffer = NULL;
   video_driver_st.poke = &poke;
   video_driver_st.data = thr;
   rgui.frame_buf.data = pixels;
   for (i = 0; i < 16; i++) pixels[i] = (uint16_t)(i + 10);
   display.framebuf_dims = VIDEO_SCALE_PACK(4, 4);
   viewport_dims = VIDEO_SCALE_PACK(8, 8);
   for (level = 1; level <= 3; level++)
   {
      settings.uints.menu_rgui_internal_upscale_level = level;
      display.flags = GFX_DISP_FLAG_FB_DIRTY;
      before = copied;
      rgui_set_texture(&rgui);
      assert(before == copied && !rgui.upscale_buf.data);
      assert(!(display.flags & GFX_DISP_FLAG_FB_DIRTY));
      taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr->texture.frames);
      width = level == RGUI_UPSCALE_AUTO ? 12 : level * 4;
      assert(taken && taken->dims == VIDEO_SCALE_PACK(width, width));
      for (y = 0; y < width; y++)
         for (x = 0; x < width; x++)
         {
            unsigned sx = (x * ((4 << 16) / width)) >> 16;
            unsigned sy = (y * ((4 << 16) / width)) >> 16;
            assert(((uint16_t*)taken->frame)[y * width + x] == pixels[sy * 4 + sx]);
         }
      rgui_set_texture(&rgui);
      assert(!retro_triple_buffer_take(&thr->texture.frames));
   }
   active = false;
   settings.uints.menu_rgui_internal_upscale_level = 2;
   display.flags = GFX_DISP_FLAG_FB_DIRTY;
   before = copied;
   rgui_set_texture(&rgui);
   assert(copied == before + 1 && rgui.upscale_buf.data);
   taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr->texture.frames);
   assert(taken && taken->dims == VIDEO_SCALE_PACK(8, 8));
   assert(!memcmp(taken->frame, rgui.upscale_buf.data, 8 * 8 * sizeof(uint16_t)));
   free(rgui.upscale_buf.data);
   rgui.upscale_buf.data = NULL;
   poke.get_menu_framebuffer = direct_get;
   poke.set_texture_frame = direct_submit;
   display.flags = GFX_DISP_FLAG_FB_DIRTY;
   before = copied;
   rgui_set_texture(&rgui);
   assert(direct_submits == 1 && copied == before && !rgui.upscale_buf.data);
   poke.get_menu_framebuffer = NULL;
   poke.set_texture_frame = thread_set_texture_frame;
   active = true;
   viewport_dims = VIDEO_SCALE_PACK(2, 2);
   display.flags = GFX_DISP_FLAG_FB_DIRTY;
   rgui_set_texture(&rgui);
   taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr->texture.frames);
   assert(taken && taken->dims == VIDEO_SCALE_PACK(4, 4));
   assert(!memcmp(taken->frame, pixels, sizeof(pixels)));
}


static struct {
   void *ptr;
   size_t size, bytes;
   unsigned busy;
   uint8_t value;
} upload[VIDEO_THREAD_MENU_MEM];
static unsigned uploaded, ready_checks, mock_align;
static bool refuse_upload;
static retro_atomic_int_t producer_done;
static void *mock_menu_alloc(void *data, size_t size, uintptr_t *id)
{
   unsigned i;
   (void)data;
   if (refuse_upload) return NULL;
   for (i = 0; i < VIDEO_THREAD_MENU_MEM; i++)
      if (!upload[i].ptr)
      {
         upload[i].ptr = malloc(size);
         assert(upload[i].ptr);
         upload[i].size = size;
         upload[i].busy = 0;
         *id = i + 1;
         return upload[i].ptr;
      }
   assert(0);
   return NULL;
}
static bool mock_menu_ready(void *data, uintptr_t id)
{
   size_t x;
   unsigned i = (unsigned)id - 1;
   (void)data;
   ready_checks++;
   assert(i < VIDEO_THREAD_MENU_MEM);
   for (x = 0; x < upload[i].bytes && upload[i].busy; x++)
      assert(((uint8_t*)upload[i].ptr)[x] == upload[i].value);
   if (upload[i].busy) upload[i].busy--;
   return upload[i].busy == 0;
}
static void mock_menu_free(void *data, uintptr_t id)
{
   unsigned i = (unsigned)id - 1;
   (void)data;
   assert(i < VIDEO_THREAD_MENU_MEM && upload[i].ptr);
   free(upload[i].ptr);
   memset(&upload[i], 0, sizeof(upload[i]));
}
static void mock_menu_submit(void *data, const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   unsigned i;
   size_t x, bytes;
   uint8_t value = *(const uint8_t*)frame;
   size_t pitch = (size_t)VIDEO_SCALE_W(dims) * (rgb32 ? 4 : 2);
   for (i = 0; i < VIDEO_THREAD_MENU_MEM; i++)
      if (frame == upload[i].ptr && mock_align)
         pitch = (pitch + mock_align - 1) & ~((size_t)mock_align - 1);
   bytes = pitch * VIDEO_SCALE_H(dims);
   (void)data; (void)alpha;
   for (x = 0; x < bytes; x++) assert(((const uint8_t*)frame)[x] == value);
   for (i = 0; i < VIDEO_THREAD_MENU_MEM; i++)
      if (frame == upload[i].ptr)
      {
         assert(!upload[i].busy && bytes <= upload[i].size);
         upload[i].bytes = bytes;
         upload[i].value = value;
         upload[i].busy = 4;
         uploaded++;
      }
}
static void menu_producer(void *data)
{
   unsigned j;
   uint8_t pixels[68 * 68 * 4];
   for (j = 0; j < 100000; j++)
   {
      unsigned w = 17u << ((j / 1000) % 3);
      unsigned dims = VIDEO_SCALE_PACK(w, w);
      bool rgb32 = (j & 1) != 0;
      size_t pitch;
      void *ptr = video_thread_get_menu_framebuffer(dims, rgb32, &pitch);
      assert(ptr);
      memset(ptr, (int)(j % 251 + 1), pitch * w);
      if (j % 13 == 0) continue;
      if (j % 17 == 0)
      {
         memset(pixels, (int)(j % 251 + 1), sizeof(pixels));
         ptr = pixels;
      }
      thread_set_texture_frame(data, ptr, rgb32, dims, 1.0f);
   }
   retro_atomic_store_release_int(&producer_done, 1);
}
static void check_mapped(thread_video_t *thr, unsigned align)
{
   struct poke poke;
   sthread_t *thread;
   unsigned i, before;
   void *ptr;
   size_t pitch;
   struct video_thread_menu_texture *back;
   memset(&poke, 0, sizeof(poke));
   mock_align = align;
   uploaded = ready_checks = 0;
   retro_atomic_store_relaxed_int(&producer_done, 0);
   poke.menu_mem_pitch_align = align;
   poke.set_texture_frame = mock_menu_submit;
   poke.menu_mem_alloc = mock_menu_alloc;
   poke.frame_mem_ready = mock_menu_ready;
   poke.frame_mem_free = mock_menu_free;
   thr->poke = &poke;
   thr->driver_data = thr;
   ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, &pitch);
   assert(ptr);
   poke.menu_mem_pitch_align = 3;
   video_thread_menu_mem_update(thr);
   assert(!thr->texture.mem[0].ptr);
   poke.menu_mem_pitch_align = align;
   video_thread_menu_mem_update(thr);
   if (align)
   {
      ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, NULL);
      back = (struct video_thread_menu_texture*)retro_triple_buffer_back(&thr->texture.frames);
      assert(ptr == back->frame && !back->mem);
   }
   ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, &pitch);
   back = (struct video_thread_menu_texture*)retro_triple_buffer_back(&thr->texture.frames);
   assert(back->mem && ptr != back->frame);
   assert(pitch == (align ? align : 8));
   memset(ptr, 7, pitch * 4);
   before = copied;
   thread_set_texture_frame(thr, ptr, false, VIDEO_SCALE_PACK(4, 4), 1.0f);
   assert(copied == before);
   thread_update_driver_state(thr);
   assert(uploaded == 1);
   /* Saturate the GPU-held pool and verify CPU fallback, without waiting. */
   for (i = 1; i < VIDEO_THREAD_MENU_MEM; i++)
   {
      ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, &pitch);
      back = (struct video_thread_menu_texture*)retro_triple_buffer_back(&thr->texture.frames);
      assert(back->mem);
      memset(ptr, (int)i, pitch * 4);
      thread_set_texture_frame(thr, ptr, false, VIDEO_SCALE_PACK(4, 4), 1.0f);
      /* Hold every submitted buffer, regardless of later readiness checks. */
      thread_update_driver_state(thr);
      {
         unsigned k;
         for (k = 0; k < VIDEO_THREAD_MENU_MEM; k++)
            if (upload[k].busy) upload[k].busy = 100;
      }
   }
   ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, &pitch);
   back = (struct video_thread_menu_texture*)retro_triple_buffer_back(&thr->texture.frames);
   assert(!back->mem && ptr == back->frame);
   for (i = 0; i < VIDEO_THREAD_MENU_MEM; i++) upload[i].busy = 0;
   video_thread_menu_mem_update(thr);
   thread = sthread_create(menu_producer, thr);
   assert(thread);
   while (!retro_atomic_load_acquire_int(&producer_done)) thread_update_driver_state(thr);
   sthread_join(thread);
   thread_update_driver_state(thr);
   for (i = 0; i < 5; i++) video_thread_menu_mem_update(thr);
   assert(uploaded > VIDEO_THREAD_MENU_MEM && ready_checks);
   video_thread_menu_mem_free(thr);
   assert(!retro_atomic_load_acquire_int(&thr->texture.request_dims));
   for (i = 0; i < VIDEO_THREAD_MENU_MEM; i++)
      assert(!thr->texture.mem[i].ptr && !upload[i].ptr);
   /* Refusal is sticky until the requested size changes. */
   refuse_upload = true;
   ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(8, 8), false, &pitch);
   assert(ptr);
   video_thread_menu_mem_update(thr);
   ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(8, 8), false, &pitch);
   back = (struct video_thread_menu_texture*)retro_triple_buffer_back(&thr->texture.frames);
   assert(ptr == back->frame && !back->mem);
   refuse_upload = false;
   video_thread_menu_mem_update(thr);
   assert(!thr->texture.mem[0].ptr);
   ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, &pitch);
   video_thread_menu_mem_update(thr);
   ptr = video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, &pitch);
   back = (struct video_thread_menu_texture*)retro_triple_buffer_back(&thr->texture.frames);
   assert(ptr && back->mem);
   video_thread_menu_mem_free(thr);
   thr->poke = NULL;
   thr->driver_data = NULL;
}

int main(void)
{
   thread_video_t thr;
   struct video_thread_menu_texture *held, *taken;
   uint32_t *loan;
   uint16_t copied_frame[16];
   unsigned i, gen, before;
   memset(&thr, 0, sizeof(thr));
   retro_triple_buffer_init(&thr.texture.frames, &thr.texture.slot[0],
         &thr.texture.slot[1], &thr.texture.slot[2]);
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, NULL));
   video_thread_thr_capture = &thr;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, NULL));
   active = true;
   assert(!video_thread_get_menu_framebuffer(0, false, NULL));
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(0, 4), false, NULL));
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 0), false, NULL));
   assert(!allocated);

   fail_alloc = true;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, NULL));
   assert(!retro_triple_buffer_take(&thr.texture.frames));
   fail_alloc = false;
   loan = (uint32_t*)video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), true, NULL);
   assert(loan);
   for (i = 0; i < 16; i++) loan[i] = 0x12345678;
   before = allocated;
   thread_set_texture_frame(&thr, loan, true, VIDEO_SCALE_PACK(4, 4), 0.5f);
   assert(!copied && before == allocated);
   held = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr.texture.frames);
   assert(held && held->frame == loan && held->rgb32 && held->alpha == 0.5f);
   assert(held->dims == VIDEO_SCALE_PACK(4, 4));

   /* The consumer keeps this image while publications replace each other. */
   for (gen = 1; gen <= 1000; gen++)
   {
      uint16_t *pixels = (uint16_t*)video_thread_get_menu_framebuffer(
            VIDEO_SCALE_PACK(8, 8), false, NULL);
      assert(pixels && pixels != held->frame);
      for (i = 0; i < 64; i++) pixels[i] = (uint16_t)gen;
      thread_set_texture_frame(&thr, pixels, false, VIDEO_SCALE_PACK(8, 8), 1.0f);
      for (i = 0; i < 16; i++) assert(((uint32_t*)held->frame)[i] == 0x12345678);
   }
   assert(!copied);
   taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr.texture.frames);
   assert(taken && !taken->rgb32 && taken->dims == VIDEO_SCALE_PACK(8, 8));
   for (i = 0; i < 64; i++) assert(((uint16_t*)taken->frame)[i] == 1000);
   assert(!retro_triple_buffer_take(&thr.texture.frames));

   /* An abandoned loan stays unpublished, and growth failure preserves it. */
   loan = (uint32_t*)video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), true, NULL);
   assert(loan);
   fail_alloc = true;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(128, 128), true, NULL));
   assert(!retro_triple_buffer_take(&thr.texture.frames));
   fail_alloc = false;
   assert(video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), true, NULL) == loan);
   for (i = 0; i < 16; i++) copied_frame[i] = (uint16_t)(i + 42);
   thread_set_texture_frame(&thr, copied_frame, false, VIDEO_SCALE_PACK(4, 4), 0.25f);
   assert(copied == 1);
   taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr.texture.frames);
   assert(taken && taken->frame != copied_frame && taken->alpha == 0.25f);
   assert(!memcmp(taken->frame, copied_frame, sizeof(copied_frame)));
   thread_set_texture_frame(&thr, NULL, false, VIDEO_SCALE_PACK(4, 4), 1.0f);
   assert(!retro_triple_buffer_take(&thr.texture.frames));
   check_upscale(&thr);
   check_mapped(&thr, 0);
   check_mapped(&thr, 256);
   active = false;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, NULL));
   video_thread_thr_capture = NULL;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false, NULL));
   for (i = 0; i < 3; i++) free(thr.texture.slot[i].frame);
   puts("PASS: menu loans copy no pixels; ownership, replacement, formats and fallback");
   return 0;
}
'''
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-menu-framebuffer-") as directory:
    directory = Path(directory)
    src = directory / "menu.c"
    exe = directory / ("menu.exe" if os.name == "nt" else "menu")
    src.write_text(fixture.replace("STRUCTURE", structure).replace("FUNCTIONS", functions).replace("FRONTEND", frontend).replace("RENDER", render).replace("WORKER", worker), encoding="utf-8")
    subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror",
                   "-Werror=declaration-after-statement", "-Wno-unused-parameter", "-DHAVE_THREADS", "-DHAVE_MENU", "-I" + str(root),
                   "-I" + str(root / "libretro-common/include"), str(src), str(root / "libretro-common/rthreads/rthreads.c"),
                   "-lwinmm" if os.name == "nt" else "-lpthread", "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
